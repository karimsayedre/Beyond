#include "pch.h"
#include "VulkanMaterial.h"

#include <ranges>

#include "Beyond/Renderer/Renderer.h"

#include "Beyond/Platform/Vulkan/VulkanContext.h"
#include "Beyond/Platform/Vulkan/VulkanRenderer.h"
#include "Beyond/Platform/Vulkan/VulkanTexture.h"
#include "Beyond/Platform/Vulkan/VulkanImage.h"
#include "Beyond/Platform/Vulkan/VulkanAPI.h"


namespace Beyond {

	VulkanMaterial::VulkanMaterial(const Ref<Shader>& shader, std::string name)
		: m_Shader(shader.As<VulkanShader>()), m_Name(std::move(name))
	{
		Init();
		Renderer::RegisterShaderDependency(shader, this);
	}

	// Copy constructor
	VulkanMaterial::VulkanMaterial(Ref<Material> material, const std::string& name)
		: m_Shader(material->GetShader().As<VulkanShader>()), m_Name(name)
	{
		Ref<VulkanMaterial> vulkanMaterial = material.As<VulkanMaterial>();
		if (name.empty())
			m_Name = vulkanMaterial->m_Name;

		Init();
		Renderer::RegisterShaderDependency(m_Shader, this);
		m_MaterialFlags = vulkanMaterial->m_MaterialFlags;
		m_BindlessResources = vulkanMaterial->m_BindlessResources;
		for (const auto& [name, resource] : m_BindlessResources)
			VulkanMaterial::Set(name, resource.As<Texture>()->GetFlaggedBindlessIndex());

		m_UniformStorageBuffer = Buffer::Copy(vulkanMaterial->m_UniformStorageBuffer);
		m_DescriptorSetManager = Ref<DescriptorSetManager>::Create(*vulkanMaterial->m_DescriptorSetManager);
	}

	void VulkanMaterial::SetShader(Ref<Shader> shader)
	{
		Renderer::UnRegisterShaderDependency(m_Shader, this);
		m_Shader = shader.As<VulkanShader>();
		Renderer::RegisterShaderDependency(m_Shader, this);
		Init(false);

		//m_UniformStorageBuffer = Buffer::Copy(vulkanMaterial->m_UniformStorageBuffer);
		//m_DescriptorSetManager = Ref<DescriptorSetManager>::Create();
		Invalidate();
	}

	VulkanMaterial::~VulkanMaterial()
	{
		m_UniformStorageBuffer.Release();
	}

	void VulkanMaterial::Init(const bool setDefaults)
	{
		if (setDefaults)
			AllocateStorage();

		//m_MaterialFlags |= (uint32_t)MaterialFlag::DepthTest;
		//m_MaterialFlags |= (uint32_t)MaterialFlag::Blend;

		DescriptorSetManagerSpecification dmSpec;
		dmSpec.DebugName = m_Name.empty() ? fmt::format("{} (Material)", m_Shader->GetName()).c_str() : m_Name.c_str();
		dmSpec.Shader = m_Shader.As<VulkanShader>();
		dmSpec.StartSet = 0;
		dmSpec.EndSet = 0;
		dmSpec.DefaultResources = true;
		m_DescriptorSetManager = Ref<DescriptorSetManager>::Create(dmSpec);

		if (setDefaults)
		{
			if (FindUniformDeclaration("u_MaterialUniforms.AlbedoTexIndex"))
				Set("u_MaterialUniforms.AlbedoTexIndex", Renderer::GetWhiteTexture());
			if (FindUniformDeclaration("u_MaterialUniforms.NormalTexIndex"))
				Set("u_MaterialUniforms.NormalTexIndex", Renderer::GetWhiteTexture());
			if (FindUniformDeclaration("u_MaterialUniforms.RoughnessTexIndex"))
				Set("u_MaterialUniforms.RoughnessTexIndex", Renderer::GetWhiteTexture());
			if (FindUniformDeclaration("u_MaterialUniforms.MetalnessTexIndex"))
				Set("u_MaterialUniforms.MetalnessTexIndex", Renderer::GetWhiteTexture());

			for (const auto& [name, decl] : m_DescriptorSetManager->InputDeclarations)
			{
				switch (decl.Type)
				{
					case RenderPassInputType::ImageSampler1D:
					case RenderPassInputType::ImageSampler2D:
					{
						for (uint32_t i = 0; i < decl.Count; i++)
							m_DescriptorSetManager->SetInput(name, Renderer::GetWhiteTexture(), i);
						BEY_CORE_WARN_TAG("Renderer", "VulkanMaterial - setting {} to white 2D texture", name);
						break;
					}
					case RenderPassInputType::ImageSampler3D:
					{
						m_DescriptorSetManager->SetInput(name, Renderer::GetBlackCubeTexture());
						BEY_CORE_WARN_TAG("Renderer", "VulkanMaterial - setting {} to black cube texture", name);
						break;
					}
				}
			}
		}

		BEY_CORE_VERIFY(m_DescriptorSetManager->Validate());
		m_DescriptorSetManager->Bake();
	}

