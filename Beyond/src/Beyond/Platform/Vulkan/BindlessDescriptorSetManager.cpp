#include "pch.h"
#include "BindlessDescriptorSetManager.h"
#include "Beyond/Renderer/Renderer.h"

#include "VulkanAPI.h"
#include "VulkanUniformBuffer.h"
#include "VulkanUniformBufferSet.h"
#include "VulkanStorageBuffer.h"
#include "VulkanStorageBufferSet.h"
#include <type_traits>

#include "VulkanBLAS.h"

#include "VulkanAccelerationStructureSet.h"
#include "VulkanIndexBuffer.h"
#include "VulkanSampler.h"
#include "VulkanTLAS.h"
#include "VulkanVertexBuffer.h"
#include "Beyond/Core/Timer.h"
#include "Beyond/Debug/Profiler.h"
#include "Beyond/Core/Application.h"
#include "Beyond/Renderer/RendererAPI.h"
#include "Beyond/Platform/Vulkan/VulkanShader.h"

namespace Beyond {

	namespace Utils {

		inline RenderPassResourceType GetDefaultResourceType(VkDescriptorType descriptorType)
		{
			switch (descriptorType)
			{
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				case VK_DESCRIPTOR_TYPE_SAMPLER:
					return RenderPassResourceType::Sampler;
				case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
					return RenderPassResourceType::Texture2D;
				case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
					return RenderPassResourceType::Image2D;
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
					return RenderPassResourceType::UniformBuffer;
				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
					return RenderPassResourceType::StorageBuffer;
				case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
					return RenderPassResourceType::AccelerationStructure;
			}

			BEY_CORE_ASSERT(false);
			return RenderPassResourceType::None;
		}
	}

	// returns true if resource is not ready
	template<class ResourceType, typename DescriptorInfoType, RenderPassResourceType resourceType>
	bool SubmitDescriptor(const RenderPassInput& input, uint32_t frameIndex, VkWriteDescriptorSet& descriptor, VkDescriptorType descriptorType, Buffer& imageInfoStorage, std::vector<const void*>& resourceHandles)
	{
		imageInfoStorage.Allocate(input.Input.size() * sizeof(DescriptorInfoType));

		bool invalidated = false;
		for (auto [index, resource] : input.Input)
		{
			Ref<ResourceType> vulkanResource = resource.As<ResourceType>();

			const DescriptorInfoType* info;
			if constexpr (resourceType == RenderPassResourceType::UniformBufferSet)
			{
				info = (const DescriptorInfoType*)&vulkanResource->Get(frameIndex).As<VulkanUniformBuffer>()->GetVulkanDescriptorInfo();
				resourceHandles[index] = info->buffer;
				if (info->buffer == nullptr)
					invalidated = true;
			}
			else if constexpr (resourceType == RenderPassResourceType::StorageBufferSet)
			{
				info = (const DescriptorInfoType*)&vulkanResource->Get(frameIndex).As<VulkanStorageBuffer>()->GetVulkanDescriptorInfo();
				resourceHandles[index] = info->buffer;
				if (info->buffer == nullptr)
					invalidated = true;
			}
			else if constexpr (resourceType == RenderPassResourceType::AccelerationStructureSet)
			{
				info = (const DescriptorInfoType*)&vulkanResource->Get(frameIndex).As<VulkanTLAS>()->GetVulkanDescriptorInfo();
				resourceHandles[index] = info->pAccelerationStructures[0];
				if (info->pAccelerationStructures == nullptr)
					invalidated = true;
			}
			else if constexpr (resourceType == RenderPassResourceType::AccelerationStructure)
			{
				info = (const DescriptorInfoType*)&vulkanResource->GetVulkanDescriptorInfo();
				resourceHandles[index] = info->pAccelerationStructures[0];
				if (info->pAccelerationStructures == nullptr)
					invalidated = true;
			}
			else if constexpr (resourceType == RenderPassResourceType::UniformBuffer || resourceType == RenderPassResourceType::StorageBuffer)
			{
				info = (const DescriptorInfoType*)&vulkanResource->GetVulkanDescriptorInfo();
				resourceHandles[index] = info->buffer;
				if (info->buffer == nullptr)
					invalidated = true;
			}
			else
			{
				if constexpr (std::is_same_v<DescriptorInfoType, VkDescriptorImageInfo>)
				{
					info = (const DescriptorInfoType*)vulkanResource->GetDescriptorInfo();
					if constexpr (resourceType == RenderPassResourceType::Sampler)
					{
						resourceHandles[index] = info->sampler;
						if (info->sampler == nullptr)
							invalidated = true;
					}
					else if constexpr (resourceType == RenderPassResourceType::Image2D || resourceType == RenderPassResourceType::Texture2D || resourceType == RenderPassResourceType::TextureCube)
					{
						resourceHandles[index] = info->imageView;
						if (info->imageView == nullptr)
							invalidated = true;
					}
					else
					{
						static_assert(false);
					}
				}
				else if constexpr (resourceType == RenderPassResourceType::IndexBuffer || resourceType == RenderPassResourceType::VertexBuffer)
				{
					info = (const DescriptorInfoType*)vulkanResource->GetDescriptorInfo();
					resourceHandles[index] = info->buffer;
					if (info->buffer == nullptr)
						invalidated = true;
				}
				else
				{
					static_assert(false);
				}
			}
			imageInfoStorage.Write(info, sizeof(DescriptorInfoType), index * sizeof(DescriptorInfoType));
		}

		if constexpr (std::is_same_v<DescriptorInfoType, VkDescriptorImageInfo>)
			descriptor.pImageInfo = (DescriptorInfoType*)imageInfoStorage.Data;
		else if constexpr (std::is_same_v<DescriptorInfoType, VkDescriptorBufferInfo>)
			descriptor.pBufferInfo = (DescriptorInfoType*)imageInfoStorage.Data;
		else if constexpr (std::is_same_v<DescriptorInfoType, VkWriteDescriptorSetAccelerationStructureKHR>)
			descriptor.pNext = (DescriptorInfoType*)imageInfoStorage.Data;
		else
			BEY_CORE_ASSERT(false);

		descriptor.dstArrayElement = input.Input.begin()->first;
		descriptor.descriptorCount = (uint32_t)input.Input.size();
		descriptor.descriptorType = descriptorType;

		return invalidated;
	}

