#include "pch.h"
#include "RaytracingPipeline.h"

#include "RendererAPI.h"
#include "Beyond/Platform/Vulkan/VulkanRaytracingPipeline.h"

namespace Beyond {

	Ref<RaytracingPipeline> RaytracingPipeline::Create(Ref<Shader> raytracingShader)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:    return nullptr;
			case RendererAPIType::Vulkan:  return Ref<VulkanRaytracingPipeline>::Create(raytracingShader);
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}
}
