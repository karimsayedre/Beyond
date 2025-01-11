#include "pch.h"
#include "VulkanAPI.h"

#include "VulkanContext.h"

#include "Beyond/Renderer/RendererStats.h"

namespace Beyond::Vulkan {

	VkDescriptorSetAllocateInfo DescriptorSetAllocInfo(const VkDescriptorSetLayout* layouts, uint32_t count, VkDescriptorPool pool)
	{
		VkDescriptorSetAllocateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		info.pSetLayouts = layouts;
		info.descriptorSetCount = count;
		info.descriptorPool = pool;
		return info;
	}

	VkSampler CreateSampler(VkSamplerCreateInfo samplerCreateInfo)
	{
		auto device = VulkanContext::GetCurrentDevice();
		VkDevice vulkanDevice = device->GetVulkanDevice();

		VkSampler sampler;
		VK_CHECK_RESULT(vkCreateSampler(vulkanDevice, &samplerCreateInfo, nullptr, &sampler));

		++RendererUtils::GetResourceAllocationCounts().Samplers;

		return sampler;
	}

	void DestroySampler(VkSampler& sampler)
	{
		auto device = VulkanContext::GetCurrentDevice();
		VkDevice vulkanDevice = device->GetVulkanDevice();
		vkDestroySampler(vulkanDevice, sampler, nullptr);
		sampler = nullptr;

		--RendererUtils::GetResourceAllocationCounts().Samplers;
	}

}
