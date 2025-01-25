#include "pch.h"
#include "DescriptorSetManager.h"
#include "Beyond/Renderer/Renderer.h"

#include "VulkanAPI.h"
#include "VulkanTexture.h"
#include "VulkanUniformBuffer.h"
#include "VulkanUniformBufferSet.h"
#include "VulkanStorageBuffer.h"
#include "VulkanStorageBufferSet.h"
//#include "VulkanAccelerationStructure.h"
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
#include "Beyond/Renderer/Sampler.h"

namespace Beyond {

	namespace Utils {

		inline RenderPassResourceType GetDefaultResourceType(VkDescriptorType descriptorType)
		{
			switch (descriptorType)
			{
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
					return RenderPassResourceType::Texture2D;
				case VK_DESCRIPTOR_TYPE_SAMPLER:
					return RenderPassResourceType::Sampler;
				case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
					return RenderPassResourceType::Image2D;
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
					return RenderPassResourceType::UniformBufferSet;
				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
					return RenderPassResourceType::StorageBufferSet;
				case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
					return RenderPassResourceType::AccelerationStructureSet;
			}

			BEY_CORE_ASSERT(false);
			return RenderPassResourceType::None;
		}

	}

	template<class ResourceType, typename DescriptorInfoType, RenderPassResourceType resourceType>
	void CheckChanges(const RenderPassInput& input, uint32_t frameIndex, uint32_t set, uint32_t binding, const std::vector<std::map<uint32_t, std::map<uint32_t, DescriptorSetManager::WriteDescriptor>>>& writeDescriptorMap, ResourceDesMap<RenderPassInput>& invalidatedInputResources)
	{
		bool invalidated = false;
		for (auto [index, resource] : input.Input)
		{

			Ref<ResourceType> vulkanResource = resource.As<ResourceType>();


			if constexpr (resourceType == RenderPassResourceType::UniformBufferSet)
			{
				if (vulkanResource->Get(frameIndex).As<VulkanUniformBuffer>()->GetVulkanDescriptorInfo().buffer != writeDescriptorMap[frameIndex].at(set).at(binding).ResourceHandles[index])
					invalidated = true;
			}
			else if  constexpr (resourceType == RenderPassResourceType::StorageBufferSet)
			{
				if (vulkanResource->Get(frameIndex).As<VulkanStorageBuffer>()->GetVulkanDescriptorInfo().buffer != writeDescriptorMap[frameIndex].at(set).at(binding).ResourceHandles[index])
					invalidated = true;
			}
			else if  constexpr (resourceType == RenderPassResourceType::AccelerationStructureSet)
			{
				if (vulkanResource->Get(frameIndex).As<VulkanTLAS>()->GetVulkanDescriptorInfo().pAccelerationStructures[0] != writeDescriptorMap[frameIndex].at(set).at(binding).ResourceHandles[index])
					invalidated = true;
			}
			else if constexpr (resourceType == RenderPassResourceType::AccelerationStructure)
			{
				if (vulkanResource.As<VulkanTLAS>()->GetVulkanDescriptorInfo().pAccelerationStructures[0] != writeDescriptorMap[frameIndex].at(set).at(binding).ResourceHandles[index])
					invalidated = true;
			}
			else if constexpr (resourceType == RenderPassResourceType::UniformBuffer || resourceType == RenderPassResourceType::StorageBuffer || resourceType == RenderPassResourceType::VertexBuffer || resourceType == RenderPassResourceType::IndexBuffer)
			{
				if (vulkanResource.As<ResourceType>()->GetVulkanDescriptorInfo().buffer != writeDescriptorMap[frameIndex].at(set).at(binding).ResourceHandles[index])
					invalidated = true;
			}
			else
			{
				if constexpr (resourceType == RenderPassResourceType::Sampler)
				{
					if (((DescriptorInfoType*)vulkanResource.As<ResourceType>()->GetDescriptorInfo())->sampler != writeDescriptorMap[frameIndex].at(set).at(binding).ResourceHandles[index])
						invalidated = true;
				}
				else
					if (((DescriptorInfoType*)vulkanResource.As<ResourceType>()->GetDescriptorInfo())->imageView != writeDescriptorMap[frameIndex].at(set).at(binding).ResourceHandles[index])
						invalidated = true;

			}

			//if constexpr (std::is_same_v<ResourceType, VulkanTexture2D>) // TODO: texture IsReady()?

		}

		if (invalidated)
			invalidatedInputResources[ResourceDesID(frameIndex, set, binding)] = input;
	}

