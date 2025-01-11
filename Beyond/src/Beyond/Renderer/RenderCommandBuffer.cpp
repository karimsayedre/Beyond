#include "pch.h"
#include "RenderCommandBuffer.h"

#include "Beyond/Platform/Vulkan/VulkanRenderCommandBuffer.h"
#include "Beyond/Renderer/RendererAPI.h"

namespace Beyond {

	Ref<RenderCommandBuffer> RenderCommandBuffer::Create(uint32_t count, bool computeQueue, const eastl::string& debugName)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:    return nullptr;
			case RendererAPIType::Vulkan:  return Ref<VulkanRenderCommandBuffer>::Create(count, computeQueue, debugName);
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

	Ref<RenderCommandBuffer> RenderCommandBuffer::CreateFromSwapChain(const eastl::string& debugName)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:    return nullptr;
			case RendererAPIType::Vulkan:  return Ref<VulkanRenderCommandBuffer>::Create(debugName, true);
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

}
