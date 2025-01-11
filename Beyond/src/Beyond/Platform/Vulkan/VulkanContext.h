#pragma once
#include "VulkanSwapChain.h"
#include "Beyond/Renderer/Renderer.h"
#include "Beyond/Renderer/RendererContext.h"


struct GLFWwindow;

namespace Beyond {
	class VulkanPhysicalDevice;
	class VulkanDevice;


	class VulkanContext : public RendererContext
	{
	public:
		VulkanContext();
		virtual ~VulkanContext();

		virtual void Init() override;

		Ref<VulkanDevice> GetDevice() { return m_Device; }

		static VkInstance GetInstance() { return s_VulkanInstance; }

		static Ref<VulkanContext> Get() { return Ref<VulkanContext>(Renderer::GetContext()); }
		static Ref<VulkanDevice> GetCurrentDevice() { return Get()->GetDevice(); }

		static const VkPhysicalDeviceLimits& GetLimits() { return VulkanContext::GetCurrentDevice()->GetPhysicalDevice()->GetLimits();  }
		static const VkPhysicalDeviceAccelerationStructurePropertiesKHR& GetASProps() { return VulkanContext::GetCurrentDevice()->GetPhysicalDevice()->GetASProps();  }
		static const VkPhysicalDeviceVulkan12Properties& GetProps12() { return VulkanContext::GetCurrentDevice()->GetPhysicalDevice()->GetProperties12();  }

	private:
		// Devices
		Ref<VulkanPhysicalDevice> m_PhysicalDevice;
		Ref<VulkanDevice> m_Device;

		// Vulkan instance
		inline static VkInstance s_VulkanInstance;
		VkDebugUtilsMessengerEXT m_DebugUtilsMessenger = VK_NULL_HANDLE;
		VkPipelineCache m_PipelineCache = nullptr;

		VulkanSwapChain m_SwapChain;
	};
}