	template<class ResourceType, typename DescriptorInfoType, RenderPassResourceType resourceType>
	void SubmitDescriptor(const RenderPassInput& input, uint32_t frameIndex, VkWriteDescriptorSet& descriptor, Buffer& imageInfoStorage, std::vector<const void*>& resourceHandles)
	{
		imageInfoStorage.Allocate(input.Input.size() * sizeof(DescriptorInfoType));

		for (size_t i = 0; auto [index, resource] : input.Input)
		{
			Ref<ResourceType> vulkanResource = resource.As<ResourceType>();

			const DescriptorInfoType* info;
			if constexpr (resourceType == RenderPassResourceType::UniformBufferSet)
			{

				info = (const DescriptorInfoType*)&vulkanResource->Get(frameIndex).template As<VulkanUniformBuffer>()->GetVulkanDescriptorInfo();
				resourceHandles[i] = info->buffer;
			}
			else if  constexpr (resourceType == RenderPassResourceType::StorageBufferSet)
			{
				info = (const DescriptorInfoType*)&vulkanResource->Get(frameIndex).template As<VulkanStorageBuffer>()->GetVulkanDescriptorInfo();
				resourceHandles[i] = info->buffer;
			}
			else if  constexpr (resourceType == RenderPassResourceType::AccelerationStructureSet)
			{

				info = (const DescriptorInfoType*)&vulkanResource->Get(frameIndex).template As<VulkanTLAS>()->GetVulkanDescriptorInfo();
				resourceHandles[i] = info->pAccelerationStructures[0];
			}
			else if constexpr (resourceType == RenderPassResourceType::AccelerationStructure)
			{
				info = (const DescriptorInfoType*)&vulkanResource->GetVulkanDescriptorInfo();
				resourceHandles[i] = info->pAccelerationStructures[0];
			}
			else if constexpr (resourceType == RenderPassResourceType::UniformBuffer || resourceType == RenderPassResourceType::StorageBuffer || resourceType == RenderPassResourceType::IndexBuffer || resourceType == RenderPassResourceType::VertexBuffer)
			{
				info = (const DescriptorInfoType*)&vulkanResource->GetVulkanDescriptorInfo();
				resourceHandles[i] = info->buffer;
			}
			else if constexpr (resourceType == RenderPassResourceType::Sampler)
			{
				info = (const DescriptorInfoType*)vulkanResource->GetDescriptorInfo();
				resourceHandles[i] = info->sampler;
			}
			else
			{
				info = (DescriptorInfoType*)vulkanResource->GetDescriptorInfo();
				resourceHandles[i] = info->imageView;
			}

			imageInfoStorage.Write(info, sizeof(DescriptorInfoType), i * sizeof(DescriptorInfoType));
			i++;
		}

		if constexpr (std::is_same_v<DescriptorInfoType, VkDescriptorImageInfo>)
			descriptor.pImageInfo = (VkDescriptorImageInfo*)imageInfoStorage.Data;
		else if constexpr (std::is_same_v<DescriptorInfoType, VkDescriptorBufferInfo>)
			descriptor.pBufferInfo = (VkDescriptorBufferInfo*)imageInfoStorage.Data;
		else if constexpr (std::is_same_v<DescriptorInfoType, VkWriteDescriptorSetAccelerationStructureKHR>)
			descriptor.pNext = imageInfoStorage.Data;
		else
			BEY_CORE_ASSERT(false);

		descriptor.dstArrayElement = input.Input.begin()->first;
	}

	template<class ResourceType, typename DescriptorInfoType, RenderPassResourceType resourceType>
	void SubmitDescriptor(const RenderPassInput& input, uint32_t frameIndex, uint32_t set, uint32_t binding, VkWriteDescriptorSet& descriptor, Buffer& imageInfoStorage, ResourceDesMap<RenderPassInput>& invalidatedInputResources, std::vector<const void*>& resourceHandles)
	{

		imageInfoStorage.Allocate(input.Input.size() * sizeof(DescriptorInfoType));

		for (size_t i = 0; const auto [index, resource] : input.Input)
		{
			Ref<ResourceType> vulkanResource = resource.As<ResourceType>();
			const DescriptorInfoType* info;
			if constexpr (resourceType == RenderPassResourceType::UniformBufferSet)
			{

				info = (const DescriptorInfoType*)&vulkanResource->Get(frameIndex).template As<VulkanUniformBuffer>()->GetVulkanDescriptorInfo();
				resourceHandles[i] = info->buffer;
			}
			else if  constexpr (resourceType == RenderPassResourceType::StorageBufferSet)
			{
				info = (const DescriptorInfoType*)&vulkanResource->Get(frameIndex).template As<VulkanStorageBuffer>()->GetVulkanDescriptorInfo();
				resourceHandles[i] = info->buffer;
			}
			else if  constexpr (resourceType == RenderPassResourceType::AccelerationStructureSet)
			{

				info = (const DescriptorInfoType*)&vulkanResource->Get(frameIndex).template As<VulkanTLAS>()->GetVulkanDescriptorInfo();
				resourceHandles[i] = info->pAccelerationStructures[0];
			}
			else if constexpr (resourceType == RenderPassResourceType::AccelerationStructure)
			{
				info = (const DescriptorInfoType*)&vulkanResource->GetVulkanDescriptorInfo();
				resourceHandles[i] = info->pAccelerationStructures[0];
			}
			else if constexpr (resourceType == RenderPassResourceType::UniformBuffer || resourceType == RenderPassResourceType::StorageBuffer || resourceType == RenderPassResourceType::IndexBuffer || resourceType == RenderPassResourceType::VertexBuffer)
			{
				info = (const DescriptorInfoType*)&vulkanResource->GetVulkanDescriptorInfo();
				resourceHandles[i] = info->buffer;
			}
			else
			{
				info = (DescriptorInfoType*)vulkanResource->GetDescriptorInfo();
				resourceHandles[i] = info->imageView;
			}

			imageInfoStorage.Write(info, sizeof(DescriptorInfoType), i * sizeof(DescriptorInfoType));

			// Defer if resource doesn't exist
			if (resourceHandles[i] == nullptr)
				invalidatedInputResources[ResourceDesID(frameIndex, set, binding)] = input;

			i++;
		}
		if constexpr (std::is_same_v<DescriptorInfoType, VkDescriptorImageInfo>)
			descriptor.pImageInfo = (VkDescriptorImageInfo*)imageInfoStorage.Data;
		else if constexpr (std::is_same_v<DescriptorInfoType, VkDescriptorBufferInfo>)
			descriptor.pBufferInfo = (VkDescriptorBufferInfo*)imageInfoStorage.Data;
		else if constexpr (std::is_same_v<DescriptorInfoType, VkWriteDescriptorSetAccelerationStructureKHR>)
			descriptor.pNext = imageInfoStorage.Data;
		else
			BEY_CORE_ASSERT(false);

		if (!input.Input.empty() && input.Input.begin()->second != nullptr)
			descriptor.dstArrayElement = input.Input.begin()->first;
		else
			invalidatedInputResources[ResourceDesID(frameIndex, set, binding)] = input;
	}


