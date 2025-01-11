#include "pch.h"
#include "TLAS.h"

#include "RendererAPI.h"
#include "Beyond/Platform/Vulkan/VulkanTLAS.h"

namespace Beyond {
	Ref<TLAS> TLAS::Create(bool motion, const eastl::string& name)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::Vulkan: return Ref<VulkanTLAS>::Create(motion, name);
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
