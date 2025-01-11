#pragma once

#include "Beyond/Renderer/RenderCommandBuffer.h"

namespace Beyond {

	class VulkanRenderCommandBuffer : public RenderCommandBuffer
	{
	public:
		VulkanRenderCommandBuffer(uint32_t count, bool computeQueue, eastl::string debugName);
		VulkanRenderCommandBuffer(eastl::string debugName, bool swapchain);
		~VulkanRenderCommandBuffer() override;

		virtual void Begin() override;
		virtual void End() override;
		virtual void Submit(Ref<GPUSemaphore> signalSemaphore, Ref<GPUSemaphore> waitSemaphore) override;

		virtual float GetExecutionGPUTime(uint32_t frameIndex, uint32_t queryIndex = 0, bool wholeFrame = false) const override
		{
			if (queryIndex == 0 && !wholeFrame || queryIndex / 2 >= m_TimestampNextAvailableQuery / 2)
				return 0.0f;

			return m_ExecutionGPUTimes[frameIndex][queryIndex / 2];
		}

		const PipelineStatistics& GetPipelineStatistics(uint32_t frameIndex) const override { return m_PipelineStatisticsQueryResults[frameIndex]; }

		virtual uint32_t BeginTimestampQuery() override;
		virtual void EndTimestampQuery(uint32_t queryID) override;

		VkCommandBuffer GetActiveCommandBuffer() const { return m_ActiveCommandBuffer; }

		VkCommandBuffer GetCommandBuffer(uint32_t frameIndex) const
		{
			BEY_CORE_ASSERT(frameIndex < m_CommandBuffers.size());
			return m_CommandBuffers[frameIndex];
		}

		void RT_Begin();
		void RT_End();
		void RT_Submit(Ref<GPUSemaphore> signalSemaphore, Ref<GPUSemaphore> waitSemaphore);

	private:
		eastl::string m_DebugName;
		VkCommandPool m_CommandPool = nullptr;
		std::vector<VkCommandBuffer> m_CommandBuffers;
		VkCommandBuffer m_ActiveCommandBuffer = nullptr;
		std::vector<VkFence> m_WaitFences;
		VkQueue m_CachedQueue = nullptr;

		bool m_OwnedBySwapChain = false;

		uint32_t m_TimestampQueryCount = 0;
		uint32_t m_TimestampNextAvailableQuery = 2;
		std::vector<VkQueryPool> m_TimestampQueryPools;
		std::vector<VkQueryPool> m_PipelineStatisticsQueryPools;
		std::vector<std::vector<uint64_t>> m_TimestampQueryResults;
		std::vector<std::vector<float>> m_ExecutionGPUTimes;

		VkSemaphore m_Semaphores[3];

		uint32_t m_PipelineQueryCount = 0;
		std::vector<PipelineStatistics> m_PipelineStatisticsQueryResults;
	};

}
