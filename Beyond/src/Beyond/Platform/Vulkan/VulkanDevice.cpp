#include "pch.h"
#include "VulkanDevice.h"

#include <magic_enum.hpp>
#include <nvsdk_ngx_vk.h>
#include "VulkanDLSS.h"

#include "VulkanContext.h"
#include "VulkanMemoryAllocator/vk_mem_alloc.h"

#define BEY_HAS_AFTERMATH !BEY_DIST

#if BEY_HAS_AFTERMATH
#include "Debug/NsightAftermathGpuCrashTracker.h"
#endif

namespace Beyond {


	void NGXLog(const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent)
	{
		switch (loggingLevel)
		{
			case NVSDK_NGX_LOGGING_LEVEL_OFF:
				BEY_CORE_ERROR("NGX Error: {} from: {}", message, magic_enum::enum_name<NVSDK_NGX_Feature>(sourceComponent));
				break;
			case NVSDK_NGX_LOGGING_LEVEL_ON:
				BEY_CORE_INFO("NGX INFO: {} from: {}", message, magic_enum::enum_name<NVSDK_NGX_Feature>(sourceComponent));
				break;
			case NVSDK_NGX_LOGGING_LEVEL_VERBOSE:
				BEY_CORE_TRACE("NGX Verbose: {} from: {}", message, magic_enum::enum_name<NVSDK_NGX_Feature>(sourceComponent));
				break;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////
	// Vulkan Physical Device
	////////////////////////////////////////////////////////////////////////////////////

	VulkanPhysicalDevice::VulkanPhysicalDevice()
	{
		auto vkInstance = VulkanContext::GetInstance();

		uint32_t gpuCount = 0;
		// Get number of available physical devices
		vkEnumeratePhysicalDevices(vkInstance, &gpuCount, nullptr);
		BEY_CORE_ASSERT(gpuCount > 0, "");
		// Enumerate devices
		std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
		VK_CHECK_RESULT(vkEnumeratePhysicalDevices(vkInstance, &gpuCount, physicalDevices.data()));

		VkPhysicalDevice selectedPhysicalDevice = nullptr;
		for (VkPhysicalDevice physicalDevice : physicalDevices)
		{
			vkGetPhysicalDeviceProperties(physicalDevice, &m_Properties);
			if (m_Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				selectedPhysicalDevice = physicalDevice;
				break;
			}
		}

		if (!selectedPhysicalDevice)
		{
			BEY_CORE_TRACE_TAG("Renderer", "Could not find discrete GPU.");
			selectedPhysicalDevice = physicalDevices.back();
		}

		BEY_CORE_ASSERT(selectedPhysicalDevice, "Could not find any physical devices!");
		m_PhysicalDevice = selectedPhysicalDevice;

		m_Properties12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
		m_Properties12.pNext = &m_AccelProperties;

		m_AccelProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
		m_AccelProperties.pNext = &m_RayTracingPipelineProperties;

		m_RayTracingInvocationReorderProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_PROPERTIES_NV;
		m_RayTracingInvocationReorderProperties.pNext = &m_RayTracingPipelineProperties;

		m_RayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		m_RayTracingPipelineProperties.pNext = nullptr;

		VkPhysicalDeviceProperties2 props = {};
		props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		props.pNext = &m_Properties12;

		vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &props);

		vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &m_Features);
		vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &m_MemoryProperties);

		//BEY_CORE_VERIFY(m_RayTracingInvocationReorderProperties.rayTracingInvocationReorderReorderingHint == VK_RAY_TRACING_INVOCATION_REORDER_MODE_REORDER_NV);

		m_RaytracingSupported = m_RayTracingPipelineProperties.shaderGroupHandleSize > 0 && m_AccelProperties.maxDescriptorSetAccelerationStructures > 0;

		uint32_t queueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
		BEY_CORE_ASSERT(queueFamilyCount > 0, "");
		m_QueueFamilyProperties.resize(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, m_QueueFamilyProperties.data());

