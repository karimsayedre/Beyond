#pragma once
#include "Image.h"

namespace Beyond
{
	struct SamplerSpecification
	{
		eastl::string DebugName;
		bool CreateBindlessDescriptor = false;
		TextureFilter                MagFilter = TextureFilter::Linear;
		TextureFilter                MinFilter = TextureFilter::Linear;
		MipmapMode     MipmapMode = MipmapMode::Linear;
		TextureWrap    AddressModeU = TextureWrap::ClampToEdge;
		TextureWrap    AddressModeV = TextureWrap::ClampToEdge;
		TextureWrap    AddressModeW = TextureWrap::ClampToEdge;
		float                   MipLodBias = -0.04f;
		bool                AnisotropyEnable = false;
		float                   MaxAnisotropy = 16.f;
		bool                CompareEnable = false;
		DepthCompareOperator             CompareOp = DepthCompareOperator::Always;
		float                   MinLod = 0.0f;
		float                   MaxLod = 100.0f;
		glm::vec4           BorderColor = {};
		bool                UnnormalizedCoordinates = false;
	};

	class Sampler : public RendererResource
	{
	public:
		static Ref<Sampler> Create(const SamplerSpecification& specification);
		virtual uint32_t GetBindlessIndex() const override = 0;
		virtual ResourceDescriptorInfo GetDescriptorInfo() const override = 0;
	};
}

