#include "pch.h"
#include "GPUSemaphore.h"

#include "RendererAPI.h"
#include <Beyond/Platform/Vulkan/VulkanGPUSemaphore.h>

namespace Beyond {

	Ref<GPUSemaphore> GPUSemaphore::Create(bool signaled)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::Vulkan: return Ref<VulkanGPUSemaphore>::Create(signaled);
		}
		BEY_CORE_ASSERT(false);
		return nullptr;
	}
}
