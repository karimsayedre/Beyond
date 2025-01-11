#pragma once

#include "Beyond/Renderer/ComputePass.h"
#include "VulkanRenderPass.h"
#include "DescriptorSetManager.h"

#include <set>

namespace Beyond {

	class VulkanComputePass : public ComputePass
	{
	public:
		VulkanComputePass(const ComputePassSpecification& spec);
		~VulkanComputePass() override;

		virtual ComputePassSpecification& GetSpecification() override { return m_Specification; }
		virtual const ComputePassSpecification& GetSpecification() const override { return m_Specification; }

		virtual Ref<Shader> GetShader() const override { return m_Specification.Pipeline->GetShader(); }

		virtual void SetInput(const eastl::string& name, Ref<UniformBufferSet> uniformBufferSet) override;
		virtual void SetInput(const eastl::string& name, Ref<UniformBuffer> uniformBuffer) override;

		virtual void SetInput(const eastl::string& name, Ref<StorageBufferSet> storageBufferSet) override;
		virtual void SetInput(const eastl::string& name, Ref<StorageBuffer> storageBuffer) override;

		virtual void SetInput(const eastl::string& name, Ref<AccelerationStructureSet> accelerationStructureSet) override;
		virtual void SetInput(const eastl::string& name, Ref<TLAS> accelerationStructure) override;

		virtual void SetInput(const eastl::string& name, Ref<Texture2D> texture) override;
		virtual void SetInput(const eastl::string& name, Ref<TextureCube> textureCube) override;
		virtual void SetInput(const eastl::string& name, Ref<Image2D> image) override;
		virtual void SetInput(const eastl::string& name, Ref<Sampler> sampler, uint32_t index = 0) override;

		virtual Ref<Image2D> GetOutput(uint32_t index) override;
		virtual Ref<Image2D> GetDepthOutput() override;
		virtual bool HasDescriptorSets() const override;
		virtual uint32_t GetFirstSetIndex() const override;

		virtual bool Validate() override;
		virtual void Bake() override;
		virtual bool Baked() const override { return (bool)m_DescriptorSetManager->GetDescriptorPool(); }
		virtual void Prepare() override;

		const std::vector<VkDescriptorSet>& GetDescriptorSets(uint32_t frameIndex) const;

		Ref<PipelineCompute> GetPipeline() const override;

		bool IsInputValid(eastl::string_view name) const;
		const RenderPassInputDeclaration* GetInputDeclaration(eastl::string_view name) const;
	private:
		std::set<uint32_t> HasBufferSets() const;
		bool IsInvalidated(uint32_t frame, uint32_t set, uint32_t binding) const;


	private:
		ComputePassSpecification m_Specification;
		Ref<DescriptorSetManager> m_DescriptorSetManager;
	};

}