	template<class ResourceType, typename DescriptorInfoType, RenderPassResourceType resourceType>
	bool CheckChanges(const RenderPassInput& input, uint32_t frameIndex, uint32_t set, uint32_t binding, const ResourceDesMap<BindlessDescriptorSetManager::WriteDescriptor>& writeDescriptorMap)
	{
		bool invalidated = false;
		for (auto [index, resource] : input.Input)
		{
			ResourceDesID key(frameIndex, set, binding);
			//TODO: invalidation should be per input not resource?
			if (!writeDescriptorMap.Contains(key) ||
				writeDescriptorMap.Get(key).ResourceHandles.size() != input.Input.size())
			{
				invalidated = true;
				break;
			}
			auto resourceHandle = writeDescriptorMap.Get(key).ResourceHandles[index];

			Ref<ResourceType> vulkanResource = resource.As<ResourceType>();

			if constexpr (resourceType == RenderPassResourceType::UniformBufferSet)
			{
				if (vulkanResource->Get(frameIndex).As<VulkanUniformBuffer>()->GetVulkanDescriptorInfo().buffer != resourceHandle)
					invalidated = true;
			}
			else if  constexpr (resourceType == RenderPassResourceType::StorageBufferSet)
			{
				if (vulkanResource->Get(frameIndex).As<VulkanStorageBuffer>()->GetVulkanDescriptorInfo().buffer != resourceHandle)
					invalidated = true;
			}
			else if  constexpr (resourceType == RenderPassResourceType::AccelerationStructureSet)
			{
				if (vulkanResource->Get(frameIndex).As<VulkanTLAS>()->GetVulkanDescriptorInfo().pAccelerationStructures[0] != resourceHandle)
					invalidated = true;
			}
			else if constexpr (resourceType == RenderPassResourceType::AccelerationStructure)
			{
				if (vulkanResource.As<VulkanTLAS>()->GetVulkanDescriptorInfo().pAccelerationStructures[0] != resourceHandle)
					invalidated = true;
			}
			else if constexpr (resourceType == RenderPassResourceType::UniformBuffer || resourceType == RenderPassResourceType::StorageBuffer)
			{
				if (vulkanResource.As<ResourceType>()->GetVulkanDescriptorInfo().buffer != resourceHandle)
					invalidated = true;
			}
			else if constexpr (resourceType == RenderPassResourceType::IndexBuffer || resourceType == RenderPassResourceType::VertexBuffer)
			{
				if (((DescriptorInfoType*)vulkanResource->GetDescriptorInfo())->buffer != resourceHandle)
					invalidated = true;
			}
			else
			{
				if constexpr (resourceType == RenderPassResourceType::Sampler)
				{
					if (((DescriptorInfoType*)vulkanResource.As<ResourceType>()->GetDescriptorInfo())->sampler != resourceHandle)
						invalidated = true;
				}
				else if constexpr (resourceType == RenderPassResourceType::Texture2D || resourceType == RenderPassResourceType::Image2D || resourceType == RenderPassResourceType::TextureCube)
				{
					if (((DescriptorInfoType*)vulkanResource.As<ResourceType>()->GetDescriptorInfo())->imageView != resourceHandle)
						invalidated = true;
				}
				else
				{
					static_assert(false, "Unknown resourceType");
				}
			}
		}
		return invalidated;
	}

