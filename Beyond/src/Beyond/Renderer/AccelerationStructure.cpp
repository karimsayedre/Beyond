#include "pch.h"
#include "AccelerationStructure.h"

#include "RendererAPI.h"
#include "Beyond/Platform/Vulkan/VulkanAccelerationStructure.h"

namespace Beyond
{
	//Ref<AccelerationStructure> AccelerationStructure::Create()
	//{
	//	switch (RendererAPI::Current())
	//	{
	//	case RendererAPIType::None: return nullptr;
	//	case RendererAPIType::Vulkan: return Ref<VulkanAccelerationStructure>::Create();
	//	}
	//	BEY_CORE_ASSERT(false, "Unknown RendererAPI!");
	//	return nullptr;
	//}
}
