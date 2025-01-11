#include "pch.h"
#include "Raytracer.h"

#include "RendererAPI.h"
#include "Beyond/Platform/Vulkan/VulkanRaytracer.h"


namespace Beyond {
	Ref<Raytracer> Raytracer::Create(const Ref<AccelerationStructureSet> as)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::Vulkan:  return Ref<VulkanRaytracer>::Create(as);
		}

		BEY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