	BindlessDescriptorSetManager::BindlessDescriptorSetManager(BindlessDescriptorSetManagerSpecification specification)
		: m_Specification(std::move(specification))
	{
		Init();
	}

	BindlessDescriptorSetManager::BindlessDescriptorSetManager(const BindlessDescriptorSetManager& other)
		: m_Specification(other.m_Specification)
	{
		Init();
		InputResources = other.InputResources;
		BakeAll();
	}

	const RenderPassInputDeclaration* BindlessDescriptorSetManager::GetInputDeclaration(const eastl::string& name) const
	{
		if (!InputDeclarations.contains(name))
			return nullptr;

		const RenderPassInputDeclaration& decl = InputDeclarations.at(name);
		return &decl;
	}

	bool BindlessDescriptorSetManager::SetBindlessInput(const RenderPassInput& input)
	{
		bool alreadySet = true;
		const RenderPassInputDeclaration* decl = GetInputDeclaration(input.Name);
		if (decl)
		{
			auto& inputs = InputResources[ResourceDesID(0, decl->Set, decl->Binding)];
			inputs.Type = input.Type;
			inputs.Name = input.Name;

			for (const auto& [index, resource] : input.Input)
			{
				if (inputs.Input[index] != resource)
					alreadySet = false;
				inputs.Input[index] = resource;
			}

		}
		else
			BEY_CORE_ERROR_TAG("Renderer", "[Bindless Manager] Input resource {} not found", input.Name);
		return alreadySet;
	}

	void BindlessDescriptorSetManager::SetShader(const Ref<VulkanShader> shader)
	{
		Renderer::Submit([instance = Ref(this), shader]() mutable
		{
			instance->m_Specification.Shaders.push_back(shader);

			// Don't have duplicates.
			std::set<VulkanShader*> shaders;
			for (auto shader : instance->m_Specification.Shaders)
				shaders.insert(shader.Raw());
			instance->m_Specification.Shaders.assign(shaders.begin(), shaders.end());

			auto addDeclarations = [&](const ShaderResource::ShaderDescriptorSet& descriptorSet, uint32_t set)
			{
				const auto& writeDescriptors = descriptorSet.WriteDescriptorSets;
				for (auto&& [bname, wd] : writeDescriptors)
				{
					// NOTE: This is a hack to fix a bad input decl name
					//				Coming from somewhere.
					const char* broken = strrchr(bname.c_str(), '.');
					eastl::string name = broken ? broken + 1 : bname;

					uint32_t binding = wd.dstBinding;
					if (instance->InputDeclarations.contains(name))
					{
						const auto& decl = instance->InputDeclarations.at(name);
						BEY_CORE_VERIFY(decl.Binding == binding && decl.Set == set && decl.Name == name/* && decl.Count == wd.descriptorCount*/, "Can't have different bindless resources in different shaders with the same name.");
					}
					RenderPassInputDeclaration& inputDecl = instance->InputDeclarations[name];
					inputDecl.Type = RenderPassInputTypeFromVulkanDescriptorType(wd.descriptorType);
					inputDecl.Set = set;
					inputDecl.Binding = binding;
					inputDecl.Name = name;
					inputDecl.Count = wd.descriptorCount;
				}
			};

			if (shader->GetShaderDescriptorSets().size() > instance->m_Specification.Set)
				addDeclarations(shader->GetShaderDescriptorSets()[instance->m_Specification.Set], instance->m_Specification.Set);
			if (shader->GetShaderDescriptorSets().size() > instance->m_Specification.DynamicSet)
				addDeclarations(shader->GetShaderDescriptorSets()[instance->m_Specification.DynamicSet], instance->m_Specification.DynamicSet);


			Renderer::RegisterShaderDependency(shader, instance);
		});
	}

