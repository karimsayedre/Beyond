#include "pch.h"
#include "RendererStats.h"

namespace Beyond {

	namespace RendererUtils {

		static ResourceAllocationCounts s_ResourceAllocationCounts;
		ResourceAllocationCounts& GetResourceAllocationCounts()
		{
			return s_ResourceAllocationCounts;
		}

	}

}
