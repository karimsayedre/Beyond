#pragma once

#include <filesystem>
#include <unordered_set>

#include "Beyond/Renderer/Shader.h"
#include "VulkanShaderResource.h"


namespace Beyond {

	struct DescriptorSet
	{
		ShaderResource::ShaderDescriptorSet ShaderDescriptorSet;
		VkDescriptorSetLayout Layout;
	};

	class VulkanShader : public Shader
	{
	public:
		struct ReflectionData
		{
			std::vector<ShaderResource::ShaderDescriptorSet> ShaderDescriptorSets;
			eastl::unordered_map<eastl::string, ShaderResourceDeclaration> Resources;
			eastl::unordered_map<eastl::string, ShaderBuffer> ConstantBuffers;
			std::vector<ShaderResource::PushConstantRange> PushConstantRanges;
		};
	public:
		VulkanShader() = default;
		VulkanShader(const std::string& path, bool forceCompile, bool disableOptimization);
		virtual ~VulkanShader() override;
		void Release();

		void Reload(bool forceCompile = false) override;
		void RT_Reload(bool forceCompile) override;

		virtual uint32_t GetHash() const override;
		RootSignature GetRootSignature() const override { return m_RootSignature; }
		static const std::unordered_map<RootSignature, std::map<uint32_t, DescriptorSet>> & GetBindlessLayouts();

		void SetMacro(const std::string& name, const std::string& value) override {}

		virtual const std::string& GetName() const override { return m_Name; }
		virtual const eastl::unordered_map<eastl::string, ShaderBuffer>& GetShaderBuffers() const override { return m_ReflectionData.ConstantBuffers; }
		virtual const eastl::unordered_map<eastl::string, ShaderResourceDeclaration>& GetResources() const override;
		virtual void AddShaderReloadedCallback(const ShaderReloadedCallback& callback) override;

		const auto& GetSpirvData() const { return m_ShaderData; }

		bool TryReadReflectionData(StreamReader* serializer);

		void SerializeReflectionData(StreamWriter* serializer);

		void SetReflectionData(const ReflectionData& reflectionData);

		// Vulkan-specific
		const std::vector<VkPipelineShaderStageCreateInfo>& GetPipelineShaderStageCreateInfos() const { return m_PipelineShaderStageCreateInfos; }
		const std::vector<VkRayTracingShaderGroupCreateInfoKHR>& GetRaytracingShaderGroupCreateInfos() const { return m_RaytracingShaderGroupCreateInfos; }

		VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set) const { return m_DescriptorSetLayouts.at(set); }
		const std::vector<VkDescriptorSetLayout>& GetAllDescriptorSetLayouts() const;

		ShaderResource::UniformBuffer& GetUniformBuffer(const uint32_t binding = 0, const uint32_t set = 0) { BEY_CORE_ASSERT(m_ReflectionData.ShaderDescriptorSets.at(set).UniformBuffers.size() > binding); return m_ReflectionData.ShaderDescriptorSets.at(set).UniformBuffers.at(binding); }
		uint32_t GetUniformBufferCount(const uint32_t set = 0) const
		{
			if (m_ReflectionData.ShaderDescriptorSets.size() < set)
				return 0;

			return (uint32_t)m_ReflectionData.ShaderDescriptorSets[set].UniformBuffers.size(); //-V557
		}

		const std::vector<ShaderResource::ShaderDescriptorSet>& GetShaderDescriptorSets() const { return m_ReflectionData.ShaderDescriptorSets; }
		bool HasDescriptorSet(uint32_t set) const { return m_TypeCounts.contains(set); }

		const std::vector<ShaderResource::PushConstantRange>& GetPushConstantRanges() const { return m_ReflectionData.PushConstantRanges; }
		ShaderUtils::SourceLang GetLanguage() const { return m_Language; }

		struct ShaderMaterialDescriptorSet
		{
			VkDescriptorPool Pool = nullptr;
			std::vector<VkDescriptorSet> DescriptorSets;
		};

		//ShaderMaterialDescriptorSet AllocateDescriptorSet(uint32_t set = 0);
		ShaderMaterialDescriptorSet CreateDescriptorSets(uint32_t set = 0);
		ShaderMaterialDescriptorSet CreateDescriptorSets(uint32_t set, uint32_t numberOfSets);
		const VkWriteDescriptorSet* GetDescriptorSet(const eastl::string& name, uint32_t set = 0) const;
	private:
		void LoadAndCreateShaders(const std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData);
		void CreateDescriptors();
	private:
		std::vector<VkPipelineShaderStageCreateInfo> m_PipelineShaderStageCreateInfos;
		std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_RaytracingShaderGroupCreateInfos;

		uint32_t m_Hash;
		std::filesystem::path m_AssetPath;
		std::string m_Name;
		bool m_DisableOptimization = false;

		std::map<VkShaderStageFlagBits, std::vector<uint32_t>> m_ShaderData;
		ReflectionData m_ReflectionData;

		std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
		//VkDescriptorPool m_DescriptorPool = nullptr;

		std::unordered_map<uint32_t, std::vector<VkDescriptorPoolSize>> m_TypeCounts;

		bool m_IsRaytracingShader = false;
		std::wstring m_EntryPoint = L"main";
		std::wstring m_TargetProfile;
		std::vector<std::pair<std::wstring, std::wstring>> m_PreDefines;
		bool m_ExternalShader = false;
		ShaderUtils::SourceLang m_Language = ShaderUtils::SourceLang::NONE;
		RootSignature m_RootSignature;

	private:
		friend class ShaderCache;
		friend class ShaderPack;
		friend class VulkanShaderCompiler;
	};

}