	bool BindlessDescriptorSetManager::IsInvalidated(uint32_t frame, uint32_t set, uint32_t binding) const
	{
		return UpdatedInputResources.Contains(frame, set, binding);
	}

	void BindlessDescriptorSetManager::Init()
	{
		Release();
		uint32_t maxResources = m_Specification.MaxResources;

		// Create Descriptor Pool
		std::vector<VkDescriptorPoolSize> poolSizes;

		// Add descriptor types based on the support
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_SAMPLER, maxResources });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxResources });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 5000 });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, maxResources });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, maxResources });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, maxResources });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxResources });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, maxResources });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, maxResources });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, maxResources });

		if (VulkanContext::GetCurrentDevice()->GetPhysicalDevice()->IsExtensionSupported(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
		{
			poolSizes.push_back({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, maxResources });
		}

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
		poolInfo.maxSets = (uint32_t)poolSizes.size() * maxResources * Renderer::GetConfig().FramesInFlight; // frames in flight should partially determine this
		poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
		poolInfo.pPoolSizes = poolSizes.data();

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool));
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, fmt::eastl_format("Vulkan Renderer Descriptor Pool"), m_DescriptorPool);
	}

	bool BindlessDescriptorSetManager::Validate()
	{
		// Go through pipeline requirements to make sure we have all required resource
		for (const auto shader : m_Specification.Shaders)
		{

			const auto& shaderDescriptorSets = shader->GetShaderDescriptorSets();

			// Nothing to validate, pipeline only contains material inputs
			//if (shaderDescriptorSets.size() < 2)
			//	return true;

			for (uint32_t set : { m_Specification.Set, m_Specification.DynamicSet })
			{
				if (set >= shaderDescriptorSets.size())
					return true;

				// No descriptors in this set
				if (!shaderDescriptorSets[set])
					return true;

				const auto& shaderDescriptor = shaderDescriptorSets[set];
				for (auto&& [name, wd] : shaderDescriptor.WriteDescriptorSets)
				{
					uint32_t binding = wd.dstBinding;
					if (!InputResources.Contains(0, set, binding))
					{
						//BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] No input resource for {}.{}", m_Specification.DebugName, set, binding);
						//BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Required resource is {} ({})", m_Specification.DebugName, name, wd.descriptorType);
						continue;
						//return false;
					}

					const auto& resource = InputResources.Get(0, set, binding);
					if (!IsCompatibleInput(resource.Type, wd.descriptorType))
					{
						BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Required resource is wrong type! {} but needs {}", m_Specification.DebugName, resource.Type, wd.descriptorType);
						return false;
					}

					if (resource.Type != RenderPassResourceType::Image2D)
					{
						for (auto input : resource.Input | std::views::values)
						{
							if (input == nullptr)
							{
								BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Resource is null! {} ({}.{})", m_Specification.DebugName, name, set, binding);
								return false;
							}
						}
					}
				}
			}
		}

		// All resources present
		return true;
	}

	void BindlessDescriptorSetManager::Release()
	{
		Renderer::SubmitResourceFree([pool = m_DescriptorPool]()
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			vkDestroyDescriptorPool(device, pool, nullptr);
		});
		m_DescriptorPool = nullptr;
	}

	void BindlessDescriptorSetManager::AllocateDescriptorSets()
	{
		BEY_PROFILE_FUNC();
		BEY_SCOPE_PERF("BindlessDescriptorSetManager::AllocateDescriptorSets");

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t maxResources = m_Specification.MaxResources;
		const uint32_t frameCount = Renderer::GetConfig().FramesInFlight;

		for (const auto& [rootSignature, layouts] : VulkanShader::GetBindlessLayouts())
		{
			for (const auto& [set, descriptorSet] :layouts)
			{
				if (m_DescriptorSets.contains(rootSignature) && m_DescriptorSets.at(rootSignature).contains(set))
					continue;

				VkDescriptorSetLayout dsl = descriptorSet.Layout;
				VkDescriptorSetLayout frameLayouts[3] = { dsl, dsl, dsl };
				VkDescriptorSetAllocateInfo descriptorSetAllocInfo = Vulkan::DescriptorSetAllocInfo(frameLayouts, frameCount, m_DescriptorPool);

				auto& descriptorSets = m_DescriptorSets[rootSignature][set];
				descriptorSets.resize(frameCount);

				uint32_t maxBinding = maxResources - 1;
				uint32_t maxBindings[3] = { maxBinding, maxBinding, maxBinding };
				VkDescriptorSetVariableDescriptorCountAllocateInfo countInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
				countInfo.descriptorSetCount = frameCount;
				countInfo.pDescriptorCounts = maxBindings;
				descriptorSetAllocInfo.pNext = &countInfo;
				VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, descriptorSets.data()));

				for (uint32_t frame = 0; frame < frameCount; frame++)
					VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, fmt::eastl_format("Bindless descriptor set, Set: {}, Frame: {}", set, frame), descriptorSets[frame]);
			}
		}
	}

	void BindlessDescriptorSetManager::BakeAll()
	{
		BEY_PROFILE_FUNC();
		BEY_SCOPE_PERF("BindlessDescriptorSetManager::BakeAll");
		// Make sure all resources are present and we can properly bake
		if (!Validate())
		{
			BEY_CORE_ERROR_TAG("Renderer", "[RenderPass] BakeAll - Validate failed! {}", m_Specification.DebugName);
			return;
		}

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VK_CHECK_RESULT(vkResetDescriptorPool(device, m_DescriptorPool, 0));
		m_DescriptorSets.clear();
		UpdatedInputResources.Clear();

		AllocateDescriptorSets();
	}

	void BindlessDescriptorSetManager::InvalidateAndUpdate()
	{
		BEY_PROFILE_FUNC();
		BEY_SCOPE_PERF("BindlessDescriptorSetManager::InvalidateAndUpdate");

		uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();

		// Check for invalidated resources
		for (const auto& [key, input] : InputResources.GetRangeForFrame(0))
		{
			uint32_t set = key.GetSet();
			uint32_t binding = key.GetBinding();

			bool invalidated = false;
			switch (input.Type)
			{
				case RenderPassResourceType::UniformBuffer:
				{
					invalidated = CheckChanges<VulkanUniformBuffer, VkDescriptorBufferInfo, RenderPassResourceType::UniformBuffer>(input, frameIndex, set, binding, WriteDescriptorMap);
					break;
				}
				case RenderPassResourceType::UniformBufferSet:
				{
					invalidated = CheckChanges<VulkanUniformBufferSet, VkDescriptorBufferInfo, RenderPassResourceType::UniformBufferSet>(input, frameIndex, set, binding, WriteDescriptorMap);
					break;
				}
				case RenderPassResourceType::StorageBuffer:
				{
					invalidated = CheckChanges<VulkanStorageBuffer, VkDescriptorBufferInfo, RenderPassResourceType::StorageBuffer>(input, frameIndex, set, binding, WriteDescriptorMap);
					break;
				}
				case RenderPassResourceType::IndexBuffer:
				case RenderPassResourceType::VertexBuffer:
				{
					invalidated = CheckChanges<RendererResource, VkDescriptorBufferInfo, RenderPassResourceType::IndexBuffer>(input, frameIndex, set, binding, WriteDescriptorMap);
					break;
				}
				case RenderPassResourceType::StorageBufferSet:
				{
					invalidated = CheckChanges<VulkanStorageBufferSet, VkDescriptorBufferInfo, RenderPassResourceType::StorageBufferSet>(input, frameIndex, set, binding, WriteDescriptorMap);
					break;
				}
				case RenderPassResourceType::AccelerationStructure:
				{
					invalidated = CheckChanges<VulkanTLAS, VkWriteDescriptorSetAccelerationStructureKHR, RenderPassResourceType::AccelerationStructure>(input, frameIndex, set, binding, WriteDescriptorMap);
					break;
				}
				case RenderPassResourceType::AccelerationStructureSet:
				{
					invalidated = CheckChanges<VulkanAccelerationStructureSet, VkWriteDescriptorSetAccelerationStructureKHR, RenderPassResourceType::AccelerationStructureSet>(input, frameIndex, set, binding, WriteDescriptorMap);
					break;
				}
				case RenderPassResourceType::Texture2D:
				{
					invalidated = CheckChanges<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::Texture2D>(input, frameIndex, set, binding, WriteDescriptorMap);
					break;
				}
				case RenderPassResourceType::TextureCube:
				{
					invalidated = CheckChanges<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::TextureCube>(input, frameIndex, set, binding, WriteDescriptorMap);
					break;
				}
				case RenderPassResourceType::Image2D:
				{
					invalidated = CheckChanges<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::Image2D>(input, frameIndex, set, binding, WriteDescriptorMap);
					break;
				}
				case RenderPassResourceType::Sampler:
				{
					invalidated = CheckChanges<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::Sampler>(input, frameIndex, set, binding, WriteDescriptorMap);
					break;
				}
				default:
					BEY_CORE_VERIFY(false);
			}
			if (invalidated)
				UpdatedInputResources[ResourceDesID(frameIndex, set, binding)].clear();
		}

		const auto& bindlessLayouts = VulkanShader::GetBindlessLayouts();
		std::vector<VkWriteDescriptorSet> writeDescriptorsToUpdate;
		std::vector<Buffer> imageInfoStorage;
		uint32_t imageInfoStorageIndex = 0;
		for (const auto& [rootSignature, layoutSets] : bindlessLayouts)
		{
			bool isHLSL = IsRootSignatureHLSL(rootSignature);
			for (auto& [key, input] : InputResources.GetRangeForFrame(0))
			{
				ResourceDesID frameKey = key;
				frameKey.SetFrame(frameIndex);
				const uint32_t set = frameKey.GetSet();
				if (!layoutSets.contains(set))
					continue;

				auto textureDescType = isHLSL ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

				const auto descriptorSet = m_DescriptorSets.at(rootSignature).at(set).at(frameIndex);
				const uint32_t binding = frameKey.GetBinding();
				// Means already updated or not in descriptor set
				if ((UpdatedInputResources.Contains(frameKey) && UpdatedInputResources.Get(frameKey).contains(rootSignature)) || !layoutSets.at(set).ShaderDescriptorSet.Bindings.contains(binding))
					continue;

				// Update stored write descriptor
				std::vector<const void*> resourceHandles;
				resourceHandles.resize(input.Input.size());
				imageInfoStorage.resize(imageInfoStorageIndex + 1);

				VkWriteDescriptorSet writeDescriptor{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				writeDescriptor.dstSet = descriptorSet;
				writeDescriptor.dstBinding = binding;
				writeDescriptor.descriptorCount = 1;
				writeDescriptor.dstArrayElement = 0;

				bool invalidated = false;
				switch (input.Type)
				{
					case RenderPassResourceType::UniformBuffer:
					{
						invalidated = SubmitDescriptor<VulkanUniformBuffer, VkDescriptorBufferInfo, RenderPassResourceType::UniformBuffer>(input, frameIndex, writeDescriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, imageInfoStorage[imageInfoStorageIndex], resourceHandles);
						break;
					}
					case RenderPassResourceType::UniformBufferSet:
					{
						invalidated = SubmitDescriptor<VulkanUniformBufferSet, VkDescriptorBufferInfo, RenderPassResourceType::UniformBufferSet>(input, frameIndex, writeDescriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, imageInfoStorage[imageInfoStorageIndex], resourceHandles);
						break;
					}
					case RenderPassResourceType::StorageBuffer:
					{
						invalidated = SubmitDescriptor<VulkanStorageBuffer, VkDescriptorBufferInfo, RenderPassResourceType::StorageBuffer>(input, frameIndex, writeDescriptor, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, imageInfoStorage[imageInfoStorageIndex], resourceHandles);
						break;
					}
					case RenderPassResourceType::StorageBufferSet:
					{
						invalidated = SubmitDescriptor<VulkanStorageBufferSet, VkDescriptorBufferInfo, RenderPassResourceType::StorageBufferSet>(input, frameIndex, writeDescriptor, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, imageInfoStorage[imageInfoStorageIndex], resourceHandles);
						break;
					}
					case RenderPassResourceType::VertexBuffer:
					case RenderPassResourceType::IndexBuffer:
					{
						invalidated = SubmitDescriptor<RendererResource, VkDescriptorBufferInfo, RenderPassResourceType::IndexBuffer>(input, frameIndex, writeDescriptor, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, imageInfoStorage[imageInfoStorageIndex], resourceHandles);
						break;
					}
					case RenderPassResourceType::AccelerationStructure:
					{
						invalidated = SubmitDescriptor<VulkanTLAS, VkWriteDescriptorSetAccelerationStructureKHR, RenderPassResourceType::AccelerationStructure>(input, frameIndex, writeDescriptor, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, imageInfoStorage[imageInfoStorageIndex], resourceHandles);
						break;
					}
					case RenderPassResourceType::AccelerationStructureSet:
					{
						invalidated = SubmitDescriptor<AccelerationStructureSet, VkWriteDescriptorSetAccelerationStructureKHR, RenderPassResourceType::AccelerationStructureSet>(input, frameIndex, writeDescriptor, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, imageInfoStorage[imageInfoStorageIndex], resourceHandles);
						break;
					}
					case RenderPassResourceType::Texture2D:
					{

						invalidated = SubmitDescriptor<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::Texture2D>(input, frameIndex, writeDescriptor, textureDescType, imageInfoStorage[imageInfoStorageIndex], resourceHandles);
						break;
					}
					case RenderPassResourceType::TextureCube:
					{
						invalidated = SubmitDescriptor<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::TextureCube>(input, frameIndex, writeDescriptor, textureDescType, imageInfoStorage[imageInfoStorageIndex], resourceHandles);
						break;
					}
					case RenderPassResourceType::Image2D:
					{
						invalidated = SubmitDescriptor<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::Image2D>(input, frameIndex, writeDescriptor, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageInfoStorage[imageInfoStorageIndex], resourceHandles);
						break;
					}
					case RenderPassResourceType::Sampler:
					{
						invalidated = SubmitDescriptor<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::Sampler>(input, frameIndex, writeDescriptor, VK_DESCRIPTOR_TYPE_SAMPLER, imageInfoStorage[imageInfoStorageIndex], resourceHandles);
						break;
					}
					default:
						BEY_CORE_VERIFY(false);
				}
				if (!invalidated)
					UpdatedInputResources[frameKey].emplace(rootSignature);

				writeDescriptorsToUpdate.emplace_back(writeDescriptor);
				WriteDescriptorMap[frameKey] = WriteDescriptor{ writeDescriptor, resourceHandles };
				imageInfoStorageIndex++;

				BEY_CORE_INFO_TAG("Renderer", "BindlessDescriptorSetManager::InvalidateAndUpdate ({}) - updating {} descriptors in set {} (frameIndex={})", m_Specification.DebugName, writeDescriptorsToUpdate.size(), set, frameIndex);
			}
		}
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkUpdateDescriptorSets(device, (uint32_t)writeDescriptorsToUpdate.size(), writeDescriptorsToUpdate.data(), 0, nullptr);
	}

	bool BindlessDescriptorSetManager::HasDescriptorSets(Ref<VulkanShader> shader) const
	{
		return m_DescriptorSets.contains(shader->GetRootSignature()) && (shader->HasDescriptorSet(m_Specification.Set) || shader->HasDescriptorSet(m_Specification.DynamicSet));
	}

	uint32_t BindlessDescriptorSetManager::GetFirstSetIndex(const RootSignature rootSignature) const
	{
		return m_DescriptorSets.at(rootSignature).begin()->first;
	}

	std::vector<VkDescriptorSet> BindlessDescriptorSetManager::GetDescriptorSets(Ref<VulkanShader> shader, uint32_t frameIndex) const
	{
		BEY_CORE_ASSERT(!m_DescriptorSets.empty());
		const auto& shaderSets = m_DescriptorSets.at(shader->GetRootSignature());

		std::vector<VkDescriptorSet> sets;
		sets.reserve(shaderSets.size());

		std::ranges::transform(shaderSets, std::back_inserter(sets),
			[frameIndex](const auto& pair) { return pair.second[frameIndex]; });

		return sets;
	}

	void BindlessDescriptorSetManager::OnShaderReloaded()
	{
		Init();
		BakeAll();
	}


}
