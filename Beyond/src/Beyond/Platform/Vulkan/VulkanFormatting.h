#pragma once

#include <vulkan/vulkan.h>
#include <magic_enum.hpp>

template <>
struct fmt::formatter<VkResult> : fmt::formatter<std::string_view>
{
	// Provide a way to format VkResult as a string
	template <typename FormatContext>
	auto format(VkResult result, FormatContext& ctx)
	{
		std::string_view name;
		switch (result)
		{
			case VK_SUCCESS: name = "VK_SUCCESS"; break;
			case VK_NOT_READY: name = "VK_NOT_READY"; break;
			case VK_TIMEOUT: name = "VK_TIMEOUT"; break;
			case VK_EVENT_SET: name = "VK_EVENT_SET"; break;
			case VK_EVENT_RESET: name = "VK_EVENT_RESET"; break;
				// Add other VkResult cases as needed
			default: name = "UNKNOWN_VK_RESULT"; break;
		}
		return fmt::formatter<std::string_view>::format(name, ctx);
	}
};



template <>
struct fmt::formatter<VkDevice> : fmt::formatter<void*>
{
	// Format VkDevice as a pointer
	template <typename FormatContext>
	auto format(VkDevice device, FormatContext& ctx)
	{
		return fmt::formatter<void*>::format(static_cast<void*>(device), ctx);
	}
};

template <typename T>
struct fmt::formatter<T, char, std::enable_if_t<std::is_enum_v<T>>>
{
	// Parse format specifications (not used here, so no-op)
	constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

	// Format the enum using magic_enum
	template <typename FormatContext>
	auto format(const T& enumValue, FormatContext& ctx)
	{
		// Use magic_enum to convert enum to string, fallback to integer value if not found
		if (auto name = magic_enum::enum_name(enumValue); !name.empty())
		{
			return fmt::format_to(ctx.out(), "{}", name);
		}
		else
		{
			return fmt::format_to(ctx.out(), "{}", static_cast<int>(enumValue));
		}
	}
};
