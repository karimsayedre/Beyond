#include "pch.h"
#include "DLSS.h"

#include "RendererAPI.h"
#include "Beyond/Platform/Vulkan/VulkanDLSS.h"

namespace Beyond {

	Ref<DLSS> DLSS::Create()
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::Vulkan: return Ref<VulkanDLSS>::Create();
		}
		BEY_CORE_ASSERT(false);
		return nullptr;
	}

}
