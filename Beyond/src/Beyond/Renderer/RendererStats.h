#pragma once

namespace Beyond {

	namespace RendererUtils {

		struct ResourceAllocationCounts
		{
			std::atomic_uint32_t Samplers = 0;
		};

		ResourceAllocationCounts& GetResourceAllocationCounts();
	}
}
