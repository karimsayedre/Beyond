#pragma once
#include <EASTL/fixed_vector.h>

//#define SUBMIT_STACK_TRACES

namespace Beyond {

	class RenderCommandQueue
	{
	public:
		typedef void(*RenderCommandFn)(void*);

		RenderCommandQueue();
		~RenderCommandQueue();

		void* Allocate(RenderCommandFn func, uint32_t size
#ifdef SUBMIT_STACK_TRACES
			, std::stacktrace&& st
#endif
		);

		void Execute();

	private:
		uint8_t* m_CommandBuffer;
		uint8_t* m_CommandBufferPtr;
		uint32_t m_CommandCount = 0;

#ifdef SUBMIT_STACK_TRACES
		eastl::fixed_vector<std::stacktrace, 500, false> m_StackTraces;
#endif
	};



}
