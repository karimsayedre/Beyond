#pragma once

#include "Beyond/Core/FastRandom.h"

namespace Beyond
{
	namespace RandomGen
	{
		// Note: Do NOT use this in the render loop.
		inline static FastRandom s_RandomGen;
	}
}
