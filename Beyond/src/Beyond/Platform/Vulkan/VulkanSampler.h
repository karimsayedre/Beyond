#pragma once
#include <Volk/volk.h>
#include "Beyond/Renderer/Sampler.h"

#include "Beyond/Renderer/Image.h"

namespace Beyond {

	inline static VkFilter VulkanSamplerFilter(TextureFilter filter)
	{
		switch (filter)
		{
			case TextureFilter::Linear:   return VK_FILTER_LINEAR;
			case TextureFilter::Nearest:  return VK_FILTER_NEAREST;
			case TextureFilter::Cubic:   return VK_FILTER_CUBIC_IMG;
		}
		BEY_CORE_ASSERT(false, "Unknown filter.");
		return (VkFilter)0;
	}

	inline static VkSamplerMipmapMode VulkanSamplerMipmapMode(MipmapMode mode)
	{
		switch (mode)
		{
			case MipmapMode::Nearest:   return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			case MipmapMode::Linear:  return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		}
		BEY_CORE_ASSERT(false, "Unknown mode.");
		return {};
	}

	inline static VkSamplerAddressMode VulkanSamplerWrap(TextureWrap mode)
	{
		switch (mode)
		{
			case TextureWrap::Repeat:   return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			case TextureWrap::MirroredRepeat:   return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			case TextureWrap::ClampToBorder:   return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			case TextureWrap::ClampToEdge:   return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			case TextureWrap::MirrorClampToEdge:   return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
		}
		BEY_CORE_ASSERT(false, "Unknown mode.");
		return {};
	}

	class VulkanSampler : public Sampler
	{
	public:
		VulkanSampler(const SamplerSpecification& spec);

		uint32_t GetBindlessIndex() const override { return m_BindlessIndex; }
		uint32_t GetFlaggedBindlessIndex() const override { return m_BindlessIndex; }

		ResourceDescriptorInfo GetDescriptorInfo() const override { return (ResourceDescriptorInfo)&m_DescriptorImageInfo; }
		const VkDescriptorImageInfo& GetVulkanDescriptorInfo() const { return m_DescriptorImageInfo; }
	private:

		VkSampler m_Sampler{};
		uint32_t m_BindlessIndex{};
		VkDescriptorImageInfo m_DescriptorImageInfo = {};

		SamplerSpecification m_Specification;
	};

}
