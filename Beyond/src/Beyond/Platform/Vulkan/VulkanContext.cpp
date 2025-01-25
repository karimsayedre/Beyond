#include "pch.h"
#include "VulkanContext.h"

#include <regex>

#include "Vulkan.h"

#include "VulkanImage.h"
#include <GLFW/glfw3.h>
#include <spdlog/fmt/bundled/color.h>

#ifdef BEY_PLATFORM_WINDOWS
#include <Windows.h>
#endif

#ifndef VK_API_VERSION_1_3
#error Wrong Vulkan SDK! Please run scripts/Setup.bat
#endif

namespace Beyond {

#if defined(BEY_DEBUG)
	bool s_Validation = true;
#elif defined(BEY_RELEASE)
	bool s_Validation = false;
#else
	bool s_Validation = false; // Let's leave this on for now...
#endif



	const char* VkDebugUtilsMessageType(const VkDebugUtilsMessageTypeFlagsEXT type)
	{
		switch (type)
		{
			case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:		return "General";
			case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:	return "Validation";
			case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:	return "Performance";
			case VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT:	return "Device Address Binding";
			default:												return "Unknown";
		}
	}

	const char* VkDebugUtilsMessageSeverity(const VkDebugUtilsMessageSeverityFlagBitsEXT severity)
	{
		switch (severity)
		{
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:		return "error";
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:	return "warning";
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:		return "info";
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:	return "verbose";
			default:												return "unknown";
		}
	}

	void wrap(std::string const& input, size_t width, std::ostream& os, size_t indent = 4)
	{
		std::istringstream in(input);

		os << std::string(indent, ' ');
		size_t current = indent;
		std::string word;

		while (in >> word)
		{
			if (current + word.size() > width)
			{
				os << "\n" << std::string(indent, ' ');
				current = indent;
			}
			os << word << ' ';
			current += word.size() + 2;
		}
	}

	std::string RemoveDuplicatedObjects(const std::string& input)
	{
		std::string output = input;
		const auto firstOccurance = output.find("Object ");
		if (firstOccurance != eastl::string::npos)
			output.erase(firstOccurance, output.find(" | MessageID") - firstOccurance);
		return output;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugUtilsMessengerCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, const VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		(void)pUserData; //Unused argument

		const bool performanceWarnings = false;
		if (!performanceWarnings)
		{
			if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
				return VK_FALSE;
		}

		std::string labels, objects;
		if (pCallbackData->cmdBufLabelCount)
		{
#define COL32(R,G,B,A)    (((uint32_t)(A) << 24) | ((uint32_t)(B) << 16) | ((uint32_t)(G) << 8) | ((uint32_t)(R)))

			labels = fmt::format("\tLabels({}): \n", pCallbackData->cmdBufLabelCount);
			for (uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; ++i)
			{
				const auto& label = pCallbackData->pCmdBufLabels[i];
				fmt::rgb color;
				if (label.color[0] == 0.0f && label.color[1] == 0.0f && label.color[2] == 0.0f && label.color[3] == 0.0f)
					color = fmt::rgb{ COL32(255, 255, 255, 255) };
				else
					color = fmt::rgb{ COL32(label.color[0] * 255, label.color[1] * 255, label.color[2] * 255, label.color[3] * 255) };
				labels.append(fmt::format(fmt::fg(fmt::rgb(color)), "\t\t- Command Buffer Label[{0}]: name: {1}\n", i, label.pLabelName ? label.pLabelName : "NULL"));
			}
		}

		if (pCallbackData->objectCount)
		{
			objects = fmt::format("\tObjects({}): \n", pCallbackData->objectCount);
			for (uint32_t i = 0; i < pCallbackData->objectCount; ++i)
			{
				const auto& object = pCallbackData->pObjects[i];
				objects.append(fmt::format("\t\t- Object[{0}] name: {1}, type: {2}, handle: {3:#x}\n", i,
					object.pObjectName ? object.pObjectName : "NULL", magic_enum::enum_name(object.objectType), object.objectHandle));
			}
		}

		int columns = 0, rows = 0;
#ifdef BEY_PLATFORM_WINDOWS
		CONSOLE_SCREEN_BUFFER_INFO csbi;

		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
		columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
		rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#endif
		std::ostringstream os;
		const std::string error = RemoveDuplicatedObjects(pCallbackData->pMessage);
		wrap(error, columns, os);

		/*if (os.str().find("MotionVectors") == eastl::string::npos)
			return false;
		if (os.str().find("LightCulling") == eastl::string::npos)
			return false;
		if (os.str().find("GTAO") == eastl::string::npos)
			return false;
		if (os.str().find("GTAO-Denoise") == eastl::string::npos)
			return false;*/

		switch (messageSeverity)
		{
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
				BEY_CORE_TRACE("{} message: \n{}\n {} {}", VkDebugUtilsMessageType(messageType), os.str(), labels, objects);
				break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
				BEY_CORE_INFO("{} message: \n{}\n {} {}", VkDebugUtilsMessageType(messageType), os.str(), labels, objects);
				break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
				BEY_CORE_WARN("{} message: \n{}\n {} {}", VkDebugUtilsMessageType(messageType), os.str(), labels, objects);
				break;
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
				BEY_CORE_ERROR("{} message: \n{}\n {} {}", VkDebugUtilsMessageType(messageType), os.str(), labels, objects);
				break;
		}
		//[[maybe_unused]] const auto& imageRefs = VulkanImage2D::GetImageRefs();
 		return VK_FALSE;
	}

