#pragma once

#include "Beyond/Renderer/IndexBuffer.h"

#include "Beyond/Core/Buffer.h"

#include "VulkanAllocator.h"

namespace Beyond {

	class VulkanIndexBuffer : public IndexBuffer
	{
	public:
		VulkanIndexBuffer(void* data, const std::string& name, uint64_t size = 0);
		~VulkanIndexBuffer() override;

		virtual void SetData(void* buffer, uint64_t size, uint64_t offset = 0) override;
		virtual void Bind() const override;

		virtual uint64_t GetCount() const override { return m_Size / sizeof(uint32_t); }

		virtual uint64_t GetSize() const override { return m_Size; }
		virtual RendererID GetRendererID() const override;

		VkBuffer GetVulkanBuffer() { return m_VulkanBuffer; }
		VkDeviceAddress GetBufferDeviceAddress(const VkDevice device);
		const Buffer& GetData() const { return m_LocalData; }
		bool IsReady() const override { return m_IsReady; }

		ResourceDescriptorInfo GetDescriptorInfo() const { return (ResourceDescriptorInfo)&m_DescriptorInfo; }
		const VkDescriptorBufferInfo& GetVulkanDescriptorInfo() const { return m_DescriptorInfo; }

		uint32_t GetBindlessIndex() const override
		{
			BEY_CORE_VERIFY(false, "Not Implemented!");
			return 0;
		}

		uint32_t GetFlaggedBindlessIndex() const override
		{
			BEY_CORE_VERIFY(false, "Not Implemented!");
			return 0;
		}

	private:
		uint64_t m_Size = 0;
		Buffer m_LocalData;
		std::atomic_bool m_IsReady = false;
		std::string m_DebugName;

		VkDescriptorBufferInfo m_DescriptorInfo{};
		VkBuffer m_VulkanBuffer = nullptr;
		VmaAllocation m_MemoryAllocation = nullptr;
		VkDeviceAddress m_DeviceAddress{};
	};

}