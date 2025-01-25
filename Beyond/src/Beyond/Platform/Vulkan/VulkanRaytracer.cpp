#include "pch.h"
#include "VulkanRaytracer.h"
#include "Beyond/Renderer/Mesh.h"
#include "Beyond/Asset/AssetManager.h"

#include "VulkanBLAS.h"
#include "VulkanContext.h"
#include "VulkanIndexBuffer.h"
#include "VulkanMaterial.h"
#include "VulkanRenderCommandBuffer.h"
#include "VulkanTLAS.h"
#include "Beyond/Debug/Profiler.h"
#include "Beyond/Renderer/RaytracingPass.h"

namespace Beyond {

	VulkanRaytracer::VulkanRaytracer(const Ref<AccelerationStructureSet> as)
		: m_AccelerationStructureSet(as)
	{
	}

	void VulkanRaytracer::AddDrawCommand(const StaticDrawCommand& dc, const MaterialAsset* material, const glm::mat3x4& transform)
	{
		//BEY_SCOPE_PERF("ulkanRaytracer::AddDrawCommand, static");
		//Renderer::Submit([dc, transform, instance = Ref(this), pass]() mutable
		{
			BEY_PROFILE_SCOPE("VulkanRaytracer::AddDrawCommand(STATIC)");
			//VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			const MeshSource* meshAsset = dc.StaticMesh->GetMeshSourceRaw();
			if (!dc.StaticMesh->IsReady())
				return;
			const auto& meshAssetSubmeshes = meshAsset->GetSubmeshes();
			const Submesh& submesh = meshAssetSubmeshes[dc.SubmeshIndex];



			auto& vulkanBuffers = m_VulkanBuffers[meshAsset->Handle];
			if (!vulkanBuffers.VertexBuffer) // No need to check the other
			{
				vulkanBuffers.VertexBuffer = meshAsset->GetVertexBuffer();
				vulkanBuffers.IndexBuffer = meshAsset->GetIndexBuffer();

				{
					RenderPassInput input;
					input.Name = "ByteAddrBuffer";
					vulkanBuffers.VertexIndex = m_DynamicBufferIndex++;
					input.Input[vulkanBuffers.VertexIndex] = vulkanBuffers.VertexBuffer;
					input.Type = RenderPassResourceType::VertexBuffer;
					Renderer::AddBindlessDescriptor(std::move(input));
				}

				{

					RenderPassInput input;
					input.Name = "ByteAddrBuffer";
					vulkanBuffers.IndexIndex = m_DynamicBufferIndex++;
					input.Input[vulkanBuffers.IndexIndex] = vulkanBuffers.IndexBuffer;
					input.Type = RenderPassResourceType::IndexBuffer;
					Renderer::AddBindlessDescriptor(std::move(input));
				}
			}

			int blasFlags = AllowCompaction | PreferFastTrace;

			const auto blasKey = BLASKey(meshAsset, material->IsTranslucent(), material->IsTwoSided());
			Ref<VulkanBLAS>& blas = m_BLASes[blasKey];
			if (!blas)
				blas = Ref<VulkanBLAS>::Create();

			blas->GetOrCreate(meshAsset, material, blasFlags);

			MaterialBuffer buffer;
			const VulkanMaterial* vulkanMaterial = (VulkanMaterial*)material->GetMaterialRaw();
			const Buffer& materialBuffer = vulkanMaterial->GetUniformStorageBuffer();
			std::memcpy(&buffer, ((std::byte*)materialBuffer.Data), sizeof(MaterialBuffer));
			//BEY_CORE_ASSERT(materialBuffer.GetSize() == sizeof(MaterialBuffer));

			//for (uint32_t i = 0; i < dc.InstanceCount; i++)
			{
				//RT_AddObjDescs(rayMesh, blas->GetBLASAddress(dc.SubmeshIndex));
				const uint32_t instanceIndex = (uint32_t)m_SceneInstances.size();

				m_SceneInstances.emplace_back(transform, instanceIndex);

				VkAccelerationStructureInstanceKHR rayInst{};
				rayInst.transform = std::bit_cast<VkTransformMatrixKHR>(transform);
				rayInst.instanceCustomIndex = instanceIndex;               // gl_InstanceCustomIndexEXT
				//rayInst.instanceCustomIndex |= (uint8_t)volume->GetIndex() << 16;  // volume index in last 8 bits
				rayInst.mask = 0xff;
				rayInst.accelerationStructureReference = blas->GetBLASAddress(dc.SubmeshIndex);
				rayInst.flags = 0x00; // TODO
				rayInst.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
				m_VulkanInstances.emplace_back(rayInst);


				m_Materials.emplace_back(buffer);



				ObjDesc desc;
				desc.VertexBufferIndex = vulkanBuffers.VertexIndex;
				desc.IndexBufferIndex = vulkanBuffers.IndexIndex;
				desc.FirstVertex = submesh.BaseVertex;
				desc.FirstIndex = submesh.BaseIndex;
				desc.MaterialIndex = instanceIndex;

				m_objDesc.emplace_back(desc);
			}
		}
		//);
	}

