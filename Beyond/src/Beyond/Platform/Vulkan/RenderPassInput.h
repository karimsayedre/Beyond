#pragma once

#include "Beyond/Renderer/AccelerationStructureSet.h"
#include "Beyond/Renderer/IndexBuffer.h"
#include "Beyond/Renderer/Sampler.h"
#include "Beyond/Renderer/StorageBuffer.h"
#include "Beyond/Renderer/StorageBufferSet.h"
#include "Beyond/Renderer/Texture.h"
#include "Beyond/Renderer/TLAS.h"
#include "Beyond/Renderer/UniformBuffer.h"
#include "Beyond/Renderer/UniformBufferSet.h"
#include "Beyond/Renderer/VertexBuffer.h"

namespace Beyond {
	class VulkanShader;


	enum class DescriptorSetAlias : uint32_t
	{
		Material = 0, Scene = 1, Renderer = 2, DDGI = 3, Bindless = 4, DynamicBindless = 5, Count = 6
	};

	// TODO:
	// - Move these enums/structs to a non-API specific place
	// - Maybe rename from RenderPassXXX to DescriptorXXX or something more
	//   generic, because these are also used for compute & materials

	enum class RenderPassResourceType : uint16_t
	{
		None = 0,
		UniformBuffer,
		UniformBufferSet,
		StorageBuffer,
		StorageBufferSet,
		AccelerationStructure,
		AccelerationStructureSet,
		Texture2D,
		TextureCube,
		Image2D,
		Sampler,
		IndexBuffer,
		VertexBuffer,
	};

	enum class RenderPassInputType : uint16_t
	{
		None = 0,
		UniformBuffer,
		StorageBuffer,
		AccelerationStructure,
		ImageSampler1D,
		ImageSampler2D,
		ImageSampler3D, // NOTE: 3D vs Cube?
		StorageImage1D,
		StorageImage2D,
		StorageImage3D,
		Sampler
	};



	struct RenderPassInput
	{
		RenderPassResourceType Type = RenderPassResourceType::None;
		std::unordered_map<uint32_t, Ref<RefCounted>> Input;
		eastl::string Name;

		RenderPassInput() = default;

		RenderPassInput(Ref<VertexBuffer> vertexBuffer, const eastl::string& name)
			: Type(RenderPassResourceType::VertexBuffer), Input({ {0u, vertexBuffer} }), Name(name)
		{
		}

		RenderPassInput(Ref<IndexBuffer> indexBuffer, const eastl::string& name)
			: Type(RenderPassResourceType::IndexBuffer), Input({ {0u, indexBuffer} }), Name(name)
		{
		}

		RenderPassInput(Ref<UniformBuffer> uniformBuffer, const eastl::string& name)
			: Type(RenderPassResourceType::UniformBuffer), Input({ {0u, uniformBuffer} }), Name(name)
		{
		}

		RenderPassInput(Ref<UniformBufferSet> uniformBufferSet, const eastl::string& name)
			: Type(RenderPassResourceType::UniformBufferSet), Input({ {0u, uniformBufferSet} }), Name(name)
		{
		}

		RenderPassInput(Ref<StorageBuffer> storageBuffer, const eastl::string& name)
			: Type(RenderPassResourceType::StorageBuffer), Input({ {0u, storageBuffer} }), Name(name)
		{
		}

		RenderPassInput(Ref<StorageBufferSet> storageBufferSet, const eastl::string& name)
			: Type(RenderPassResourceType::StorageBufferSet), Input({ {0u, storageBufferSet} }), Name(name)
		{
		}

		RenderPassInput(Ref<TLAS> AccelerationStrcuture, const eastl::string& name)
			: Type(RenderPassResourceType::AccelerationStructure), Input({ {0u, AccelerationStrcuture} }), Name(name)
		{
		}

		RenderPassInput(Ref<AccelerationStructureSet> AccelerationStrcutureSet, const eastl::string& name)
			: Type(RenderPassResourceType::AccelerationStructureSet), Input({ {0u, AccelerationStrcutureSet} }), Name(name)
		{
		}

		RenderPassInput(Ref<Texture2D> texture, const eastl::string& name)
			: Type(RenderPassResourceType::Texture2D), Input({ {0u, texture} }), Name(name)
		{
		}

		RenderPassInput(Ref<Texture2D> texture, const eastl::string& name, uint32_t count)
			: Type(RenderPassResourceType::Texture2D), Input({ {0u, texture} }), Name(name)
		{
		}

		RenderPassInput(Ref<TextureCube> texture, const eastl::string& name)
			: Type(RenderPassResourceType::TextureCube), Input({ {0u, texture} }), Name(name)
		{
		}

		RenderPassInput(Ref<Image2D> image, const eastl::string& name)
			: Type(RenderPassResourceType::Image2D), Input({ {0u, image} }), Name(name)
		{
		}

		RenderPassInput(Ref<ImageView> image, const eastl::string& name)
			: Type(RenderPassResourceType::Image2D), Input({ {0u, image} }), Name(name)
		{
		}

		RenderPassInput(Ref<Sampler> sampler, const eastl::string& name)
			: Type(RenderPassResourceType::Sampler), Input({ {0u, sampler} }), Name(name)
		{
		}

