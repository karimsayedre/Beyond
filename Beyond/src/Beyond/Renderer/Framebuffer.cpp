#include "pch.h"
#include "Framebuffer.h"

#include "Beyond/Platform/Vulkan/VulkanFramebuffer.h"

#include "Beyond/Renderer/RendererAPI.h"

namespace Beyond {

	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
	{
		Ref<Framebuffer> result = nullptr;

		switch (RendererAPI::Current())
		{
			case RendererAPIType::None:		return nullptr;
			case RendererAPIType::Vulkan:	result = Ref<VulkanFramebuffer>::Create(spec); break;
		}
		return result;
	}

}
