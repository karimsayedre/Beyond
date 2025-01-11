#pragma once

#include "Beyond/Renderer/RenderPass.h"

#include "DescriptorSetManager.h"


namespace Beyond {


	class VulkanRenderPass : public RenderPass
	{
	public:
		VulkanRenderPass(const RenderPassSpecification& spec);
		virtual ~VulkanRenderPass();

		virtual RenderPassSpecification& GetSpecification() override { return m_Specification; }
		virtual const RenderPassSpecification& GetSpecification() const override { return m_Specification; }

		virtual void SetInput(const eastl::string& name, Ref<UniformBufferSet> uniformBufferSet) override;
		virtual void SetInput(const eastl::string& name, Ref<UniformBuffer> uniformBuffer) override;

		virtual void SetInput(const eastl::string& name, Ref<StorageBufferSet> storageBufferSet) override;
		virtual void SetInput(const eastl::string& name, Ref<StorageBuffer> storageBuffer) override;

		virtual void SetInput(const eastl::string& name, Ref<Texture2D> texture) override;
		virtual void SetInput(const eastl::string& name, Ref<TextureCube> textureCube) override;
		virtual void SetInput(const eastl::string& name, Ref<Image2D> image) override;

		virtual Ref<Image2D> GetOutput(uint32_t index) override;
		virtual Ref<Image2D> GetDepthOutput() override;
		virtual uint32_t GetFirstSetIndex() const override;

		virtual Ref<Framebuffer> GetTargetFramebuffer() const override;
		virtual Ref<RasterPipeline> GetPipeline() const override;

		virtual bool Validate() override;
		virtual void Bake() override;
		virtual bool Baked() const override { return (bool)m_DescriptorSetManager->GetDescriptorPool(); }
		virtual void Prepare() override;

		bool HasDescriptorSets() const;
		const std::vector<VkDescriptorSet>& GetDescriptorSets(uint32_t frameIndex) const;

		bool IsInputValid(eastl::string_view name) const;
		const RenderPassInputDeclaration* GetInputDeclaration(eastl::string_view name) const;
	private:
		bool IsInvalidated(uint32_t frame, uint32_t set, uint32_t binding) const;
	private:
		RenderPassSpecification m_Specification;
		Ref<DescriptorSetManager> m_DescriptorSetManager;
	};

}
