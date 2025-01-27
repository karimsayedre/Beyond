#include "pch.h"
#include "VulkanSwapChain.h"

#include "Beyond/Debug/Profiler.h"

#include <GLFW/glfw3.h>

#include "Beyond/Core/Application.h"


namespace Beyond {

	void VulkanSwapChain::Init(VkInstance instance, const Ref<VulkanDevice>& device)
	{
		m_Instance = instance;
		m_Device = device;
	}

	void VulkanSwapChain::InitSurface(GLFWwindow* windowHandle)
	{
		VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice()->GetVulkanPhysicalDevice();

		glfwCreateWindowSurface(m_Instance, windowHandle, nullptr, &m_Surface);

		// Get available queue family properties
		uint32_t queueCount;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);
		BEY_CORE_ASSERT(queueCount >= 1);

		std::vector<VkQueueFamilyProperties> queueProps(queueCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProps.data());

		// Iterate over each queue to learn whether it supports presenting:
		// Find a queue with present support
		// Will be used to present the swap chain images to the windowing system
		std::vector<VkBool32> supportsPresent(queueCount);
		for (uint32_t i = 0; i < queueCount; i++)
		{
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_Surface, &supportsPresent[i]);
		}

		// Search for a graphics and a present queue in the array of queue
		// families, try to find one that supports both
		uint32_t graphicsQueueNodeIndex = UINT32_MAX;
		uint32_t presentQueueNodeIndex = UINT32_MAX;
		for (uint32_t i = 0; i < queueCount; i++)
		{
			if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
			{
				if (graphicsQueueNodeIndex == UINT32_MAX)
				{
					graphicsQueueNodeIndex = i;
				}

				if (supportsPresent[i] == VK_TRUE)
				{
					graphicsQueueNodeIndex = i;
					presentQueueNodeIndex = i;
					break;
				}
			}
		}
		if (presentQueueNodeIndex == UINT32_MAX)
		{
			// If there's no queue that supports both present and graphics
			// try to find a separate present queue
			for (uint32_t i = 0; i < queueCount; ++i)
			{
				if (supportsPresent[i] == VK_TRUE)
				{
					presentQueueNodeIndex = i;
					break;
				}
			}
		}

		BEY_CORE_ASSERT(graphicsQueueNodeIndex != UINT32_MAX);
		BEY_CORE_ASSERT(presentQueueNodeIndex != UINT32_MAX);

		m_QueueNodeIndex = graphicsQueueNodeIndex;

		FindImageFormatAndColorSpace();
	}

	void VulkanSwapChain::Create(uint32_t* width, uint32_t* height, bool vsync)
	{
		m_VSync = vsync;

		VkDevice device = m_Device->GetVulkanDevice();
		VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice()->GetVulkanPhysicalDevice();

		VkSwapchainKHR oldSwapchain = m_SwapChain;

		// Get physical device surface properties and formats
		VkSurfaceCapabilitiesKHR surfCaps;
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &surfCaps));

		// Get available present modes
		uint32_t presentModeCount;
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, NULL));
		BEY_CORE_ASSERT(presentModeCount > 0);
		std::vector<VkPresentModeKHR> presentModes(presentModeCount);
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, presentModes.data()));

		VkExtent2D swapchainExtent = {};
		// If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
		if (surfCaps.currentExtent.width == (uint32_t)-1)
		{
			// If the surface size is undefined, the size is set to
			// the size of the images requested.
			swapchainExtent.width = *width;
			swapchainExtent.height = *height;
		}
		else
		{
			// If the surface size is defined, the swap chain size must match
			swapchainExtent = surfCaps.currentExtent;
			*width = surfCaps.currentExtent.width;
			*height = surfCaps.currentExtent.height;
		}

		m_Width = *width;
		m_Height = *height;

		if (*width == 0 || *height == 0)
			return;

		// Select a present mode for the swapchain

		// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
		// This mode waits for the vertical blank ("v-sync")
		VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

		// If v-sync is not requested, try to find a mailbox mode
		// It's the lowest latency non-tearing present mode available
		if (!vsync)
		{
			for (size_t i = 0; i < presentModeCount; i++)
			{
				if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
				{
					swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
					break;
				}
				if ((swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) && (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
				{
					swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
				}
			}
		}

		// Determine the number of images
		uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;
		if ((surfCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCaps.maxImageCount))
		{
			desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
		}

		// Find the transformation of the surface
		VkSurfaceTransformFlagsKHR preTransform;
		if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
		{
			// We prefer a non-rotated transform
			preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		}
		else
		{
			preTransform = surfCaps.currentTransform;
		}

		// Find a supported composite alpha format (not all devices support alpha opaque)
		VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		// Simply select the first composite alpha format available
		std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		};
		for (auto& compositeAlphaFlag : compositeAlphaFlags)
		{
			if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag)
			{
				compositeAlpha = compositeAlphaFlag;
				break;
			};
		}

		VkSwapchainCreateInfoKHR swapchainCI = {};
		swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCI.pNext = NULL;
		swapchainCI.surface = m_Surface;
		swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
		swapchainCI.imageFormat = m_ColorFormat;
		swapchainCI.imageColorSpace = m_ColorSpace;
		swapchainCI.imageExtent = { swapchainExtent.width, swapchainExtent.height };
		swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
		swapchainCI.imageArrayLayers = 1;
		swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCI.queueFamilyIndexCount = 0;
		swapchainCI.pQueueFamilyIndices = NULL;
		swapchainCI.presentMode = swapchainPresentMode;
		swapchainCI.oldSwapchain = oldSwapchain;
		// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
		swapchainCI.clipped = VK_TRUE;
		swapchainCI.compositeAlpha = compositeAlpha;

		// Enable transfer source on swap chain images if supported
		if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		{
			swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}

		// Enable transfer destination on swap chain images if supported
		if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		{
			swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}

		VkResult result = vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &m_SwapChain);
		VK_CHECK_RESULT(result);
		if (result != VK_SUCCESS)
		{
			// Handle swapchain creation failure
			return;
		}

		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_SWAPCHAIN_KHR, "Swapchain", m_SwapChain);

		// Clean up old resources
		for (auto& image : m_Images)
		{
			if (image.ImageView != VK_NULL_HANDLE)
			{
				vkDestroyImageView(device, image.ImageView, nullptr);
				image.ImageView = VK_NULL_HANDLE;
			}
		}
		if (oldSwapchain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
			oldSwapchain = VK_NULL_HANDLE;
		}

		m_Images.clear();
		VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device, m_SwapChain, &m_ImageCount, nullptr));
		m_Images.resize(m_ImageCount);
		std::vector<VkImage> swapChainImages(m_ImageCount);
		VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device, m_SwapChain, &m_ImageCount, swapChainImages.data()));

		for (uint32_t i = 0; i < m_ImageCount; i++)
		{
			m_Images[i].Image = swapChainImages[i];
			VkImageViewCreateInfo colorAttachmentView = {};
			colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			colorAttachmentView.pNext = nullptr;
			colorAttachmentView.format = m_ColorFormat;
			colorAttachmentView.image = swapChainImages[i];
			colorAttachmentView.components = {
				VK_COMPONENT_SWIZZLE_R,
				VK_COMPONENT_SWIZZLE_G,
				VK_COMPONENT_SWIZZLE_B,
				VK_COMPONENT_SWIZZLE_A
			};
			colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorAttachmentView.subresourceRange.baseMipLevel = 0;
			colorAttachmentView.subresourceRange.levelCount = 1;
			colorAttachmentView.subresourceRange.baseArrayLayer = 0;
			colorAttachmentView.subresourceRange.layerCount = 1;
			colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			colorAttachmentView.flags = 0;

			result = vkCreateImageView(device, &colorAttachmentView, nullptr, &m_Images[i].ImageView);
			if (result != VK_SUCCESS)
			{
				// Handle image view creation failure
				// You might want to log the error or take appropriate action
				return;
			}

			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, fmt::eastl_format("Swapchain ImageView: {}", i), m_Images[i].ImageView);
		}

		// Create command buffers
		{
			for (auto& commandBuffer : m_CommandBuffers)
				vkDestroyCommandPool(device, commandBuffer.CommandPool, nullptr);

			VkCommandPoolCreateInfo cmdPoolInfo = {};
			cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			cmdPoolInfo.queueFamilyIndex = m_QueueNodeIndex;
			cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

			VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
			commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandBufferAllocateInfo.commandBufferCount = 1;

			m_CommandBuffers.resize(m_ImageCount);
			uint32_t frame = 0;
			for (auto& commandBuffer : m_CommandBuffers)
			{
				VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &commandBuffer.CommandPool));
				VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_COMMAND_POOL, fmt::eastl_format("Swap chain command pool, frame: {}", frame), commandBuffer.CommandPool);
				commandBufferAllocateInfo.commandPool = commandBuffer.CommandPool;
				VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer.CommandBuffer));
				VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_COMMAND_BUFFER, fmt::eastl_format("Swap chain command Buffer, frame: {}", frame), commandBuffer.CommandBuffer);
				frame++;
			}
		}

		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Synchronization Objects
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		{
			VkSemaphoreCreateInfo semaphoreCreateInfo{};
			semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			const auto& config = Renderer::GetConfig();
			m_Semaphores.RenderComplete.resize(config.FramesInFlight);
			m_Semaphores.PresentComplete.resize(config.FramesInFlight);

			for (uint32_t i = 0; i < config.FramesInFlight; i++)
			{
				VK_CHECK_RESULT(vkCreateSemaphore(m_Device->GetVulkanDevice(), &semaphoreCreateInfo, nullptr, &m_Semaphores.RenderComplete[i]));
				VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, fmt::eastl_format("Swapchain Semaphore RenderComplete {}", i), m_Semaphores.RenderComplete[i]);

				VK_CHECK_RESULT(vkCreateSemaphore(m_Device->GetVulkanDevice(), &semaphoreCreateInfo, nullptr, &m_Semaphores.PresentComplete[i]));
				VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, fmt::eastl_format("Swapchain Semaphore PresentComplete {}", i), m_Semaphores.PresentComplete[i]);
			}
		}

		if (m_WaitFences.size() != m_ImageCount)
		{
			VkFenceCreateInfo fenceCreateInfo{};
			fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			m_WaitFences.resize(m_ImageCount);
			for (auto& fence : m_WaitFences)
			{
				VK_CHECK_RESULT(vkCreateFence(m_Device->GetVulkanDevice(), &fenceCreateInfo, nullptr, &fence));
				VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_FENCE, "Swapchain Fence", fence);
			}
		}

		constexpr static VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		const auto& config = Renderer::GetConfig();
		m_SubmitInfos.resize(config.FramesInFlight);

		for (uint32_t i = 0; i < config.FramesInFlight; i++)
		{
			m_SubmitInfos[i] = {};
			m_SubmitInfos[i].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			m_SubmitInfos[i].pWaitDstStageMask = &pipelineStageFlags;
			m_SubmitInfos[i].waitSemaphoreCount = 1;
			m_SubmitInfos[i].pWaitSemaphores = &m_Semaphores.PresentComplete[i];
			m_SubmitInfos[i].signalSemaphoreCount = 1;
			m_SubmitInfos[i].pSignalSemaphores = &m_Semaphores.RenderComplete[i];
		}


		VkFormat depthFormat = m_Device->GetPhysicalDevice()->GetDepthFormat();

		// Render Pass
		VkAttachmentDescription colorAttachmentDesc = {};
		// Color attachment
		colorAttachmentDesc.format = m_ColorFormat;
		colorAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorReference = {};
		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 1;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;
		subpassDescription.inputAttachmentCount = 0;
		subpassDescription.pInputAttachments = nullptr;
		subpassDescription.preserveAttachmentCount = 0;
		subpassDescription.pPreserveAttachments = nullptr;
		subpassDescription.pResolveAttachments = nullptr;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachmentDesc;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDescription;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VK_CHECK_RESULT(vkCreateRenderPass(m_Device->GetVulkanDevice(), &renderPassInfo, nullptr, &m_RenderPass));
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_RENDER_PASS, "Swapchain render pass", m_RenderPass);

		// Create framebuffers for every swapchain image
		{
			for (auto& framebuffer : m_Framebuffers)
				vkDestroyFramebuffer(device, framebuffer, nullptr);

			VkFramebufferCreateInfo frameBufferCreateInfo = {};
			frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferCreateInfo.renderPass = m_RenderPass;
			frameBufferCreateInfo.attachmentCount = 1;
			frameBufferCreateInfo.width = m_Width;
			frameBufferCreateInfo.height = m_Height;
			frameBufferCreateInfo.layers = 1;

			m_Framebuffers.resize(m_ImageCount);
			for (uint32_t i = 0; i < m_Framebuffers.size(); i++)
			{
				frameBufferCreateInfo.pAttachments = &m_Images[i].ImageView;
				VK_CHECK_RESULT(vkCreateFramebuffer(m_Device->GetVulkanDevice(), &frameBufferCreateInfo, nullptr, &m_Framebuffers[i]));
				VKUtils::SetDebugUtilsObjectName(m_Device->GetVulkanDevice(), VK_OBJECT_TYPE_FRAMEBUFFER, fmt::eastl_format("Swapchain framebuffer (Frame in flight: {})", i), m_Framebuffers[i]);
			}
		}
	}

	void VulkanSwapChain::Destroy()
	{
		BEY_CORE_WARN_TAG("Renderer", "VulkanSwapChain::OnDestroy");

		auto device = m_Device->GetVulkanDevice();
		vkDeviceWaitIdle(device);

		if (m_SwapChain)
			vkDestroySwapchainKHR(device, m_SwapChain, nullptr);

		for (auto& image : m_Images)
			vkDestroyImageView(device, image.ImageView, nullptr);

		for (auto& commandBuffer : m_CommandBuffers)
			vkDestroyCommandPool(device, commandBuffer.CommandPool, nullptr);

		if (m_RenderPass)
			vkDestroyRenderPass(device, m_RenderPass, nullptr);

		for (auto framebuffer : m_Framebuffers)
			vkDestroyFramebuffer(device, framebuffer, nullptr);

		for (auto& semaphore : m_Semaphores.RenderComplete)
			vkDestroySemaphore(device, semaphore, nullptr);

		for (auto& semaphore : m_Semaphores.PresentComplete)
			vkDestroySemaphore(device, semaphore, nullptr);

		for (auto& fence : m_WaitFences)
			vkDestroyFence(device, fence, nullptr);

		vkDeviceWaitIdle(device);
	}

	void VulkanSwapChain::OnResize(uint32_t width, uint32_t height)
	{
		BEY_CORE_WARN_TAG("Renderer", "VulkanSwapChain::OnResize");

		auto device = m_Device->GetVulkanDevice();
		vkDeviceWaitIdle(device);
		Create(&width, &height, m_VSync);
		vkDeviceWaitIdle(device);
	}

	void VulkanSwapChain::BeginFrame()
	{
		BEY_SCOPE_PERF("VulkanSwapChain::BeginFrame");
		// Resource release queue
		auto& queue = Renderer::GetRenderResourceReleaseQueue(m_CurrentBufferIndex);
		queue.Execute();

		m_CurrentImageIndex = AcquireNextImage();

		VK_CHECK_RESULT(vkResetCommandPool(m_Device->GetVulkanDevice(), m_CommandBuffers[m_CurrentBufferIndex].CommandPool, 0));
	}

	void VulkanSwapChain::Present()
	{
		BEY_PROFILE_FUNC();
		BEY_SCOPE_PERF("VulkanSwapChain::Present");

		const uint64_t DEFAULT_FENCE_TIMEOUT = 100000000000;

		VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo& submitInfo = m_SubmitInfos[m_CurrentBufferIndex];
		submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentBufferIndex].CommandBuffer;
		submitInfo.commandBufferCount = 1;

		VK_CHECK_RESULT(vkResetFences(m_Device->GetVulkanDevice(), 1, &m_WaitFences[m_CurrentBufferIndex]));
		VK_CHECK_RESULT(vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &submitInfo, m_WaitFences[m_CurrentBufferIndex]));

		// Present the current buffer to the swap chain
		// Pass the semaphore signaled by the command buffer submission from the submit info as the wait semaphore for swap chain presentation
		// This ensures that the image is not presented to the windowing system until all commands have been submitted
		VkResult result;
		{
			BEY_SCOPE_PERF("VulkanSwapChain::Present - QueuePresent");
			
			VkPresentInfoKHR presentInfo = {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = &m_SwapChain;
			presentInfo.pImageIndices = &m_CurrentImageIndex;
			presentInfo.pWaitSemaphores = &m_Semaphores.RenderComplete[m_CurrentBufferIndex];
			presentInfo.waitSemaphoreCount = 1;
			result = vkQueuePresentKHR(m_Device->GetGraphicsQueue(), &presentInfo);
		}

		if (result != VK_SUCCESS)
		{
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			{
				OnResize(m_Width, m_Height);
			}
			else
			{
				VK_CHECK_RESULT(result);
			}
		}

		{
			BEY_PROFILE_SCOPE("VulkanSwapChain::Present - WaitForFences");

			auto& performanceTimers = Application::Get().GetPerformanceTimers();
			Timer gpuWaitTimer;

			const auto& config = Renderer::GetConfig();
			m_CurrentBufferIndex = (m_CurrentBufferIndex + 1) % config.FramesInFlight;
			// Make sure the frame we're requesting has finished rendering
			VK_CHECK_RESULT(vkWaitForFences(m_Device->GetVulkanDevice(), 1, &m_WaitFences[m_CurrentBufferIndex], VK_TRUE, UINT64_MAX));

			performanceTimers.RenderThreadGPUWaitTime = gpuWaitTimer.ElapsedMillis();
		}
	}

	uint32_t VulkanSwapChain::AcquireNextImage()
	{
		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(m_Device->GetVulkanDevice(), m_SwapChain, UINT64_MAX, m_Semaphores.PresentComplete[m_CurrentBufferIndex], (VkFence)nullptr, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			OnResize(m_Width, m_Height);
			result = vkAcquireNextImageKHR(m_Device->GetVulkanDevice(), m_SwapChain, UINT64_MAX, m_Semaphores.PresentComplete[m_CurrentBufferIndex], (VkFence)nullptr, &imageIndex);
		}

		if (result != VK_SUCCESS)
		{
			VK_CHECK_RESULT(result);  // This will handle any other errors appropriately
		}

		return imageIndex;
	}

	void VulkanSwapChain::FindImageFormatAndColorSpace()
	{
		VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice()->GetVulkanPhysicalDevice();

		// Get list of supported surface formats
		uint32_t formatCount;
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, NULL));
		BEY_CORE_ASSERT(formatCount > 0);

		std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, surfaceFormats.data()));

		// If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
		// there is no preferered format, so we assume VK_FORMAT_B8G8R8A8_UNORM
		if ((formatCount == 1) && (surfaceFormats[0].format == VK_FORMAT_UNDEFINED))
		{
			m_ColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
			m_ColorSpace = surfaceFormats[0].colorSpace;
		}
		else
		{
			// iterate over the list of available surface format and
			// check for the presence of VK_FORMAT_B8G8R8A8_UNORM
			bool found_B8G8R8A8_UNORM = false;
			for (auto&& surfaceFormat : surfaceFormats)
			{
				if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
				{
					m_ColorFormat = surfaceFormat.format;
					m_ColorSpace = surfaceFormat.colorSpace;
					found_B8G8R8A8_UNORM = true;
					break;
				}
			}

			// in case VK_FORMAT_B8G8R8A8_UNORM is not available
			// select the first available color format
			if (!found_B8G8R8A8_UNORM)
			{
				m_ColorFormat = surfaceFormats[0].format;
				m_ColorSpace = surfaceFormats[0].colorSpace;
			}
		}

	}

}
