#pragma once

#include <EASTL/string.h>
#include <EASTL/algorithm.h>
#include <charconv>
#include <type_traits>
#include <filesystem>

namespace fmt {

	// Helper function to convert arithmetic types to eastl::string
	template<typename T>
	eastl::string to_eastl_string_arithmetic(T value)
	{
		constexpr size_t buf_size = 32; // Should be enough for most numeric types
		char buffer[buf_size];
		auto [ptr, ec] = std::to_chars(buffer, buffer + buf_size, value);
		return eastl::string(buffer, ptr - buffer);
	}

	// Main to_eastl_string function
	template<typename T>
	eastl::string to_eastl_string(const T& value)
	{
		if constexpr (std::is_same_v<T, eastl::string>)
		{
			return value;
		}
		else if constexpr (std::is_arithmetic_v<T>)
		{
			return to_eastl_string_arithmetic(value);
		}
		else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>)
		{
			return eastl::string(value.data(), value.size());
		}
		else if constexpr (std::is_same_v<T, const char*>)
		{
			return eastl::string(value);
		}
		else if constexpr (std::is_same_v<T, std::filesystem::path>)
		{
			return eastl::string(value.string().c_str());
		}
		else
		{
			// Fallback for types not explicitly handled
			return eastl::string(std::to_string(value).c_str());
		}
	}

	// Specializations for common types
	template<> inline eastl::string to_eastl_string(const bool& value)
	{
		return value ? eastl::string("true") : eastl::string("false");
	}

	template<> inline eastl::string to_eastl_string(const char& value)
	{
		return eastl::string(1, value);
	}

	// Helper function for floating-point types to handle precision
	template<typename T>
	eastl::string to_eastl_string_float(const T& value, int precision = 6)
	{
		eastl::string result;
		result.reserve(32); // Reserve some space to avoid small allocations

		char format[16];
		std::snprintf(format, sizeof(format), "%%.%df", precision);

		char buffer[32];
		int size = std::snprintf(buffer, sizeof(buffer), format, value);

		if (size > 0)
		{
			result.assign(buffer, size);
		}

		return result;
	}

	//template <typename... Args>
	//class eastl_format_string
	//{
	//public:
	//	// Constructor takes an eastl::string and stores it
	//	eastl_format_string(const eastl::string& formatStr)
	//		: m_formatString(formatStr.c_str())
	//	{
	//	}

	//	//// Constructor takes a const char* (to support string literals)
	//	//eastl_format_string(const char* formatStr)
	//	//	: m_formatString(formatStr)
	//	//{
	//	//}

	//	// Provide access to the underlying C-string for fmt::format
	//	const char* c_str() const { return m_formatString; }

	//private:
	//	const char* m_formatString;
	//};


	//// Overload fmt::format to accept eastl_format_string
	//template <typename... Args>
	//std::string eastl_format(const eastl_format_string<Args...>& formatStr, Args&&... args)
	//{
	//	// Use fmt::format with the underlying format string
	//	return fmt::format(formatStr.c_str(), std::forward<Args>(args)...);
	//}

	//template <typename... Args>
	//inline std::string format(const char* fmtStr, Args&&... args)
	//{
	//	return fmt::format(std::string_view(fmtStr), std::forward<Args>(args)...);
	//}

	//template <typename... Args>
	//inline std::string format(const eastl::string& fmtStr, Args&&... args)
	//{
	//	return fmt::format(std::string_view(fmtStr.c_str(), fmtStr.size()), std::forward<Args>(args)...);
	//}

	template <>
	struct fmt::formatter<eastl::string> : fmt::formatter<std::string_view>
	{
		template <typename FormatContext>
		auto format(const eastl::string& s, FormatContext& ctx)
		{
			return fmt::formatter<std::string_view>::format(std::string_view(s.c_str(), s.size()), ctx);
		}
	};

	template<> inline eastl::string to_eastl_string(const float& value)
	{
		return to_eastl_string_float(value);
	}

	template<> inline eastl::string to_eastl_string(const double& value)
	{
		return to_eastl_string_float(value);
	}

	template<> inline eastl::string to_eastl_string(const long double& value)
	{
		return to_eastl_string_float(value);
	}

}
