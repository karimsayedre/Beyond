#include "pch.h"
#include "UniformBuffer.h"

#include "Beyond/Renderer/Renderer.h"

#include "Beyond/Platform/Vulkan/VulkanUniformBuffer.h"

#include "Beyond/Renderer/RendererAPI.h"

namespace Beyond {

	Ref<UniformBuffer> UniformBuffer::Create(uint32_t size, const eastl::string& debugName)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:     return nullptr;
			case RendererAPIType::Vulkan:  return Ref<VulkanUniformBuffer>::Create(size, debugName);
		}

		BEY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
