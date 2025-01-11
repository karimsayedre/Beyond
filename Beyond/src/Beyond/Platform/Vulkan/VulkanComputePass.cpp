#include "pch.h"
#include "VulkanComputePass.h"

#include "VulkanAPI.h"
#include "VulkanShader.h"
#include "VulkanContext.h"
#include "VulkanUniformBuffer.h"
#include "VulkanUniformBufferSet.h"
#include "VulkanStorageBuffer.h"
#include "VulkanStorageBufferSet.h"

#include "VulkanImage.h"
#include "VulkanTexture.h"

namespace Beyond {

	VulkanComputePass::VulkanComputePass(const ComputePassSpecification& spec)
		: m_Specification(spec)
	{
		BEY_CORE_VERIFY(spec.Pipeline);

		DescriptorSetManagerSpecification dmSpec;
		dmSpec.DebugName = spec.DebugName;
		dmSpec.Shader = spec.Pipeline->GetShader().As<VulkanShader>();
		dmSpec.StartSet = 1;
		m_DescriptorSetManager = Ref<DescriptorSetManager>::Create(dmSpec);
	}

	VulkanComputePass::~VulkanComputePass()
	{
	}

	bool VulkanComputePass::IsInvalidated(uint32_t frame, uint32_t set, uint32_t binding) const
	{
		return m_DescriptorSetManager->IsInvalidated(frame, set, binding);
	}

	void VulkanComputePass::SetInput(const eastl::string& name, Ref<UniformBufferSet> uniformBufferSet)
	{
		m_DescriptorSetManager->SetInput(name, uniformBufferSet);
	}

	void VulkanComputePass::SetInput(const eastl::string& name, Ref<UniformBuffer> uniformBuffer)
	{
		m_DescriptorSetManager->SetInput(name, uniformBuffer);
	}

	void VulkanComputePass::SetInput(const eastl::string& name, Ref<StorageBufferSet> storageBufferSet)
	{
		m_DescriptorSetManager->SetInput(name, storageBufferSet);
	}

	void VulkanComputePass::SetInput(const eastl::string& name, Ref<StorageBuffer> storageBuffer)
	{
		m_DescriptorSetManager->SetInput(name, storageBuffer);
	}

	void VulkanComputePass::SetInput(const eastl::string& name, Ref<AccelerationStructureSet> accelerationStructureSet)
	{
		m_DescriptorSetManager->SetInput(name, accelerationStructureSet);
	}

	void VulkanComputePass::SetInput(const eastl::string& name, Ref<TLAS> accelerationStructure)
	{
		m_DescriptorSetManager->SetInput(name, accelerationStructure);
	}

	void VulkanComputePass::SetInput(const eastl::string& name, Ref<Texture2D> texture)
	{
		m_DescriptorSetManager->SetInput(name, texture);
	}

	void VulkanComputePass::SetInput(const eastl::string& name, Ref<TextureCube> textureCube)
	{
		m_DescriptorSetManager->SetInput(name, textureCube);
	}

	void VulkanComputePass::SetInput(const eastl::string& name, Ref<Image2D> image)
	{
		m_DescriptorSetManager->SetInput(name, image);
	}

	void VulkanComputePass::SetInput(const eastl::string& name, Ref<Sampler> sampler, uint32_t index)
	{
		m_DescriptorSetManager->SetInput(name, sampler, index);
	}

	Ref<Image2D> VulkanComputePass::GetOutput(uint32_t index)
	{
		BEY_CORE_VERIFY(false, "Not implemented");
		return nullptr;
	}

	Ref<Image2D> VulkanComputePass::GetDepthOutput()
	{
		BEY_CORE_VERIFY(false, "Not implemented");
		return nullptr;
	}

	bool VulkanComputePass::HasDescriptorSets() const
	{
		return m_DescriptorSetManager->HasDescriptorSets();
	}

	uint32_t VulkanComputePass::GetFirstSetIndex() const
	{
		return m_DescriptorSetManager->GetFirstSetIndex();
	}

	bool VulkanComputePass::Validate()
	{
		return m_DescriptorSetManager->Validate();
	}

	void VulkanComputePass::Bake()
	{
		m_DescriptorSetManager->Bake();
	}

	void VulkanComputePass::Prepare()
	{
		m_DescriptorSetManager->InvalidateAndUpdate();
	}

	const std::vector<VkDescriptorSet>& VulkanComputePass::GetDescriptorSets(uint32_t frameIndex) const
	{
		return m_DescriptorSetManager->GetDescriptorSets(frameIndex);
	}

	Ref<PipelineCompute> VulkanComputePass::GetPipeline() const
	{
		return m_Specification.Pipeline;
	}

	bool VulkanComputePass::IsInputValid(eastl::string_view name) const
	{
		eastl::string nameStr(name);
		return m_DescriptorSetManager->InputDeclarations.contains(nameStr);
	}

	const RenderPassInputDeclaration* VulkanComputePass::GetInputDeclaration(eastl::string_view name) const
	{
		eastl::string nameStr(name);
		return m_DescriptorSetManager->GetInputDeclaration(nameStr);
	}

	}
