#include "pch.h"
#include "RasterPipeline.h"

#include "Renderer.h"

#include "Beyond/Platform/Vulkan/VulkanRasterPipeline.h"

#include "Beyond/Renderer/RendererAPI.h"

namespace Beyond {

	Ref<RasterPipeline> RasterPipeline::Create(const PipelineSpecification& spec)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:    return nullptr;
			case RendererAPIType::Vulkan:  return Ref<VulkanRasterPipeline>::Create(spec);
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

}
