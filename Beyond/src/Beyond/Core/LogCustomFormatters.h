#pragma once

#include <filesystem>
#include <stacktrace>
#include <glm/glm.hpp>
#include <spdlog/fmt/fmt.h>
#include <EASTL/string.h>

namespace fmt {

	template<>
	struct formatter<glm::vec2>
	{
		char presentation = 'f';

		constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
		{
			auto it = ctx.begin(), end = ctx.end();
			if (it != end && (*it == 'f' || *it == 'e')) presentation = *it++;

			if (it != end && *it != '}') throw format_error("invalid format");

			return it;
		}

		template <typename FormatContext>
		auto format(const glm::vec2& vec, FormatContext& ctx) const -> decltype(ctx.out())
		{
			return presentation == 'f'
				? fmt::format_to(ctx.out(), "({:.3f}, {:.3f})", vec.x, vec.y)
				: fmt::format_to(ctx.out(), "({:.3e}, {:.3e})", vec.x, vec.y);
		}
	};

	template<>
	struct formatter<glm::vec3>
	{
		char presentation = 'f';

		constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
		{
			auto it = ctx.begin(), end = ctx.end();
			if (it != end && (*it == 'f' || *it == 'e')) presentation = *it++;

			if (it != end && *it != '}') throw format_error("invalid format");

			return it;
		}

		template <typename FormatContext>
		auto format(const glm::vec3& vec, FormatContext& ctx) const -> decltype(ctx.out())
		{
			return presentation == 'f'
				? fmt::format_to(ctx.out(), "({:.3f}, {:.3f}, {:.3f})", vec.x, vec.y, vec.z)
				: fmt::format_to(ctx.out(), "({:.3e}, {:.3e}, {:.3e})", vec.x, vec.y, vec.z);
		}
	};

	template<>
	struct formatter<glm::vec4>
	{
		char presentation = 'f';

		constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
		{
			auto it = ctx.begin(), end = ctx.end();
			if (it != end && (*it == 'f' || *it == 'e')) presentation = *it++;

			if (it != end && *it != '}') throw format_error("invalid format");

			return it;
		}

		template <typename FormatContext>
		auto format(const glm::vec4& vec, FormatContext& ctx) const -> decltype(ctx.out())
		{
			return presentation == 'f'
				? fmt::format_to(ctx.out(), "({:.3f}, {:.3f}, {:.3f}, {:.3f})", vec.x, vec.y, vec.z, vec.w)
				: fmt::format_to(ctx.out(), "({:.3e}, {:.3e}, {:.3e}, {:.3e})", vec.x, vec.y, vec.z, vec.w);
		}
	};

	template <>
	struct fmt::formatter<std::filesystem::path> : formatter<string_view>
	{
		template <typename FormatContext>
		auto format(const std::filesystem::path& p, FormatContext& ctx)
		{
			return formatter<string_view>::format(p.string(), ctx);
		}
	};


	template <typename CharT>
	struct fmt::formatter<eastl::basic_string<CharT>> : formatter<std::basic_string<CharT>>
	{
		auto format(const eastl::basic_string<CharT>& s, format_context& ctx) const
		{
			return formatter<std::basic_string<CharT>>::format(std::basic_string<CharT>(s.c_str(), s.size()), ctx);
		}
	};

	template <>
	struct fmt::formatter<eastl::string_view> : formatter<std::basic_string<char>>
	{
		auto format(const eastl::string_view s, format_context& ctx) const
		{
			return formatter<std::basic_string<char>>::format(std::basic_string<char>(s.data(), s.size()), ctx);
		}
	};

	// eastl_format_to function
	template<typename... Args>
	void eastl_format_to(eastl::string& out, fmt::format_string<Args...> fmt, Args&&... args)
	{
		// Get the formatted size
		size_t size = fmt::formatted_size(fmt, std::forward<Args>(args)...);

		// Reserve additional space in the existing string
		size_t original_size = out.size();
		out.resize(original_size + size);

		// Format directly into the eastl::string's buffer
		fmt::format_to(out.data() + original_size, fmt, std::forward<Args>(args)...);
	}

	template <typename Char, size_t SIZE>
	FMT_NODISCARD auto eastl_to_string(const basic_memory_buffer<Char, SIZE>& buf)
		-> eastl::basic_string<Char>
	{
		auto size = buf.size();
		detail::assume(size < eastl::basic_string<Char>().max_size());
		return eastl::basic_string<Char>(buf.data(), size);
	}

	FMT_FUNC auto eastl_vformat(string_view fmt, format_args args) -> eastl::string
	{
		// Don't optimize the "{}" case to keep the binary size small and because it
		// can be better optimized in fmt::format anyway.
		auto buffer = memory_buffer();
		detail::vformat_to(buffer, fmt, args);
		return eastl_to_string(buffer);
	}

	template <>
	struct fmt::formatter<std::stacktrace> : fmt::formatter<std::string>
	{
		auto format(const std::stacktrace& st, fmt::format_context& ctx) const
		{
			// Convert stacktrace to string
			std::string stacktrace_str = std::to_string(st);
			// Use the base formatter to format the string
			return fmt::formatter<std::string>::format(stacktrace_str, ctx);
		}
	};

	// Optimized eastl_format function
	template<typename... Args>
	eastl::string eastl_format(fmt::format_string<Args...> fmt, Args&&... args)
	{
		return eastl_vformat(fmt, fmt::make_format_args(args...));
	}

	//int Vsnprintf16(char16_t* pDestination, size_t n, const char16_t* pFormat, va_list args)
	//{
	//	return vsnprintf(pDestination, n, format, args);

	//}
	//int Vsnprintf32(char32_t* pDestination, size_t n, const char32_t* pFormat, va_list args)
	//{
	//	return vsnprintf(pDestination, n, format, args);
	//}
}
