#include "pch.h"
#include "VulkanRenderPass.h"

#include "VulkanShader.h"
#include "VulkanContext.h"
#include "VulkanUniformBuffer.h"
#include "VulkanUniformBufferSet.h"
#include "VulkanStorageBuffer.h"
#include "VulkanStorageBufferSet.h"

#include "VulkanImage.h"
#include "VulkanTexture.h"

namespace Beyond {

	VulkanRenderPass::VulkanRenderPass(const RenderPassSpecification& spec)
		: m_Specification(spec)
	{
		BEY_CORE_VERIFY(spec.Pipeline);

		DescriptorSetManagerSpecification dmSpec;
		dmSpec.DebugName = spec.DebugName;
		dmSpec.Shader = spec.Pipeline->GetSpecification().Shader.As<VulkanShader>();
		dmSpec.StartSet = 1;
		m_DescriptorSetManager = Ref<DescriptorSetManager>::Create(dmSpec);
	}

	VulkanRenderPass::~VulkanRenderPass() = default;

	bool VulkanRenderPass::IsInvalidated(uint32_t frame, uint32_t set, uint32_t binding) const
	{
		return m_DescriptorSetManager->IsInvalidated(frame, set, binding);
	}

	void VulkanRenderPass::SetInput(const eastl::string& name, Ref<UniformBufferSet> uniformBufferSet)
	{
		m_DescriptorSetManager->SetInput(name, uniformBufferSet);
	}

	void VulkanRenderPass::SetInput(const eastl::string& name, Ref<UniformBuffer> uniformBuffer)
	{
		m_DescriptorSetManager->SetInput(name, uniformBuffer);
	}

	void VulkanRenderPass::SetInput(const eastl::string& name, Ref<StorageBufferSet> storageBufferSet)
	{
		m_DescriptorSetManager->SetInput(name, storageBufferSet);
	}

	void VulkanRenderPass::SetInput(const eastl::string& name, Ref<StorageBuffer> storageBuffer)
	{
		m_DescriptorSetManager->SetInput(name, storageBuffer);
	}

	void VulkanRenderPass::SetInput(const eastl::string& name, Ref<Texture2D> texture)
	{
		m_DescriptorSetManager->SetInput(name, texture);
	}

	void VulkanRenderPass::SetInput(const eastl::string& name, Ref<TextureCube> textureCube)
	{
		m_DescriptorSetManager->SetInput(name, textureCube);
	}

	void VulkanRenderPass::SetInput(const eastl::string& name, Ref<Image2D> image)
	{
		m_DescriptorSetManager->SetInput(name, image);
	}

	Ref<Image2D> VulkanRenderPass::GetOutput(uint32_t index)
	{
		Ref<Framebuffer> framebuffer = m_Specification.Pipeline->GetSpecification().TargetFramebuffer;
		if (index > framebuffer->GetColorAttachmentCount() + 1)
			return nullptr; // Invalid index
		if (index < framebuffer->GetColorAttachmentCount())
			return framebuffer->GetImage(index);
		return framebuffer->GetDepthImage();
	}
	Ref<Image2D> VulkanRenderPass::GetDepthOutput()
	{
		Ref<Framebuffer> framebuffer = m_Specification.Pipeline->GetSpecification().TargetFramebuffer;
		if (!framebuffer->HasDepthAttachment())
			return nullptr; // No depth output
		return framebuffer->GetDepthImage();
	}
	uint32_t VulkanRenderPass::GetFirstSetIndex() const
	{
		return m_DescriptorSetManager->GetFirstSetIndex();
	}
	Ref<Framebuffer> VulkanRenderPass::GetTargetFramebuffer() const
	{
		return m_Specification.Pipeline->GetSpecification().TargetFramebuffer;
	}
	Ref<RasterPipeline> VulkanRenderPass::GetPipeline() const
	{
		return m_Specification.Pipeline;
	}
	bool VulkanRenderPass::Validate()
	{
		return m_DescriptorSetManager->Validate();
	}
	void VulkanRenderPass::Bake()
	{
		m_DescriptorSetManager->Bake();
	}
	void VulkanRenderPass::Prepare()
	{
		m_DescriptorSetManager->InvalidateAndUpdate();
	}
	bool VulkanRenderPass::HasDescriptorSets() const
	{
		return m_DescriptorSetManager->HasDescriptorSets();
	}
	const std::vector<VkDescriptorSet>& VulkanRenderPass::GetDescriptorSets(uint32_t frameIndex) const
	{
		BEY_CORE_ASSERT(!m_DescriptorSetManager->m_DescriptorSets.empty());
		if (frameIndex > 0 && m_DescriptorSetManager->m_DescriptorSets.size() == 1)
			return m_DescriptorSetManager->m_DescriptorSets[0]; // Frame index is irrelevant for this type of render pass
		return m_DescriptorSetManager->m_DescriptorSets[frameIndex];
	}
	bool VulkanRenderPass::IsInputValid(eastl::string_view name) const
	{
		eastl::string nameStr(name);
		return m_DescriptorSetManager->InputDeclarations.contains(nameStr);
	}
	const RenderPassInputDeclaration* VulkanRenderPass::GetInputDeclaration(eastl::string_view name) const
	{
		eastl::string nameStr(name);
		if (!m_DescriptorSetManager->InputDeclarations.contains(nameStr))
			return nullptr;
		const RenderPassInputDeclaration& decl = m_DescriptorSetManager->InputDeclarations.at(nameStr);
		return &decl;
	}
}
