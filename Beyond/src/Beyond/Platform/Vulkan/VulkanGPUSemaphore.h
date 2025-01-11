#pragma once
#include <vulkan/vulkan_core.h>

#include "Beyond/Renderer/GPUSemaphore.h"

namespace Beyond {

	class VulkanGPUSemaphore : public GPUSemaphore
	{
	public:
		VulkanGPUSemaphore(bool signaled = false);
		~VulkanGPUSemaphore() override;

		VkSemaphore GetSemaphore(uint32_t index) { return m_Semaphores[index]; }
	private:
		VkSemaphore m_Semaphores[3] { nullptr, nullptr, nullptr };
	};

}
