#include "pch.h"
#include "VulkanTLAS.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanRenderCommandBuffer.h"
#include "VulkanStorageBuffer.h"
#include "Beyond/Core/Application.h"

namespace Beyond {

	VulkanTLAS::VulkanTLAS(bool motion, eastl::string name)
		: m_Motion(motion), m_Name(std::move(name))
	{
		//Renderer::Submit([instance = Ref(this)]() mutable
		//{
		VkAccelerationStructureMotionInfoNV motionInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MOTION_INFO_NV };
		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		RT_CreateAccelerationStructure(motionInfo, sizeInfo);

		VkSemaphoreCreateInfo semaphoreCreateInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0
		};

		for (uint32_t i = 0; i < Renderer::GetConfig().FramesInFlight; i++)
			VK_CHECK_RESULT(vkCreateSemaphore(VulkanContext::GetCurrentDevice()->GetVulkanDevice(), &semaphoreCreateInfo, nullptr, &m_TlasSemaphore[i]));
		//});
	}

	void VulkanTLAS::RT_CreateAccelerationStructure(const VkAccelerationStructureMotionInfoNV& motionInfo,
																			 const VkAccelerationStructureBuildSizesInfoKHR& sizeInfo)
	{
		Release();
		m_IsBuilt = false;
		VkDevice vkDevice = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VulkanAllocator allocator("Tlas Allocator");
		VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		createInfo.size = sizeInfo.accelerationStructureSize;
#ifdef VK_NV_ray_tracing_motion_blur
		if (m_Motion)
		{
			createInfo.createFlags = VK_ACCELERATION_STRUCTURE_CREATE_MOTION_BIT_NV;
			createInfo.pNext = &motionInfo;
		}
#endif

		// Create TLAS Buffer
		{
			// Actual allocation of buffer and acceleration structure.
			VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			bufferInfo.size = glm::max(sizeInfo.accelerationStructureSize, 1ull) + 50000;
			bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			// TODO: For some reason, setting this to GPU_ONLY breaks ray tracing when loading multiple big scenes.
			m_TLAS.buffer.memHandle = allocator.AllocateBuffer(bufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_TLAS.buffer.buffer);
			Beyond::VKUtils::SetDebugUtilsObjectName(vkDevice, VK_OBJECT_TYPE_BUFFER, fmt::eastl_format("Tlas Buffer"), m_TLAS.buffer.buffer);
		}

		createInfo.buffer = m_TLAS.buffer.buffer;
		vkCreateAccelerationStructureKHR(vkDevice, &createInfo, nullptr, &m_TLAS.accel);
		Beyond::VKUtils::SetDebugUtilsObjectName(vkDevice, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, fmt::eastl_format("Tlas: {}", m_Name), m_TLAS.accel);

		//TODO: Uncomment this so we can update the TLAS and not recreate it every frame
		m_Size = sizeInfo.accelerationStructureSize;

		m_DescriptorInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		m_DescriptorInfo.accelerationStructureCount = 1;
		m_DescriptorInfo.pAccelerationStructures = &m_TLAS.accel;
	}

	void VulkanTLAS::RT_BuildTlas(Ref<VulkanRenderCommandBuffer> commandBuffer, const eastl::vector<VkAccelerationStructureInstanceKHR>& instances,
														   VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR)
	{
		Ref<VulkanDevice> device = VulkanContext::GetCurrentDevice();
		VkDevice vkDevice = device->GetVulkanDevice();

		VulkanAllocator allocator("Tlas Allocator");

		// Create a buffer holding the actual instance data (matrices++) for use by the AS builder
		nvvk::Buffer instancesBuffer;  // Buffer of instances containing the matrices and BLAS ids
		if (!instances.empty())
		{
			// Allocate the scratch buffers holding the temporary data of the acceleration structure builder
			VkBufferCreateInfo bci{};
			bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			bci.size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);
			bci.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
			instancesBuffer.memHandle = allocator.AllocateBuffer(bci, VMA_MEMORY_USAGE_CPU_TO_GPU, instancesBuffer.buffer);
			VKUtils::SetDebugUtilsObjectName(vkDevice, VK_OBJECT_TYPE_BUFFER, fmt::eastl_format("Tlas instance buffer: {}", m_Name), instancesBuffer.buffer);

			auto* mappedData = allocator.MapMemory<VkAccelerationStructureInstanceKHR>(instancesBuffer.memHandle);
			memcpy(mappedData, instances.data(), instances.size() * sizeof(VkAccelerationStructureInstanceKHR));
			allocator.UnmapMemory(instancesBuffer.memHandle);

			RT_BuildTLAS(commandBuffer, instancesBuffer, (uint32_t)instances.size(), flags);
		}
	}

	void VulkanTLAS::RT_BuildTlas(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanStorageBuffer> storageBuffer, VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR)
	{
		uint32_t frame = Renderer::RT_GetCurrentFrameIndex();

		uint32_t countInstance = static_cast<uint32_t>(storageBuffer->GetSize() / sizeof(VkAccelerationStructureInstanceKHR));

		Ref<VulkanDevice> device = VulkanContext::GetCurrentDevice();
		VkDevice vkDevice = device->GetVulkanDevice();

		VulkanAllocator allocator("Tlas Allocator");

		// Create a buffer holding the actual instance data (matrices++) for use by the AS builder
		nvvk::Buffer instancesBuffer;  // Buffer of instances containing the matrices and BLAS ids
		{
			// Allocate the scratch buffers holding the temporary data of the acceleration structure builder
			VkBufferCreateInfo bci{};
			bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			bci.size = countInstance * sizeof(VkAccelerationStructureInstanceKHR);
			bci.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
			instancesBuffer.memHandle = allocator.AllocateBuffer(bci, VMA_MEMORY_USAGE_GPU_ONLY, instancesBuffer.buffer);
			VKUtils::SetDebugUtilsObjectName(vkDevice, VK_OBJECT_TYPE_BUFFER, fmt::eastl_format("Tlas instance buffer: {}", m_Name), instancesBuffer.buffer);

			VkBufferCopy region{ 0, 0, bci.size };
			vkCmdCopyBuffer(renderCommandBuffer->GetActiveCommandBuffer(), storageBuffer->GetVulkanBuffer(), instancesBuffer.buffer, 1, &region);


			VkBufferMemoryBarrier2 copyBarrier{};
			copyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
			copyBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			copyBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			copyBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
			copyBarrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
			copyBarrier.buffer = instancesBuffer.buffer;
			copyBarrier.offset = 0;
			copyBarrier.size = VK_WHOLE_SIZE;

			VkDependencyInfo copyDepInfo{};
			copyDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			copyDepInfo.bufferMemoryBarrierCount = 1;
			copyDepInfo.pBufferMemoryBarriers = &copyBarrier;

			vkCmdPipelineBarrier2(renderCommandBuffer->GetActiveCommandBuffer(), &copyDepInfo);
		}

		RT_BuildTLAS(renderCommandBuffer, instancesBuffer, countInstance, flags);
	}


	void VulkanTLAS::RT_BuildTLAS(Ref<VulkanRenderCommandBuffer> commandBuffer, nvvk::Buffer instancesBuffer, uint32_t instancesCount, VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR)
	{
		BEY_SCOPE_PERF("VulkanTLAS::RT_BuildTLAS");
		BEY_PROFILE_SCOPE_DYNAMIC("VulkanTLAS::RT_BuildTLAS");

		VulkanAllocator allocator("Tlas Allocator");

		// TODO: what if it's the same number but different BLASes
		m_IsBuilt = instancesCount == m_LastInstanceCount;

		m_LastInstanceCount = instancesCount;

		uint32_t frame = Renderer::RT_GetCurrentFrameIndex();

		Ref<VulkanDevice> device = VulkanContext::GetCurrentDevice();
		VkDevice vkDevice = device->GetVulkanDevice();
		VkBufferDeviceAddressInfo instanceBufferAddressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, instancesBuffer.buffer };
		VkDeviceAddress           instBufferAddr = vkGetBufferDeviceAddress(vkDevice, &instanceBufferAddressInfo);

		VkCommandBuffer   cmdBuf = commandBuffer ? commandBuffer->GetActiveCommandBuffer() : device->CreateCommandBuffer(m_Name, false, true);

		//commandBuffer->RT_Begin();
		cmdBuf = commandBuffer->GetActiveCommandBuffer();

		// Add explicit synchronization for graphics queue
		VkMemoryBarrier2 preBarrier{};
		preBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
		preBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		preBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		preBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
		preBarrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR |
			VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.memoryBarrierCount = 1;
		depInfo.pMemoryBarriers = &preBarrier;

		vkCmdPipelineBarrier2(cmdBuf, &depInfo);

		// Wraps a device pointer to the above uploaded instances.
		VkAccelerationStructureGeometryInstancesDataKHR instancesVk{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
		instancesVk.data.deviceAddress = instBufferAddr;

		// Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance data.
		VkAccelerationStructureGeometryKHR topASGeometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		topASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		topASGeometry.geometry.instances = instancesVk;

		// Find sizes
		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		buildInfo.flags = flags;
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &topASGeometry;
		buildInfo.mode = m_IsBuilt ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

		// Use consistent mode throughout
		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		vkGetAccelerationStructureBuildSizesKHR(vkDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo,
												&instancesCount, &sizeInfo);


#ifdef VK_NV_ray_tracing_motion_blur
		VkAccelerationStructureMotionInfoNV motionInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MOTION_INFO_NV };
		motionInfo.maxInstances = instancesCount;
