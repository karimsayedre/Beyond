#include "pch.h"
#include "RendererContext.h"

#include "Beyond/Renderer/RendererAPI.h"

#include "Beyond/Platform/Vulkan/VulkanContext.h"

namespace Beyond {

	Ref<RendererContext> RendererContext::Create()
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:    return nullptr;
			case RendererAPIType::Vulkan:  return Ref<VulkanContext>::Create();
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

}