#pragma once

#define BEY_ENABLE_PROFILING !BEY_DIST

#if BEY_ENABLE_PROFILING 
#include <tracy/Tracy.hpp>
#endif

#if BEY_ENABLE_PROFILING
#define BEY_PROFILE_MARK_FRAME			FrameMark
// NOTE: Use BEY_PROFILE_FUNC ONLY at the top of a function
//				Use BEY_PROFILE_SCOPE / BEY_PROFILE_SCOPE_DYNAMIC for an inner scope
#define BEY_PROFILE_FUNC(...)			ZoneScoped##__VA_OPT__(N(__VA_ARGS__))
#define BEY_PROFILE_SCOPE(...)			BEY_PROFILE_FUNC(__VA_ARGS__)
#define BEY_PROFILE_SCOPE_DYNAMIC(NAME)  ZoneScoped; ZoneName(NAME, strlen(NAME))
#define BEY_PROFILE_THREAD(...)          tracy::SetThreadName(__VA_ARGS__)
#else
#define BEY_PROFILE_FRAME(...) do {} while(false)
#define BEY_PROFILE_FUNC(...) do {} while(false)
#define BEY_PROFILE_TAG(NAME, ...)  do {} while(false)
#define BEY_PROFILE_SCOPE(...)	do {} while(false)
#define BEY_PROFILE_SCOPE_DYNAMIC(NAME) do {} while(false)
#define BEY_PROFILE_THREAD(...) do {} while(false)
#define BEY_PROFILE_MARK_FRAME

#endif
