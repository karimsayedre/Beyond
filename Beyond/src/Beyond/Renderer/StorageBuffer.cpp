#include "pch.h"
#include "StorageBuffer.h"

#include "Beyond/Platform/Vulkan/VulkanStorageBuffer.h"
#include "Beyond/Renderer/RendererAPI.h"

namespace Beyond {

	Ref<StorageBuffer> StorageBuffer::Create(uint32_t size, const StorageBufferSpecification& specification)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:     return nullptr;
			case RendererAPIType::Vulkan:  return Ref<VulkanStorageBuffer>::Create(size, specification);
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
