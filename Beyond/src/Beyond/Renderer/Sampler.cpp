#include "pch.h"
#include "Sampler.h"

#include "RendererAPI.h"
#include "Beyond/Platform/Vulkan/VulkanSampler.h"

namespace Beyond {

	Ref<Sampler> Sampler::Create(const SamplerSpecification& specification)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None: return nullptr;
			case RendererAPIType::Vulkan: return Ref<VulkanSampler>::Create(specification);
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

}
