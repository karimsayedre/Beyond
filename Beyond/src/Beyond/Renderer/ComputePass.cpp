#include "pch.h"
#include "ComputePass.h"

#include "Renderer.h"

#include "Beyond/Platform/Vulkan/VulkanComputePass.h"

#include "Beyond/Renderer/RendererAPI.h"

namespace Beyond {

	Ref<ComputePass> ComputePass::Create(const ComputePassSpecification& spec)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:    BEY_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPIType::Vulkan:  return Ref<VulkanComputePass>::Create(spec);
		}

		BEY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
