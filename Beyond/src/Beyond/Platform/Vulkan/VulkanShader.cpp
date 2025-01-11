#include "pch.h"
#include "VulkanShader.h"

#if BEY_HAS_SHADER_COMPILER
#include "ShaderCompiler/VulkanShaderCompiler.h"
#endif

#include <filesystem>

#include "Beyond/Renderer/Renderer.h"
#include "Beyond/Utilities/StringUtils.h"

#include "Beyond/Platform/Vulkan/VulkanContext.h"

#include "Beyond/Core/Hash.h"
#include "Beyond/Platform/Vulkan/VulkanRenderer.h"
#include "VulkanShaderUtils.h"

#include "Beyond/ImGui/ImGui.h"

namespace Beyond {


	// RootSignature -> set -> layout
	static std::unordered_map<RootSignature, std::map<uint32_t, DescriptorSet>> s_BindlessSetLayouts;

	bool IsRaytracingShader(const std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderSources)
	{
		for (const auto& stage : shaderSources | std::views::keys)
		{
			if (stage != VK_SHADER_STAGE_RAYGEN_BIT_KHR && stage != VK_SHADER_STAGE_ANY_HIT_BIT_KHR && stage != VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR &&
				stage != VK_SHADER_STAGE_MISS_BIT_KHR && stage != VK_SHADER_STAGE_INTERSECTION_BIT_KHR && stage != VK_SHADER_STAGE_CALLABLE_BIT_KHR)
				return false;
		}
		return true;
	}

	VulkanShader::VulkanShader(const std::string& path, bool forceCompile, bool disableOptimization)
		: m_Hash(Hash::GenerateFNVHash(path)), m_AssetPath(path), m_DisableOptimization(disableOptimization)
	{
		// TODO: This should be more "general"
		size_t found = path.find_last_of("/\\");
		m_Name = found != eastl::string::npos ? path.substr(found + 1) : path;
		found = m_Name.find_last_of('.');
		m_Name = found != eastl::string::npos ? m_Name.substr(0, found) : m_Name;

		VulkanShader::Reload(forceCompile);
	}

	void VulkanShader::Release()
	{
		Renderer::SubmitResourceFree([pipelineCIs = m_PipelineShaderStageCreateInfos, layouts = m_DescriptorSetLayouts, rootSignature = m_RootSignature]()
		{
			const auto vulkanDevice = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			for (auto& descriptorSetLayout : layouts)
				if (s_BindlessSetLayouts.contains(rootSignature))
					vkDestroyDescriptorSetLayout(vulkanDevice, descriptorSetLayout, nullptr);

			for (const auto& ci : pipelineCIs)
				if (ci.module)
					vkDestroyShaderModule(vulkanDevice, ci.module, nullptr);
		});

		// Only clear layouts when shader is already used before
		// TODO: Make sure we're not leaking any...
		if (!m_ShaderData.empty() && s_BindlessSetLayouts.contains(m_RootSignature))
			s_BindlessSetLayouts.at(m_RootSignature).clear();
		m_PipelineShaderStageCreateInfos.clear();
		m_DescriptorSetLayouts.clear();
		m_TypeCounts.clear();
		m_ReflectionData = {};
		m_ShaderData.clear();
		m_RaytracingShaderGroupCreateInfos.clear();
	}

	// Is HLSL in set -> layout
	const std::unordered_map<RootSignature, std::map<uint32_t, DescriptorSet>>& VulkanShader::GetBindlessLayouts()
	{
		return s_BindlessSetLayouts;
	}

	VulkanShader::~VulkanShader()
	{
		Release();
	}

	void VulkanShader::RT_Reload(const bool forceCompile)
	{
#if BEY_HAS_SHADER_COMPILER 
		if (!VulkanShaderCompiler::TryRecompile(this))
		{
			BEY_CORE_FATAL("Failed to recompile shader!");
		}
#endif
	}

	void VulkanShader::Reload(bool forceCompile)
	{
		Renderer::Submit([instance = Ref(this), forceCompile]() mutable
		{
			//instance->Release();
			instance->RT_Reload(forceCompile);
		});
	}

	uint32_t VulkanShader::GetHash() const
	{
		return m_Hash;
	}

