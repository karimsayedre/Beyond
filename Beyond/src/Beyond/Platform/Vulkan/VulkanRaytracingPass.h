#pragma once
#include "DescriptorSetManager.h"
#include "Beyond/Renderer/RaytracingPass.h"
namespace Beyond {
	class VulkanRaytracingPass : public RaytracingPass
	{
	public:
		explicit VulkanRaytracingPass(const RaytracingPassSpecification& spec);
		virtual RaytracingPassSpecification& GetSpecification() override { return m_Specification; }
		virtual const RaytracingPassSpecification& GetSpecification() const override { return m_Specification; }

		virtual Ref<Shader> GetShader() const override { return m_Specification.Pipeline->GetShader(); }

		virtual void SetInput(const eastl::string& name, Ref<UniformBufferSet> uniformBufferSet) override;
		virtual void SetInput(const eastl::string& name, Ref<UniformBuffer> uniformBuffer) override;

		virtual void SetInput(const eastl::string& name, Ref<StorageBufferSet> storageBufferSet) override;
		virtual void SetInput(const eastl::string& name, Ref<StorageBuffer> storageBuffer) override;

		virtual void SetInput(const eastl::string& name, Ref<AccelerationStructureSet> accelerationStructureSet) override;
		virtual void SetInput(const eastl::string& name, Ref<TLAS> accelerationStructure) override;

		virtual void SetInput(const eastl::string&name, Ref<Texture2D> texture) override;
		virtual void SetInput(const eastl::string&name, Ref<TextureCube> textureCube) override;
		virtual void SetInput(const eastl::string&name, Ref<Image2D> image) override;
		virtual void SetInput(const eastl::string&name, Ref<Sampler> sampler) override;
		virtual void SetInput(const eastl::string&name, Ref<Sampler> sampler, uint32_t index) override;

		virtual Ref<Image2D> GetOutput(uint32_t index) override;
		virtual Ref<Image2D> GetDepthOutput() override;
		virtual bool HasDescriptorSets() const override;
		virtual uint32_t GetFirstSetIndex() const override;

		virtual bool Validate() override;
		virtual void Bake() override;
		virtual bool Baked() const override { return (bool)m_DescriptorSetManager->GetDescriptorPool(); }
		virtual void Prepare() override;

		const std::vector<VkDescriptorSet>& GetDescriptorSets(uint32_t frameIndex) const;

		Ref<RaytracingPipeline> GetPipeline() const override;
		Ref<DescriptorSetManager> GetDescriptorManager() const { return m_DescriptorSetManager; }

		bool IsInputValid(eastl::string_view name) const;
		const RenderPassInputDeclaration* GetInputDeclaration(eastl::string_view name) const;
	private:
		RaytracingPassSpecification m_Specification;
		Ref<DescriptorSetManager> m_DescriptorSetManager;
	};

}
