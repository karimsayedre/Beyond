#pragma once

#include "Beyond/Renderer/UniformBufferSet.h"

#include <map>

namespace Beyond {

	class VulkanUniformBufferSet : public UniformBufferSet
	{
	public:
		VulkanUniformBufferSet(uint32_t size, uint32_t framesInFlight, const eastl::string& debugName)
			: m_FramesInFlight(framesInFlight)
		{
			if (framesInFlight == 0)
				m_FramesInFlight = Renderer::GetConfig().FramesInFlight;

			m_UniformBuffers.resize(m_FramesInFlight);
			for (uint32_t frame = 0; frame < m_FramesInFlight; frame++)
				m_UniformBuffers[frame] = UniformBuffer::Create(size, fmt::eastl_format("{} frame: {}", debugName, frame));
		}

		virtual ~VulkanUniformBufferSet() {}

		virtual Ref<UniformBuffer> Get() override
		{
			uint32_t frame = Renderer::GetCurrentFrameIndex();
			return Get(frame);
		}

		virtual Ref<UniformBuffer> RT_Get() override
		{
			uint32_t frame = Renderer::RT_GetCurrentFrameIndex();
			return Get(frame);
		}

		virtual Ref<UniformBuffer> Get(uint32_t frame) override
		{
			BEY_CORE_ASSERT(m_UniformBuffers.size() > frame);
			return m_UniformBuffers.at(frame);
		}

		virtual void Set(Ref<UniformBuffer> uniformBuffer, uint32_t frame = 0) override
		{
			m_UniformBuffers[frame] = uniformBuffer;
		}
	private:
		uint32_t m_FramesInFlight = 0;
		std::vector<Ref<UniformBuffer>> m_UniformBuffers;
	};
}
