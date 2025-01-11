#pragma once

#include "Beyond/Core/Base.h"
#include "Beyond/Core/LogCustomFormatters.h"

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

#include <map>

#include "EASTLFormat.h"

#define BEY_ASSERT_MESSAGE_BOX (!BEY_DIST && BEY_PLATFORM_WINDOWS)

#if BEY_ASSERT_MESSAGE_BOX
#ifdef BEY_PLATFORM_WINDOWS
#include <Windows.h>
#endif
#endif

namespace Beyond {

	class Log
	{
	public:
		enum class Type : uint8_t
		{
			Core = 0, Client = 1
		};
		enum class Level : uint8_t
		{
			Trace = 0, Info, Warn, Error, Fatal
		};
		struct TagDetails
		{
			bool Enabled = true;
			Level LevelFilter = Level::Trace;
		};

	public:
		static void Init();
		static void Shutdown();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetEditorConsoleLogger() { return s_EditorConsoleLogger; }

		static bool HasTag(const std::string& tag) { return s_EnabledTags.contains(tag); }
		static std::map<std::string, TagDetails>& EnabledTags() { return s_EnabledTags; }

		template<typename FormatStr, typename... Args>
		constexpr static void PrintMessage(Log::Type type, Log::Level level, std::string_view tag, FormatStr&& format, Args&&... args);
		template<typename FormatStr, typename... Args>
		static void PrintAssertMessage(Log::Type type, std::string_view prefix, FormatStr&& format, Args&&... args);

		template<typename... Args>
		static void PrintAssertMessage(Log::Type type, std::string_view prefix);

	public:
		// Enum utils
		static const char* LevelToString(Level level)
		{
			switch (level)
			{
				case Level::Trace: return "Trace";
				case Level::Info:  return "Info";
				case Level::Warn:  return "Warn";
				case Level::Error: return "Error";
				case Level::Fatal: return "Fatal";
			}
			return "";
		}
		static Level LevelFromString(std::string_view string)
		{
			if (string == "Trace") return Level::Trace;
			if (string == "Info")  return Level::Info;
			if (string == "Warn")  return Level::Warn;
			if (string == "Error") return Level::Error;
			if (string == "Fatal") return Level::Fatal;

			return Level::Trace;
		}

	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
		static std::shared_ptr<spdlog::logger> s_EditorConsoleLogger;

		inline static std::map<std::string, TagDetails> s_EnabledTags;
	};

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tagged logs (prefer these!)                                                                                      //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Core logging
#define BEY_CORE_TRACE_TAG(tag, ...) ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Core, ::Beyond::Log::Level::Trace, tag, __VA_ARGS__)
#define BEY_CORE_INFO_TAG(tag, ...)  ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Core, ::Beyond::Log::Level::Info, tag, __VA_ARGS__)
#define BEY_CORE_WARN_TAG(tag, ...)  ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Core, ::Beyond::Log::Level::Warn, tag, __VA_ARGS__)
#define BEY_CORE_ERROR_TAG(tag, ...) ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Core, ::Beyond::Log::Level::Error, tag, __VA_ARGS__)
#define BEY_CORE_FATAL_TAG(tag, ...) ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Core, ::Beyond::Log::Level::Fatal, tag, __VA_ARGS__)

// Client logging
#define BEY_TRACE_TAG(tag, ...) ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Client, ::Beyond::Log::Level::Trace, tag, __VA_ARGS__)
#define BEY_INFO_TAG(tag, ...)  ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Client, ::Beyond::Log::Level::Info, tag, __VA_ARGS__)
#define BEY_WARN_TAG(tag, ...)  ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Client, ::Beyond::Log::Level::Warn, tag, __VA_ARGS__)
#define BEY_ERROR_TAG(tag, ...) ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Client, ::Beyond::Log::Level::Error, tag, __VA_ARGS__)
#define BEY_FATAL_TAG(tag, ...) ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Client, ::Beyond::Log::Level::Fatal, tag, __VA_ARGS__)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Core Logging
#define BEY_CORE_TRACE(...)  ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Core, ::Beyond::Log::Level::Trace, "", __VA_ARGS__)
#define BEY_CORE_INFO(...)   ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Core, ::Beyond::Log::Level::Info, "", __VA_ARGS__)
#define BEY_CORE_WARN(...)   ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Core, ::Beyond::Log::Level::Warn, "", __VA_ARGS__)
#define BEY_CORE_ERROR(...)  ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Core, ::Beyond::Log::Level::Error, "", __VA_ARGS__)
#define BEY_CORE_FATAL(...)  ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Core, ::Beyond::Log::Level::Fatal, "", __VA_ARGS__)

// Client Logging
#define BEY_TRACE(...)   ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Client, ::Beyond::Log::Level::Trace, "", __VA_ARGS__)
#define BEY_INFO(...)    ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Client, ::Beyond::Log::Level::Info, "", __VA_ARGS__)
#define BEY_WARN(...)    ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Client, ::Beyond::Log::Level::Warn, "", __VA_ARGS__)
#define BEY_ERROR(...)   ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Client, ::Beyond::Log::Level::Error, "", __VA_ARGS__)
#define BEY_FATAL(...)   ::Beyond::Log::PrintMessage(::Beyond::Log::Type::Client, ::Beyond::Log::Level::Fatal, "", __VA_ARGS__)

// Editor Console Logging Macros
#define BEY_CONSOLE_LOG_TRACE(...)   Beyond::Log::GetEditorConsoleLogger()->trace(__VA_ARGS__)
#define BEY_CONSOLE_LOG_INFO(...)    Beyond::Log::GetEditorConsoleLogger()->info(__VA_ARGS__)
#define BEY_CONSOLE_LOG_WARN(...)    Beyond::Log::GetEditorConsoleLogger()->warn(__VA_ARGS__)
#define BEY_CONSOLE_LOG_ERROR(...)   Beyond::Log::GetEditorConsoleLogger()->error(__VA_ARGS__)
#define BEY_CONSOLE_LOG_FATAL(...)   Beyond::Log::GetEditorConsoleLogger()->critical(__VA_ARGS__)

namespace Beyond {

