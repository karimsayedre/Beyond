#include "pch.h"
#include "PipelineCompute.h"

#include "Beyond/Renderer/RendererAPI.h"
#include "Beyond/Platform/Vulkan/VulkanComputePipeline.h"

namespace Beyond {

	Ref<PipelineCompute> PipelineCompute::Create(Ref<Shader> computeShader)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None: return nullptr;
			case RendererAPIType::Vulkan: return Ref<VulkanComputePipeline>::Create(computeShader);
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

}