	void VulkanMaterial::Invalidate()
	{
		// Allocate descriptor set 0 based on shader layout
		if (m_Shader->HasDescriptorSet(0))
		{
			const VkDescriptorSetLayout dsl = m_Shader->GetDescriptorSetLayout(0);
			VkDescriptorSetAllocateInfo descriptorSetAllocInfo = Vulkan::DescriptorSetAllocInfo(&dsl);
			const uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;
			m_MaterialDescriptorSets.resize(framesInFlight);
			for (uint32_t i = 0; i < framesInFlight; i++)
				m_MaterialDescriptorSets[i] = VulkanRenderer::AllocateMaterialDescriptorSet(descriptorSetAllocInfo, 0, i, m_Shader->GetName());

			// Sort into map sorted by binding
			const auto& shaderDescriptorSets = m_Shader->GetShaderDescriptorSets();
			std::map<uint32_t, VkWriteDescriptorSet> writeDescriptors;
			for (const auto& [name, writeDescriptor] : shaderDescriptorSets[0].WriteDescriptorSets)
			{
				if (
					writeDescriptor.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
					writeDescriptor.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
					writeDescriptor.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
				{
					writeDescriptors[writeDescriptor.dstBinding] = writeDescriptor;
				}
			}

			// Ordered map
			for (const auto& [binding, writeDescriptor] : writeDescriptors)
			{
				m_MaterialWriteDescriptors[binding] = writeDescriptor;
				m_MaterialDescriptorImages[binding] = std::vector<Ref<RendererResource>>(writeDescriptor.descriptorCount);

				if (writeDescriptor.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
				{
					// Set default image infos
					for (size_t i = 0; i < writeDescriptor.descriptorCount; i++)
					{
						auto found = eastl::find_if(m_Shader->GetResources().begin(), m_Shader->GetResources().end(), [binding](const eastl::pair<eastl::string, ShaderResourceDeclaration>& resource)
						{
							return resource.second.GetRegister() == binding && resource.second.GetSet() == 0;
						});

						BEY_CORE_ASSERT(found != m_Shader->GetResources().end());

						const RenderPassInputType type = found.get_node()->mValue.second.GetType();
						switch (type)
						{
							case RenderPassInputType::ImageSampler1D:
							case RenderPassInputType::ImageSampler2D:
							case RenderPassInputType::StorageImage1D:
							case RenderPassInputType::StorageImage2D:
							{
								m_MaterialDescriptorImages[binding][i] = Renderer::GetWhiteTexture();
								break;
							}
							case RenderPassInputType::ImageSampler3D:
							case RenderPassInputType::StorageImage3D:
							{
								m_MaterialDescriptorImages[binding][i] = Renderer::GetBlackCubeTexture();
								break;
							}
							default:
							{
								BEY_CORE_ASSERT(false, "Unknown resource type!");
								break;
							}
						}
					}
				}
			}
		}
		else
		{
			BEY_CORE_WARN_TAG("Renderer", "[Material] - shader {} has no Set 0!", m_Shader->GetName());
		}
	}

	void VulkanMaterial::AllocateStorage()
	{
		const auto& shaderBuffers = m_Shader->GetShaderBuffers();

		if (shaderBuffers.size() > 0 || shaderBuffers.size() != m_UniformStorageBuffer.GetSize())
		{
			uint32_t size = 0;
			for (const auto& [name, shaderBuffer] : shaderBuffers)
				size += shaderBuffer.Size;

			m_UniformStorageBuffer.Allocate(size);
			m_UniformStorageBuffer.ZeroInitialize();
		}
	}

	void VulkanMaterial::OnShaderReloaded()
	{
		//Init();
	}

	const ShaderUniform* VulkanMaterial::FindUniformDeclaration(const eastl::string& name)
	{
		const auto& shaderBuffers = m_Shader->GetShaderBuffers();

		//BEY_CORE_ASSERT(shaderBuffers.size() <= 1, "We currently only support ONE material buffer!");

		//if (!shaderBuffers.empty())
		for (const auto& namedBuffer : shaderBuffers)
		{
			const ShaderBuffer& buffer = namedBuffer.second;
			if (buffer.Uniforms.find(name) == buffer.Uniforms.end())
				return nullptr;

			return &buffer.Uniforms.at(name);
		}
		return nullptr;
	}

	const ShaderResourceDeclaration* VulkanMaterial::FindResourceDeclaration(const eastl::string& name)
	{
		auto& resources = m_Shader->GetResources();
		if (resources.find(name) != resources.end())
			return &resources.at(name);

		return nullptr;
	}

	void VulkanMaterial::SetVulkanDescriptor(const eastl::string& name, const Ref<Texture2D>& texture)
	{
		m_DescriptorSetManager->SetInput(name, texture);
	}

	void VulkanMaterial::SetVulkanDescriptor(const eastl::string& name, const Ref<Texture2D>& texture, uint32_t arrayIndex)
	{
		m_DescriptorSetManager->SetInput(name, texture, arrayIndex);
	}

	void VulkanMaterial::SetVulkanDescriptor(const eastl::string& name, const Ref<TextureCube>& texture)
	{
		m_DescriptorSetManager->SetInput(name, texture);
	}

	void VulkanMaterial::SetVulkanDescriptor(const eastl::string& name, const Ref<Image2D>& image)
	{
		BEY_CORE_VERIFY(image);
		m_DescriptorSetManager->SetInput(name, image);
	}

	void VulkanMaterial::SetVulkanDescriptor(const eastl::string& name, const Ref<Image2D>& image, uint32_t arrayIndex)
	{
		BEY_CORE_VERIFY(image);
		m_DescriptorSetManager->SetInput(name, image, arrayIndex);
	}

	void VulkanMaterial::SetVulkanDescriptor(const eastl::string& name, const Ref<ImageView>& image)
	{
		BEY_CORE_VERIFY(image);
		m_DescriptorSetManager->SetInput(name, image);
	}

	void VulkanMaterial::SetVulkanDescriptor(const eastl::string& name, const Ref<Sampler>& sampler)
	{
		BEY_CORE_VERIFY(sampler);
		m_DescriptorSetManager->SetInput(name, sampler);
	}

	void VulkanMaterial::SetVulkanDescriptor(const eastl::string& name, const Ref<Sampler>& sampler, uint32_t arrayIndex)
	{
		BEY_CORE_VERIFY(sampler);
		m_DescriptorSetManager->SetInput(name, sampler, arrayIndex);
	}

	void VulkanMaterial::SetVulkanDescriptor(const eastl::string& name, const Ref<ImageView>& image, uint32_t arrayIndex)
	{
		BEY_CORE_VERIFY(image);
		m_DescriptorSetManager->SetInput(name, image, arrayIndex);
	}

	void VulkanMaterial::Set(const eastl::string& name, float value)
	{
		Set<float>(name, value);
	}

	void VulkanMaterial::Set(const eastl::string& name, int value)
	{
		Set<int>(name, value);
	}

	void VulkanMaterial::Set(const eastl::string& name, uint32_t value)
	{
		Set<uint32_t>(name, value);
	}

	void VulkanMaterial::Set(const eastl::string& name, bool value)
	{
		// Bools are 4-byte ints
		Set<int>(name, (int)value);
	}

	void VulkanMaterial::Set(const eastl::string& name, const glm::ivec2& value)
	{
		Set<glm::ivec2>(name, value);
	}

	void VulkanMaterial::Set(const eastl::string& name, const glm::ivec3& value)
	{
		Set<glm::ivec3>(name, value);
	}

	void VulkanMaterial::Set(const eastl::string& name, const glm::ivec4& value)
	{
		Set<glm::ivec4>(name, value);
	}

	void VulkanMaterial::Set(const eastl::string& name, const glm::vec2& value)
	{
		Set<glm::vec2>(name, value);
	}

	void VulkanMaterial::Set(const eastl::string& name, const glm::vec3& value)
	{
		Set<glm::vec3>(name, value);
	}

	void VulkanMaterial::Set(const eastl::string& name, const glm::vec4& value)
	{
		Set<glm::vec4>(name, value);
	}

	void VulkanMaterial::Set(const eastl::string& name, const glm::mat3& value)
	{
		Set<glm::mat3>(name, value);
	}

	void VulkanMaterial::Set(const eastl::string& name, const glm::mat4& value)
	{
		Set<glm::mat4>(name, value);
	}

	void VulkanMaterial::Set(const eastl::string& name, const Ref<Texture2D>& texture)
	{
		if (name.find("u_MaterialUniforms.") != eastl::string::npos)
		{
			m_BindlessResources[name] = texture;
			Set(name, texture.As<Texture2D>()->GetFlaggedBindlessIndex());
		}
		else
			SetVulkanDescriptor(name, texture);
	}

	void VulkanMaterial::Set(const eastl::string& name, const Ref<Texture2D>& texture, uint32_t arrayIndex)
	{
		if (name.find("u_MaterialUniforms.") != eastl::string::npos)
		{
			m_BindlessResources[name] = texture;
			Set(name, texture.As<Texture2D>()->GetFlaggedBindlessIndex());
		}
		else
			SetVulkanDescriptor(name, texture, arrayIndex);
	}

	void VulkanMaterial::Set(const eastl::string& name, const Ref<TextureCube>& texture)
	{
		if (name.find("u_MaterialUniforms.") != eastl::string::npos)
		{
			m_BindlessResources[name] = texture;
			Set(name, texture.As<Texture2D>()->GetFlaggedBindlessIndex());
		}
		else
			SetVulkanDescriptor(name, texture);
	}

	void VulkanMaterial::Set(const eastl::string& name, const Ref<Image2D>& image)
	{
		if (name.find("u_MaterialUniforms.") != eastl::string::npos)
		{
			m_BindlessResources[name] = image;
			Set(name, image.As<Texture2D>()->GetFlaggedBindlessIndex());
		}
		else
			SetVulkanDescriptor(name, image);
	}

	void VulkanMaterial::Set(const eastl::string& name, const Ref<Image2D>& image, uint32_t arrayIndex)
	{
		SetVulkanDescriptor(name, image, arrayIndex);
	}

	void VulkanMaterial::Set(const eastl::string& name, const Ref<ImageView>& image)
	{
		if (name.find("u_MaterialUniforms.") != eastl::string::npos)
		{
			m_BindlessResources[name] = image;
			Set(name, image.As<Texture2D>()->GetFlaggedBindlessIndex());
		}
		else
			SetVulkanDescriptor(name, image);
	}

	void VulkanMaterial::Set(const eastl::string& name, const Ref<ImageView>& image, uint32_t arrayIndex)
	{
		SetVulkanDescriptor(name, image, arrayIndex);
	}

	void VulkanMaterial::Set(const eastl::string& name, const Ref<Sampler>& sampler, uint32_t arrayIndex)
	{
		SetVulkanDescriptor(name, sampler, arrayIndex);
	}

	void VulkanMaterial::Set(const eastl::string& name, const Ref<Sampler>& sampler)
	{
		SetVulkanDescriptor(name, sampler);

	}

	float& VulkanMaterial::GetFloat(const eastl::string& name)
	{
		return Get<float>(name);
	}

	int32_t& VulkanMaterial::GetInt(const eastl::string& name)
	{
		return Get<int32_t>(name);
	}

	uint32_t& VulkanMaterial::GetUInt(const eastl::string& name)
	{
		return Get<uint32_t>(name);
	}

	bool& VulkanMaterial::GetBool(const eastl::string& name)
	{
		return Get<bool>(name);
	}

	glm::vec2& VulkanMaterial::GetVector2(const eastl::string& name)
	{
		return Get<glm::vec2>(name);
	}

	glm::vec3& VulkanMaterial::GetVector3(const eastl::string& name)
	{
		return Get<glm::vec3>(name);
	}

	glm::vec4& VulkanMaterial::GetVector4(const eastl::string& name)
	{
		return Get<glm::vec4>(name);
	}

	glm::mat3& VulkanMaterial::GetMatrix3(const eastl::string& name)
	{
		return Get<glm::mat3>(name);
	}

	glm::mat4& VulkanMaterial::GetMatrix4(const eastl::string& name)
	{
		return Get<glm::mat4>(name);
	}

	Ref<Texture2D> VulkanMaterial::GetTexture2D(const eastl::string& name)
	{
		return GetResource<Texture2D>(name);
	}

	Ref<Texture2D> VulkanMaterial::GetBindlessTexture2D(const eastl::string& name) const
	{
		return GetBindlessResource<Texture2D>(name);
	}

	Ref<TextureCube> VulkanMaterial::TryGetTextureCube(const eastl::string& name)
	{
		return TryGetResource<TextureCube>(name);
	}

	Ref<Texture2D> VulkanMaterial::TryGetTexture2D(const eastl::string& name)
	{
		return TryGetResource<Texture2D>(name);
	}

	Ref<TextureCube> VulkanMaterial::GetTextureCube(const eastl::string& name)
	{
		return GetResource<TextureCube>(name);
	}

	void VulkanMaterial::Prepare()
	{
		m_DescriptorSetManager->InvalidateAndUpdate();
	}

}