	void VulkanRaytracer::AddDrawCommand(const DrawCommand& dc, const MaterialAsset* material, const glm::mat3x4& transform)
	{
		//BEY_SCOPE_PERF("VulkanRaytracer::AddDrawCommand, dynamic");
		//Renderer::Submit([dc, transform, instance = Ref(this), pass]() mutable
		{
			BEY_PROFILE_SCOPE("VulkanRaytracer::AddDrawCommand()");
			//VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			const MeshSource* meshAsset = dc.Mesh->GetMeshSourceRaw();
			if (!dc.Mesh->IsReady())
				return;
			const auto& meshAssetSubmeshes = meshAsset->GetSubmeshes();
			const Submesh& submesh = meshAssetSubmeshes[dc.SubmeshIndex];

			auto& vulkanBuffers = m_VulkanBuffers[meshAsset->Handle];
			if (!vulkanBuffers.VertexBuffer) // No need to check the other
			{
				vulkanBuffers.VertexBuffer = meshAsset->GetVertexBuffer();
				vulkanBuffers.IndexBuffer = meshAsset->GetIndexBuffer();

				{
					RenderPassInput input;
					input.Name = "ByteAddrBuffer";
					vulkanBuffers.VertexIndex = m_DynamicBufferIndex++;
					input.Input[vulkanBuffers.VertexIndex] = vulkanBuffers.VertexBuffer;
					input.Type = RenderPassResourceType::VertexBuffer;
					Renderer::AddBindlessDescriptor(std::move(input));
				}

				{
					RenderPassInput input;
					input.Name = "ByteAddrBuffer";
					vulkanBuffers.IndexIndex = m_DynamicBufferIndex++;
					input.Input[vulkanBuffers.IndexIndex] = vulkanBuffers.IndexBuffer;
					input.Type = RenderPassResourceType::IndexBuffer;
					Renderer::AddBindlessDescriptor(std::move(input));
				}
			}

			constexpr int blasFlags = AllowCompaction | PreferFastTrace;

			const auto blasKey = BLASKey(meshAsset, material->IsTranslucent(), material->IsTwoSided());
			Ref<VulkanBLAS>& blas = m_BLASes[blasKey];
			if (!blas)
				blas = Ref<VulkanBLAS>::Create();

			blas->GetOrCreate(meshAsset, material, blasFlags);


			MaterialBuffer buffer;
			const VulkanMaterial* vulkanMaterial = (VulkanMaterial*)material->GetMaterialRaw();
			const Buffer& materialBuffer = vulkanMaterial->GetUniformStorageBuffer();
			std::memcpy(&buffer, ((std::byte*)materialBuffer.Data), sizeof(MaterialBuffer));
			BEY_CORE_ASSERT(materialBuffer.GetSize() == sizeof(MaterialBuffer));

			//for (uint32_t i = 0; i < dc.InstanceCount; i++)
			{
				const uint32_t instanceIndex = (uint32_t)m_SceneInstances.size();

				m_SceneInstances.emplace_back(transform, instanceIndex);

				VkAccelerationStructureInstanceKHR rayInst{};
				rayInst.transform = std::bit_cast<VkTransformMatrixKHR>(transform);
				rayInst.instanceCustomIndex = instanceIndex;               // gl_InstanceCustomIndexEXT
				//rayInst.instanceCustomIndex |= (uint8_t)volume->GetIndex() << 16;  // volume index in last 8 bits
				rayInst.mask = 0xff;
				rayInst.accelerationStructureReference = blas->GetBLASAddress(dc.SubmeshIndex);
				rayInst.flags = 0x00; // TODO
				rayInst.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
				m_VulkanInstances.emplace_back(rayInst);
				m_Materials.emplace_back(buffer);


				ObjDesc desc;
				desc.VertexBufferIndex = vulkanBuffers.VertexIndex;
				desc.IndexBufferIndex = vulkanBuffers.IndexIndex;
				desc.FirstVertex = submesh.BaseVertex;
				desc.FirstIndex = submesh.BaseIndex;
				desc.MaterialIndex = instanceIndex;

				m_objDesc.emplace_back(desc);
			}

		}
		//);
	}

