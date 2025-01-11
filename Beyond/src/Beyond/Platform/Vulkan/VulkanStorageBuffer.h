#pragma once


#include "Beyond/Renderer/StorageBuffer.h"
#include "VulkanAllocator.h"
#include "Beyond/Renderer/Memory.h"

namespace Beyond {

	class VulkanStorageBuffer : public StorageBuffer
	{
	public:
		VulkanStorageBuffer(uint32_t size, const StorageBufferSpecification& specification);
		virtual ~VulkanStorageBuffer() override;

		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;
		virtual void RT_SetData(const void* data, uint32_t size, uint32_t offset = 0) override;
		virtual void Resize(uint32_t newSize) override;
		virtual void RT_Resize(uint32_t newSize) override;

		VkBuffer GetVulkanBuffer() const { return m_Buffer; }
		uint64_t GetSize() const { return m_Size; }
		VkBuffer GetUploadBuffer() const
		{
			BEY_CORE_VERIFY(m_StagingBuffer);
			return m_StagingBuffer;
		}
		VkDeviceMemory GetUploadMemory()
		{
			BEY_CORE_VERIFY(m_UploadMemory);
			return m_UploadMemory;
		}

		VmaAllocation GetAllocation() const { return m_MemoryAlloc; }

		const VkDescriptorBufferInfo& GetVulkanDescriptorInfo() const { return m_DescriptorInfo; }

	private:
		void Release();
		void RT_Invalidate();
	private:
		StorageBufferSpecification m_Specification;
		VmaAllocation m_MemoryAlloc = nullptr;
		VkBuffer m_Buffer {};
		VkDescriptorBufferInfo m_DescriptorInfo{};
		uint32_t m_Size = 0;
		//VkShaderStageFlagBits m_ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		VmaAllocation m_StagingAlloc = nullptr;

		VkBuffer m_StagingBuffer = nullptr;
		VkDeviceMemory m_UploadMemory = nullptr;

		std::vector<uint8_t> m_LocalStorage;
		//MemoryUsage m_MemoryUsage = MemoryUsage::GPU_ONLY;
	};
}