	DescriptorSetManager::DescriptorSetManager(const DescriptorSetManagerSpecification& specification)
		: m_Specification(specification)
	{
		Init();
		Renderer::RegisterShaderDependency(m_Specification.Shader, Ref<DescriptorSetManager>(this));
	}

	DescriptorSetManager& DescriptorSetManager::operator=(const DescriptorSetManager& other)
	{
		m_Specification = other.m_Specification;
		Renderer::RegisterShaderDependency(m_Specification.Shader, Ref<DescriptorSetManager>(this));
		Init();
		InputResources = other.InputResources;
		Bake();
		return *this;
	}

	DescriptorSetManager::DescriptorSetManager(const DescriptorSetManager& other)
		: m_Specification(other.m_Specification)
	{
		Renderer::RegisterShaderDependency(m_Specification.Shader, Ref<DescriptorSetManager>(this));
		Init();
		InputResources = other.InputResources;
		Bake();
	}

	void DescriptorSetManager::Init()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		const auto& shaderDescriptorSets = m_Specification.Shader->GetShaderDescriptorSets();
		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;
		WriteDescriptorMap.resize(framesInFlight);

		for (uint32_t set = m_Specification.StartSet; set <= m_Specification.EndSet; set++)
		{
			if (set >= shaderDescriptorSets.size())
				break;

			const auto& shaderDescriptor = shaderDescriptorSets[set];
			for (auto&& [bname, wd] : shaderDescriptor.WriteDescriptorSets)
			{
				// NOTE: This is a hack to fix a bad input decl name
				//				Coming from somewhere.
				const char* broken = strrchr(bname.c_str(), '.');
				eastl::string name = broken ? broken + 1 : bname;

				uint32_t binding = wd.dstBinding;
				RenderPassInputDeclaration& inputDecl = InputDeclarations[name];
				inputDecl.Type = RenderPassInputTypeFromVulkanDescriptorType(wd.descriptorType);
				inputDecl.Set = set;
				inputDecl.Binding = binding;
				inputDecl.Name = name;
				inputDecl.Count = wd.descriptorCount;

				// Create RenderPassInput
				RenderPassInput& input = InputResources[set][binding];
				input.Type = Utils::GetDefaultResourceType(wd.descriptorType);

				//// Insert default resources (useful for materials)
				//if (m_Specification.DefaultResources && set == (uint32_t)DescriptorSetAlias::Material)
				//{
				//	// Set default textures
				//	if (inputDecl.Type == RenderPassInputType::ImageSampler2D)
				//	{
				//		for (size_t i = 0; i < input.Input.size(); i++)
				//			input.Input[i] = Renderer::GetWhiteTexture();
				//	}
				//	else if (inputDecl.Type == RenderPassInputType::ImageSampler3D)
				//	{
				//		for (size_t i = 0; i < input.Input.size(); i++)
				//			input.Input[i] = Renderer::GetBlackCubeTexture();
				//	}
				//}

				for (uint32_t frameIndex = 0; frameIndex < framesInFlight; frameIndex++)
					WriteDescriptorMap[frameIndex][set][binding] = { wd, std::vector<const void*>(wd.descriptorCount) };

				if (shaderDescriptor.ImageSamplers.contains(binding))
				{
					auto& imageSampler = shaderDescriptor.ImageSamplers.at(binding);
					uint32_t dimension = imageSampler.Dimension;
					if (wd.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || wd.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
					{
						switch (dimension)
						{
							case 1:
								inputDecl.Type = RenderPassInputType::ImageSampler1D;
								break;
							case 2:
								inputDecl.Type = RenderPassInputType::ImageSampler2D;
								break;
							case 3:
								inputDecl.Type = RenderPassInputType::ImageSampler3D;
								break;
						}
					}
					else if (wd.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
					{
						switch (dimension)
						{
							case 1:
								inputDecl.Type = RenderPassInputType::StorageImage1D;
								break;
							case 2:
								inputDecl.Type = RenderPassInputType::StorageImage2D;
								break;
							case 3:
								inputDecl.Type = RenderPassInputType::StorageImage3D;
								break;
						}
					}
				}
			}
		}
	}

	void DescriptorSetManager::MatchInputs()
	{
		auto result = InputResources;
		for (auto& [set, map] : result)
		{
			for (auto& [binding, input] : map)
			{
				const RenderPassInputDeclaration* decl = GetInputDeclaration(input.Name);
				if (decl == nullptr)
					InputResources.at(set).erase(binding);
			}
			if (map.empty())
				InputResources.erase(set);
		}

		for (const auto& [name, input] : UnavailableInputResources)
		{
			SetInput(name, input);
		}
	}

	void DescriptorSetManager::Invalidate()
	{
		Renderer::Submit([instance = Ref(this)]() mutable
		{
			instance->RT_Invalidate();
		});
	}

	void DescriptorSetManager::RT_Invalidate()
	{
		Release();
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto bufferSets = HasBufferSets();
		bool perFrameInFlight = !bufferSets.empty();
		perFrameInFlight = true; // always
		uint32_t descriptorSetCount = Renderer::GetConfig().FramesInFlight;
		if (!perFrameInFlight)
			descriptorSetCount = 1;

		if (m_DescriptorSets.size() < 1)
		{
			for (uint32_t i = 0; i < descriptorSetCount; i++)
				m_DescriptorSets.emplace_back();
		}

		for (auto& descriptorSet : m_DescriptorSets)
			descriptorSet.clear();

		const auto& shaderDescriptorSets = m_Specification.Shader->GetShaderDescriptorSets();
		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;
		WriteDescriptorMap.resize(framesInFlight);

		for (uint32_t set = m_Specification.StartSet; set <= m_Specification.EndSet; set++)
		{
			if (set >= shaderDescriptorSets.size())
				break;

			const auto& shaderDescriptor = shaderDescriptorSets[set];
			for (auto&& [bname, wd] : shaderDescriptor.WriteDescriptorSets)
			{
				// NOTE: This is a hack to fix a bad input decl name
				//				Coming from somewhere.
				const char* broken = strrchr(bname.c_str(), '.');
				eastl::string name = broken ? broken + 1 : bname;

				uint32_t binding = wd.dstBinding;
				RenderPassInputDeclaration& inputDecl = InputDeclarations[name];
				inputDecl.Type = RenderPassInputTypeFromVulkanDescriptorType(wd.descriptorType);
				inputDecl.Set = set;
				inputDecl.Binding = binding;
				inputDecl.Name = name;
				inputDecl.Count = wd.descriptorCount;

				for (uint32_t frameIndex = 0; frameIndex < framesInFlight; frameIndex++)
					WriteDescriptorMap[frameIndex][set][binding] = { wd, std::vector<const void*>(wd.descriptorCount) };

				if (shaderDescriptor.ImageSamplers.contains(binding))
				{
					auto& imageSampler = shaderDescriptor.ImageSamplers.at(binding);
					uint32_t dimension = imageSampler.Dimension;
					if (wd.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || wd.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
					{
						switch (dimension)
						{
							case 1:
								inputDecl.Type = RenderPassInputType::ImageSampler1D;
								break;
							case 2:
								inputDecl.Type = RenderPassInputType::ImageSampler2D;
								break;
							case 3:
								inputDecl.Type = RenderPassInputType::ImageSampler3D;
								break;
						}
					}
					else if (wd.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
					{
						switch (dimension)
						{
							case 1:
								inputDecl.Type = RenderPassInputType::StorageImage1D;
								break;
							case 2:
								inputDecl.Type = RenderPassInputType::StorageImage2D;
								break;
							case 3:
								inputDecl.Type = RenderPassInputType::StorageImage3D;
								break;
						}

					}
				}
			}
		}

		MatchInputs();
		Bake();
		m_IsDirty = false;
	}

	void DescriptorSetManager::SetInput(const eastl::string& name, const RenderPassInput& input)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
			InputResources[decl->Set][decl->Binding] = input;
		else
			BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Input Uniform Buffer Set {} not found", m_Specification.DebugName, name);
	}

	void DescriptorSetManager::SetInput(const eastl::string& name, Ref<UniformBufferSet> uniformBufferSet)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
			InputResources.at(decl->Set).at(decl->Binding).Set(uniformBufferSet, name);
		else
		{
			UnavailableInputResources[eastl::string(name)].Set(uniformBufferSet, name);
			BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Input Uniform Buffer Set {} not found", m_Specification.DebugName, name);
		}
	}

	void DescriptorSetManager::SetInput(const eastl::string& name, Ref<UniformBuffer> uniformBuffer)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
			InputResources.at(decl->Set).at(decl->Binding).Set(uniformBuffer, name);
		else
		{
			UnavailableInputResources[eastl::string(name)].Set(uniformBuffer, name);
			BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Input Uniform Buffer {} not found", m_Specification.DebugName, name);
		}
	}