	static bool CheckDriverAPIVersionSupport(uint32_t minimumSupportedVersion)
	{
		uint32_t instanceVersion;
		vkEnumerateInstanceVersion(&instanceVersion);

		if (instanceVersion < minimumSupportedVersion)
		{
			BEY_CORE_FATAL("Incompatible Vulkan driver version!");
			BEY_CORE_FATAL("  You have {}.{}.{}", VK_API_VERSION_MAJOR(instanceVersion), VK_API_VERSION_MINOR(instanceVersion), VK_API_VERSION_PATCH(instanceVersion));
			BEY_CORE_FATAL("  You need at least {}.{}.{}", VK_API_VERSION_MAJOR(minimumSupportedVersion), VK_API_VERSION_MINOR(minimumSupportedVersion), VK_API_VERSION_PATCH(minimumSupportedVersion));

			return false;
		}

		return true;
	}

	VulkanContext::VulkanContext()
	{
	}

	VulkanContext::~VulkanContext()
	{
		// Its too late to destroy the device here, because Destroy() asks for the context (which we're in the middle of destructing)
		// Device is destroyed in GLFWWindow::Shutdown()
		//m_Device->Destroy();

		vkDestroyInstance(s_VulkanInstance, nullptr);
		s_VulkanInstance = nullptr;
	}

	void VulkanContext::Init()
	{
		BEY_CORE_INFO_TAG("Renderer", "VulkanContext::Create");

		BEY_CORE_ASSERT(glfwVulkanSupported(), "GLFW must support Vulkan!");
		VK_CHECK_RESULT(volkInitialize());

		if (!CheckDriverAPIVersionSupport(VK_API_VERSION_1_3))
		{
#ifdef BEY_PLATFORM_WINDOWS
			MessageBox(nullptr, L"Incompatible Vulkan driver version.\nUpdate your GPU drivers!", L"Beyond Error", MB_OK | MB_ICONERROR);
#else
			BEY_CORE_ERROR("Incompatible Vulkan driver version.\nUpdate your GPU drivers!");
#endif
			BEY_CORE_VERIFY(false);
		}
		//
		//		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//		// Application Info
		//		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Beyond";
		appInfo.pEngineName = "Beyond";
		appInfo.apiVersion = VK_API_VERSION_1_3;
		//
		//		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//		// Extensions and Validation
		//		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// TODO: GLFW can handle this for us
#ifdef BEY_PLATFORM_WINDOWS
#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"
#elif defined(BEY_PLATFORM_LINUX)
#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_xcb_surface"
#endif
		std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };

