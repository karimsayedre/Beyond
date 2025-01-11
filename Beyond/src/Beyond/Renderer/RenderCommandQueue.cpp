#include "pch.h"
#include "RenderCommandQueue.h"

#include "Beyond/Core/Timer.h"
#include "Beyond/Debug/Profiler.h"

#define BEY_RENDER_TRACE(...) BEY_CORE_TRACE(__VA_ARGS__)

namespace Beyond {

	RenderCommandQueue::RenderCommandQueue()
	{
		m_CommandBuffer = hnew uint8_t[10 * 1024 * 1024]; // 10mb buffer
		m_CommandBufferPtr = m_CommandBuffer;
		memset(m_CommandBuffer, 0, 10 * 1024 * 1024);
	}

	RenderCommandQueue::~RenderCommandQueue()
	{
		delete[] m_CommandBuffer;
	}

	void* RenderCommandQueue::Allocate(RenderCommandFn fn, uint32_t size
#ifdef SUBMIT_STACK_TRACES
		, std::stacktrace&& st
#endif
	)
	{
		// TODO: alignment
		*(RenderCommandFn*)m_CommandBufferPtr = fn;
		m_CommandBufferPtr += sizeof(RenderCommandFn);

		*(uint32_t*)m_CommandBufferPtr = size;
		m_CommandBufferPtr += sizeof(uint32_t);

		void* memory = m_CommandBufferPtr;
		m_CommandBufferPtr += size;

#ifdef SUBMIT_STACK_TRACES
		m_StackTraces.emplace_back(st);
#endif

		m_CommandCount++;
		return memory;
	}

	void RenderCommandQueue::Execute()
	{
		BEY_PROFILE_FUNC();
		//BEY_RENDER_TRACE("RenderCommandQueue::Execute -- {0} commands, {1} bytes", m_CommandCount, (m_CommandBufferPtr - m_CommandBuffer));
		byte* buffer = m_CommandBuffer;

		for (uint32_t i = 0; i < m_CommandCount; i++)
		{
			RenderCommandFn function = *(RenderCommandFn*)buffer;
			buffer += sizeof(RenderCommandFn);
			uint32_t size = *(uint32_t*)buffer;
			buffer += sizeof(uint32_t);
#ifdef SUBMIT_STACK_TRACES
			[[maybe_unused]] const auto& st = m_StackTraces.at(i);
#endif
			function(buffer);
			buffer += size;
		}

		m_CommandBufferPtr = m_CommandBuffer;
		m_CommandCount = 0;
#ifdef SUBMIT_STACK_TRACES
		m_StackTraces.clear();
#endif
	}

}