	void VulkanRaytracer::AddInstancedDrawCommand(const StaticDrawCommand& dc, const glm::mat3x4& transform)
	{
		//Renderer::Submit([dc, transform, instance = Ref(this), pass]() mutable
		{
			BEY_PROFILE_SCOPE("VulkanRaytracer::AddInstancedDrawCommand()");
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			const MeshSource* meshAsset = dc.StaticMesh->GetMeshSourceRaw();
			if (!dc.StaticMesh->IsReady())
				return;
			const auto& meshAssetSubmeshes = meshAsset->GetSubmeshes();
			const Submesh& submesh = meshAssetSubmeshes[dc.SubmeshIndex];

			auto& vulkanBuffers = m_VulkanBuffers[meshAsset->Handle];
			if (!vulkanBuffers.VertexBuffer) // No need to check the other
			{
				vulkanBuffers.VertexBuffer = meshAsset->GetVertexBuffer();
				vulkanBuffers.IndexBuffer = meshAsset->GetIndexBuffer();

				{
					RenderPassInput input;
					input.Name = "ByteAddrBuffer";
					vulkanBuffers.VertexIndex = m_DynamicBufferIndex++;
					input.Input[vulkanBuffers.VertexIndex] = vulkanBuffers.VertexBuffer;
					input.Type = RenderPassResourceType::VertexBuffer;
					Renderer::AddBindlessDescriptor(std::move(input));
				}

				{

					RenderPassInput input;
					input.Name = "ByteAddrBuffer";
					vulkanBuffers.IndexIndex = m_DynamicBufferIndex++;
					input.Input[vulkanBuffers.IndexIndex] = vulkanBuffers.IndexBuffer;
					input.Type = RenderPassResourceType::IndexBuffer;
					Renderer::AddBindlessDescriptor(std::move(input));
				}
			}

			constexpr int blasFlags = AllowCompaction | PreferFastTrace;

			const auto blasKey = BLASKey(meshAsset, false, false);
			Ref<VulkanBLAS>& blas = m_BLASes[blasKey];
			if (!blas)
				blas = Ref<VulkanBLAS>::Create();


			blas->GetOrCreate(meshAsset, m_DefaultMaterial.Raw(), blasFlags);


			for (uint32_t instance = 0; instance < dc.InstanceCount; instance++)
			{

				m_SceneInstances.emplace_back(transform, instance);

				VkAccelerationStructureInstanceKHR rayInst{};
				rayInst.transform = std::bit_cast<VkTransformMatrixKHR>(transform);
				rayInst.instanceCustomIndex = instance;               // gl_InstanceCustomIndexEXT
				//rayInst.instanceCustomIndex |= (uint8_t)volume->GetIndex() << 16;  // volume index in last 8 bits
				rayInst.mask = 0x2;
				rayInst.accelerationStructureReference = blas->GetBLASAddress(dc.SubmeshIndex);
				rayInst.flags = 0x00; // TODO
				rayInst.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
				m_VulkanInstances.emplace_back(rayInst);


				//ObjDesc desc;
				//desc.VertexBufferIndex = vulkanBuffers.VertexIndex;
				//desc.IndexBufferIndex = vulkanBuffers.IndexIndex;
				//desc.FirstVertex = submesh.BaseVertex;
				//desc.FirstIndex = submesh.BaseIndex;
				//desc.MaterialIndex = instance;

				//m_objDesc.emplace_back(desc);
			}

		}
		//);
	}

	void VulkanRaytracer::BuildTlas(Ref<RenderCommandBuffer> commandBuffer)
	{
		Renderer::Submit([commandBuffer, instance = Ref(this), instances = m_VulkanInstances]() mutable
		{

			/*if (instances.empty())
				return;*/
			instance->m_AccelerationStructureSet->RT_Get().As<VulkanTLAS>()->RT_BuildTlas(commandBuffer, instances,
				VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR);
		});
	}

	void VulkanRaytracer::BuildTlas(Ref<RenderCommandBuffer> commandBuffer, Ref<StorageBuffer> storageBuffer)
	{
		Renderer::Submit([commandBuffer = Ref<VulkanRenderCommandBuffer>(commandBuffer), instance = Ref(this), storageBuffer]() mutable
		{

			BEY_CORE_ASSERT(storageBuffer->GetSize());
			instance->m_AccelerationStructureSet->RT_Get().As<VulkanTLAS>()->RT_BuildTlas(commandBuffer, storageBuffer,
				VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR);
		});
	}
}
