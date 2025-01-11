#include "pch.h"

#include "UniformBufferSet.h"

#include "Beyond/Renderer/Renderer.h"

#include "StorageBufferSet.h"

#include "Beyond/Platform/Vulkan/VulkanStorageBufferSet.h"
#include "Beyond/Renderer/RendererAPI.h"

namespace Beyond {

	Ref<StorageBufferSet> StorageBufferSet::Create(const StorageBufferSpecification& specification, uint32_t size, uint32_t framesInFlight)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:   return nullptr;
			case RendererAPIType::Vulkan: return Ref<VulkanStorageBufferSet>::Create(specification, size, framesInFlight);
		}

		BEY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}