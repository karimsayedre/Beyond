#pragma once
#include "AccelerationStructure.h"
#include "TLAS.h"

namespace Beyond
{
	
	class AccelerationStructureSet : public RefCounted
	{
	public:
	public:
		virtual ~AccelerationStructureSet() override = default;

		virtual Ref<TLAS> Get() = 0;
		virtual Ref<TLAS> RT_Get() = 0;
		virtual Ref<TLAS> Get(uint32_t frame) = 0;

		virtual void Set(Ref<TLAS> accelerationStructure, uint32_t frame) = 0;

		static Ref<AccelerationStructureSet> Create(bool motion, const eastl::string& name, uint32_t framesInFlight = 0);
	};

}
