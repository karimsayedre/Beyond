#pragma once

#include "Beyond/Core/Events/MaterialEvent.h"
#include "Beyond/Renderer/Material.h"

#include "Beyond/Platform/Vulkan/VulkanShader.h"
#include "Beyond/Platform/Vulkan/VulkanImage.h"
#include "Beyond/Platform/Vulkan/DescriptorSetManager.h"

namespace Beyond {

	class VulkanMaterial : public Material
	{
	public:
		VulkanMaterial(const Ref<Shader>& shader, std::string name = "");
		VulkanMaterial(Ref<Material> material, const std::string& name = ""); // Copy constructor
		virtual void SetShader(Ref<Shader> shader) override;
		virtual ~VulkanMaterial() override;

		virtual void Invalidate() override;
		virtual void OnShaderReloaded() override;

		virtual void Set(const eastl::string& name, float value) override;
		virtual void Set(const eastl::string& name, int value) override;
		virtual void Set(const eastl::string& name, uint32_t value) override;
		virtual void Set(const eastl::string& name, bool value) override;
		virtual void Set(const eastl::string& name, const glm::ivec2& value) override;
		virtual void Set(const eastl::string& name, const glm::ivec3& value) override;
		virtual void Set(const eastl::string& name, const glm::ivec4& value) override;
		virtual void Set(const eastl::string& name, const glm::vec2& value) override;
		virtual void Set(const eastl::string& name, const glm::vec3& value) override;
		virtual void Set(const eastl::string& name, const glm::vec4& value) override;
		virtual void Set(const eastl::string& name, const glm::mat3& value) override;
		virtual void Set(const eastl::string& name, const glm::mat4& value) override;

		virtual void Set(const eastl::string& name, const Ref<Texture2D>& texture) override;
		virtual void Set(const eastl::string& name, const Ref<Texture2D>& texture, uint32_t arrayIndex) override;
		virtual void Set(const eastl::string& name, const Ref<TextureCube>& texture) override;
		virtual void Set(const eastl::string& name, const Ref<Image2D>& image) override;
		virtual void Set(const eastl::string& name, const Ref<Image2D>& image, uint32_t arrayIndex) override;
		virtual void Set(const eastl::string& name, const Ref<ImageView>& image) override;
		virtual void Set(const eastl::string& name, const Ref<ImageView>& image, uint32_t arrayIndex) override;
		virtual void Set(const eastl::string& name, const Ref<Sampler>& sampler, uint32_t arrayIndex) override;
		virtual void Set(const eastl::string& name, const Ref<Sampler>& sampler) override;

		virtual float& GetFloat(const eastl::string& name) override;
		virtual int32_t& GetInt(const eastl::string& name) override;
		virtual uint32_t& GetUInt(const eastl::string& name) override;
		virtual bool& GetBool(const eastl::string& name) override;
		virtual glm::vec2& GetVector2(const eastl::string& name) override;
		virtual glm::vec3& GetVector3(const eastl::string& name) override;
		virtual glm::vec4& GetVector4(const eastl::string& name) override;
		virtual glm::mat3& GetMatrix3(const eastl::string& name) override;
		virtual glm::mat4& GetMatrix4(const eastl::string& name) override;

		virtual Ref<Texture2D> GetTexture2D(const eastl::string& name) override;
		virtual Ref<Texture2D> GetBindlessTexture2D(const eastl::string& name) const override;
		virtual Ref<TextureCube> GetTextureCube(const eastl::string& name) override;

		virtual Ref<Texture2D> TryGetTexture2D(const eastl::string& name) override;
		virtual Ref<TextureCube> TryGetTextureCube(const eastl::string& name) override;

		template <typename T>
		void Set(const eastl::string& name, const T& value)
		{
			auto decl = FindUniformDeclaration(name);
			BEY_CORE_ASSERT(decl, "Could not find uniform named: {}!", name);
			BEY_CORE_ASSERT(decl->GetSize() == sizeof(T));
			if (!decl)
				return;

			auto& buffer = m_UniformStorageBuffer;
			buffer.Write((byte*)&value, decl->GetSize(), decl->GetOffset());
		}

		template<typename T>
		T& Get(const eastl::string& name)
		{
			auto decl = FindUniformDeclaration(name);
			BEY_CORE_ASSERT(decl, "Could not find uniform with name '{}'", name);
			auto& buffer = m_UniformStorageBuffer;
			return buffer.Read<T>(decl->GetOffset());
		}