		uint32_t extCount = 0;
		vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extCount, nullptr);
		if (extCount > 0)
		{
			std::vector<VkExtensionProperties> extensions(extCount);
			if (vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
			{
				BEY_CORE_TRACE_TAG("Renderer", "Selected physical device has {0} extensions", extensions.size());
				for (const auto& ext : extensions)
				{
					m_SupportedExtensions.emplace(ext.extensionName);
					BEY_CORE_TRACE_TAG("Renderer", "  {0}", ext.extensionName);
				}
			}
		}

		// Queue families
		// Desired queues need to be requested upon logical device creation
		// Due to differing queue family configurations of Vulkan implementations this can be a bit tricky, especially if the application
		// requests different queue types

		// Get queue family indices for the requested queue family types
		// Note that the indices may overlap depending on the implementation

		static const float defaultQueuePriority(0.0f);

		int requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
		m_QueueFamilyIndices = GetQueueFamilyIndices(requestedQueueTypes);

		// Graphics queue
		if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT) //-V547
		{
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = m_QueueFamilyIndices.Graphics;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			m_QueueCreateInfos.push_back(queueInfo);
		}

		// Dedicated compute queue
		if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT) //-V547
		{
			if (m_QueueFamilyIndices.Compute != m_QueueFamilyIndices.Graphics)
			{
				// If compute family index differs, we need an additional queue create info for the compute queue
				VkDeviceQueueCreateInfo queueInfo{};
				queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueInfo.queueFamilyIndex = m_QueueFamilyIndices.Compute;
				queueInfo.queueCount = 1;
				queueInfo.pQueuePriorities = &defaultQueuePriority;
				m_QueueCreateInfos.push_back(queueInfo);
			}
		}

		// Dedicated transfer queue
		if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT) //-V547
		{
			if ((m_QueueFamilyIndices.Transfer != m_QueueFamilyIndices.Graphics) && (m_QueueFamilyIndices.Transfer != m_QueueFamilyIndices.Compute))
			{
				// If compute family index differs, we need an additional queue create info for the compute queue
				VkDeviceQueueCreateInfo queueInfo{};
				queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueInfo.queueFamilyIndex = m_QueueFamilyIndices.Transfer;
				queueInfo.queueCount = 1;
				queueInfo.pQueuePriorities = &defaultQueuePriority;
				m_QueueCreateInfos.push_back(queueInfo);
			}
		}

		m_DepthFormat = FindDepthFormat();
		BEY_CORE_ASSERT(m_DepthFormat != 0);
	}

	VulkanPhysicalDevice::~VulkanPhysicalDevice()
	{
	}

	VkFormat VulkanPhysicalDevice::FindDepthFormat() const
	{
		// Since all depth formats may be optional, we need to find a suitable depth format to use
		// Start with the highest precision packed format
		std::vector<VkFormat> depthFormats = {
			//VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM
		};

		// TODO: Move to VulkanPhysicalDevice
		for (auto& format : depthFormats)
		{
			VkFormatProperties formatProps;
			vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &formatProps);
			// Format must support depth stencil attachment for optimal tiling
			if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
				return format;
		}
		return VK_FORMAT_UNDEFINED;
	}

	bool VulkanPhysicalDevice::IsExtensionSupported(const eastl::string& extensionName) const
	{
		return m_SupportedExtensions.contains(extensionName);
	}

	VulkanPhysicalDevice::QueueFamilyIndices VulkanPhysicalDevice::GetQueueFamilyIndices(int flags)
	{
		QueueFamilyIndices indices;

		// Dedicated queue for compute
		// Try to find a queue family index that supports compute but not graphics
		if (flags & VK_QUEUE_COMPUTE_BIT)
		{
			for (uint32_t i = 0; i < m_QueueFamilyProperties.size(); i++)
			{
				auto& queueFamilyProperties = m_QueueFamilyProperties[i];
				if ((queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) && ((queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
				{
					indices.Compute = i;
					break;
				}
			}
		}

		// Dedicated queue for transfer
		// Try to find a queue family index that supports transfer but not graphics and compute
		if (flags & VK_QUEUE_TRANSFER_BIT)
		{
			for (uint32_t i = 0; i < m_QueueFamilyProperties.size(); i++)
			{
				auto& queueFamilyProperties = m_QueueFamilyProperties[i];
				if ((queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) && ((queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
				{
					indices.Transfer = i;
					break;
				}
			}
		}

		// For other queue types or if no separate compute queue is present, return the first one to support the requested flags
		for (uint32_t i = 0; i < m_QueueFamilyProperties.size(); i++)
		{
			if ((flags & VK_QUEUE_TRANSFER_BIT) && indices.Transfer == -1)
			{
				if (m_QueueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
					indices.Transfer = i;
			}

			if ((flags & VK_QUEUE_COMPUTE_BIT) && indices.Compute == -1)
			{
				if (m_QueueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
					indices.Compute = i;
			}

			if (flags & VK_QUEUE_GRAPHICS_BIT)
			{
				if (m_QueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
					indices.Graphics = i;
			}
		}

		return indices;
	}

	uint32_t VulkanPhysicalDevice::GetMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const
	{
		// Iterate over all memory types available for the device used in this example
		for (uint32_t i = 0; i < m_MemoryProperties.memoryTypeCount; i++)
		{
			if ((typeBits & 1) == 1)
			{
				if ((m_MemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
					return i;
			}
			typeBits >>= 1;
		}

		BEY_CORE_ASSERT(false, "Could not find a suitable memory type!");
		return UINT32_MAX;
	}

	Ref<VulkanPhysicalDevice> VulkanPhysicalDevice::Select()
	{
		return Ref<VulkanPhysicalDevice>::Create();
	}

	////////////////////////////////////////////////////////////////////////////////////
	// Vulkan Device
	////////////////////////////////////////////////////////////////////////////////////

	VulkanDevice::VulkanDevice(const Ref<VulkanPhysicalDevice>& physicalDevice, VkPhysicalDeviceFeatures enabledFeatures)
		: m_PhysicalDevice(physicalDevice), m_EnabledFeatures(enabledFeatures)
	{
		const bool enableAftermath = true;

		// Do we need to enable any other extensions (eg. NV_RAYTRACING?)
		std::vector<const char*> deviceExtensions;
		// If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
		BEY_CORE_ASSERT(m_PhysicalDevice->IsExtensionSupported(VK_KHR_SWAPCHAIN_EXTENSION_NAME));
		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		deviceExtensions.push_back(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
		deviceExtensions.push_back(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME);
		deviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
		deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		deviceExtensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
		deviceExtensions.push_back(VK_NV_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME);
		deviceExtensions.push_back(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);
		deviceExtensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
		deviceExtensions.push_back(VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME);

		deviceExtensions.erase(std::ranges::remove_if(deviceExtensions, [physicalDevice = m_PhysicalDevice](const char* ext) mutable
		{
			return !physicalDevice->IsExtensionSupported(ext);
		}).begin(), deviceExtensions.end());
		//deviceExtensions.push_back(VK_GOOGLE_USER_TYPE_EXTENSION_NAME);
		//deviceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		// Opt-in into mandatory device features.
		VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {};
		bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
		bufferDeviceAddressFeatures.bufferDeviceAddress = true;

		VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Feature = {};
		synchronization2Feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
		synchronization2Feature.synchronization2 = VK_TRUE;

		VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT pageableDeviceLocalMemoryFeatures = {};
		pageableDeviceLocalMemoryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT;
		pageableDeviceLocalMemoryFeatures.pageableDeviceLocalMemory = true;

		VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV rayTracingInvocationReorderFeatures = {};
		rayTracingInvocationReorderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV;
		rayTracingInvocationReorderFeatures.rayTracingInvocationReorder = true;

		VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {};
		indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		indexingFeatures.runtimeDescriptorArray = true;
		indexingFeatures.shaderSampledImageArrayNonUniformIndexing = true;
		indexingFeatures.descriptorBindingPartiallyBound = true;
		indexingFeatures.descriptorBindingVariableDescriptorCount = true;
		indexingFeatures.descriptorBindingSampledImageUpdateAfterBind = true;
		indexingFeatures.descriptorBindingStorageImageUpdateAfterBind = true;
		indexingFeatures.descriptorBindingStorageBufferUpdateAfterBind = true;

		VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {};
		accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		accelerationStructureFeatures.accelerationStructure = true;

		VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {};
		rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
		rayQueryFeatures.rayQuery = true;

		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures = {};
		rayTracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		rayTracingFeatures.rayTracingPipeline = true;
		rayTracingFeatures.rayTracingPipelineTraceRaysIndirect = true;

		VkPhysicalDeviceScalarBlockLayoutFeatures scalarBlockLayoutFeatures = {};
		scalarBlockLayoutFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES;
		scalarBlockLayoutFeatures.scalarBlockLayout = true;

		VkPhysicalDeviceHostQueryResetFeatures hostQueryResetFeatures = {};
		hostQueryResetFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES;
		hostQueryResetFeatures.hostQueryReset = true;

		VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures separateDepthStencilFeatures = {};
		separateDepthStencilFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES;
		separateDepthStencilFeatures.separateDepthStencilLayouts = true;

		// Start with the last feature in the chain
		void* featureChainHead = nullptr;

		// Add features conditionally based on extension support
		//if (physicalDevice->IsExtensionSupported(VK_EXT_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME))
		{
			separateDepthStencilFeatures.pNext = featureChainHead;
			featureChainHead = &separateDepthStencilFeatures;
		}

		if (physicalDevice->IsExtensionSupported(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME))
		{
			hostQueryResetFeatures.pNext = featureChainHead;
			featureChainHead = &hostQueryResetFeatures;
		}

		{
			scalarBlockLayoutFeatures.pNext = featureChainHead;
			featureChainHead = &scalarBlockLayoutFeatures;
		}

		if (physicalDevice->IsExtensionSupported(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME))
		{
			rayTracingFeatures.pNext = featureChainHead;
			featureChainHead = &rayTracingFeatures;
			m_RaytracingPipelineSupported = true;
		}

		if (physicalDevice->IsExtensionSupported(VK_KHR_RAY_QUERY_EXTENSION_NAME))
		{
			rayQueryFeatures.pNext = featureChainHead;
			featureChainHead = &rayQueryFeatures;
			m_RayQuerySupported = true;
		}

		if (physicalDevice->IsExtensionSupported(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
		{
			accelerationStructureFeatures.pNext = featureChainHead;
			featureChainHead = &accelerationStructureFeatures;
			m_AccelerationStructuresSupported = true;

		}

		if (physicalDevice->IsExtensionSupported(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME))
		{
			indexingFeatures.pNext = featureChainHead;
			featureChainHead = &indexingFeatures;
			m_BindlessSupported = true;
		}

		if (physicalDevice->IsExtensionSupported(VK_NV_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME))
		{
			rayTracingInvocationReorderFeatures.pNext = featureChainHead;
			featureChainHead = &rayTracingInvocationReorderFeatures;
		}

		if (physicalDevice->IsExtensionSupported(VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME))
		{
			pageableDeviceLocalMemoryFeatures.pNext = featureChainHead;
			featureChainHead = &pageableDeviceLocalMemoryFeatures;
		}

		if (physicalDevice->IsExtensionSupported(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME))
		{
			synchronization2Feature.pNext = featureChainHead;
			featureChainHead = &synchronization2Feature;
		}

		if (physicalDevice->IsExtensionSupported(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
		{
			bufferDeviceAddressFeatures.pNext = featureChainHead;
			featureChainHead = &bufferDeviceAddressFeatures;
		}



		//NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements


#if BEY_HAS_AFTERMATH
		VkDeviceDiagnosticsConfigCreateInfoNV aftermathInfo = {};
		bool canEnableAftermath = enableAftermath && m_PhysicalDevice->IsExtensionSupported(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME) && m_PhysicalDevice->IsExtensionSupported(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME);
		if (canEnableAftermath)
		{
			// Must be initialized ~before~ device has been created
			//s_GPUCrashTracker = GpuCrashTracker(s_GPUCrashMarkerMap);
			s_GPUCrashTracker.Initialize();

			VkDeviceDiagnosticsConfigFlagBitsNV aftermathFlags = (VkDeviceDiagnosticsConfigFlagBitsNV)(VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV |
				VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV |
				VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_ERROR_REPORTING_BIT_NV);

			aftermathInfo.sType = VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV;
			aftermathInfo.flags = aftermathFlags;
			aftermathInfo.pNext = featureChainHead;
			featureChainHead = &aftermathInfo;
		}
#endif


		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		//////////////////////////////////////////////////////////////////////////
		// version features and physical device extensions

#if BEY_HAS_AFTERMATH
		if (canEnableAftermath)
			deviceCreateInfo.pNext = &aftermathInfo;
#else
		deviceCreateInfo.pNext = &separateDepthStencilFeatures;
#endif
		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(physicalDevice->m_QueueCreateInfos.size());
		deviceCreateInfo.pQueueCreateInfos = physicalDevice->m_QueueCreateInfos.data();
		deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

		// If a pNext(Chain) has been passed, we need to add it to the device creation info
		VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
		// Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
		if (m_PhysicalDevice->IsExtensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
		{
			//deviceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
			deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
			m_EnableDebugMarkers = true;
		}

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// DLSS
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		NVSDK_NGX_ProjectIdDescription projectIDDescription{ "Beyond Engine", NVSDK_NGX_ENGINE_TYPE_CUSTOM, "0.1a" };
		const NVSDK_NGX_Application_Identifier applicationIdentifier{ .IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id, .v = {projectIDDescription} };
#ifdef BEY_DEBUG
		NVSDK_NGX_LoggingInfo loggingInfo{ NGXLog, NVSDK_NGX_LOGGING_LEVEL_ON, true };
#else
		NVSDK_NGX_LoggingInfo loggingInfo{ NGXLog, NVSDK_NGX_LOGGING_LEVEL_OFF, true };
#endif

		const wchar_t* const paths[] = { L"%USERPROFILE%AppData/Local/DLSS" };
		NVSDK_NGX_PathListInfo pathListInfo{ paths, 1 };
		NVSDK_NGX_FeatureCommonInfo featureCommonInfo{ .PathListInfo = pathListInfo, .InternalData = {}, .LoggingInfo = loggingInfo };
		m_NgxFeatureDiscoveryInfo = { NVSDK_NGX_Version_API, NVSDK_NGX_Feature_SuperSampling, applicationIdentifier, L"./DLSSLogs", &featureCommonInfo };
		uint32_t dlssPropertiesCount = 0;
		VkInstance instance = VulkanContext::GetInstance();
		auto res = NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(instance, m_PhysicalDevice->GetVulkanPhysicalDevice(), &m_NgxFeatureDiscoveryInfo, &dlssPropertiesCount, nullptr);
		if (res == NVSDK_NGX_Result_Success)
		{
			VkExtensionProperties* dlssProperties = (VkExtensionProperties*)alloca(sizeof(VkExtensionProperties) * dlssPropertiesCount);
			res = NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(VulkanContext::GetInstance(), m_PhysicalDevice->GetVulkanPhysicalDevice(), &m_NgxFeatureDiscoveryInfo, &dlssPropertiesCount, &dlssProperties);

			for (uint32_t i = 0; i < dlssPropertiesCount; i++)
				deviceExtensions.emplace_back(dlssProperties[i].extensionName);
			deviceExtensions.erase(std::ranges::unique(deviceExtensions).begin(), deviceExtensions.end());

			deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
			deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

			VkResult result = vkCreateDevice(m_PhysicalDevice->GetVulkanPhysicalDevice(), &deviceCreateInfo, nullptr, &m_LogicalDevice);
			BEY_CORE_ASSERT(result == VK_SUCCESS);

			res = NVSDK_NGX_VULKAN_Init_with_ProjectID("{dc561bab-3dbb-404e-b3e0-889c0cf450d6}", NVSDK_NGX_ENGINE_TYPE_CUSTOM, "0.1a", L"./DLSSLogs",
				instance, m_PhysicalDevice->GetVulkanPhysicalDevice(), m_LogicalDevice, vkGetInstanceProcAddr, vkGetDeviceProcAddr, &featureCommonInfo);
			BEY_CORE_VERIFY(res == NVSDK_NGX_Result_Success);

			m_DLSSSupported = true;
		}
		else
		{
			deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
			deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

			VkResult result = vkCreateDevice(m_PhysicalDevice->GetVulkanPhysicalDevice(), &deviceCreateInfo, nullptr, &m_LogicalDevice);
			BEY_CORE_ASSERT(result == VK_SUCCESS);
		}
		volkLoadDevice(m_LogicalDevice);

		// Get a graphics queue from the device
		vkGetDeviceQueue(m_LogicalDevice, m_PhysicalDevice->m_QueueFamilyIndices.Graphics, 0, &m_GraphicsQueue);
		vkGetDeviceQueue(m_LogicalDevice, m_PhysicalDevice->m_QueueFamilyIndices.Compute, 0, &m_ComputeQueue);
	}

	VulkanDevice::~VulkanDevice()
	{}

	void VulkanDevice::Destroy()
	{
		m_CommandPools.clear();
		NVSDK_NGX_VULKAN_Shutdown1(m_LogicalDevice);
		vkDeviceWaitIdle(m_LogicalDevice);
		vkDestroyDevice(m_LogicalDevice, nullptr);
	}

	VkCommandBuffer VulkanDevice::CreateCommandBuffer(const eastl::string& name, bool begin, bool compute)
	{
		return GetOrCreateThreadLocalCommandPool()->AllocateCommandBuffer(name, begin, compute);
	}

	void VulkanDevice::FlushCommandBuffer(VkCommandBuffer commandBuffer)
	{
		GetThreadLocalCommandPool()->FlushCommandBuffer(commandBuffer);
	}

	void VulkanDevice::FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue)
	{
		GetThreadLocalCommandPool()->FlushCommandBuffer(commandBuffer, queue);
	}

	VkCommandBuffer VulkanDevice::CreateSecondaryCommandBuffer(const char* debugName)
	{
		VkCommandBuffer cmdBuffer;

		VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
		cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufAllocateInfo.commandPool = GetOrCreateThreadLocalCommandPool()->GetGraphicsCommandPool();
		cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		cmdBufAllocateInfo.commandBufferCount = 1;

		VK_CHECK_RESULT(vkAllocateCommandBuffers(m_LogicalDevice, &cmdBufAllocateInfo, &cmdBuffer));
		VKUtils::SetDebugUtilsObjectName(m_LogicalDevice, VK_OBJECT_TYPE_COMMAND_BUFFER, debugName, cmdBuffer);
		return cmdBuffer;
	}

	Ref<VulkanCommandPool> VulkanDevice::GetThreadLocalCommandPool()
	{
		auto threadID = std::this_thread::get_id();
		BEY_CORE_VERIFY(m_CommandPools.find(threadID) != m_CommandPools.end());

		return m_CommandPools.at(threadID);
	}

	Ref<VulkanCommandPool> VulkanDevice::GetOrCreateThreadLocalCommandPool()
	{
		auto threadID = std::this_thread::get_id();
		auto commandPoolIt = m_CommandPools.find(threadID);
		if (commandPoolIt != m_CommandPools.end())
			return commandPoolIt->second;

		Ref<VulkanCommandPool> commandPool = Ref<VulkanCommandPool>::Create();
		m_CommandPools[threadID] = commandPool;
		return commandPool;
	}

	VulkanCommandPool::VulkanCommandPool()
	{
		auto device = VulkanContext::GetCurrentDevice();
		auto vulkanDevice = device->GetVulkanDevice();

		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = device->GetPhysicalDevice()->GetQueueFamilyIndices().Graphics;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK_RESULT(vkCreateCommandPool(vulkanDevice, &cmdPoolInfo, nullptr, &m_GraphicsCommandPool));
		VKUtils::SetDebugUtilsObjectName(vulkanDevice, VK_OBJECT_TYPE_COMMAND_POOL, "Graphics Command Pool", m_GraphicsCommandPool);

		cmdPoolInfo.queueFamilyIndex = device->GetPhysicalDevice()->GetQueueFamilyIndices().Compute;
		VK_CHECK_RESULT(vkCreateCommandPool(vulkanDevice, &cmdPoolInfo, nullptr, &m_ComputeCommandPool));
		VKUtils::SetDebugUtilsObjectName(vulkanDevice, VK_OBJECT_TYPE_COMMAND_POOL, "Compute Command Pool", m_ComputeCommandPool);
	}

	VulkanCommandPool::~VulkanCommandPool()
	{
		auto device = VulkanContext::GetCurrentDevice();
		auto vulkanDevice = device->GetVulkanDevice();

		vkDestroyCommandPool(vulkanDevice, m_GraphicsCommandPool, nullptr);
		vkDestroyCommandPool(vulkanDevice, m_ComputeCommandPool, nullptr);
	}

	VkCommandBuffer VulkanCommandPool::AllocateCommandBuffer(const eastl::string& name, bool begin, bool compute)
	{
		auto device = VulkanContext::GetCurrentDevice();
		auto vulkanDevice = device->GetVulkanDevice();

		VkCommandBuffer cmdBuffer;

		VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
		cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufAllocateInfo.commandPool = compute ? m_ComputeCommandPool : m_GraphicsCommandPool;
		cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufAllocateInfo.commandBufferCount = 1;

		VK_CHECK_RESULT(vkAllocateCommandBuffers(vulkanDevice, &cmdBufAllocateInfo, &cmdBuffer));
		VKUtils::SetDebugUtilsObjectName(vulkanDevice, VK_OBJECT_TYPE_COMMAND_BUFFER, name, cmdBuffer);

		// If requested, also start the new command buffer
		if (begin)
		{
			VkCommandBufferBeginInfo cmdBufferBeginInfo{};
			cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));
		}

		return cmdBuffer;
	}

	void VulkanCommandPool::FlushCommandBuffer(VkCommandBuffer commandBuffer)
	{
		auto device = VulkanContext::GetCurrentDevice();
		FlushCommandBuffer(commandBuffer, device->GetGraphicsQueue());
	}

	void VulkanCommandPool::FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue)
	{
		auto device = VulkanContext::GetCurrentDevice();
		auto vulkanDevice = device->GetVulkanDevice();

		const uint64_t DEFAULT_FENCE_TIMEOUT = 100000000000;

		BEY_CORE_ASSERT(commandBuffer != VK_NULL_HANDLE);

		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		// Create fence to ensure that the command buffer has finished executing
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = 0;
		VkFence fence;
		VK_CHECK_RESULT(vkCreateFence(vulkanDevice, &fenceCreateInfo, nullptr, &fence));

		{
			static std::mutex submissionLock;
			std::scoped_lock<std::mutex> lock(submissionLock);

			// Submit to the queue
			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
		}
		// Wait for the fence to signal that command buffer has finished executing
		VK_CHECK_RESULT(vkWaitForFences(vulkanDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

		vkDestroyFence(vulkanDevice, fence, nullptr);
		vkResetCommandBuffer(commandBuffer, 0);
	}

}
