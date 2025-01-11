#include "pch.h"
#include "BLAS.h"

#include "RendererAPI.h"
#include "Beyond/Platform/Vulkan/VulkanBLAS.h"

namespace Beyond {

	Ref<BLAS> BLAS::Create()
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:    return nullptr;
			case RendererAPIType::Vulkan:  return Ref<VulkanBLAS>::Create();
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

	Ref<BLAS> BLAS::Create(Ref<BLAS> other)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:    return nullptr;
			case RendererAPIType::Vulkan:  return Ref<VulkanBLAS>::Create(other.As<VulkanBLAS>());
		}
		BEY_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

}