		template<typename T>
		Ref<T> GetResource(const eastl::string& name)
		{
			return m_DescriptorSetManager->GetInput<T>(name);
		}

		uint32_t GetResourceBindlessIndex(const eastl::string& name)
		{
			return m_BindlessResources.find(name) != m_BindlessResources.end() ? m_BindlessResources.at(name)->GetBindlessIndex() : 0;
		}

		template<typename T>
		Ref<T> TryGetResource(const eastl::string& name)
		{
			return m_DescriptorSetManager->GetInput<T>(name);
		}

		template<typename T>
		Ref<T> GetBindlessResource(const eastl::string& name) const
		{
			return m_BindlessResources.find(name) != m_BindlessResources.end() ? m_BindlessResources.at(name).As<T>() : nullptr;
		}

		virtual uint32_t GetFlags() const override { return m_MaterialFlags; }
		virtual void SetFlags(uint32_t flags) override { m_MaterialFlags = flags; }
		virtual bool GetFlag(MaterialFlag flag) const override { return (uint32_t)flag & m_MaterialFlags; }
		virtual void SetFlag(MaterialFlag flag, bool value = true) override
		{
			if (value)
			{
				m_MaterialFlags |= (uint32_t)flag;
			}
			else
			{
				m_MaterialFlags &= ~(uint32_t)flag;
			}
			if (FindUniformDeclaration("u_MaterialUniforms.Flags"))
				Set("u_MaterialUniforms.Flags", m_MaterialFlags);
		}

		virtual Ref<Shader> GetShader() override { return m_Shader; }

		virtual const std::string& GetName() const override { return m_Name; }

		Buffer GetUniformStorageBuffer()
		{
			return m_UniformStorageBuffer;
		}

		const Buffer& GetUniformStorageBuffer() const
		{
			return m_UniformStorageBuffer;
		}

		VkDescriptorSet GetDescriptorSet(uint32_t index)
		{
			if (m_DescriptorSetManager->GetFirstSetIndex() == UINT32_MAX)
				return nullptr;

			Prepare();
			return m_DescriptorSetManager->GetDescriptorSets(index)[0];
		}

		void Prepare();
	private:
		void Init(bool setDefaults = true);
		void AllocateStorage();

		void SetVulkanDescriptor(const eastl::string& name, const Ref<Texture2D>& texture);
		void SetVulkanDescriptor(const eastl::string& name, const Ref<Texture2D>& texture, uint32_t arrayIndex);
		void SetVulkanDescriptor(const eastl::string& name, const Ref<TextureCube>& texture);
		void SetVulkanDescriptor(const eastl::string& name, const Ref<Image2D>& image);
		void SetVulkanDescriptor(const eastl::string& name, const Ref<Image2D>& image, uint32_t arrayIndex);
		void SetVulkanDescriptor(const eastl::string& name, const Ref<ImageView>& image);
		void SetVulkanDescriptor(const eastl::string& name, const Ref<ImageView>& image, uint32_t arrayIndex);
		void SetVulkanDescriptor(const eastl::string& name, const Ref<Sampler>& sampler);
		void SetVulkanDescriptor(const eastl::string& name, const Ref<Sampler>& sampler, uint32_t arrayIndex);

		const ShaderUniform* FindUniformDeclaration(const eastl::string& name);
		const ShaderResourceDeclaration* FindResourceDeclaration(const eastl::string& name);

	public:


	private:
		Ref<VulkanShader> m_Shader;
		std::string m_Name;

		enum class PendingDescriptorType
		{
			None = 0, Texture2D, TextureCube, Image2D
		};

		eastl::unordered_map<eastl::string, Ref<RendererResource>> m_BindlessResources;

		// -- NEW v --
		// Per frame in flight
		Ref<DescriptorSetManager> m_DescriptorSetManager;
		std::vector<VkDescriptorSet> m_MaterialDescriptorSets;

		// Map key is binding, vector index is array index (size 1 for non-array)
		std::map<uint32_t, std::vector<Ref<RendererResource>>> m_MaterialDescriptorImages;
		std::map<uint32_t, VkWriteDescriptorSet> m_MaterialWriteDescriptors;
		// -- NEW ^ --

		uint32_t m_MaterialFlags = 0;

		Buffer m_UniformStorageBuffer;
	};

}
