#include "pch.h"
#include "RaytracingPass.h"

#include "RendererAPI.h"
#include "Beyond/Platform/Vulkan/VulkanRaytracingPass.h"

namespace Beyond {
	Ref<RaytracingPass> RaytracingPass::Create(const RaytracingPassSpecification& spec)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::Vulkan:  return Ref<VulkanRaytracingPass>::Create(spec);
		}

		BEY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
