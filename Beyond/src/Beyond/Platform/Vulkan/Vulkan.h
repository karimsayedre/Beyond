#pragma once

#include <Volk/volk.h>

#include "Beyond/Renderer/Renderer.h"


namespace Beyond::Utils {
	void VulkanLoadDebugUtilsExtensions(VkInstance instance);

	void RetrieveDiagnosticCheckpoints();

	inline void VulkanCheckResult(VkResult result)
	{
		if (result != VK_SUCCESS)
		{
			BEY_CORE_ERROR("VkResult is '{0}'", magic_enum::enum_name(result));
			if (result == VK_ERROR_DEVICE_LOST)
			{
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(3s);
				Utils::RetrieveDiagnosticCheckpoints();
				Utils::DumpGPUInfo();
			}
			BEY_CORE_ASSERT(result == VK_SUCCESS);
		}
	}

	inline void VulkanCheckResult(VkResult result, const char* file, int line)
	{
		if (result != VK_SUCCESS)
		{
			BEY_CORE_ERROR("VkResult is '{0}' in {1}:{2}", magic_enum::enum_name(result), file, line);
			if (result == VK_ERROR_DEVICE_LOST)
			{
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(3s);
				Utils::RetrieveDiagnosticCheckpoints();
				Utils::DumpGPUInfo();
			}
			BEY_CORE_ASSERT(result == VK_SUCCESS);
		}
	}
}

#define VK_CHECK_RESULT(f)\
do {\
	VkResult res = (f);\
	::Beyond::Utils::VulkanCheckResult(res, __FILE__, __LINE__);\
} while(false)

namespace Beyond::VKUtils {
	inline static void SetDebugUtilsObjectName(const VkDevice device, const VkObjectType objectType, const eastl::string& name, const void* handle)
	{
		//if (s_Validation)
		{
			VkDebugUtilsObjectNameInfoEXT nameInfo;
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = objectType;
			nameInfo.pObjectName = name.c_str();
			nameInfo.objectHandle = (uint64_t)handle;
			nameInfo.pNext = VK_NULL_HANDLE;

			VK_CHECK_RESULT(vkSetDebugUtilsObjectNameEXT(device, &nameInfo));
		}
	}
}
