#include "pch.h"
#include "VulkanGPUSemaphore.h"

#include "Vulkan.h"
#include "VulkanContext.h"

namespace Beyond {

	Beyond::VulkanGPUSemaphore::VulkanGPUSemaphore(bool signaled)
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = nullptr,
			.flags = (signaled ? VK_FENCE_CREATE_SIGNALED_BIT : VkSemaphoreCreateFlags(0))
		};

		for (uint32_t i = 0; i < Renderer::GetConfig().FramesInFlight; i++)
			VK_CHECK_RESULT(vkCreateSemaphore(VulkanContext::GetCurrentDevice()->GetVulkanDevice(), &semaphoreCreateInfo, nullptr, &m_Semaphores[i]));
	}

	VulkanGPUSemaphore::~VulkanGPUSemaphore()
	{
		Renderer::SubmitResourceFree([instance = Ref(this)]()
		{
			for (uint32_t i = 0; i < Renderer::GetConfig().FramesInFlight; i++)
				vkDestroySemaphore(VulkanContext::GetCurrentDevice()->GetVulkanDevice(), instance->m_Semaphores[i], nullptr);
		});
	}
}
