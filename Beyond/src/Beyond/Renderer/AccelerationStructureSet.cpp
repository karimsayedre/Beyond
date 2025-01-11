#include "pch.h"
#include "AccelerationStructureSet.h"

#include "RendererAPI.h"
#include "Beyond/Renderer/Renderer.h"
#include "Beyond/Platform/Vulkan/VulkanAccelerationStructureSet.h"

namespace Beyond {
	Ref<AccelerationStructureSet> AccelerationStructureSet::Create(bool motion, const eastl::string& name, uint32_t framesInFlight)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:    return nullptr;
			case RendererAPIType::Vulkan:  return Ref<VulkanAccelerationStructureSet>::Create(motion, name, framesInFlight);
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}
}

