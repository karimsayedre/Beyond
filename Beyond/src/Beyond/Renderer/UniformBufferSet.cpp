#include "pch.h"
#include "UniformBufferSet.h"

#include "Beyond/Renderer/Renderer.h"

#include "Beyond/Platform/Vulkan/VulkanUniformBufferSet.h"

#include "Beyond/Renderer/RendererAPI.h"

namespace Beyond {

	Ref<UniformBufferSet> UniformBufferSet::Create(uint32_t size, const eastl::string& debugName, uint32_t framesInFlight)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:   return nullptr;
			case RendererAPIType::Vulkan: return Ref<VulkanUniformBufferSet>::Create(size, framesInFlight, debugName);
		}

		BEY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
