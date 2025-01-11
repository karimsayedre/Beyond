#pragma once
#include <Volk/volk.h>

namespace Beyond::Utils {

	struct VulkanCheckpointData
	{
		char Data[64 + 1] {};
	};

	void SetVulkanCheckpoint(VkCommandBuffer commandBuffer, const eastl::string& data);


#ifdef BEY_DEBUG
#define SET_VULKAN_CHECKPOINT(cmdbuf, data) do { Utils::SetVulkanCheckpoint(cmdbuf, data); } while(false)
#else
#define SET_VULKAN_CHECKPOINT(cmdbuf, data) do {} while(false)
#endif
}