		void Set(Ref<VertexBuffer> vertexBuffer, const eastl::string& name, uint32_t index = 0)
		{
			Type = RenderPassResourceType::VertexBuffer;
			Input[index] = vertexBuffer;
			Name = name;
		}

		void Set(Ref<IndexBuffer> indexBuffer, const eastl::string& name, uint32_t index = 0)
		{
			Type = RenderPassResourceType::IndexBuffer;
			Input[index] = indexBuffer;
			Name = name;
		}

		void Set(Ref<UniformBuffer> uniformBuffer, const eastl::string& name, uint32_t index = 0)
		{
			Type = RenderPassResourceType::UniformBuffer;
			Input[index] = uniformBuffer;
			Name = name;
		}

		void Set(Ref<UniformBufferSet> uniformBufferSet, const eastl::string& name, uint32_t index = 0)
		{
			Type = RenderPassResourceType::UniformBufferSet;
			Input[index] = uniformBufferSet;
			Name = name;
		}

		void Set(Ref<StorageBuffer> storageBuffer, const eastl::string& name, uint32_t index = 0)
		{
			Type = RenderPassResourceType::StorageBuffer;
			Input[index] = storageBuffer;
			Name = name;
		}

		void Set(Ref<StorageBufferSet> storageBufferSet, const eastl::string& name, uint32_t index = 0)
		{
			Type = RenderPassResourceType::StorageBufferSet;
			Input[index] = storageBufferSet;
			Name = name;
		}

		void Set(Ref<TLAS> accelerationStructure, const eastl::string& name, uint32_t index = 0)
		{
			Type = RenderPassResourceType::AccelerationStructure;
			Input[index] = accelerationStructure;
			Name = name;
		}

		void Set(Ref<AccelerationStructureSet> accelerationStructureSet, const eastl::string& name, uint32_t index = 0)
		{
			Type = RenderPassResourceType::AccelerationStructureSet;
			Input[index] = accelerationStructureSet;
			Name = name;
		}

		void Set(Ref<Texture2D> texture, const eastl::string& name, uint32_t index = 0)
		{
			Type = RenderPassResourceType::Texture2D;
			Input[index] = texture;
			Name = name;
		}

		void Set(Ref<TextureCube> texture, const eastl::string& name, uint32_t index = 0)
		{
			Type = RenderPassResourceType::TextureCube;
			Input[index] = texture;
			Name = name;
		}

		void Set(Ref<Image2D> image, const eastl::string& name, uint32_t index = 0)
		{
			Type = RenderPassResourceType::Image2D;
			Input[index] = image;
			Name = name;
		}

		void Set(Ref<ImageView> image, const eastl::string& name, uint32_t index = 0)
		{
			Type = RenderPassResourceType::Image2D;
			Input[index] = image;
			Name = name;
		}

		void Set(Ref<Sampler> image, const eastl::string& name, uint32_t index = 0)
		{
			Type = RenderPassResourceType::Sampler;
			Input[index] = image;
			Name = name;
		}
	};

	

	inline bool IsCompatibleInput(RenderPassResourceType input, VkDescriptorType descriptorType)
	{
		switch (descriptorType)
		{
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			{
				return input == RenderPassResourceType::Texture2D || input == RenderPassResourceType::TextureCube || input == RenderPassResourceType::Image2D;
			}
			case VK_DESCRIPTOR_TYPE_SAMPLER:
			{
				return input == RenderPassResourceType::Sampler;
			}
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			{
				return input == RenderPassResourceType::Image2D;
			}
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			{
				return input == RenderPassResourceType::UniformBuffer || input == RenderPassResourceType::UniformBufferSet;
			}
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			{
				return input == RenderPassResourceType::StorageBuffer || input == RenderPassResourceType::StorageBufferSet || input == RenderPassResourceType::VertexBuffer || input == RenderPassResourceType::IndexBuffer;
			}
			case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
			{
				return input == RenderPassResourceType::AccelerationStructure || input == RenderPassResourceType::AccelerationStructureSet;
			}
		}
		return false;
	}

	inline RenderPassInputType RenderPassInputTypeFromVulkanDescriptorType(VkDescriptorType descriptorType)
	{
		switch (descriptorType)
		{
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				return RenderPassInputType::ImageSampler2D;
			case VK_DESCRIPTOR_TYPE_SAMPLER:
				return RenderPassInputType::Sampler;
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				return RenderPassInputType::StorageImage2D;
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				return RenderPassInputType::UniformBuffer;
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				return RenderPassInputType::StorageBuffer;
			case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
				return RenderPassInputType::AccelerationStructure;
		}

		BEY_CORE_ASSERT(false);
		return RenderPassInputType::None;
	}

	struct RenderPassInputDeclaration
	{
		RenderPassInputType Type = RenderPassInputType::None;
		uint32_t Set = 0;
		uint32_t Binding = 0;
		uint32_t Count = 0;
		eastl::string Name;
	};

	struct DescriptorSetManagerSpecification
	{
		Ref<VulkanShader> Shader;
		eastl::string DebugName;

		// Which descriptor sets should be managed
		uint32_t StartSet = 0, EndSet = (uint32_t)DescriptorSetAlias::DDGI;

		bool DefaultResources = false;
	};

}
