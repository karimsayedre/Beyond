#include "pch.h"

#include "VulkanBLAS.h"

#include <numeric>

#include "VulkanIndexBuffer.h"
#include "VulkanVertexBuffer.h"
#include "Beyond/Asset/AssetManager.h"
#include "Beyond/Renderer/Mesh.h"

namespace Beyond {
	bool hasFlag(VkFlags item, VkFlags flag) { return (item & flag) == flag; }

	bool IsMaterialTransparency(Ref<MaterialAsset> materialAsset)
	{
		return materialAsset->GetMaterial()->GetBindlessTexture2D("u_MaterialUniforms.AlbedoTexIndex")->IsTransparent() || (materialAsset->GetMaterial()->GetVector4("u_MaterialUniforms.AlbedoColor").w < 1.0f);
		return false;
	}

	VulkanBLAS::VulkanBLAS(const Ref<VulkanBLAS> other)
		: m_Inputs(other->m_Inputs), m_Name(other->m_Name)
	{

	}

	void VulkanBLAS::GetOrCreate(const MeshSource* mesh, const MaterialAsset* materialAsset, int flags)
	{
		if (m_IsReady)
			return;
		RT_CreateBlasesInfo(mesh, materialAsset, flags);
	}

	void VulkanBLAS::RT_CreateBlasesInfo(const MeshSource* mesh, const MaterialAsset* materialAsset, int flags)
	{
		Ref<VulkanDevice> device = VulkanContext::GetCurrentDevice();
		VkDevice vkDevice = device->GetVulkanDevice();

		Ref<IndexBuffer> indexBuffer = mesh->GetIndexBuffer();
		Ref<VertexBuffer> vertexBuffer = mesh->GetVertexBuffer();

		VkDeviceAddress indexAddress = indexBuffer.As<VulkanIndexBuffer>()->GetBufferDeviceAddress(vkDevice);
		VkDeviceAddress vertexAddress = vertexBuffer.As<VulkanVertexBuffer>()->GetBufferDeviceAddress(vkDevice);
		VkDeviceSize maxScratchSize{ 0 };  // Largest scratch size

		VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
		triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		triangles.vertexData.deviceAddress = vertexAddress;
		triangles.vertexStride = sizeof(Vertex);
		triangles.indexType = VK_INDEX_TYPE_UINT32;
		triangles.indexData.deviceAddress = indexAddress;
		triangles.transformData = {};
		triangles.maxVertex = (uint32_t)(vertexBuffer->GetSize() / sizeof(Vertex)) - 1;

		// Setting up the build info of the acceleration
		VkAccelerationStructureGeometryKHR asGeom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		asGeom.geometry.triangles = triangles;

		uint32_t primitiveOffset = 0;
		uint32_t     nbCompactions{ 0 };   // Nb of BLAS requesting compaction
		VkDeviceSize asTotalSize{ 0 };     // Memory size of all allocated BLAS

		// Building part
		const auto& meshAssetSubmeshes = mesh->GetSubmeshes();
		for (const auto& subMesh : meshAssetSubmeshes)
		{
			VkAccelerationStructureBuildRangeInfoKHR offset{};
			offset.firstVertex = subMesh.BaseVertex;
			offset.primitiveCount = subMesh.IndexCount / 3;  // Nb triangles
			offset.primitiveOffset = primitiveOffset;
			offset.transformOffset = 0;

			bool hasTransparency = false;
			if (materialAsset)
				hasTransparency = materialAsset->IsTranslucent() || materialAsset->IsBlended();
			else
			{
				if (mesh->GetMaterials().size() > subMesh.MaterialIndex)
				{
					auto material = mesh->GetMaterials().at(subMesh.MaterialIndex);
					hasTransparency = material->GetFlag(MaterialFlag::Blend) || material->GetFlag(MaterialFlag::Translucent);
				}
			}

			asGeom.flags = hasTransparency ? 0 : VK_GEOMETRY_OPAQUE_BIT_KHR;


			primitiveOffset += subMesh.IndexCount / 3 * sizeof(Index);
			BEY_CORE_VERIFY(offset.primitiveOffset <= +indexBuffer->GetSize());

			auto& input = m_Inputs.emplace_back();
			input.asGeometry.emplace_back(asGeom);
			input.asBuildOffsetInfo.emplace_back(offset);

			// Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for querying the build sizes.
			// Other information will be filled in the createBlas (see #2)
			auto& buildInfo = m_SubmeshBuild.emplace_back();
			buildInfo.buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			buildInfo.buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
			buildInfo.buildInfo.flags = input.flags | static_cast<VkBuildAccelerationStructureFlagBitsKHR>(flags);
			buildInfo.buildInfo.geometryCount = static_cast<uint32_t>(input.asGeometry.size());
			buildInfo.buildInfo.pGeometries = input.asGeometry.data();

			// Build range information

			buildInfo.rangeInfo = input.asBuildOffsetInfo;

			// Finding sizes to create acceleration structures and scratch
			std::vector<uint32_t> maxPrimCount(buildInfo.rangeInfo.size());
			for (auto tt = 0ull; tt < buildInfo.rangeInfo.size(); tt++)
				maxPrimCount[tt] = buildInfo.rangeInfo[tt].primitiveCount;  // Number of primitives/triangles
			vkGetAccelerationStructureBuildSizesKHR(vkDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
													&buildInfo.buildInfo, maxPrimCount.data(), &buildInfo.sizeInfo);

			maxScratchSize = std::max(maxScratchSize, buildInfo.sizeInfo.buildScratchSize);
			nbCompactions += hasFlag(buildInfo.buildInfo.flags, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
			asTotalSize += buildInfo.sizeInfo.accelerationStructureSize;
		}

		nvvk::Buffer scratchBuffer;
		{
			// Allocate the scratch buffers holding the temporary data of the acceleration structure builder
			VulkanAllocator allocator("Blas Scratch Buffer allocator");
			VkBufferCreateInfo bci{};
			bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			bci.size = maxScratchSize;
			bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
			scratchBuffer.memHandle = allocator.AllocateBuffer(bci, VMA_MEMORY_USAGE_GPU_ONLY, scratchBuffer.buffer);
			Beyond::VKUtils::SetDebugUtilsObjectName(vkDevice, VK_OBJECT_TYPE_BUFFER, "Blas scratch buffer", scratchBuffer.buffer);
		}


		VkBufferDeviceAddressInfo bufferAddressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, scratchBuffer.buffer };
		VkDeviceAddress           scratchAddress = vkGetBufferDeviceAddress(vkDevice, &bufferAddressInfo);

		uint32_t nbBlas = (uint32_t)m_SubmeshBuild.size();
		// Allocate a query pool for storing the needed size for every BLAS compaction.
		VkQueryPool queryPool{ VK_NULL_HANDLE };
		if (nbCompactions > 0)  // Is compaction requested?
		{
			BEY_CORE_ASSERT(nbCompactions == nbBlas);  // Don't allow mix of on/off compaction
			VkQueryPoolCreateInfo qpci{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
			qpci.queryCount = nbBlas;
			qpci.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
			vkCreateQueryPool(vkDevice, &qpci, nullptr, &queryPool);
		}


		VulkanAllocator allocator("Blas allocator");

		// Batching creation/compaction of BLAS to allow staying in restricted amount of memory
		std::vector<uint32_t> indices;  // Indices of the BLAS to create
		VkDeviceSize          batchSize{ 0 };
		VkDeviceSize          batchLimit{ 256'000'000 };  // 256 MB

		for (uint32_t idx = 0; idx < nbBlas; idx++)
		{
			indices.push_back(idx);
			batchSize += m_SubmeshBuild[idx].sizeInfo.accelerationStructureSize;
			// Over the limit or last BLAS element
			if (batchSize >= batchLimit || idx == nbBlas - 1)
			{
				VkCommandBuffer cmdBuf = device->CreateCommandBuffer(fmt::eastl_format("BLAS named: {}, Creation", m_Name), true, true);
				VKUtils::SetDebugUtilsObjectName(device->GetVulkanDevice(), VK_OBJECT_TYPE_COMMAND_BUFFER, fmt::eastl_format("Creating Blas: {}:{}:{}", __FILE__, __FUNCTION__, __LINE__), cmdBuf);
				if (queryPool)  // For querying the compaction size
					vkResetQueryPool(vkDevice, queryPool, 0, static_cast<uint32_t>(indices.size()));
				uint32_t queryCnt{ 0 };

				for (const auto idx : indices)
				{
					auto& buildData = m_SubmeshBuild[idx];
					// Actual allocation of buffer and acceleration structure.
					VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
					bufferInfo.size = buildData.sizeInfo.accelerationStructureSize;
					bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
					bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
					buildData.as.buffer.memHandle = allocator.AllocateBuffer(bufferInfo, VMA_MEMORY_USAGE_GPU_ONLY, buildData.as.buffer.buffer);
					Beyond::VKUtils::SetDebugUtilsObjectName(vkDevice, VK_OBJECT_TYPE_BUFFER, fmt::eastl_format("Blas Buffer Idx: {}", idx), buildData.as.buffer.buffer);

					VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
					createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
					createInfo.size = buildData.sizeInfo.accelerationStructureSize;  // Will be used to allocate memory.
					createInfo.buffer = buildData.as.buffer.buffer;

					vkCreateAccelerationStructureKHR(vkDevice, &createInfo, nullptr, &buildData.as.accel);
					Beyond::VKUtils::SetDebugUtilsObjectName(vkDevice, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, fmt::eastl_format("Mesh: {} Blas submesh Idx: {}", mesh->GetFilePath(), idx), buildData.as.accel);

					// BuildInfo #2 part
					buildData.buildInfo.dstAccelerationStructure = buildData.as.accel;  // Setting where the build lands
					buildData.buildInfo.scratchData.deviceAddress = scratchAddress;  // All build are using the same scratch buffer

					const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo = buildData.rangeInfo.data();
					// Building the bottom-level-acceleration-structure
					vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildData.buildInfo, &rangeInfo);

					// Since the scratch buffer is reused across builds, we need a barrier to ensure one build
					// is finished before starting the next one.
					VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
					barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
					barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
					vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
										 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
					if (queryPool)
					{
						// Add a query to find the 'real' amount of memory needed, use for compaction
						vkCmdWriteAccelerationStructuresPropertiesKHR(cmdBuf, 1, &buildData.buildInfo.dstAccelerationStructure,
																	  VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, queryCnt++);
					}
				}

				device->FlushCommandBuffer(cmdBuf, device->GetComputeQueue());  // Submit command buffer and call vkQueueWaitIdle

				if (queryPool)
				{
					VkCommandBuffer cmdBuf = device->CreateCommandBuffer(fmt::eastl_format("BLAS named: {}, Compaction", m_Name), true, false);
					VKUtils::SetDebugUtilsObjectName(device->GetVulkanDevice(), VK_OBJECT_TYPE_COMMAND_BUFFER, fmt::eastl_format("Creating Compacted Blas: {}:{}:{}", __FILE__, __FUNCTION__, __LINE__), cmdBuf);
					uint32_t queryCtn{ 0 };

					// Get the compacted size result back
					std::vector<VkDeviceSize> compactSizes(static_cast<uint32_t>(indices.size()));
					VK_CHECK_RESULT(vkGetQueryPoolResults(vkDevice, queryPool, 0, (uint32_t)compactSizes.size(), compactSizes.size() * sizeof(VkDeviceSize),
						compactSizes.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT));

					m_AddressCache.reserve(indices.size());
					for (auto idx : indices)
					{
						auto& buildData = m_SubmeshBuild[idx];

						buildData.cleanupAS = buildData.as;           // previous AS to destroy
						buildData.sizeInfo.accelerationStructureSize = compactSizes[queryCtn++];  // new reduced size

						// Creating a compact version of the AS
						VkAccelerationStructureCreateInfoKHR asCreateInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
						asCreateInfo.size = buildData.sizeInfo.accelerationStructureSize;
						asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;


						// Actual allocation of buffer and acceleration structure.
						VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
						bufferInfo.size = asCreateInfo.size;
						bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
						bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
						buildData.as.buffer.memHandle = allocator.AllocateBuffer(bufferInfo, VMA_MEMORY_USAGE_GPU_ONLY, buildData.as.buffer.buffer);
						Beyond::VKUtils::SetDebugUtilsObjectName(vkDevice, VK_OBJECT_TYPE_BUFFER, fmt::eastl_format("Blas Compacted Buffer Idx: {}", idx), buildData.as.buffer.buffer);


						asCreateInfo.buffer = buildData.as.buffer.buffer;

						vkCreateAccelerationStructureKHR(vkDevice, &asCreateInfo, nullptr, &buildData.as.accel);
						Beyond::VKUtils::SetDebugUtilsObjectName(vkDevice, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, fmt::eastl_format("Mesh: {} Blas Compacted submesh Idx: {}", mesh->GetFilePath(), idx), buildData.as.accel);


						// Copy the original BLAS to a compact version
						VkCopyAccelerationStructureInfoKHR copyInfo{ VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR };
						copyInfo.src = buildData.buildInfo.dstAccelerationStructure;
						copyInfo.dst = buildData.as.accel;
						copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
						vkCmdCopyAccelerationStructureKHR(cmdBuf, &copyInfo);

						VkAccelerationStructureDeviceAddressInfoKHR addressInfo;
						addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
						addressInfo.accelerationStructure = buildData.as.accel;
						addressInfo.pNext = nullptr;
						m_AddressCache.emplace_back(vkGetAccelerationStructureDeviceAddressKHR(device->GetVulkanDevice(), &addressInfo));
					}
					device->FlushCommandBuffer(cmdBuf);
					// Destroy the non-compacted version
					for (auto& i : indices)
					{
						allocator.DestroyAS(m_SubmeshBuild[i].cleanupAS);
					}
				}

				// Reset
				batchSize = 0;
				indices.clear();
			}

		}

		// Logging reduction
		if (queryPool)
		{
			VkDeviceSize compactSize = std::accumulate(m_SubmeshBuild.begin(), m_SubmeshBuild.end(), 0ULL, [](const auto& a, const auto& b) {
				return a + b.sizeInfo.accelerationStructureSize;
			});
			const float  fractionSmaller = (asTotalSize == 0) ? 0 : float(asTotalSize - compactSize) / float(asTotalSize);

			BEY_CORE_INFO("BLAS: reducing from: {} to: {} = {} ({}% smaller) ",
				 asTotalSize,
				 compactSize, asTotalSize - compactSize, fractionSmaller * 100.f);
		}

		vkDestroyQueryPool(vkDevice, queryPool, nullptr);

		allocator.DestroyBuffer(scratchBuffer.buffer, scratchBuffer.memHandle);
		m_IsReady = true;
	}

	VulkanBLAS::~VulkanBLAS()
	{
		Renderer::SubmitResourceFree([blases = m_SubmeshBuild]()
		{
			VulkanAllocator allocator("Blas De-Allocator");
			for (auto blas : blases)
			{
				allocator.DestroyAS(blas.as);
			}
		});
	}

	VkDeviceAddress VulkanBLAS::GetBLASAddress(size_t submeshIndex) const noexcept
	{
		if (m_SubmeshBuild[submeshIndex].as.accel == VK_NULL_HANDLE)
			BEY_CORE_VERIFY(false);

		return m_AddressCache[submeshIndex];

		/*VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VkAccelerationStructureDeviceAddressInfoKHR info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
		info.accelerationStructure = m_SubmeshBuild[submeshIndex].as.accel;
		return vkGetAccelerationStructureDeviceAddressKHR(device, &info);*/
	}
}
