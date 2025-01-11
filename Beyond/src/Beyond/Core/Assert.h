#pragma once

#include "Beyond/Core/Base.h"
#include "Log.h"

#ifdef BEY_PLATFORM_WINDOWS
	#define BEY_DEBUG_BREAK __debugbreak()
#elif defined(BEY_COMPILER_CLANG)
	#define BEY_DEBUG_BREAK __builtin_debugtrap()
#else
	#define BEY_DEBUG_BREAK
#endif

#ifdef BEY_DEBUG
	#define BEY_ENABLE_ASSERTS
#endif

#define BEY_ENABLE_VERIFY

#ifdef BEY_ENABLE_ASSERTS
	#ifdef BEY_COMPILER_CLANG
		#define BEY_CORE_ASSERT_MESSAGE_INTERNAL(...)  ::Beyond::Log::PrintAssertMessage(::Beyond::Log::Type::Core, "Assertion Failed", ##__VA_ARGS__)
		#define BEY_ASSERT_MESSAGE_INTERNAL(...)  ::Beyond::Log::PrintAssertMessage(::Beyond::Log::Type::Client, "Assertion Failed", ##__VA_ARGS__)
	#else
		#define BEY_CORE_ASSERT_MESSAGE_INTERNAL(...)  ::Beyond::Log::PrintAssertMessage(::Beyond::Log::Type::Core, "Assertion Failed" __VA_OPT__(,) __VA_ARGS__)
		#define BEY_ASSERT_MESSAGE_INTERNAL(...)  ::Beyond::Log::PrintAssertMessage(::Beyond::Log::Type::Client, "Assertion Failed" __VA_OPT__(,) __VA_ARGS__)
	#endif

	#define BEY_CORE_ASSERT(condition, ...) do{ if(!(condition)) { BEY_CORE_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); BEY_DEBUG_BREAK; } } while(false)
	#define BEY_ASSERT(condition, ...) do { if(!(condition)) { BEY_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); BEY_DEBUG_BREAK; } } while(false) 
#else
#define BEY_CORE_ASSERT(condition, ...) do {} while (false)
#define BEY_ASSERT(condition, ...) do {} while (false)
#endif

#ifdef BEY_ENABLE_VERIFY
	#ifdef BEY_COMPILER_CLANG
		#define BEY_CORE_VERIFY_MESSAGE_INTERNAL(...)  ::Beyond::Log::PrintAssertMessage(::Beyond::Log::Type::Core, "Verify Failed", ##__VA_ARGS__)
		#define BEY_VERIFY_MESSAGE_INTERNAL(...)  ::Beyond::Log::PrintAssertMessage(::Beyond::Log::Type::Client, "Verify Failed", ##__VA_ARGS__)
	#else
		#define BEY_CORE_VERIFY_MESSAGE_INTERNAL(...)  ::Beyond::Log::PrintAssertMessage(::Beyond::Log::Type::Core, "Verify Failed" __VA_OPT__(,) __VA_ARGS__)
		#define BEY_VERIFY_MESSAGE_INTERNAL(...)  ::Beyond::Log::PrintAssertMessage(::Beyond::Log::Type::Client, "Verify Failed" __VA_OPT__(,) __VA_ARGS__)

		#define BEY_CORE_REL_ASSERT_MESSAGE_INTERNAL(...)  ::Beyond::Log::GetCoreLogger()->error("Verify Failed" __VA_OPT__(,) __VA_ARGS__)
		#define BEY_REL_ASSERT_MESSAGE_INTERNAL(...)  ::Beyond::Log::GetCoreLogger()->error("Verify Failed" __VA_OPT__(,) __VA_ARGS__)
	#endif

	#define BEY_CORE_VERIFY(condition, ...) do { if(!(condition)) { BEY_CORE_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); BEY_DEBUG_BREAK; } } while(false)
	#define BEY_VERIFY(condition, ...) do { if(!(condition)) { BEY_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); BEY_DEBUG_BREAK; } } while(false)

	#define BEY_CORE_REL_ASSERT(condition, ...) do { if(!(condition)) { BEY_CORE_REL_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); } } while(false)
	#define BEY_VERIFY_REL_ASSERT(condition, ...) do { if(!(condition)) { BEY_REL_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); } } while(false)
#else
	#define BEY_CORE_VERIFY(condition, ...) do { } while (false)
	#define BEY_VERIFY(condition, ...) do { } while (false)
#endif
