#pragma once

#include <vulkan/vulkan_core.h>

#include "Beyond/Renderer/UniformBufferSet.h"
#include "Beyond/Renderer/StorageBufferSet.h"
#include "Beyond/Renderer/AccelerationStructureSet.h"
#include "Beyond/Renderer/Image.h"
#include "Beyond/Renderer/Texture.h"
#include <set>

#include "BindlessDescriptorSetManager.h"
#include "RenderPassInput.h"
#include "VulkanShader.h"
#include "Beyond/Core/Timer.h"
#include "Beyond/Debug/Profiler.h"
#include "Beyond/Core/Application.h"

namespace Beyond {

	struct DescriptorSetManager : public RefCounted
	{
		//
		// Input Resources (map of set->binding->resource)
		// 
		// Invalidated input resources will attempt to be assigned on Renderer::BeginRenderPass
		// This is useful for resources that may not exist at RenderPass creation but will be
		// present during actual rendering
		std::map<uint32_t, std::map<uint32_t, RenderPassInput>> InputResources;
		ResourceDesMap<RenderPassInput> InvalidatedInputResources;
		std::map<eastl::string, RenderPassInputDeclaration> InputDeclarations;

		eastl::unordered_map<eastl::string, RenderPassInput> UnavailableInputResources;

		// Per-frame in flight
		std::vector<std::vector<VkDescriptorSet>> m_DescriptorSets;

		struct WriteDescriptor
		{
			VkWriteDescriptorSet WriteDescriptorSet{};
			std::vector<const void*> ResourceHandles;
		};
		std::vector<std::map<uint32_t, std::map<uint32_t, WriteDescriptor>>> WriteDescriptorMap;

		DescriptorSetManager() = default;
		DescriptorSetManager(const DescriptorSetManager& other);
		DescriptorSetManager(const DescriptorSetManagerSpecification& specification);
		DescriptorSetManager& operator=(const DescriptorSetManager& other);

		void SetInput(const eastl::string& name, Ref<UniformBufferSet> uniformBufferSet);
		void SetInput(const eastl::string& name, Ref<UniformBuffer> uniformBuffer);
		void SetInput(const eastl::string& name, Ref<StorageBufferSet> storageBufferSet);
		void SetInput(const eastl::string& name, Ref<StorageBuffer> storageBuffer);
		void SetInput(const eastl::string& name, Ref<AccelerationStructureSet> accelerationStructureSet);
		void SetInput(const eastl::string& name, Ref<TLAS> accelerationStructure);
		void SetInput(const eastl::string& name, Ref<Texture2D> texture, uint32_t index = 0);
		void SetInput(const eastl::string& name, Ref<TextureCube> textureCube);
		void SetInput(const eastl::string& name, Ref<Image2D> image, uint32_t index = 0);
		void SetInput(const eastl::string& name, Ref<ImageView> image, uint32_t index = 0);
		void SetInput(const eastl::string& name, Ref<Sampler> sampler, uint32_t index = 0);

		template<typename T>
		Ref<T> GetInput(const eastl::string& name)
		{
			BEY_PROFILE_FUNC();
			BEY_SCOPE_PERF("DescriptorSetManager::GetInput");
			if (const RenderPassInputDeclaration* decl = GetInputDeclaration(name))
			{
				if (const auto setIt = InputResources.find(decl->Set); setIt != InputResources.end())
				{
					if (const auto resourceIt = setIt->second.find(decl->Binding); resourceIt != setIt->second.end())
						return resourceIt->second.Input[0].As<T>();
				}
			}
			return nullptr;
		}

		bool IsInvalidated(uint32_t frame, uint32_t set, uint32_t binding) const;
		bool Validate();
		void Release();
		void Bake();

		std::set<uint32_t> HasBufferSets() const;
		void InvalidateAndUpdate();

		VkDescriptorPool GetDescriptorPool() const { return m_DescriptorPool; }
		bool HasDescriptorSets() const;
		uint32_t GetFirstSetIndex() const;
		const std::vector<VkDescriptorSet>& GetDescriptorSets(uint32_t frameIndex) const;
		bool IsInputValid(const eastl::string& name) const;
		const RenderPassInputDeclaration* GetInputDeclaration(const eastl::string& name) const;
		void OnShaderReloaded();

	private:
		void Init();
		void MatchInputs();
		void Invalidate();
		void RT_Invalidate();
		void SetInput(const eastl::string& name, const RenderPassInput& input);

	private:
		DescriptorSetManagerSpecification m_Specification;
		VkDescriptorPool m_DescriptorPool = nullptr;
		bool m_IsDirty = false;
	};

}
