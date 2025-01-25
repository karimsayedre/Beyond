#include "pch.h"
#include "VulkanStorageBuffer.h"

#include "VulkanAPI.h"
#include "VulkanContext.h"
#include "VulkanRenderer.h"
#include "Beyond/Core/Timer.h"
#include "Beyond/Debug/Profiler.h"
#include "Beyond/Core/Application.h"

namespace Beyond {

#define NO_STAGING 1

	VulkanStorageBuffer::VulkanStorageBuffer(uint32_t size, const StorageBufferSpecification& specification)
		: m_Specification(specification), m_Size(size)
	{
		Ref<VulkanStorageBuffer> instance = this;
		Renderer::Submit([instance]() mutable
		{
			instance->RT_Invalidate();
		});
	}

	VulkanStorageBuffer::~VulkanStorageBuffer()
	{
		Release();
	}

	void VulkanStorageBuffer::Release()
	{
		if (!m_MemoryAlloc)
			return;

		Renderer::SubmitResourceFree([buffer = m_Buffer, memoryAlloc = m_MemoryAlloc, stagingAlloc = m_StagingAlloc, stagingBuffer = m_StagingBuffer]() mutable
		{
			VulkanAllocator allocator("StorageBuffer");
			allocator.DestroyBuffer(buffer, memoryAlloc);
			if (stagingBuffer)
				allocator.DestroyBuffer(stagingBuffer, stagingAlloc);
		});

		m_Buffer = nullptr;
		m_MemoryAlloc = nullptr;
	}

	void VulkanStorageBuffer::RT_Invalidate()
	{
		Release();

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = m_Size;

		VulkanAllocator allocator("StorageBuffer");

		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		m_StagingAlloc = allocator.AllocateBuffer(bufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_StagingBuffer);
		VKUtils::SetDebugUtilsObjectName(VulkanContext::GetCurrentDevice()->GetVulkanDevice(), VK_OBJECT_TYPE_BUFFER, fmt::eastl_format("Staging buffer: {}", m_Specification.DebugName), m_StagingBuffer);

		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		m_MemoryAlloc = allocator.AllocateBuffer(bufferInfo, m_Specification.GPUOnly ? VMA_MEMORY_USAGE_GPU_ONLY : VMA_MEMORY_USAGE_CPU_TO_GPU, m_Buffer);
		VKUtils::SetDebugUtilsObjectName(VulkanContext::GetCurrentDevice()->GetVulkanDevice(), VK_OBJECT_TYPE_BUFFER, m_Specification.DebugName, m_Buffer);

		m_DescriptorInfo.buffer = m_Buffer;
		m_DescriptorInfo.offset = 0;
		m_DescriptorInfo.range = m_Size;

		//if (!m_Specification.GPUOnly)
		{
			VmaAllocationInfo info{};
			vmaGetAllocationInfo(VulkanAllocator::GetVMAAllocator(), m_MemoryAlloc, &info);
			m_UploadMemory = info.deviceMemory;
		}

#if 0
		VkBufferCreateInfo stagingBufferInfo = {};
		stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingBufferInfo.size = m_Size;

		m_StagingAlloc = allocator.AllocateBuffer(stagingBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_StagingBuffer);
#endif
	}

	void VulkanStorageBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
	{
		m_LocalStorage.resize(size);
		memcpy(m_LocalStorage.data(), data, size);
		Ref<VulkanStorageBuffer> instance = this;
		Renderer::Submit([instance, size, offset]() mutable
		{
			instance->RT_SetData(instance->m_LocalStorage.data(), size, offset);
		});
	}

	void VulkanStorageBuffer::RT_SetData(const void* data, uint32_t size, uint32_t offset)
	{
		BEY_PROFILE_FUNC();
		BEY_SCOPE_PERF("VulkanStorageBuffer::RT_SetData");

		// Cannot call SetData if GPU only
		BEY_CORE_VERIFY(!m_Specification.GPUOnly);

#if NO_STAGING
		VulkanAllocator allocator("StorageBuffer");
		uint8_t* pData = allocator.MapMemory<uint8_t>(m_MemoryAlloc);
		memcpy(pData, data, size);
		allocator.UnmapMemory(m_MemoryAlloc);
#else
		VulkanAllocator allocator("Staging");

		{
			BEY_SCOPE_PERF("VulkanStorageBuffer::RT_SetData - MemoryMap");
			uint8_t* pData = allocator.MapMemory<uint8_t>(m_StagingAlloc);
			memcpy(pData, data, size);
			allocator.UnmapMemory(m_StagingAlloc);
		}

		{
			BEY_SCOPE_PERF("VulkanStorageBuffer::RT_SetData - CommandBuffer");
			VkCommandBuffer commandBuffer = VulkanContext::GetCurrentDevice()->CreateCommandBuffer("Setting Storage buffer, from staging buffer", true);

			VkBufferCopy copyRegion = {
				0,
				offset,
				size
			};
			vkCmdCopyBuffer(commandBuffer, m_StagingBuffer, m_Buffer, 1, &copyRegion);

			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(commandBuffer);
		}
#endif
	}

	void VulkanStorageBuffer::Resize(uint32_t newSize)
	{
		if (m_Size == newSize)
			return;
		m_Size = newSize;
		Ref<VulkanStorageBuffer> instance = this;
		Renderer::Submit([instance]() mutable
		{
			instance->RT_Invalidate();
		});
	}

	void VulkanStorageBuffer::RT_Resize(uint32_t newSize)
	{
		if (m_Size == newSize)
			return;
		m_Size = newSize;
		RT_Invalidate();
	}
}
