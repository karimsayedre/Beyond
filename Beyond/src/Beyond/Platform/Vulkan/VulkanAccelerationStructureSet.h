#pragma once
#include "Beyond/Renderer/AccelerationStructureSet.h"
#include "Beyond/Renderer/Renderer.h"

namespace Beyond {

	class VulkanAccelerationStructureSet : public AccelerationStructureSet
	{
	public:
		VulkanAccelerationStructureSet(bool motion, const eastl::string& name, uint32_t framesInFlight)
			: m_FramesInFlight(framesInFlight), m_Name(name)
		{
			if (framesInFlight == 0)
				m_FramesInFlight = Renderer::GetConfig().FramesInFlight;

			m_AccelerationStructures.resize(m_FramesInFlight);
			for (uint32_t frame = 0; frame < m_FramesInFlight; frame++)
				m_AccelerationStructures[frame] = TLAS::Create(motion, fmt::eastl_format("{} at frame: {}", name, frame));
		}

		virtual ~VulkanAccelerationStructureSet() override {}

		virtual Ref<TLAS> Get() override
		{
			uint32_t frame = Renderer::GetCurrentFrameIndex();
			return Get(frame);
		}

		virtual Ref<TLAS> RT_Get() override
		{
			uint32_t frame = Renderer::RT_GetCurrentFrameIndex();
			return Get(frame);
		}

		virtual Ref<TLAS> Get(uint32_t frame) override
		{
			BEY_CORE_ASSERT(m_AccelerationStructures.size() > frame);
			return m_AccelerationStructures.at(frame);
		}

		virtual void Set(Ref<TLAS> accelerationStructure, uint32_t frame = 0) override
		{
			m_AccelerationStructures[frame] = accelerationStructure;
		}

	private:
		uint32_t m_FramesInFlight = 0;
		std::vector<Ref<TLAS>> m_AccelerationStructures;
		eastl::string m_Name;
	};

}

