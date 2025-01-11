#pragma once

#include "GPUSemaphore.h"
#include "Beyond/Core/Ref.h"

#include "RasterPipeline.h"

namespace Beyond {

	class RenderCommandBuffer : public RefCounted
	{
	public:
		virtual ~RenderCommandBuffer() = default;

		virtual void Begin() = 0;
		virtual void End() = 0;
		virtual void Submit(Ref<GPUSemaphore> signalSemaphore = nullptr, Ref<GPUSemaphore> waitSemaphore = nullptr) = 0;

		virtual float GetExecutionGPUTime(uint32_t frameIndex, uint32_t queryIndex = 0, bool wholeFrame = false) const = 0;
		virtual const PipelineStatistics& GetPipelineStatistics(uint32_t frameIndex) const = 0;

		virtual uint32_t BeginTimestampQuery() = 0;
		virtual void EndTimestampQuery(uint32_t queryID) = 0;

		static Ref<RenderCommandBuffer> Create(uint32_t count, bool computeQueue, const eastl::string& debugName);
		static Ref<RenderCommandBuffer> CreateFromSwapChain(const eastl::string& debugName = "");
	};

}
