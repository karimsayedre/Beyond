#include "pch.h"
#include "IndexBuffer.h"

#include "Renderer.h"

#include "Beyond/Platform/Vulkan/VulkanIndexBuffer.h"

#include "Beyond/Renderer/RendererAPI.h"

namespace Beyond {

	Ref<IndexBuffer> IndexBuffer::Create(void* data, const std::string& name, uint64_t size)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:    return nullptr;
			case RendererAPIType::Vulkan:  return Ref<VulkanIndexBuffer>::Create(data, name, size);
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

}
