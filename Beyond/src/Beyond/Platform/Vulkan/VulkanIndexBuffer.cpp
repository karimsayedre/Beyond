#include "pch.h"
#include "VulkanIndexBuffer.h"

#include "VulkanContext.h"

#include "Beyond/Renderer/Renderer.h"

namespace Beyond {


	VulkanIndexBuffer::VulkanIndexBuffer(void* data, const std::string& name, uint64_t size)
		: m_Size(size), m_DebugName(name)
	{
		m_LocalData = Buffer::Copy(data, size);

		Ref<VulkanIndexBuffer> instance = this;
		Renderer::Submit([instance]() mutable
		{
			auto device = VulkanContext::GetCurrentDevice();
			VulkanAllocator allocator("IndexBuffer");

#define USE_STAGING 1
#if USE_STAGING
			VkBufferCreateInfo bufferCreateInfo{};
			bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferCreateInfo.size = instance->m_Size;
			bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VkBuffer stagingBuffer;
			VmaAllocation stagingBufferAllocation = allocator.AllocateBuffer(bufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBuffer);

			// Copy data to staging buffer
			uint8_t* destData = allocator.MapMemory<uint8_t>(stagingBufferAllocation);
			memcpy(destData, instance->m_LocalData.Data, instance->m_LocalData.Size);
			allocator.UnmapMemory(stagingBufferAllocation);

			VkBufferCreateInfo indexBufferCreateInfo = {};
			indexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			indexBufferCreateInfo.size = instance->m_Size;
			indexBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
			instance->m_MemoryAllocation = allocator.AllocateBuffer(indexBufferCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY, instance->m_VulkanBuffer);
			VKUtils::SetDebugUtilsObjectName(device->GetVulkanDevice(), VK_OBJECT_TYPE_BUFFER, fmt::eastl_format("IndexBuffer: {}", instance->m_DebugName), instance->m_VulkanBuffer);

			instance->m_DescriptorInfo = { instance->m_VulkanBuffer, 0, VK_WHOLE_SIZE };

			VkCommandBuffer copyCmd = device->CreateCommandBuffer("Copying Index buffer from staging buffer", true);

			VkBufferCopy copyRegion = {};
			copyRegion.size = instance->m_LocalData.Size;
			vkCmdCopyBuffer(
				copyCmd,
				stagingBuffer,
				instance->m_VulkanBuffer,
				1,
				&copyRegion);

			device->FlushCommandBuffer(copyCmd);

			allocator.DestroyBuffer(stagingBuffer, stagingBufferAllocation);
			instance->m_IsReady = true;
#else
			VkBufferCreateInfo indexbufferCreateInfo = {};
			indexbufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			indexbufferCreateInfo.size = instance->m_Size;
			indexbufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

			auto bufferAlloc = allocator.AllocateBuffer(indexbufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, instance->m_VulkanBuffer);

			void* dstBuffer = allocator.MapMemory<void>(bufferAlloc);
			memcpy(dstBuffer, instance->m_LocalData.Data, instance->m_Size);
			allocator.UnmapMemory(bufferAlloc);
#endif
		});
	}

	VulkanIndexBuffer::~VulkanIndexBuffer()
	{
		VkBuffer buffer = m_VulkanBuffer;
		VmaAllocation allocation = m_MemoryAllocation;
		Renderer::SubmitResourceFree([buffer, allocation]() mutable
		{
			VulkanAllocator allocator("IndexBuffer");
			allocator.DestroyBuffer(buffer, allocation);
		});
		m_LocalData.Release();
	}

	void VulkanIndexBuffer::SetData(void* buffer, uint64_t size, uint64_t offset)
	{
		BEY_CORE_VERIFY(false, "Not implemented!");
	}

	void VulkanIndexBuffer::Bind() const
	{
	}

	RendererID VulkanIndexBuffer::GetRendererID() const
	{
		return 0;
	}

	VkDeviceAddress VulkanIndexBuffer::GetBufferDeviceAddress(const VkDevice device)
	{
		if (m_VulkanBuffer == VK_NULL_HANDLE || !m_IsReady)
			return 0ULL;

		if (m_DeviceAddress)
			return m_DeviceAddress;

		VkBufferDeviceAddressInfo info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
		info.buffer = m_VulkanBuffer;
		return m_DeviceAddress = vkGetBufferDeviceAddress(device, &info);
	}
}
