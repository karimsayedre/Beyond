#pragma once
#include <shaderc/shaderc.h>

#include <vulkan/vulkan.h>
#include "Beyond/Renderer/Shader.h"


namespace Beyond {
	namespace ShaderUtils {

		inline static const char* VKStageToShaderMacro(const VkShaderStageFlagBits stage)
		{
			if (stage == VK_SHADER_STAGE_VERTEX_BIT)   return "__VERTEX_STAGE__";
			if (stage == VK_SHADER_STAGE_FRAGMENT_BIT) return "__FRAGMENT_STAGE__";
			if (stage == VK_SHADER_STAGE_COMPUTE_BIT)  return "__COMPUTE_STAGE__";
			if (stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR) return "__RAYGEN_STAGE__";
			if (stage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR) return "__ANY_HIT_STAGE__";
			if (stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) return "__CLOSEST_HIT_STAGE__";
			if (stage == VK_SHADER_STAGE_MISS_BIT_KHR) return "__MISS_STAGE__";
			if (stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR) return "__INTERSECTION_STAGE__";
			if (stage == VK_SHADER_STAGE_CALLABLE_BIT_KHR) return "__CALLABLE_STAGE__";

			BEY_CORE_VERIFY(false, "Unknown shader stage.");
			return "";
		}

		inline static eastl::string_view StageToShaderMacro(const std::string_view stage)
		{
			if (stage == "vert")   return "__VERTEX_STAGE__";
			if (stage == "frag")   return "__FRAGMENT_STAGE__";
			if (stage == "comp")   return "__COMPUTE_STAGE__";
			if (stage == "rgen")   return "__RAYGEN_STAGE__";
			if (stage == "ahit")   return "__ANY_HIT_STAGE__";
			if (stage == "chit")   return "__CLOSEST_HIT_STAGE__";
			if (stage == "miss")   return "__MISS_STAGE__";
			if (stage == "sect")   return "__INTERSECTION_STAGE__";
			if (stage == "call")   return "__CALLABLE_STAGE__";

			BEY_CORE_VERIFY(false, "Unknown shader stage.");
			return "";
		}

		inline static const char* ShaderStageToString(const VkShaderStageFlagBits stage)
		{
			switch (stage)
			{
				case VK_SHADER_STAGE_VERTEX_BIT:    return "vert";
				case VK_SHADER_STAGE_FRAGMENT_BIT:  return "frag";
				case VK_SHADER_STAGE_COMPUTE_BIT:   return "comp";
				case VK_SHADER_STAGE_RAYGEN_BIT_KHR:  return "rgen";
				case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:  return "ahit";
				case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:  return "chit";
				case VK_SHADER_STAGE_MISS_BIT_KHR:  return "miss";
				case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:  return "sect";
				case VK_SHADER_STAGE_CALLABLE_BIT_KHR:  return "call";
			}
			BEY_CORE_ASSERT(false);
			return "UNKNOWN";
		}

		inline static eastl::string ShaderStagesToString(const VkShaderStageFlagBits stage)
		{
			eastl::string result;
			if (stage & VK_SHADER_STAGE_VERTEX_BIT)    result.append("vert|");
			if (stage & VK_SHADER_STAGE_FRAGMENT_BIT)  result.append("frag|");
			if (stage & VK_SHADER_STAGE_COMPUTE_BIT)   result.append("comp|");
			if (stage & VK_SHADER_STAGE_RAYGEN_BIT_KHR)  result.append("rgen|");
			if (stage & VK_SHADER_STAGE_ANY_HIT_BIT_KHR)  result.append("anyhit|");
			if (stage & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)  result.append("closesthit|");
			if (stage & VK_SHADER_STAGE_MISS_BIT_KHR)  result.append("miss|");
			if (stage & VK_SHADER_STAGE_INTERSECTION_BIT_KHR)  result.append("intersect|");
			if (stage & VK_SHADER_STAGE_CALLABLE_BIT_KHR)  result.append("call|");

			result.pop_back();

			BEY_CORE_ASSERT(!result.empty());
			return result;
		}

		inline static VkShaderStageFlagBits ShaderTypeFromString(const std::string_view type)
		{
			if (type == "vert") return VK_SHADER_STAGE_VERTEX_BIT;
			if (type == "frag") return VK_SHADER_STAGE_FRAGMENT_BIT;
			if (type == "comp") return VK_SHADER_STAGE_COMPUTE_BIT;
			if (type == "rgen") return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
			if (type == "ahit") return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
			if (type == "chit") return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
			if (type == "miss") return VK_SHADER_STAGE_MISS_BIT_KHR;
			if (type == "sect") return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
			if (type == "rint") return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
			if (type == "call") return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
			BEY_CORE_ASSERT(false);

			return VK_SHADER_STAGE_ALL;
		}

		inline static SourceLang ShaderLangFromExtension(const std::string_view type)
		{
			if (type == ".glsl")    return SourceLang::GLSL;
			if (type == ".hlsl")    return SourceLang::HLSL;

			BEY_CORE_ASSERT(false);

			return SourceLang::NONE;
		}