#endif

		if (!m_IsBuilt || m_Size < sizeInfo.accelerationStructureSize)
			RT_CreateAccelerationStructure(motionInfo, sizeInfo);

		VkDeviceSize scratchSize = std::max(sizeInfo.buildScratchSize, sizeInfo.updateScratchSize);

		if (m_ScratchBuffer.buffer == nullptr || m_ScratchBuffer.memHandle == nullptr ||
			allocator.GetAllocationSize(m_ScratchBuffer.memHandle) < scratchSize)
		{
			// If we already have a scratch buffer, destroy it first
			if (m_ScratchBuffer.buffer != VK_NULL_HANDLE)
			{
				allocator.DestroyBuffer(m_ScratchBuffer.buffer, m_ScratchBuffer.memHandle);
			}



			VkBufferCreateInfo bci{};
			bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			bci.size = scratchSize;
			bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			m_ScratchBuffer.memHandle = allocator.AllocateBuffer(bci, VMA_MEMORY_USAGE_GPU_ONLY, m_ScratchBuffer.buffer);
			Beyond::VKUtils::SetDebugUtilsObjectName(vkDevice, VK_OBJECT_TYPE_BUFFER,
				fmt::eastl_format("Tlas {} scratch buffer: {}", m_IsBuilt ? "update" : "build", m_Name), m_ScratchBuffer.buffer);
		}

		VkBufferDeviceAddressInfo scratchAddressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, m_ScratchBuffer.buffer };
		VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(vkDevice, &scratchAddressInfo);

		VkDeviceSize scratchAlignment = device->GetPhysicalDevice()->GetASProps().minAccelerationStructureScratchOffsetAlignment;
		scratchAddress = (scratchAddress + scratchAlignment - 1) & ~(scratchAlignment - 1);

		// Update build information
		buildInfo.srcAccelerationStructure = m_IsBuilt ? m_TLAS.accel : VK_NULL_HANDLE;
		buildInfo.dstAccelerationStructure = m_TLAS.accel;
		buildInfo.scratchData.deviceAddress = scratchAddress;
		buildInfo.mode = m_IsBuilt ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

		// Build Offsets info: n instances
		VkAccelerationStructureBuildRangeInfoKHR        buildOffsetInfo{ instancesCount, 0, 0, 0 };
		const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

		// Build the TLAS
		vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfo, &pBuildOffsetInfo);

		// After build completion barrier
		VkMemoryBarrier2 postBarrier{};
		postBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
		postBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
		postBarrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		postBarrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		postBarrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;

		depInfo.pMemoryBarriers = &postBarrier;
		vkCmdPipelineBarrier2(cmdBuf, &depInfo);

		Renderer::SubmitResourceFree([instancesBuffer]() mutable
		{
			//Finalizing and destroying temporary data
			VulkanAllocator allocator("Tlas Allocator");
			allocator.DestroyBuffer(instancesBuffer.buffer, instancesBuffer.memHandle);
		});
		m_IsBuilt = true;
	}

	void VulkanTLAS::Release()
	{
		if (m_TLAS.accel == VK_NULL_HANDLE)
			return;
		Renderer::SubmitResourceFree([tlas = m_TLAS, scratchBuffer = m_ScratchBuffer]() mutable
		{
			VulkanAllocator allocator("Tlas De-Allocator");
			allocator.DestroyAS(tlas);
			if (scratchBuffer.buffer)
				allocator.DestroyBuffer(scratchBuffer.buffer, scratchBuffer.memHandle);
		});
		m_TLAS = {};
		m_ScratchBuffer = {};
	}

	VulkanTLAS::~VulkanTLAS()
	{
		Release();
	}
}


