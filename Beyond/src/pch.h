#pragma once

#ifdef BEY_PLATFORM_WINDOWS
#define NOMINMAX
#include <Windows.h>
#endif

#include <algorithm>
#include <memory_resource>
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <random>
#include <set>
#include <string>
//#include <string_view>
//#include <unordered_map>
#include <vector>
#include <filesystem>
#include <ranges>


#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/string_map.h>
#include <EASTL/set.h>
#include <EASTL/unordered_map.h>
#include <EASTL/unordered_set.h>
#include <EASTL/vector.h>
#include <EASTL/vector_map.h>
#include "EASTL/fixed_vector.h"
#include "EASTL/fixed_hash_map.h"
#include "EASTL/fixed_map.h"


#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/bundled/color.h>
#include <spdlog/spdlog.h>

#include <Beyond/Core/Assert.h>
#include <Beyond/Core/Base.h>
#include <Beyond/Core/Events/Event.h>
#include <Beyond/Core/Log.h>
#include <Beyond/Core/Math/Mat4.h>
#include <Beyond/Core/Memory.h>
#include <Beyond/Core/Delegate.h>

#include <magic_enum.hpp>
using namespace magic_enum::bitwise_operators;

// Jolt (Safety because this file has to be included before all other Jolt headers, at all times)
#ifdef BEY_DEBUG // NOTE: This is a bit of a hacky fix for some dark magic that happens in Jolt
				// 				We'll need to address this in future.
#define JPH_ENABLE_ASSERTS
#endif
#include <Jolt/Jolt.h>