	void VulkanShader::LoadAndCreateShaders(const std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData)
	{
		m_ShaderData = shaderData;
		m_IsRaytracingShader = IsRaytracingShader(shaderData);

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		m_PipelineShaderStageCreateInfos.clear();
		for (auto [stage, data] : shaderData)
		{
			BEY_CORE_ASSERT(data.size());
			VkShaderModuleCreateInfo moduleCreateInfo{};

			moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			moduleCreateInfo.codeSize = data.size() * sizeof(uint32_t);
			moduleCreateInfo.pCode = data.data();

			VkShaderModule shaderModule;
			VK_CHECK_RESULT(vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_SHADER_MODULE, fmt::eastl_format("{}:{}", m_Name, ShaderUtils::ShaderStageToString(stage)), shaderModule);

			VkPipelineShaderStageCreateInfo& shaderStage = m_PipelineShaderStageCreateInfos.emplace_back();
			shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStage.stage = stage;
			shaderStage.module = shaderModule;
			shaderStage.pName = "main";
		}

		if (m_IsRaytracingShader)
		{
			// Shader groups
			VkRayTracingShaderGroupCreateInfoKHR group{};
			group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			group.intersectionShader = VK_SHADER_UNUSED_KHR;
			group.closestHitShader = VK_SHADER_UNUSED_KHR;
			group.anyHitShader = VK_SHADER_UNUSED_KHR;
			group.generalShader = VK_SHADER_UNUSED_KHR;

			VkRayTracingShaderGroupCreateInfoKHR hitGroup{};
			hitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			hitGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
			hitGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
			hitGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
			hitGroup.generalShader = VK_SHADER_UNUSED_KHR;

			for (uint32_t i = 0; const auto & stage : m_ShaderData | std::views::keys)
			{
				switch (stage)
				{
					case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
					case VK_SHADER_STAGE_MISS_BIT_KHR:
					case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
						group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
						group.generalShader = i;
						m_RaytracingShaderGroupCreateInfos.push_back(group);
						break;
					case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
						hitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
						hitGroup.closestHitShader = i;
						break;
					case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
						hitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
						hitGroup.anyHitShader = i;
						break;
					case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
						hitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
						hitGroup.intersectionShader = i;
						break;
					default: BEY_CORE_ASSERT(false);
				}
				i++;
			}
			m_RaytracingShaderGroupCreateInfos.push_back(hitGroup); // TODO: Currently only one hit group
		}
	}

	void VulkanShader::CreateDescriptors()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		//////////////////////////////////////////////////////////////////////
		// Descriptor Pool
		//////////////////////////////////////////////////////////////////////