	template<typename FormatStr, typename... Args>
	constexpr void Log::PrintMessage(Log::Type type, Log::Level level, const std::string_view tag, FormatStr&& format, Args&&... args)
	{
		std::string formattedMessage;
		if constexpr (sizeof...(args) == 0)
		{
			if constexpr (std::is_convertible_v<FormatStr, const char*> || std::is_convertible_v<std::string_view, FormatStr>)
				formattedMessage = std::string(format);
			else if constexpr (std::is_convertible_v<FormatStr, eastl::string_view> || std::is_convertible_v<FormatStr, eastl::string>)
				formattedMessage = std::string(format.c_str(), format.size());
			else
				formattedMessage = fmt::to_string(format);

		}
		if constexpr (std::is_convertible_v<FormatStr, const char*> || std::is_convertible_v<std::string_view, FormatStr>)
		{
			formattedMessage = fmt::v10::vformat(format, fmt::make_format_args(args...));
		}
		else if constexpr (std::is_same_v<FormatStr, eastl::string_view> || std::is_same_v<FormatStr, eastl::string>)
		{
			formattedMessage = fmt::v10::vformat(fmt::string_view(format.c_str(), format.size()), fmt::make_format_args(args...));
		}


		auto detail = s_EnabledTags[std::string(tag)];
		if (detail.Enabled && detail.LevelFilter <= level)
		{
			auto logger = (type == Type::Core) ? GetCoreLogger() : GetClientLogger();
			std::string logString = tag.empty() ? "{0}{1}" : "[{0}] {1}";

			switch (level)
			{
				case Level::Trace:
					logger->trace(fmt::runtime(logString), tag, formattedMessage);
					break;
				case Level::Info:
					logger->info(fmt::runtime(logString), tag, formattedMessage);
					break;
				case Level::Warn:
					logger->warn(fmt::runtime(logString), tag, formattedMessage);
					break;
				case Level::Error:
					logger->error(fmt::runtime(logString), tag, formattedMessage);
					break;
				case Level::Fatal:
					logger->critical(fmt::runtime(logString), tag, formattedMessage);
					break;
			}

		}
	}

	template<typename FormatStr, typename... Args>
	void Log::PrintAssertMessage(Log::Type type, std::string_view prefix, FormatStr&& format, Args&&... args)
	{
		fmt::v10::string_view fmtString;

		if constexpr (std::is_same_v<FormatStr, eastl::string>)
		{
			fmtString = fmt::v10::string_view(format.c_str(), format.size());
		}
		else
		{
			fmtString = fmt::v10::string_view(format);
		}

		auto formattedMessage = fmt::v10::vformat(fmtString, fmt::make_format_args(args...));

		auto logger = (type == Type::Core) ? GetCoreLogger() : GetClientLogger();
		logger->error("{0}: {1}", prefix, formattedMessage);

#if BEY_ASSERT_MESSAGE_BOX
		MessageBoxA(nullptr, formattedMessage.data(), "Beyond Assert", MB_OK | MB_ICONERROR);
#endif
	}

	template<typename... Args>
	inline void Log::PrintAssertMessage(Log::Type type, std::string_view prefix)
	{
		auto logger = (type == Type::Core) ? GetCoreLogger() : GetClientLogger();
		logger->error("{0}", prefix);
#if BEY_ASSERT_MESSAGE_BOX
		MessageBoxA(nullptr, "No message :(", "Beyond Assert", MB_OK | MB_ICONERROR);
#endif
	}
}