	void DescriptorSetManager::SetInput(const eastl::string& name, Ref<StorageBufferSet> storageBufferSet)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
			InputResources.at(decl->Set).at(decl->Binding).Set(storageBufferSet, name);
		else
		{
			UnavailableInputResources[eastl::string(name)].Set(storageBufferSet, name);
			BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Input Storage Buffer Set {} not found", m_Specification.DebugName, name);
		}
	}

	void DescriptorSetManager::SetInput(const eastl::string& name, Ref<StorageBuffer> storageBuffer)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
			InputResources.at(decl->Set).at(decl->Binding).Set(storageBuffer, name);
		else
		{
			UnavailableInputResources[eastl::string(name)].Set(storageBuffer, name);
			BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Input Storage Buffer {} not found", m_Specification.DebugName, name);
		}
	}

	void DescriptorSetManager::SetInput(const eastl::string& name, Ref<AccelerationStructureSet> accelerationStructureSet)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
			InputResources.at(decl->Set).at(decl->Binding).Set(accelerationStructureSet, name);
		else
		{
			UnavailableInputResources[eastl::string(name)].Set(accelerationStructureSet, name);
			BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Input Acceleration Strucuture Set {} not found", m_Specification.DebugName, name);
		}
	}

	void DescriptorSetManager::SetInput(const eastl::string& name, Ref<TLAS> accelerationStructure)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
			InputResources.at(decl->Set).at(decl->Binding).Set(accelerationStructure, name);
		else
		{
			UnavailableInputResources[eastl::string(name)].Set(accelerationStructure, name);
			BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Input Acceleration Strucuture {} not found", m_Specification.DebugName, name);
		}
	}

	void DescriptorSetManager::SetInput(const eastl::string& name, Ref<Texture2D> texture, uint32_t index)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
		{
			InputResources.at(decl->Set).at(decl->Binding).Set(texture, name, index);
			BEY_CORE_VERIFY(index < decl->Count);
		}
		else
		{
			UnavailableInputResources[eastl::string(name)] = RenderPassInput(texture, name, index);
			BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Input 2D Texture {} Index: {} not found", m_Specification.DebugName, name, index);
		}
	}

	void DescriptorSetManager::SetInput(const eastl::string& name, Ref<TextureCube> textureCube)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
			InputResources.at(decl->Set).at(decl->Binding).Set(textureCube, name);
		else
		{
			UnavailableInputResources[eastl::string(name)].Set(textureCube, name);
			BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Input Texture Cube {} not found", m_Specification.DebugName, name);
		}
	}

	void DescriptorSetManager::SetInput(const eastl::string& name, Ref<Image2D> image, uint32_t index)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
			InputResources.at(decl->Set).at(decl->Binding).Set(image, name, index);
		else
		{
			UnavailableInputResources[eastl::string(name)].Set(image, name, index);
			if (name != "Bey_DebugImage")
				BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Input Image {} Index: {} not found", m_Specification.DebugName, name, index);
		}
	}

	void DescriptorSetManager::SetInput(const eastl::string& name, Ref<ImageView> image, uint32_t index)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
			InputResources.at(decl->Set).at(decl->Binding).Set(image, name, index);
		else
		{
			UnavailableInputResources[eastl::string(name)].Set(image, name, index);
			BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Input Image View {} Index: {} not found", m_Specification.DebugName, name, index);
		}
	}

	void DescriptorSetManager::SetInput(const eastl::string& name, Ref<Sampler> sampler, uint32_t index)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
			InputResources.at(decl->Set).at(decl->Binding).Set(sampler, name, index);
		else
		{
			UnavailableInputResources[eastl::string(name)].Set(sampler, name, index);
			BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Input Sampler {} Index: {} not found", m_Specification.DebugName, name, index);
		}
	}

	bool DescriptorSetManager::IsInvalidated(uint32_t frame, uint32_t set, uint32_t binding) const
	{
		return InvalidatedInputResources.Contains(frame, set, binding);
	}

	std::set<uint32_t> DescriptorSetManager::HasBufferSets() const
	{
		// Find all descriptor sets that have either UniformBufferSet or StorageBufferSet descriptors
		std::set<uint32_t> sets;

		for (const auto& [set, resources] : InputResources)
		{
			for (const auto& [binding, input] : resources)
			{
				if (input.Type == RenderPassResourceType::UniformBufferSet || input.Type == RenderPassResourceType::StorageBufferSet)
				{
					sets.insert(set);
					break;
				}
			}
		}
		return sets;
	}


	bool DescriptorSetManager::Validate()
	{
		// Go through pipeline requirements to make sure we have all required resource
		const auto& shaderDescriptorSets = m_Specification.Shader->GetShaderDescriptorSets();

		// Nothing to validate, pipeline only contains material inputs
		//if (shaderDescriptorSets.size() < 2)
		//	return true;

		for (uint32_t set = m_Specification.StartSet; set <= m_Specification.EndSet; set++)
		{
			if (set >= shaderDescriptorSets.size())
				break;

			// No descriptors in this set
			if (!shaderDescriptorSets[set])
				continue;

			if (!InputResources.contains(set))
			{
				BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] No input resources for Set {}", m_Specification.DebugName, set);
				return false;
			}

			const auto& setInputResources = InputResources.at(set);

			const auto& shaderDescriptor = shaderDescriptorSets[set];
			for (auto&& [name, wd] : shaderDescriptor.WriteDescriptorSets)
			{
				uint32_t binding = wd.dstBinding;
				if (!setInputResources.contains(binding))
				{
					BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] No input resource for {}.{}", m_Specification.DebugName, set, binding);
					BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Required resource is {} ({})", m_Specification.DebugName, name, wd.descriptorType);
					return false;
				}

				const auto& resource = setInputResources.at(binding);
				if (!IsCompatibleInput(resource.Type, wd.descriptorType))
				{
					BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Required resource named: {} is wrong type! {} but needs {}", m_Specification.DebugName, resource.Name, resource.Type, wd.descriptorType);
					return false;
				}

				if (resource.Type != RenderPassResourceType::Image2D)
				{
					if (resource.Input.empty())
					{
						BEY_CORE_ERROR_TAG("Renderer", "[RenderPass ({})] Resource has not been set! {} ({}.{})", m_Specification.DebugName, name, set, binding);
						return false;
					}
					else
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

	void DescriptorSetManager::Release()
	{
		Renderer::SubmitResourceFree([pool = m_DescriptorPool]()
		{
			vkDestroyDescriptorPool(VulkanContext::GetCurrentDevice()->GetVulkanDevice(), pool, nullptr);
		});
		InputDeclarations.clear();
		m_DescriptorSets.clear();
		WriteDescriptorMap.clear();
	}

	void DescriptorSetManager::Bake()
	{
		BEY_PROFILE_FUNC();
		BEY_SCOPE_PERF("BindlessDescriptorSetManager::Bake");

		// Make sure all resources are present and we can properly bake
		if (!Validate())
		{
			BEY_CORE_ERROR_TAG("Renderer", "[RenderPass] Bake - Validate failed! {}", m_Specification.DebugName);
			return;
		}
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// Create Descriptor Pool
		std::vector<VkDescriptorPoolSize> poolSizes;

		// Add descriptor types based on the support
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 5000 });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 });

		if (VulkanContext::GetCurrentDevice()->GetPhysicalDevice()->IsExtensionSupported(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
		{
			poolSizes.push_back({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1000 });
		}

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolInfo.maxSets = 10 * 3; // frames in flight should partially determine this
		poolInfo.poolSizeCount = 10;
		poolInfo.pPoolSizes = poolSizes.data();

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool));
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, fmt::eastl_format("Descriptor Set Manager({})", m_Specification.DebugName), m_DescriptorPool);

		auto bufferSets = HasBufferSets();
		bool perFrameInFlight = !bufferSets.empty();
		perFrameInFlight = true; // always
		uint32_t descriptorSetCount = Renderer::GetConfig().FramesInFlight;
		if (!perFrameInFlight)
			descriptorSetCount = 1;

		if (m_DescriptorSets.size() < 1)
		{
			for (uint32_t i = 0; i < descriptorSetCount; i++)
				m_DescriptorSets.emplace_back();
		}

		for (auto& descriptorSet : m_DescriptorSets)
			descriptorSet.clear();


		for (const auto& [set, setData] : InputResources)
		{
			BEY_CORE_VERIFY(set != (uint32_t)DescriptorSetAlias::Bindless && set != (uint32_t)DescriptorSetAlias::DynamicBindless, "Use BindlessDescriptorSetManager for bindless sets.");

			uint32_t descriptorCountInSet = bufferSets.contains(set) ? descriptorSetCount : 1;
			for (uint32_t frameIndex = 0; frameIndex < descriptorSetCount; frameIndex++)
			{
				VkDescriptorSetLayout dsl = m_Specification.Shader->GetDescriptorSetLayout(set);
				VkDescriptorSetAllocateInfo descriptorSetAllocInfo = Vulkan::DescriptorSetAllocInfo(&dsl, 1, m_DescriptorPool);
				VkDescriptorSet descriptorSet = nullptr;

				VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, &descriptorSet));
				VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, fmt::eastl_format("Shader: {}, Set: {}, frame: {}", m_Specification.Shader->GetName(), set, frameIndex), descriptorSet);

				m_DescriptorSets[frameIndex].emplace_back(descriptorSet);

				if (!WriteDescriptorMap[frameIndex].contains(set))
					return;

				auto& writeDescriptorMap = WriteDescriptorMap[frameIndex].at(set);
				std::vector<Buffer> imageInfoStorage;
				uint32_t imageInfoStorageIndex = 0;

				for (const auto& [binding, input] : setData)
				{
					auto& storedWriteDescriptor = writeDescriptorMap.at(binding);

					VkWriteDescriptorSet& writeDescriptor = storedWriteDescriptor.WriteDescriptorSet;
					writeDescriptor.dstSet = descriptorSet;
					imageInfoStorage.resize(imageInfoStorageIndex + 1);
					switch (input.Type)
					{
						case RenderPassResourceType::UniformBuffer:
						{
							SubmitDescriptor<VulkanUniformBuffer, VkDescriptorBufferInfo, RenderPassResourceType::UniformBuffer>(input, frameIndex, set, binding, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], InvalidatedInputResources, storedWriteDescriptor.ResourceHandles);
							break;
						}
						case RenderPassResourceType::UniformBufferSet:
						{
							SubmitDescriptor<VulkanUniformBufferSet, VkDescriptorBufferInfo, RenderPassResourceType::UniformBufferSet>(input, frameIndex, set, binding, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], InvalidatedInputResources, storedWriteDescriptor.ResourceHandles);
							break;
						}
						case RenderPassResourceType::StorageBuffer:
						{
							SubmitDescriptor<VulkanStorageBuffer, VkDescriptorBufferInfo, RenderPassResourceType::StorageBuffer>(input, frameIndex, set, binding, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], InvalidatedInputResources, storedWriteDescriptor.ResourceHandles);
							break;
						}
						case RenderPassResourceType::StorageBufferSet:
						{
							SubmitDescriptor<VulkanStorageBufferSet, VkDescriptorBufferInfo, RenderPassResourceType::StorageBufferSet>(input, frameIndex, set, binding, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], InvalidatedInputResources, storedWriteDescriptor.ResourceHandles);
							break;
						}
						case RenderPassResourceType::VertexBuffer:
						{
							SubmitDescriptor<VulkanVertexBuffer, VkDescriptorBufferInfo, RenderPassResourceType::VertexBuffer>(input, frameIndex, set, binding, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], InvalidatedInputResources, storedWriteDescriptor.ResourceHandles);
							break;
						}
						case RenderPassResourceType::IndexBuffer:
						{
							SubmitDescriptor<VulkanIndexBuffer, VkDescriptorBufferInfo, RenderPassResourceType::IndexBuffer>(input, frameIndex, set, binding, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], InvalidatedInputResources, storedWriteDescriptor.ResourceHandles);
							break;
						}
						case RenderPassResourceType::AccelerationStructure:
						{
							SubmitDescriptor<VulkanTLAS, VkWriteDescriptorSetAccelerationStructureKHR, RenderPassResourceType::AccelerationStructure>(input, frameIndex, set, binding, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], InvalidatedInputResources, storedWriteDescriptor.ResourceHandles);
							break;
						}
						case RenderPassResourceType::AccelerationStructureSet:
						{
							SubmitDescriptor<AccelerationStructureSet, VkWriteDescriptorSetAccelerationStructureKHR, RenderPassResourceType::AccelerationStructureSet>(input, frameIndex, set, binding, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], InvalidatedInputResources, storedWriteDescriptor.ResourceHandles);
							break;
						}
						case RenderPassResourceType::Texture2D:
						{
							SubmitDescriptor<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::Texture2D>(input, frameIndex, set, binding, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], InvalidatedInputResources, storedWriteDescriptor.ResourceHandles);
							break;
						}
						case RenderPassResourceType::TextureCube:
						{
							SubmitDescriptor<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::TextureCube>(input, frameIndex, set, binding, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], InvalidatedInputResources, storedWriteDescriptor.ResourceHandles);
							break;
						}
						case RenderPassResourceType::Image2D:
						{
							SubmitDescriptor<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::Image2D>(input, frameIndex, set, binding, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], InvalidatedInputResources, storedWriteDescriptor.ResourceHandles);
							break;
						}
						case RenderPassResourceType::Sampler:
						{
							SubmitDescriptor<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::Sampler>(input, frameIndex, set, binding, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], InvalidatedInputResources, storedWriteDescriptor.ResourceHandles);
							break;
						}
					}
					imageInfoStorageIndex++;

				}

				std::vector<VkWriteDescriptorSet> writeDescriptors;
				for (auto&& [binding, writeDescriptor] : writeDescriptorMap)
				{
					// Include if valid, otherwise defer (these will be resolved if possible at Prepare stage)
					if (!IsInvalidated(frameIndex, set, binding))
						writeDescriptors.emplace_back(writeDescriptor.WriteDescriptorSet);
				}

				if (!writeDescriptors.empty())
				{
					BEY_CORE_INFO_TAG("Renderer", "Render pass update {} descriptors in set {}", writeDescriptors.size(), set);
					vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);
				}
				for (auto buffer : imageInfoStorage)
					buffer.Release();
			}
		}


	}

	void DescriptorSetManager::InvalidateAndUpdate()
	{
		BEY_PROFILE_FUNC();
		BEY_SCOPE_PERF("DescriptorSetManager::InvalidateAndUpdate");
		if (m_IsDirty)
		{
			RT_Invalidate();
			//return;
		}


		uint32_t currentFrameIndex = Renderer::RT_GetCurrentFrameIndex();

		// Check for invalidated resources
		for (const auto& [set, inputs] : InputResources)
		{
			for (const auto& [binding, input] : inputs)
			{
				switch (input.Type)
				{
					case RenderPassResourceType::UniformBuffer:
					{
						//for (uint32_t frameIndex = 0; frameIndex < (uint32_t)WriteDescriptorMap.size(); frameIndex++)
						{
							CheckChanges<VulkanUniformBuffer, VkDescriptorBufferInfo, RenderPassResourceType::UniformBuffer>(input, currentFrameIndex, set, binding, WriteDescriptorMap, InvalidatedInputResources);
						}
						break;
					}
					case RenderPassResourceType::UniformBufferSet:
					{
						//for (uint32_t frameIndex = 0; frameIndex < (uint32_t)WriteDescriptorMap.size(); frameIndex++)
						{
							CheckChanges<VulkanUniformBufferSet, VkDescriptorBufferInfo, RenderPassResourceType::UniformBufferSet>(input, currentFrameIndex, set, binding, WriteDescriptorMap, InvalidatedInputResources);
						}
						break;
					}
					case RenderPassResourceType::StorageBuffer:
					{

						//for (uint32_t frameIndex = 0; frameIndex < (uint32_t)WriteDescriptorMap.size(); frameIndex++)
						{
							CheckChanges<VulkanStorageBuffer, VkDescriptorBufferInfo, RenderPassResourceType::StorageBuffer>(input, currentFrameIndex, set, binding, WriteDescriptorMap, InvalidatedInputResources);
						}
						break;
					}
					case RenderPassResourceType::StorageBufferSet:
					{
						//for (uint32_t frameIndex = 0; frameIndex < (uint32_t)WriteDescriptorMap.size(); frameIndex++)
						{
							CheckChanges<VulkanStorageBufferSet, VkDescriptorBufferInfo, RenderPassResourceType::StorageBufferSet>(input, currentFrameIndex, set, binding, WriteDescriptorMap, InvalidatedInputResources);
						}
						break;
					}
					case RenderPassResourceType::AccelerationStructure:
					{
						//for (uint32_t frameIndex = 0; frameIndex < (uint32_t)WriteDescriptorMap.size(); frameIndex++)
						{
							CheckChanges<VulkanTLAS, VkWriteDescriptorSetAccelerationStructureKHR, RenderPassResourceType::AccelerationStructure>(input, currentFrameIndex, set, binding, WriteDescriptorMap, InvalidatedInputResources);
						}
						break;
					}
					case RenderPassResourceType::AccelerationStructureSet:
					{
						//for (uint32_t frameIndex = 0; frameIndex < (uint32_t)WriteDescriptorMap.size(); frameIndex++)
						{
							CheckChanges<VulkanAccelerationStructureSet, VkWriteDescriptorSetAccelerationStructureKHR, RenderPassResourceType::AccelerationStructureSet>(input, currentFrameIndex, set, binding, WriteDescriptorMap, InvalidatedInputResources);
						}
						break;
					}
					case RenderPassResourceType::Texture2D:
					{
						CheckChanges<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::Texture2D>(input, currentFrameIndex, set, binding, WriteDescriptorMap, InvalidatedInputResources);
						break;
					}
					case RenderPassResourceType::TextureCube:
					{
						//for (uint32_t frameIndex = 0; frameIndex < (uint32_t)WriteDescriptorMap.size(); frameIndex++)
						{
							CheckChanges<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::TextureCube>(input, currentFrameIndex, set, binding, WriteDescriptorMap, InvalidatedInputResources);
						}
						break;
					}
					case RenderPassResourceType::Image2D:
					{
						CheckChanges<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::Image2D>(input, currentFrameIndex, set, binding, WriteDescriptorMap, InvalidatedInputResources);
						break;
					}
					case RenderPassResourceType::Sampler:
					{
						//for (uint32_t frameIndex = 0; frameIndex < (uint32_t)WriteDescriptorMap.size(); frameIndex++)
						{
							CheckChanges<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::Sampler>(input, currentFrameIndex, set, binding, WriteDescriptorMap, InvalidatedInputResources);
						}
						break;
					}
				}
			}
		}

		// Nothing to do
		if (InvalidatedInputResources.GetRangeForFrame(currentFrameIndex).Empty())
			return;

		auto bufferSets = HasBufferSets();
		bool perFrameInFlight = !bufferSets.empty();
		perFrameInFlight = true; // always
		uint32_t descriptorSetCount = Renderer::GetConfig().FramesInFlight;
		if (!perFrameInFlight)
			descriptorSetCount = 1;

		uint32_t imageInfoStorageIndex = 0;
		std::vector<Buffer> imageInfoStorage;

		// TODO: handle these if they fail (although Vulkan will probably give us a validation error if they do anyway)
		for (const auto& [key, input] : InvalidatedInputResources.GetRangeForFrame(currentFrameIndex))
		{
			ResourceDesID frameKey = key;
			frameKey.SetFrame(currentFrameIndex);
			const uint32_t set = frameKey.GetSet();
			const uint32_t binding = frameKey.GetBinding();

			uint32_t descriptorCountInSet = bufferSets.contains(set) ? descriptorSetCount : 1;
			//for (uint32_t frameIndex = currentFrameIndex; frameIndex < descriptorSetCount; frameIndex++)
			uint32_t frameIndex = perFrameInFlight ? currentFrameIndex : 0;

			// Go through every resource here and call vkUpdateDescriptorSets with write descriptors
			// If we don't have valid buffers/images to bind to here, that's an error and needs to be
			// probably handled by putting in some error resources, otherwise we'll crash
			std::vector<VkWriteDescriptorSet> writeDescriptorsToUpdate;
			//writeDescriptorsToUpdate.reserve(setData.size());
			//for (const auto& [binding, input] : setData)
			{
				BEY_CORE_VERIFY(!input.Input.empty() && input.Type != RenderPassResourceType::None, "Invalid input(set: {}, binding:{}) named: \"{}\" of type: {}!", set, binding, input.Name, magic_enum::enum_name(input.Type));

				// Update stored write descriptor
				auto& wd = WriteDescriptorMap[frameIndex].at(set).at(binding);
				VkWriteDescriptorSet& writeDescriptor = wd.WriteDescriptorSet;
				imageInfoStorage.resize(imageInfoStorageIndex + 1);
				switch (input.Type)
				{
					case RenderPassResourceType::UniformBuffer:
					{
						SubmitDescriptor<VulkanUniformBuffer, VkDescriptorBufferInfo, RenderPassResourceType::UniformBuffer>(input, frameIndex, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], wd.ResourceHandles);
						break;
					}
					case RenderPassResourceType::UniformBufferSet:
					{
						SubmitDescriptor<VulkanUniformBufferSet, VkDescriptorBufferInfo, RenderPassResourceType::UniformBufferSet>(input, frameIndex, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], wd.ResourceHandles);
						break;
					}
					case RenderPassResourceType::StorageBuffer:
					{
						SubmitDescriptor<VulkanStorageBuffer, VkDescriptorBufferInfo, RenderPassResourceType::StorageBuffer>(input, frameIndex, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], wd.ResourceHandles);
						break;
					}
					case RenderPassResourceType::StorageBufferSet:
					{
						SubmitDescriptor<VulkanStorageBufferSet, VkDescriptorBufferInfo, RenderPassResourceType::StorageBufferSet>(input, frameIndex, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], wd.ResourceHandles);
						break;
					}
					case RenderPassResourceType::AccelerationStructure:
					{
						SubmitDescriptor<VulkanTLAS, VkWriteDescriptorSetAccelerationStructureKHR, RenderPassResourceType::AccelerationStructure>(input, frameIndex, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], wd.ResourceHandles);
						break;
					}
					case RenderPassResourceType::AccelerationStructureSet:
					{
						SubmitDescriptor<VulkanAccelerationStructureSet, VkWriteDescriptorSetAccelerationStructureKHR, RenderPassResourceType::AccelerationStructureSet>(input, frameIndex, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], wd.ResourceHandles);
						break;
					}
					case RenderPassResourceType::Texture2D:
					{
						SubmitDescriptor<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::Texture2D>(input, frameIndex, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], wd.ResourceHandles);
						break;
					}
					case RenderPassResourceType::TextureCube:
					{
						SubmitDescriptor<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::TextureCube>(input, frameIndex, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], wd.ResourceHandles);
						break;
					}
					case RenderPassResourceType::Image2D:
					{
						SubmitDescriptor<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::Image2D>(input, frameIndex, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], wd.ResourceHandles);
						break;
					}
					case RenderPassResourceType::Sampler:
					{
						SubmitDescriptor<RendererResource, VkDescriptorImageInfo, RenderPassResourceType::Sampler>(input, frameIndex, writeDescriptor, imageInfoStorage[imageInfoStorageIndex], wd.ResourceHandles);
						break;
					}
				}
				writeDescriptorsToUpdate.emplace_back(writeDescriptor);
				imageInfoStorageIndex++;
			}
			// BEY_CORE_INFO_TAG("Renderer", "RenderPass::Prepare ({}) - updating {} descriptors in set {} (frameIndex={})", m_Specification.DebugName, writeDescriptorsToUpdate.size(), set, frameIndex);
			BEY_CORE_INFO_TAG("Renderer", "DescriptorSetManager::InvalidateAndUpdate ({}) - updating {} descriptors in set {} (frameIndex={})", m_Specification.DebugName, writeDescriptorsToUpdate.size(), set, frameIndex);
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			vkUpdateDescriptorSets(device, (uint32_t)writeDescriptorsToUpdate.size(), writeDescriptorsToUpdate.data(), 0, nullptr);

			for (auto buffer : imageInfoStorage)
				buffer.Release();
			imageInfoStorage.clear();
		}

		InvalidatedInputResources.ClearFrameRange(currentFrameIndex);
	}

	bool DescriptorSetManager::HasDescriptorSets() const
	{
		return !m_DescriptorSets.empty() && !m_DescriptorSets[0].empty();
	}

	uint32_t DescriptorSetManager::GetFirstSetIndex() const
	{
		if (InputResources.empty())
			return UINT32_MAX;

		// Return first key (key == descriptor set index)
		return InputResources.begin()->first;
	}

	const std::vector<VkDescriptorSet>& DescriptorSetManager::GetDescriptorSets(uint32_t frameIndex) const
	{
		BEY_CORE_ASSERT(!m_DescriptorSets.empty());

		if (frameIndex > 0 && m_DescriptorSets.size() == 1)
			return m_DescriptorSets[0]; // Frame index is irrelevant for this type of render pass

		return m_DescriptorSets[frameIndex];
	}

	bool DescriptorSetManager::IsInputValid(const eastl::string& name) const
	{
		return InputDeclarations.contains(name);
	}

	const RenderPassInputDeclaration* DescriptorSetManager::GetInputDeclaration(const eastl::string& name) const
	{
		if (!InputDeclarations.contains(name))
			return nullptr;

		const RenderPassInputDeclaration& decl = InputDeclarations.at(name);
		return &decl;
	}

	void DescriptorSetManager::OnShaderReloaded()
	{
		m_IsDirty = true;
		//RT_Invalidate();
	}
}