		inline static shaderc_shader_kind ShaderStageToShaderC(const VkShaderStageFlagBits stage)
		{
			switch (stage)
			{
				case VK_SHADER_STAGE_VERTEX_BIT:    return shaderc_vertex_shader;
				case VK_SHADER_STAGE_FRAGMENT_BIT:  return shaderc_fragment_shader;
				case VK_SHADER_STAGE_COMPUTE_BIT:   return shaderc_compute_shader;
				case VK_SHADER_STAGE_RAYGEN_BIT_KHR:  return shaderc_raygen_shader;
				case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:  return shaderc_anyhit_shader;
				case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:  return shaderc_closesthit_shader;
				case VK_SHADER_STAGE_MISS_BIT_KHR:  return shaderc_miss_shader;
				case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:  return shaderc_intersection_shader;
				case VK_SHADER_STAGE_CALLABLE_BIT_KHR:  return shaderc_callable_shader;
			}
			BEY_CORE_ASSERT(false);
			return {};
		}

		inline static const char* ShaderStageCachedFileExtension(const VkShaderStageFlagBits stage, bool debug)
		{
			if (debug)
			{
				switch (stage)
				{
					case VK_SHADER_STAGE_VERTEX_BIT:    return ".cached_vulkan_debug.vert";
					case VK_SHADER_STAGE_FRAGMENT_BIT:  return ".cached_vulkan_debug.frag";
					case VK_SHADER_STAGE_COMPUTE_BIT:   return ".cached_vulkan_debug.comp";
					case VK_SHADER_STAGE_RAYGEN_BIT_KHR:  return ".cached_vulkan_debug.rgen";
					case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:  return ".cached_vulkan_debug.ahit";
					case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:  return ".cached_vulkan_debug.chit";
					case VK_SHADER_STAGE_MISS_BIT_KHR:  return ".cached_vulkan_debug.miss";
					case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:  return ".cached_vulkan_debug.sect";
					case VK_SHADER_STAGE_CALLABLE_BIT_KHR:  return ".cached_vulkan_debug.call";
				}
			}
			else
			{
				switch (stage)
				{
					case VK_SHADER_STAGE_VERTEX_BIT:    return ".cached_vulkan.vert";
					case VK_SHADER_STAGE_FRAGMENT_BIT:  return ".cached_vulkan.frag";
					case VK_SHADER_STAGE_COMPUTE_BIT:   return ".cached_vulkan.comp";
					case VK_SHADER_STAGE_RAYGEN_BIT_KHR:  return ".cached_vulkan.rgen";
					case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:  return ".cached_vulkan.ahit";
					case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:  return ".cached_vulkan.chit";
					case VK_SHADER_STAGE_MISS_BIT_KHR:  return ".cached_vulkan.miss";
					case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:  return ".cached_vulkan.sect";
					case VK_SHADER_STAGE_CALLABLE_BIT_KHR:  return ".cached_vulkan.call";
				}
			}
			BEY_CORE_ASSERT(false);
			return "";
		}

#ifdef BEY_PLATFORM_WINDOWS
		inline static const wchar_t* HLSLShaderProfile(const VkShaderStageFlagBits stage)
		{
			switch (stage)
			{
				case VK_SHADER_STAGE_VERTEX_BIT:			return L"vs_6_0";
				case VK_SHADER_STAGE_FRAGMENT_BIT:			return L"ps_6_0";
				case VK_SHADER_STAGE_COMPUTE_BIT:			return L"cs_6_0";
				case VK_SHADER_STAGE_RAYGEN_BIT_KHR:		[[fallthrough]];
				case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:		[[fallthrough]];
				case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:	[[fallthrough]];
				case VK_SHADER_STAGE_MISS_BIT_KHR:			[[fallthrough]];
				case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:	[[fallthrough]];
				case VK_SHADER_STAGE_CALLABLE_BIT_KHR:		return L"lib_6_3";
			}
			BEY_CORE_ASSERT(false);
			return L"";
		}

		inline static VkShaderStageFlagBits HLSLShaderProfile(const wchar_t* stage)
		{
			if (std::wcscmp(L"vs_6_0", stage) == 0) return VK_SHADER_STAGE_VERTEX_BIT;
			if (std::wcscmp(L"ps_6_0", stage) == 0) return VK_SHADER_STAGE_FRAGMENT_BIT;
			if (std::wcscmp(L"cs_6_0", stage) == 0) return VK_SHADER_STAGE_COMPUTE_BIT;
			if (std::wcscmp(L"cs_6_6", stage) == 0) return VK_SHADER_STAGE_COMPUTE_BIT;
			if (std::wcscmp(L"lib_6_3", stage) == 0) return (VkShaderStageFlagBits)(VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR);
			BEY_CORE_ASSERT(false);
			return {};
		}
#else
		inline static const char* HLSLShaderProfile(const VkShaderStageFlagBits stage)
		{
			switch (stage)
			{
				case VK_SHADER_STAGE_VERTEX_BIT:    return "vs_6_0";
				case VK_SHADER_STAGE_FRAGMENT_BIT:  return "ps_6_0";
				case VK_SHADER_STAGE_COMPUTE_BIT:   return "cs_6_0";
				case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
				case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
				case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
				case VK_SHADER_STAGE_MISS_BIT_KHR:
				case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
				case VK_SHADER_STAGE_CALLABLE_BIT_KHR:  return "lib_6_3";
			}
			BEY_CORE_ASSERT(false);
			return "";
		}
#endif
	}
}

