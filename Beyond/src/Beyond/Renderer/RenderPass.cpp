#include "pch.h"
#include "RenderPass.h"

#include "Renderer.h"

#include "Beyond/Platform/Vulkan/VulkanRenderPass.h"

#include "Beyond/Renderer/RendererAPI.h"

namespace Beyond {

	Ref<RenderPass> RenderPass::Create(const RenderPassSpecification& spec)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:    BEY_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPIType::Vulkan:  return Ref<VulkanRenderPass>::Create(spec);
		}

		BEY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}