#pragma once

#include "VulkanShader.h"
#include <map>

#include "sbtwrapper_vk.hpp"
#include "Beyond/Renderer/RaytracingPipeline.h"


namespace Beyond {

	class VulkanRaytracingPipeline : public RaytracingPipeline
	{
	public:
		VulkanRaytracingPipeline(Ref<Shader> shader);


		virtual void Begin(Ref<RenderCommandBuffer> renderCommandBuffer = nullptr) override;
		virtual void RT_Begin(Ref<RenderCommandBuffer> renderCommandBuffer = nullptr) override;
		void RT_Begin(VkCommandBuffer commandBuffer);
		void Dispatch(uint32_t width, uint32_t height, const uint32_t depth = 1) const;
		virtual void End() override;

		virtual Ref<Shader> GetShader() const override { return m_Shader; }

		VkCommandBuffer GetActiveCommandBuffer() { return m_ActiveComputeCommandBuffer; }
		VkPipelineLayout GetLayout() const { return m_PipelineLayout; }

		void RT_SetPushConstants(Buffer constants) const;
		void RT_SetPushConstants(Buffer constants, VkShaderStageFlagBits stages) const;

		~VulkanRaytracingPipeline() override;
		void CreatePipeline();

		virtual void BufferMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<StorageBuffer> storageBuffer, ResourceAccessFlags fromAccess, ResourceAccessFlags toAccess) override;
		virtual void BufferMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<StorageBuffer> storageBuffer, PipelineStage fromStage, ResourceAccessFlags fromAccess, PipelineStage toStage, ResourceAccessFlags toAccess) override;

		virtual void ImageMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image, ResourceAccessFlags fromAccess, ResourceAccessFlags toAccess) override;
		virtual void ImageMemoryBarrier(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Image2D> image, PipelineStage fromStage, ResourceAccessFlags fromAccess, PipelineStage toStage, ResourceAccessFlags toAccess) override;
		void RT_CreatePipeline();
	private:

		void Release() override;

	private:
		Ref<VulkanShader> m_Shader;

		VkCommandBuffer m_ActiveComputeCommandBuffer = nullptr;

		bool m_UsingGraphicsQueue = false;
		PipelineSpecification m_Specification;

		VkPipelineLayout m_PipelineLayout = nullptr;
		VkPipeline m_VulkanPipeline = nullptr;
		VkPipelineCache m_PipelineCache = nullptr;
		nvvk::SBTWrapper m_RaytracingSBT;

	};

}
