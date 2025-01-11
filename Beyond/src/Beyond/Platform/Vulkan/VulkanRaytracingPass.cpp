#include "pch.h"
#include "VulkanRaytracingPass.h"

namespace Beyond {



	VulkanRaytracingPass::VulkanRaytracingPass(const RaytracingPassSpecification& spec)
		: m_Specification(spec)
	{
		BEY_CORE_VERIFY(spec.Pipeline);

		DescriptorSetManagerSpecification dmSpec;
		dmSpec.DebugName = spec.DebugName;
		dmSpec.Shader = spec.Pipeline->GetShader().As<VulkanShader>();
		dmSpec.StartSet = 1;
		m_DescriptorSetManager = Ref<DescriptorSetManager>::Create(dmSpec);
	}

	void VulkanRaytracingPass::SetInput(const eastl::string& name, Ref<UniformBufferSet> uniformBufferSet)
	{
		m_DescriptorSetManager->SetInput(name, uniformBufferSet);
	}

	void VulkanRaytracingPass::SetInput(const eastl::string& name, Ref<UniformBuffer> uniformBuffer)
	{
		m_DescriptorSetManager->SetInput(name, uniformBuffer);
	}

	void VulkanRaytracingPass::SetInput(const eastl::string& name, Ref<StorageBufferSet> storageBufferSet)
	{
		m_DescriptorSetManager->SetInput(name, storageBufferSet);
	}

	void VulkanRaytracingPass::SetInput(const eastl::string& name, Ref<StorageBuffer> storageBuffer)
	{
		m_DescriptorSetManager->SetInput(name, storageBuffer);
	}

	void VulkanRaytracingPass::SetInput(const eastl::string& name, Ref<AccelerationStructureSet> accelerationStructureSet)
	{
		m_DescriptorSetManager->SetInput(name, accelerationStructureSet);
	}

	void VulkanRaytracingPass::SetInput(const eastl::string& name, Ref<TLAS> accelerationStructure)
	{
		m_DescriptorSetManager->SetInput(name, accelerationStructure);
	}

	void VulkanRaytracingPass::SetInput(const eastl::string& name, Ref<Texture2D> texture)
	{
		m_DescriptorSetManager->SetInput(name, texture);
	}

	void VulkanRaytracingPass::SetInput(const eastl::string& name, Ref<TextureCube> textureCube)
	{
		m_DescriptorSetManager->SetInput(name, textureCube);
	}

	void VulkanRaytracingPass::SetInput(const eastl::string& name, Ref<Image2D> image)
	{
		m_DescriptorSetManager->SetInput(name, image);
	}

	void VulkanRaytracingPass::SetInput(const eastl::string& name, Ref<Sampler> sampler, uint32_t index)
	{
		m_DescriptorSetManager->SetInput(name, sampler, index);
	}

	void VulkanRaytracingPass::SetInput(const eastl::string& name, Ref<Sampler> sampler)
	{
		m_DescriptorSetManager->SetInput(name, sampler);
	}

	Ref<Image2D> VulkanRaytracingPass::GetOutput(uint32_t index)
	{
		BEY_CORE_VERIFY(false, "Not implemented");
		return nullptr;
	}

	Ref<Image2D> VulkanRaytracingPass::GetDepthOutput()
	{
		BEY_CORE_VERIFY(false, "Not implemented");
		return nullptr;
	}


	bool VulkanRaytracingPass::HasDescriptorSets() const
	{
		return m_DescriptorSetManager->HasDescriptorSets();
	}

	uint32_t VulkanRaytracingPass::GetFirstSetIndex() const
	{
		return m_DescriptorSetManager->GetFirstSetIndex();
	}

	bool VulkanRaytracingPass::Validate()
	{
		return m_DescriptorSetManager->Validate();
	}

	void VulkanRaytracingPass::Bake()
	{
		m_DescriptorSetManager->Bake();
	}

	void VulkanRaytracingPass::Prepare()
	{
		m_DescriptorSetManager->InvalidateAndUpdate();
	}

	const std::vector<VkDescriptorSet>& VulkanRaytracingPass::GetDescriptorSets(uint32_t frameIndex) const
	{
		return m_DescriptorSetManager->GetDescriptorSets(frameIndex);
	}

	Ref<RaytracingPipeline> VulkanRaytracingPass::GetPipeline() const
	{
		return m_Specification.Pipeline;
	}

	bool VulkanRaytracingPass::IsInputValid(eastl::string_view name) const
	{
		eastl::string nameStr(name);
		return m_DescriptorSetManager->InputDeclarations.contains(nameStr);
	}

	const RenderPassInputDeclaration* VulkanRaytracingPass::GetInputDeclaration(eastl::string_view name) const
	{
		eastl::string nameStr(name);
		return m_DescriptorSetManager->GetInputDeclaration(nameStr);
	}

}
