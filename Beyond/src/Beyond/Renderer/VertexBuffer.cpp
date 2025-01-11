#include "pch.h"
#include "VertexBuffer.h"

#include "Renderer.h"

#include "Beyond/Platform/Vulkan/VulkanVertexBuffer.h"

#include "Beyond/Renderer/RendererAPI.h"

namespace Beyond {

	Ref<VertexBuffer> VertexBuffer::Create(void* data, uint64_t size, const std::string& name, VertexBufferUsage usage)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:    return nullptr;
			case RendererAPIType::Vulkan:  return Ref<VulkanVertexBuffer>::Create(data, size, name, usage);
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

	Ref<VertexBuffer> VertexBuffer::Create(uint64_t size, const std::string& name, VertexBufferUsage usage)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:    return nullptr;
			case RendererAPIType::Vulkan:  return Ref<VulkanVertexBuffer>::Create(size, name, usage);
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

}