		//if (s_Validation)
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // Very little performance hit, can be used in Release.

		VkValidationFeatureEnableEXT enables[] =
		{
			VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT, VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
			VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT, //Gives false positives?
			VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
		};
		VkValidationFeaturesEXT features = {};
		features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
		features.enabledValidationFeatureCount = sizeof(enables) / sizeof(VkValidationFeatureEnableEXT);
		features.pEnabledValidationFeatures = enables;
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		///// DLSS
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		uint32_t dlssExtCount = 0;
		auto res = NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&m_Device->GetNgxFeatureDiscoveryInfo(), &dlssExtCount, nullptr);
		BEY_CORE_VERIFY(res == NVSDK_NGX_Result_Success);
		VkExtensionProperties* dlssProperties = (VkExtensionProperties*)alloca(sizeof(VkExtensionProperties) * dlssExtCount);

		res = NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&m_Device->GetNgxFeatureDiscoveryInfo(), &dlssExtCount, &dlssProperties);
		BEY_CORE_VERIFY(res == NVSDK_NGX_Result_Success);

		for (uint32_t i = 0; i < dlssExtCount; i++)
			instanceExtensions.emplace_back(dlssProperties->extensionName);
		instanceExtensions.erase(std::ranges::unique(instanceExtensions).begin(), instanceExtensions.end());

		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		VkInstanceCreateInfo instanceCreateInfo = {};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		//instanceCreateInfo.pNext = &features;
		instanceCreateInfo.pApplicationInfo = &appInfo;
		instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

		// TODO: Extract all validation into separate class
		if (s_Validation)
		{
			static const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
			// Check if this layer is available at instance level
			uint32_t instanceLayerCount;
			vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
			std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
			vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
			bool validationLayerPresent = false;
			BEY_CORE_TRACE_TAG("Renderer", "Vulkan Instance Layers:");
			for (const VkLayerProperties& layer : instanceLayerProperties)
			{
				BEY_CORE_TRACE_TAG("Renderer", "  {0}", layer.layerName);
				if (strcmp(layer.layerName, validationLayerName) == 0)
				{
					validationLayerPresent = true;
					//break;
				}
			}
			if (validationLayerPresent)
			{
				instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
				instanceCreateInfo.enabledLayerCount = 1;
			}
			else
			{
				BEY_CORE_ERROR_TAG("Renderer", "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled");
			}
		}



		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Instance and Surface Creation
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		VK_CHECK_RESULT(vkCreateInstance(&instanceCreateInfo, nullptr, &s_VulkanInstance));
		volkLoadInstanceOnly(s_VulkanInstance);


		//Utils::VulkanLoadDebugUtilsExtensions(s_VulkanInstance);

		if (s_Validation)
		{
			VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo{};
			debugUtilsCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;
			debugUtilsCreateInfo.pfnUserCallback = VulkanDebugUtilsMessengerCallback;
			debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT /*  | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT*/;

			VK_CHECK_RESULT(vkCreateDebugUtilsMessengerEXT(s_VulkanInstance, &debugUtilsCreateInfo, nullptr, &m_DebugUtilsMessenger));
		}

		m_PhysicalDevice = VulkanPhysicalDevice::Select();

		VkPhysicalDeviceFeatures enabledFeatures = {};
		enabledFeatures.samplerAnisotropy = true;
		enabledFeatures.wideLines = true;
		enabledFeatures.fillModeNonSolid = true;
		enabledFeatures.independentBlend = true;
		enabledFeatures.pipelineStatisticsQuery = true;
		enabledFeatures.shaderInt64 = true;
		enabledFeatures.textureCompressionBC = true;

		m_Device = Ref<VulkanDevice>::Create(m_PhysicalDevice, enabledFeatures);


		VulkanAllocator::Init(m_Device);

		// Pipeline Cache
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK_RESULT(vkCreatePipelineCache(m_Device->GetVulkanDevice(), &pipelineCacheCreateInfo, nullptr, &m_PipelineCache));
	}

	}
