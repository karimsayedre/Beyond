#pragma once

#include "Beyond/Renderer/UniformBuffer.h"

#include "VulkanAllocator.h"
#include "Beyond/Renderer/Memory.h"

namespace Beyond {

	class VulkanUniformBuffer : public UniformBuffer
	{
	public:
		VulkanUniformBuffer(uint32_t size, const eastl::string& debugName);
		virtual ~VulkanUniformBuffer();

		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;
		virtual void RT_SetData(const void* data, uint32_t size, uint32_t offset = 0) override;

		const VkDescriptorBufferInfo& GetDescriptorInfo() const { return m_DescriptorInfo; }
		//virtual ResourceDescriptorInfo GetDescriptorInfo() const { return (ResourceDescriptorInfo)&m_DescriptorInfo; }
		const VkDescriptorBufferInfo& GetVulkanDescriptorInfo() const { return *(VkDescriptorBufferInfo*)&GetDescriptorInfo(); }
	private:
		void Release();
		void RT_Invalidate();
	private:
		VmaAllocation m_MemoryAlloc = nullptr;
		VkBuffer m_Buffer;
		VkDescriptorBufferInfo m_DescriptorInfo{};
		uint32_t m_Size = 0;
		eastl::string m_Name;
		VkShaderStageFlagBits m_ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		MemoryUsage m_MemoryUsage = MemoryUsage::CPU_TO_GPU;
		uint8_t* m_LocalStorage = nullptr;
	};
}
