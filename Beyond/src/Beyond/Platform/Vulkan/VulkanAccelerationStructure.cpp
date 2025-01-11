#include "pch.h"
#include "VulkanAccelerationStructure.h"

#include <inttypes.h>

#include "VulkanContext.h"
#include "VulkanRenderCommandBuffer.h"
#include "VulkanBLAS.h"


namespace Beyond {

#if 0

	VulkanAccelerationStructure::VulkanAccelerationStructure()
	{
		Ref<VulkanAccelerationStructure> instance = this;
		Renderer::Submit([instance]() mutable
		{
			instance->RT_Invalidate();
		});
	}

	VulkanAccelerationStructure::~VulkanAccelerationStructure()
	{
		Release();
	}

	void VulkanAccelerationStructure::RT_CreateBlas(const std::vector<uint32_t>& indices, const VkDeviceSize asSize, VkDeviceAddress scratchAddress)
	{
		//VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		//VulkanAllocator allocator("Blas Allocator");

		//m_BLASes.reserve(indices.size());
		//	auto& blas = m_BLASes.emplace_back();

		//for (const auto& idx : indices)
		//{

		//	// Actual allocation of buffer and acceleration structure.
		//	VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		//	bufferInfo.size = asSize;
		//	bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		//	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		//	blas.buffer.memHandle = allocator.AllocateBuffer(bufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, blas.buffer.buffer);
		//	Beyond::VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_BUFFER, fmt::format("Blas Buffer Idx: {}", idx), blas.buffer.buffer);

		//	VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
		//	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		//	createInfo.size = asSize;  // Will be used to allocate memory.
		//	createInfo.buffer = blas.buffer.buffer;

		//	vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &blas.accel);

		//	Beyond::VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, fmt::format("Blas Idx: {}", idx), blas.accel);

		//	//blas.buildGeometryInfo.scratchData.deviceAddress = scratchAddress;  // All build are using the same scratch buffer
		//	blas.buildGeometryInfo.dstAccelerationStructure = blas.accel;  // Setting where the build lands
		//}

	}

	void VulkanAccelerationStructure::RT_Build(const VkCommandBuffer cmdBuf, const VkAccelerationStructureBuildGeometryInfoKHR& buildInfo, const VkAccelerationStructureBuildRangeInfoKHR* buildRangeInfos)
	{
		//// Building the bottom-level-acceleration-structure
		//vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &m_BLASes[idx].buildGeometryInfo, &buildRangeInfos);
		//
		//// Since the scratch buffer is reused across builds, we need a barrier to ensure one build
		//// is finished before starting the next one.
		//VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		//barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		//barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		//vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		//					 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
		//
		//if (m_QueryPool)
		//{
		//	// Add a query to find the 'real' amount of memory needed, use for compaction
		//	vkCmdWriteAccelerationStructuresPropertiesKHR(cmdBuffer, 1, &m_BLASes[idx].buildGeometryInfo.dstAccelerationStructure,
		//												  VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, m_QueryPool, queryCnt++);
		//}
	}

	Ref<VulkanBLAS> VulkanAccelerationStructure::GetBLASes() const
	{
		return m_BLASes;
	}

	void VulkanAccelerationStructure::Release()
	{
		//if (!m_MemoryAlloc)
		//	return;

		//Renderer::SubmitResourceFree([buffer = m_Buffer, memoryAlloc = m_MemoryAlloc]()
		//{
		//	{
		//		for (auto& b : m_AccelerationStructure->GetBLASes())
		//		{
		//			m_alloc.DestroyBuffer(b.buffer.buffer, b.buffer.memHandle);
		//		}
		//		m_alloc.DestroyAS(m_AccelerationStructure->GetTLAS());
		//	}
		//	m_AccelerationStructure->ClearBlases();


		//	VulkanAllocator allocator("AccelerationStructure");
		//	allocator.DestroyBuffer(buffer, memoryAlloc);
		//});

		//m_Buffer = nullptr;
		//m_MemoryAlloc = nullptr;
	}

	void VulkanAccelerationStructure::RT_Invalidate()
	{
		//Release();

		//VkBufferCreateInfo bufferInfo = {};
		//bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		//bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
		//bufferInfo.size = m_Size;

		//VulkanAllocator allocator("AccelerationStructure");
		//m_MemoryAlloc = allocator.AllocateBuffer(bufferInfo, VMA_MEMORY_USAGE_GPU_ONLY, m_Buffer);

		//VkAccelerationStructureCreateInfoKHR createInfo = {};
		//createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		//createInfo.buffer = m_Buffer;
		//createInfo.size = m_Size;
		//createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		////createInfo. = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;



		//m_DescriptorInfo.buffer = m_Buffer;
		//m_DescriptorInfo.offset = 0;
		//m_DescriptorInfo.range = m_Size;
	}

#endif

}
