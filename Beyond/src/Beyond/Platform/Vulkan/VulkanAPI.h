#pragma once

#include <vulkan/vulkan.h>
#include "Beyond/Platform/Vulkan/VulkanFormatting.h"

namespace Beyond::Vulkan {

	VkDescriptorSetAllocateInfo DescriptorSetAllocInfo(const VkDescriptorSetLayout* layouts, uint32_t count = 1, VkDescriptorPool pool = nullptr);

	VkSampler CreateSampler(VkSamplerCreateInfo samplerCreateInfo);
	void DestroySampler(VkSampler& sampler);

}
