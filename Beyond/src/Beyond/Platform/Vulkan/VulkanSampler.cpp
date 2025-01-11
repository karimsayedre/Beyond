#include "pch.h"
#include "Beyond/Renderer/Renderer.h"
#include "VulkanAPI.h"
#include "VulkanContext.h"
#include "VulkanSampler.h"

#include "Beyond/Renderer/RendererStats.h"

namespace Beyond {


	Beyond::VulkanSampler::VulkanSampler(const SamplerSpecification& spec) : m_Specification(spec)
	{
		//if (m_Specification.CreateBindlessDescriptor)
		Renderer::Submit([inst = Ref(this)]() mutable
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			VkSamplerCreateInfo samplerCreateInfo = {};
			samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerCreateInfo.maxAnisotropy = inst->m_Specification.MaxAnisotropy;
			samplerCreateInfo.magFilter = VulkanSamplerFilter(inst->m_Specification.MagFilter);
			samplerCreateInfo.minFilter = VulkanSamplerFilter(inst->m_Specification.MinFilter);
			samplerCreateInfo.mipmapMode = VulkanSamplerMipmapMode(inst->m_Specification.MipmapMode);
			samplerCreateInfo.anisotropyEnable = inst->m_Specification.AnisotropyEnable;

			samplerCreateInfo.addressModeU = VulkanSamplerWrap(inst->m_Specification.AddressModeU);
			samplerCreateInfo.addressModeV = VulkanSamplerWrap(inst->m_Specification.AddressModeV);
			samplerCreateInfo.addressModeW = VulkanSamplerWrap(inst->m_Specification.AddressModeW);
			samplerCreateInfo.mipLodBias = inst->m_Specification.MipLodBias;
			samplerCreateInfo.minLod = inst->m_Specification.MinLod;
			samplerCreateInfo.maxLod = inst->m_Specification.MaxLod;
			samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; 
			samplerCreateInfo.flags = 0;

			inst->m_Sampler = Vulkan::CreateSampler(samplerCreateInfo);
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_SAMPLER, fmt::eastl_format("{} sampler", inst->m_Specification.DebugName), inst->m_Sampler);

			inst->m_DescriptorImageInfo.sampler = inst->m_Sampler;

			inst->m_BindlessIndex = RendererUtils::GetResourceAllocationCounts().Samplers++;
		});
	}

}