		m_TypeCounts.clear();
		for (uint32_t set = 0; set < m_ReflectionData.ShaderDescriptorSets.size(); set++)
		{
			auto& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[set];

			if (shaderDescriptorSet.UniformBuffers.size())
			{
				VkDescriptorPoolSize& typeCount = m_TypeCounts[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				typeCount.descriptorCount = (uint32_t)(shaderDescriptorSet.UniformBuffers.size());
			}
			if (shaderDescriptorSet.StorageBuffers.size())
			{
				VkDescriptorPoolSize& typeCount = m_TypeCounts[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				typeCount.descriptorCount = (uint32_t)(shaderDescriptorSet.StorageBuffers.size());
			}
			if (shaderDescriptorSet.ImageSamplers.size())
			{
				VkDescriptorPoolSize& typeCount = m_TypeCounts[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				typeCount.descriptorCount = (uint32_t)(shaderDescriptorSet.ImageSamplers.size());
			}
			if (shaderDescriptorSet.SeparateTextures.size())
			{
				VkDescriptorPoolSize& typeCount = m_TypeCounts[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				typeCount.descriptorCount = (uint32_t)(shaderDescriptorSet.SeparateTextures.size());
			}
			if (shaderDescriptorSet.SeparateSamplers.size())
			{
				VkDescriptorPoolSize& typeCount = m_TypeCounts[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_SAMPLER;
				typeCount.descriptorCount = (uint32_t)(shaderDescriptorSet.SeparateSamplers.size());
			}
			if (shaderDescriptorSet.StorageImages.size())
			{
				VkDescriptorPoolSize& typeCount = m_TypeCounts[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				typeCount.descriptorCount = (uint32_t)(shaderDescriptorSet.StorageImages.size());
			}
			if (shaderDescriptorSet.AccelerationStructures.size())
			{
				VkDescriptorPoolSize& typeCount = m_TypeCounts[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
				typeCount.descriptorCount = (uint32_t)(shaderDescriptorSet.AccelerationStructures.size());
			}

#if 0
			// TODO: Move this to the centralized renderer
			VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
			descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptorPoolInfo.pNext = nullptr;
			descriptorPoolInfo.poolSizeCount = m_TypeCounts.size();
			descriptorPoolInfo.pPoolSizes = m_TypeCounts.data();
			descriptorPoolInfo.maxSets = 1;

			VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &m_DescriptorPool));
#endif

			//////////////////////////////////////////////////////////////////////
			// Descriptor Set Layout
			//////////////////////////////////////////////////////////////////////

			// non-bindless
			bool bindlessSet = false;
			for (uint32_t bset : Renderer::GetBindlessSets())
				if (set == bset)
					bindlessSet = true;

			std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
			for (auto& [binding, uniformBuffer] : shaderDescriptorSet.UniformBuffers)
			{
				VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.emplace_back();
				layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				layoutBinding.descriptorCount = bindlessSet ? 1000 : uniformBuffer.ArraySize;
				layoutBinding.stageFlags = uniformBuffer.ShaderStage;
				layoutBinding.pImmutableSamplers = nullptr;
				layoutBinding.binding = binding;
				BEY_CORE_ASSERT(!shaderDescriptorSet.UniformBuffers.contains(binding) || uniformBuffer.ShaderStage & shaderDescriptorSet.UniformBuffers.at(binding).ShaderStage, "Binding is already present!");


				VkWriteDescriptorSet& writeDescriptorSet = shaderDescriptorSet.WriteDescriptorSets[uniformBuffer.Name];
				writeDescriptorSet = {};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.descriptorType = layoutBinding.descriptorType;
				writeDescriptorSet.descriptorCount = 1;
				writeDescriptorSet.dstBinding = layoutBinding.binding;
				shaderDescriptorSet.Bindings.emplace(binding);
			}

			for (auto& [binding, storageBuffer] : shaderDescriptorSet.StorageBuffers)
			{
				VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.emplace_back();
				layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				layoutBinding.descriptorCount = bindlessSet ? 1000 : storageBuffer.ArraySize;
				layoutBinding.stageFlags = storageBuffer.ShaderStage;
				layoutBinding.pImmutableSamplers = nullptr;
				layoutBinding.binding = binding;
				BEY_CORE_ASSERT(!shaderDescriptorSet.UniformBuffers.contains(binding), "Binding is already present!");

				VkWriteDescriptorSet& writeDescriptorSet = shaderDescriptorSet.WriteDescriptorSets[storageBuffer.Name];
				writeDescriptorSet = {};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.descriptorType = layoutBinding.descriptorType;
				writeDescriptorSet.descriptorCount = 1;
				writeDescriptorSet.dstBinding = layoutBinding.binding;
				shaderDescriptorSet.Bindings.emplace(binding);
			}

			for (auto& [binding, accelerationStructure] : shaderDescriptorSet.AccelerationStructures)
			{
				VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.emplace_back();
				layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
				layoutBinding.descriptorCount = bindlessSet ? 1000 : accelerationStructure.ArraySize;
				layoutBinding.stageFlags = accelerationStructure.ShaderStage;
				layoutBinding.pImmutableSamplers = nullptr;
				layoutBinding.binding = binding;
				BEY_CORE_ASSERT(!shaderDescriptorSet.UniformBuffers.contains(binding), "Binding is already present!");
				BEY_CORE_ASSERT(!shaderDescriptorSet.StorageBuffers.contains(binding), "Binding is already present!");

				VkWriteDescriptorSet& set = shaderDescriptorSet.WriteDescriptorSets[accelerationStructure.Name];
				set = {};
				set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				set.descriptorType = layoutBinding.descriptorType;
				set.descriptorCount = 1;
				set.dstBinding = layoutBinding.binding;
				shaderDescriptorSet.Bindings.emplace(binding);
			}

			for (auto& [binding, imageSampler] : shaderDescriptorSet.ImageSamplers)
			{
				VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.emplace_back();
				layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				layoutBinding.descriptorCount = bindlessSet ? 1000 : imageSampler.ArraySize;
				layoutBinding.stageFlags = imageSampler.ShaderStage;
				layoutBinding.pImmutableSamplers = nullptr;
				layoutBinding.binding = binding;

				BEY_CORE_ASSERT(!shaderDescriptorSet.UniformBuffers.contains(binding), "Binding is already present!");
				BEY_CORE_ASSERT(!shaderDescriptorSet.StorageBuffers.contains(binding), "Binding is already present!");
				BEY_CORE_ASSERT(!shaderDescriptorSet.AccelerationStructures.contains(binding), "Binding is already present!");

				VkWriteDescriptorSet& writeDescriptorSet = shaderDescriptorSet.WriteDescriptorSets[imageSampler.Name];
				writeDescriptorSet = {};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.descriptorType = layoutBinding.descriptorType;
				writeDescriptorSet.descriptorCount = layoutBinding.descriptorCount;
				writeDescriptorSet.dstBinding = layoutBinding.binding;
				shaderDescriptorSet.Bindings.emplace(binding);
			}

			for (auto& [binding, imageSampler] : shaderDescriptorSet.SeparateTextures)
			{
				VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.emplace_back();
				layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				layoutBinding.descriptorCount = bindlessSet ? 10000 : imageSampler.ArraySize;
				layoutBinding.stageFlags = imageSampler.ShaderStage;
				layoutBinding.pImmutableSamplers = nullptr;
				layoutBinding.binding = binding;

				BEY_CORE_ASSERT(!shaderDescriptorSet.UniformBuffers.contains(binding), "Binding is already present!");
				BEY_CORE_ASSERT(!shaderDescriptorSet.ImageSamplers.contains(binding), "Binding is already present!");
				BEY_CORE_ASSERT(!shaderDescriptorSet.StorageBuffers.contains(binding), "Binding is already present!");
				BEY_CORE_ASSERT(!shaderDescriptorSet.AccelerationStructures.contains(binding), "Binding is already present!");

				VkWriteDescriptorSet& writeDescriptorSet = shaderDescriptorSet.WriteDescriptorSets[imageSampler.Name];
				writeDescriptorSet = {};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.descriptorType = layoutBinding.descriptorType;
				writeDescriptorSet.descriptorCount = imageSampler.ArraySize;
				writeDescriptorSet.dstBinding = layoutBinding.binding;
				shaderDescriptorSet.Bindings.emplace(binding);
			}

			for (auto& [binding, imageSampler] : shaderDescriptorSet.SeparateSamplers)
			{
				VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.emplace_back();
				layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				layoutBinding.descriptorCount = bindlessSet ? 1000 : imageSampler.ArraySize;
				layoutBinding.stageFlags = imageSampler.ShaderStage;
				layoutBinding.pImmutableSamplers = nullptr;
				layoutBinding.binding = binding;

				BEY_CORE_ASSERT(!shaderDescriptorSet.UniformBuffers.contains(binding), "Binding is already present!");
				BEY_CORE_ASSERT(!shaderDescriptorSet.ImageSamplers.contains(binding), "Binding is already present!");
				BEY_CORE_ASSERT(!shaderDescriptorSet.StorageBuffers.contains(binding), "Binding is already present!");
				BEY_CORE_ASSERT(!shaderDescriptorSet.SeparateTextures.contains(binding), "Binding is already present!");
				BEY_CORE_ASSERT(!shaderDescriptorSet.AccelerationStructures.contains(binding), "Binding is already present!");

				VkWriteDescriptorSet& writeDescriptorSet = shaderDescriptorSet.WriteDescriptorSets[imageSampler.Name];
				writeDescriptorSet = {};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.descriptorType = layoutBinding.descriptorType;
				writeDescriptorSet.descriptorCount = imageSampler.ArraySize;
				writeDescriptorSet.dstBinding = layoutBinding.binding;
				shaderDescriptorSet.Bindings.emplace(binding);
			}

			for (auto& [bindingAndSet, imageSampler] : shaderDescriptorSet.StorageImages)
			{
				VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.emplace_back();
				layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				layoutBinding.descriptorCount = bindlessSet ? 1000 : imageSampler.ArraySize;
				layoutBinding.stageFlags = imageSampler.ShaderStage;
				layoutBinding.pImmutableSamplers = nullptr;

				uint32_t binding = bindingAndSet & 0xffffffff;
				//uint32_t descriptorSet = (bindingAndSet >> 32);
				layoutBinding.binding = binding;

				BEY_CORE_ASSERT(!shaderDescriptorSet.UniformBuffers.contains(binding), "Binding is already present!");
				BEY_CORE_ASSERT(!shaderDescriptorSet.StorageBuffers.contains(binding), "Binding is already present!");
				BEY_CORE_ASSERT(!shaderDescriptorSet.ImageSamplers.contains(binding), "Binding is already present!");
				BEY_CORE_ASSERT(!shaderDescriptorSet.SeparateTextures.contains(binding), "Binding is already present!");
				BEY_CORE_ASSERT(!shaderDescriptorSet.SeparateSamplers.contains(binding), "Binding is already present!");
				BEY_CORE_ASSERT(!shaderDescriptorSet.AccelerationStructures.contains(binding), "Binding is already present!");

				VkWriteDescriptorSet& writeDescriptorSet = shaderDescriptorSet.WriteDescriptorSets[imageSampler.Name];
				writeDescriptorSet = {};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.descriptorType = layoutBinding.descriptorType;
				writeDescriptorSet.descriptorCount = layoutBinding.descriptorCount;
				writeDescriptorSet.dstBinding = layoutBinding.binding;
				shaderDescriptorSet.Bindings.emplace(binding);
			}

			VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
			descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorLayout.pNext = nullptr;
			descriptorLayout.bindingCount = (uint32_t)(layoutBindings.size());
			descriptorLayout.pBindings = layoutBindings.data();

			VkDescriptorBindingFlags bindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
				VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT /*| VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT*/;
			std::vector<VkDescriptorBindingFlags> bindingFlagsVec(layoutBindings.size(), bindingFlags);
			VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr };

			if (bindlessSet)
			{
				bindingFlagsCreateInfo.bindingCount = (uint32_t)bindingFlagsVec.size();
				bindingFlagsCreateInfo.pBindingFlags = bindingFlagsVec.data();

				descriptorLayout.pNext = &bindingFlagsCreateInfo;
				descriptorLayout.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

				Renderer::Submit([instance = Ref(this)]()
				{
					Renderer::AddBindlessShader(instance);
				});
			}

			const eastl::string descriptorSetInfo = fmt::eastl_format("set {0} layout with {1} ubo's, {2} ssbo's, {3} samplers, {4} separate textures, {5} separate samplers, {6} storage images, and {7} acceleration structures.",
			set,
			shaderDescriptorSet.UniformBuffers.size(),
			shaderDescriptorSet.StorageBuffers.size(),
			shaderDescriptorSet.ImageSamplers.size(),
			shaderDescriptorSet.SeparateTextures.size(),
			shaderDescriptorSet.SeparateSamplers.size(),
			shaderDescriptorSet.StorageImages.size(),
			shaderDescriptorSet.AccelerationStructures.size());
			BEY_CORE_INFO_TAG("Renderer", "Creating descriptor {}", descriptorSetInfo);


			if (set >= m_DescriptorSetLayouts.size())
				m_DescriptorSetLayouts.resize((size_t)set + 1u);

			// Ideally descriptor sets should be shared between shaders with the same "root signature".
			// For now, only bindless ones are shared.
			if (!bindlessSet)
			{
				VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &m_DescriptorSetLayouts[set]));
				VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, fmt::eastl_format("Shader: {} with descriptor set: {}", m_Name, set), m_DescriptorSetLayouts[set]);
			}
			else
			{

				if (s_BindlessSetLayouts.contains(m_RootSignature) && s_BindlessSetLayouts.at(m_RootSignature).contains(set))
				{
					BEY_CORE_ASSERT(shaderDescriptorSet == s_BindlessSetLayouts.at(m_RootSignature).at(set).ShaderDescriptorSet, "This shader has a different bindless descriptor set layout at set {}.", set);
					m_DescriptorSetLayouts[set] = s_BindlessSetLayouts.at(m_RootSignature).at(set).Layout;
				}
				else
				{
					BEY_CORE_VERIFY(m_RootSignature != RootSignature::None);


					s_BindlessSetLayouts[m_RootSignature][set].ShaderDescriptorSet = shaderDescriptorSet;
					auto& bindlessVkSetLayout = s_BindlessSetLayouts[m_RootSignature][set].Layout;
					VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &bindlessVkSetLayout));
					VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, fmt::eastl_format("Bindless layout created from: {} with root signature {} and descriptor set: {}", m_Name, magic_enum::enum_name(m_RootSignature), set), bindlessVkSetLayout);
					m_DescriptorSetLayouts[set] = bindlessVkSetLayout;
				}
			}

			}
		}

	VulkanShader::ShaderMaterialDescriptorSet VulkanShader::CreateDescriptorSets(uint32_t set)
	{
		ShaderMaterialDescriptorSet result;

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		BEY_CORE_ASSERT(m_TypeCounts.contains(set));

		// TODO: Move this to the centralized renderer
		VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.pNext = nullptr;
		descriptorPoolInfo.poolSizeCount = (uint32_t)m_TypeCounts.at(set).size();
		descriptorPoolInfo.pPoolSizes = m_TypeCounts.at(set).data();
		descriptorPoolInfo.maxSets = 1;

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &result.Pool));

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = result.Pool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_DescriptorSetLayouts[set];

		result.DescriptorSets.emplace_back();
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, result.DescriptorSets.data()));
		//std::vector<std::string> layouts;
		//for (uint32_t i = 0; i < allocInfo.descriptorSetCount; i++)
			//layouts.push_back(std::to_string(allocInfo.pSetLayouts[i]));
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, fmt::eastl_format("Shader Material Descriptor Set [{}]"/* with Layouts: {}"*/, set/*, fmt::join(layouts, ", ")*/), result.DescriptorSets[result.DescriptorSets.size() - 1]);

		return result;
	}

	VulkanShader::ShaderMaterialDescriptorSet VulkanShader::CreateDescriptorSets(uint32_t set, uint32_t numberOfSets)
	{
		ShaderMaterialDescriptorSet result;

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		std::unordered_map<uint32_t, std::vector<VkDescriptorPoolSize>> poolSizes;
		for (uint32_t set = 0; set < m_ReflectionData.ShaderDescriptorSets.size(); set++)
		{
			auto& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[set];
			if (!shaderDescriptorSet) // Empty descriptor set
				continue;

			if (shaderDescriptorSet.UniformBuffers.size())
			{
				VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				typeCount.descriptorCount = (uint32_t)shaderDescriptorSet.UniformBuffers.size() * numberOfSets;
			}
			if (shaderDescriptorSet.StorageBuffers.size())
			{
				VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				typeCount.descriptorCount = (uint32_t)shaderDescriptorSet.StorageBuffers.size() * numberOfSets;
			}
			if (shaderDescriptorSet.ImageSamplers.size())
			{
				VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				uint32_t descriptorSetCount = 0;
				for (auto&& [binding, imageSampler] : shaderDescriptorSet.ImageSamplers)
					descriptorSetCount += imageSampler.ArraySize;

				typeCount.descriptorCount = descriptorSetCount * numberOfSets;
			}
			if (shaderDescriptorSet.SeparateTextures.size())
			{
				VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				uint32_t descriptorSetCount = 0;
				for (auto&& [binding, imageSampler] : shaderDescriptorSet.SeparateTextures)
					descriptorSetCount += imageSampler.ArraySize;

				typeCount.descriptorCount = descriptorSetCount * numberOfSets;
			}
			if (shaderDescriptorSet.SeparateTextures.size())
			{
				VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_SAMPLER;
				uint32_t descriptorSetCount = 0;
				for (auto&& [binding, imageSampler] : shaderDescriptorSet.SeparateSamplers)
					descriptorSetCount += imageSampler.ArraySize;

				typeCount.descriptorCount = descriptorSetCount * numberOfSets;
			}
			if (shaderDescriptorSet.StorageImages.size())
			{
				VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				typeCount.descriptorCount = (uint32_t)shaderDescriptorSet.StorageImages.size() * numberOfSets;
			}
			if (shaderDescriptorSet.AccelerationStructures.size())
			{
				VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
				typeCount.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
				typeCount.descriptorCount = (uint32_t)shaderDescriptorSet.AccelerationStructures.size() * numberOfSets;
			}

		}

		BEY_CORE_ASSERT(poolSizes.contains(set));

		// TODO: Move this to the centralized renderer
		VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.pNext = nullptr;
		descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizes.at(set).size();
		descriptorPoolInfo.pPoolSizes = poolSizes.at(set).data();
		descriptorPoolInfo.maxSets = numberOfSets;

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &result.Pool));

		result.DescriptorSets.resize(numberOfSets);

		for (uint32_t i = 0; i < numberOfSets; i++)
		{
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = result.Pool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &m_DescriptorSetLayouts[set];

			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &result.DescriptorSets[i]));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, fmt::eastl_format("Shader reflected Descriptor Set [{}]", i), result.DescriptorSets[i]);
		}
		return result;
	}

	const VkWriteDescriptorSet* VulkanShader::GetDescriptorSet(const eastl::string& name, uint32_t set) const
	{
		BEY_CORE_ASSERT(set < m_ReflectionData.ShaderDescriptorSets.size());
		BEY_CORE_ASSERT(m_ReflectionData.ShaderDescriptorSets[set]);
		const auto& descriptorSet = m_ReflectionData.ShaderDescriptorSets.at(set).WriteDescriptorSets;
		if (descriptorSet.find(name) == descriptorSet.end())
		{
			BEY_CORE_WARN_TAG("Renderer", "Shader {0} does not contain requested descriptor set {1}", m_Name, name);
			return nullptr;
		}
		return &m_ReflectionData.ShaderDescriptorSets.at(set).WriteDescriptorSets.at(name);
	}

	const std::vector<VkDescriptorSetLayout>& VulkanShader::GetAllDescriptorSetLayouts() const
	{
		return m_DescriptorSetLayouts;
	}

	const eastl::unordered_map<eastl::string, ShaderResourceDeclaration>& VulkanShader::GetResources() const
	{
		return m_ReflectionData.Resources;
	}

	void VulkanShader::AddShaderReloadedCallback(const ShaderReloadedCallback& callback)
	{
	}

	bool VulkanShader::TryReadReflectionData(StreamReader* serializer)
	{
		uint32_t shaderDescriptorSetCount;
		serializer->ReadRaw<uint32_t>(shaderDescriptorSetCount);

		for (uint32_t i = 0; i < shaderDescriptorSetCount; i++)
		{
			auto& descriptorSet = m_ReflectionData.ShaderDescriptorSets.emplace_back();
			serializer->ReadMap(descriptorSet.UniformBuffers);
			serializer->ReadMap(descriptorSet.StorageBuffers);
			serializer->ReadMap(descriptorSet.ImageSamplers);
			serializer->ReadMap(descriptorSet.StorageImages);
			serializer->ReadMap(descriptorSet.SeparateTextures);
			serializer->ReadMap(descriptorSet.SeparateSamplers);
			serializer->ReadMap(descriptorSet.AccelerationStructures);
			serializer->ReadMap(descriptorSet.WriteDescriptorSets);
		}

		serializer->ReadMap(m_ReflectionData.Resources);
		serializer->ReadMap(m_ReflectionData.ConstantBuffers);
		serializer->ReadArray(m_ReflectionData.PushConstantRanges);

		return true;
	}

	void VulkanShader::SerializeReflectionData(StreamWriter* serializer)
	{
		serializer->WriteRaw<uint32_t>((uint32_t)m_ReflectionData.ShaderDescriptorSets.size());
		for (const auto& descriptorSet : m_ReflectionData.ShaderDescriptorSets)
		{
			serializer->WriteMap(descriptorSet.UniformBuffers);
			serializer->WriteMap(descriptorSet.StorageBuffers);
			serializer->WriteMap(descriptorSet.ImageSamplers);
			serializer->WriteMap(descriptorSet.StorageImages);
			serializer->WriteMap(descriptorSet.SeparateTextures);
			serializer->WriteMap(descriptorSet.SeparateSamplers);
			serializer->WriteMap(descriptorSet.AccelerationStructures);
			serializer->WriteMap(descriptorSet.WriteDescriptorSets);
		}

		serializer->WriteMap(m_ReflectionData.Resources);
		serializer->WriteMap(m_ReflectionData.ConstantBuffers);
		serializer->WriteArray(m_ReflectionData.PushConstantRanges);
	}

	void VulkanShader::SetReflectionData(const ReflectionData& reflectionData)
	{
		m_ReflectionData = reflectionData;
	}

	}
