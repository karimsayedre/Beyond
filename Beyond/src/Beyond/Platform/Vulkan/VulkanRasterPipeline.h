#pragma once

#include "Beyond/Renderer/RasterPipeline.h"
#include <Beyond/Platform/Vulkan/Vulkan.h>

namespace Beyond {

	class VulkanRasterPipeline : public RasterPipeline
	{
	public:
		VulkanRasterPipeline(const PipelineSpecification& spec);
		void Release();
		virtual ~VulkanRasterPipeline();

		virtual PipelineSpecification& GetSpecification() { return m_Specification; }
		virtual const PipelineSpecification& GetSpecification() const { return m_Specification; }

		virtual void Invalidate() override;
		virtual void RT_Invalidate();

		virtual Ref<Shader> GetShader() const override { return m_Specification.Shader; }

		bool IsDynamicLineWidth() const;

		VkPipeline GetVulkanPipeline() { return m_VulkanPipeline; }
		VkPipelineLayout GetVulkanPipelineLayout() { return m_PipelineLayout; }

	private:
		PipelineSpecification m_Specification;

		VkPipelineLayout m_PipelineLayout = nullptr;
		VkPipeline m_VulkanPipeline = nullptr;
		VkPipelineCache m_PipelineCache = nullptr;
	};

}
