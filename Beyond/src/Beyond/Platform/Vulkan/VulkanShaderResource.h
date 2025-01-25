#pragma once


#include "Beyond/Serialization/StreamReader.h"
#include "Beyond/Serialization/StreamWriter.h"

#include <string>

#include "VulkanShaderUtils.h"
#include "VulkanMemoryAllocator/vk_mem_alloc.h"

namespace Beyond {

	namespace ShaderResource {

		struct UniformBuffer
		{
			VkDescriptorBufferInfo Descriptor;
			uint32_t Size = 0;
			uint32_t BindingPoint = 0;
			eastl::string Name;
			VkShaderStageFlagBits ShaderStage = {};
			uint32_t ArraySize = 0;

			static void Serialize(StreamWriter* serializer, const UniformBuffer& instance)
			{
				serializer->WriteRaw(instance.Descriptor);
				serializer->WriteRaw(instance.Size);
				serializer->WriteRaw(instance.BindingPoint);
				serializer->WriteString(instance.Name);
				serializer->WriteRaw(instance.ShaderStage);
				serializer->WriteRaw(instance.ArraySize);
			}

			static void Deserialize(StreamReader* deserializer, UniformBuffer& instance)
			{
				deserializer->ReadRaw(instance.Descriptor);
				deserializer->ReadRaw(instance.Size);
				deserializer->ReadRaw(instance.BindingPoint);
				deserializer->ReadString(instance.Name);
				deserializer->ReadRaw(instance.ShaderStage);
				deserializer->ReadRaw(instance.ArraySize);
			}
		};

		struct StorageBuffer
		{
			VmaAllocation MemoryAlloc = nullptr;
			VkDescriptorBufferInfo Descriptor;
			uint32_t Size = 0;
			uint32_t BindingPoint = 0;
			eastl::string Name;
			VkShaderStageFlagBits ShaderStage = {};
			uint32_t ArraySize = 0;

			static void Serialize(StreamWriter* serializer, const StorageBuffer& instance)
			{
				serializer->WriteRaw(instance.Descriptor);
				serializer->WriteRaw(instance.Size);
				serializer->WriteRaw(instance.BindingPoint);
				serializer->WriteString(instance.Name);
				serializer->WriteRaw(instance.ShaderStage);
				serializer->WriteRaw(instance.ArraySize);
			}

			static void Deserialize(StreamReader* deserializer, StorageBuffer& instance)
			{
				deserializer->ReadRaw(instance.Descriptor);
				deserializer->ReadRaw(instance.Size);
				deserializer->ReadRaw(instance.BindingPoint);
				deserializer->ReadString(instance.Name);
				deserializer->ReadRaw(instance.ShaderStage);
				deserializer->ReadRaw(instance.ArraySize);
			}
		};

		struct AccelerationStructure
		{
			uint32_t DescriptorSet = 0;
			uint32_t BindingPoint = 0;
			eastl::string Name;
			VkShaderStageFlagBits ShaderStage{};
			uint32_t ArraySize = 0;

			static void Serialize(StreamWriter* serializer, const AccelerationStructure& instance)
			{
				serializer->WriteRaw(instance.DescriptorSet);
				serializer->WriteRaw(instance.BindingPoint);
				serializer->WriteString(instance.Name);
				serializer->WriteRaw(instance.ShaderStage);
				serializer->WriteRaw(instance.ArraySize);
			}

			static void Deserialize(StreamReader* deserializer, AccelerationStructure& instance)
			{
				deserializer->ReadRaw(instance.DescriptorSet);
				deserializer->ReadRaw(instance.BindingPoint);
				deserializer->ReadString(instance.Name);
				deserializer->ReadRaw(instance.ShaderStage);
				deserializer->ReadRaw(instance.ArraySize);
			}
		};

		struct ImageSampler
		{
			uint32_t BindingPoint = 0;
			uint32_t DescriptorSet = 0;
			uint32_t Dimension = 0;
			uint32_t ArraySize = 0;
			eastl::string Name;
			VkShaderStageFlagBits ShaderStage = {};

			static void Serialize(StreamWriter* serializer, const ImageSampler& instance)
			{
				serializer->WriteRaw(instance.BindingPoint);
				serializer->WriteRaw(instance.DescriptorSet);
				serializer->WriteRaw(instance.Dimension);
				serializer->WriteRaw(instance.ArraySize);
				serializer->WriteString(instance.Name);
				serializer->WriteRaw(instance.ShaderStage);
			}

			static void Deserialize(StreamReader* deserializer, ImageSampler& instance)
			{
				deserializer->ReadRaw(instance.BindingPoint);
				deserializer->ReadRaw(instance.DescriptorSet);
				deserializer->ReadRaw(instance.Dimension);
				deserializer->ReadRaw(instance.ArraySize);
				deserializer->ReadString(instance.Name);
				deserializer->ReadRaw(instance.ShaderStage);
			}
		};

		struct PushConstantRange
		{
			VkShaderStageFlagBits ShaderStage = {};
			uint32_t Offset = 0;
			uint32_t Size = 0;

			static void Serialize(StreamWriter* writer, const PushConstantRange& range) { writer->WriteRaw(range); }
			static void Deserialize(StreamReader* reader, PushConstantRange& range) { reader->ReadRaw(range); }
		};




		struct ShaderDescriptorSet
		{
			std::unordered_map<uint32_t, UniformBuffer> UniformBuffers;
			std::unordered_map<uint32_t, StorageBuffer> StorageBuffers;
			std::unordered_map<uint32_t, ImageSampler> ImageSamplers;
			std::unordered_map<uint32_t, ImageSampler> StorageImages;
			std::unordered_map<uint32_t, ImageSampler> SeparateTextures; // Not really an image sampler.
			std::unordered_map<uint32_t, ImageSampler> SeparateSamplers;
			std::unordered_map<uint32_t, AccelerationStructure> AccelerationStructures;

			eastl::unordered_map<eastl::string, VkWriteDescriptorSet> WriteDescriptorSets;

			std::set<uint32_t> Bindings;

			operator bool() const { return !(StorageBuffers.empty() && UniformBuffers.empty() && ImageSamplers.empty() && StorageImages.empty() && AccelerationStructures.empty()); }

			inline bool CompareUniformBuffers(const UniformBuffer& buffer1, const UniformBuffer& buffer2) const
			{
				// Compare relevant properties (you can adjust this based on your needs)
				return buffer1.Size == buffer2.Size &&
					buffer1.BindingPoint == buffer2.BindingPoint &&
					buffer1.Name == buffer2.Name;
			}

			inline bool CompareStorageBuffers(const StorageBuffer& buffer1, const StorageBuffer& buffer2) const
			{
				// Compare relevant properties (adjust as needed)
				return buffer1.Size == buffer2.Size &&
					buffer1.BindingPoint == buffer2.BindingPoint &&
					buffer1.Name == buffer2.Name;
			}

			inline bool CompareAccelerationStructures(const AccelerationStructure& accel1, const AccelerationStructure& accel2) const
			{
				// Compare relevant properties (adjust as needed)
				return accel1.DescriptorSet == accel2.DescriptorSet &&
					accel1.BindingPoint == accel2.BindingPoint &&
					accel1.Name == accel2.Name;
			}

			inline bool CompareImageSamplers(const ImageSampler& sampler1, const ImageSampler& sampler2) const
			{
				// Compare relevant properties (adjust as needed)
				return sampler1.BindingPoint == sampler2.BindingPoint &&
					sampler1.DescriptorSet == sampler2.DescriptorSet &&
					sampler1.Dimension == sampler2.Dimension &&
					sampler1.Name == sampler2.Name;
					//sampler1.ShaderStage == sampler2.ShaderStage;
			}

			inline bool CompareShaderDescriptorSets(const ShaderDescriptorSet& set1, const ShaderDescriptorSet& set2) const
			{
				// Compare UniformBuffers
				bool result = true;
				for (const auto& [binding, buffer1] : set1.UniformBuffers)
				{
					if (!set2.UniformBuffers.contains(binding))
					{
						BEY_CORE_ERROR("Uniform Buffer with binding {} with name : {} at stage: {} not found in set2.", binding, buffer1.Name, ShaderUtils::ShaderStagesToString(buffer1.ShaderStage));
						result = false;
						continue;
					}

					const auto& buffer2 = set2.UniformBuffers.at(binding);
					if (!CompareUniformBuffers(buffer1, buffer2))
					{
						BEY_CORE_ERROR("Uniform Buffer mismatch for binding {}.", binding);
						return false;
					}
				}

				// Compare StorageBuffers
				for (const auto& [binding, buffer1] : set1.StorageBuffers)
				{
					if (!set2.StorageBuffers.contains(binding))
					{
						BEY_CORE_ERROR("Storage Buffer with binding {} with name : {} at stage: {} not found in set2.", binding, buffer1.Name, ShaderUtils::ShaderStagesToString(buffer1.ShaderStage));
						result = false;
						continue;
					}

					const auto& buffer2 = set2.StorageBuffers.at(binding);
					if (!CompareStorageBuffers(buffer1, buffer2))
					{
						BEY_CORE_ERROR("Storage Buffer mismatch for binding {}.", binding);
						return false;
					}
				}

				// Compare AccelerationStructures
				for (const auto& [binding, buffer1] : set1.AccelerationStructures)
				{
					if (!set2.AccelerationStructures.contains(binding))
					{
						BEY_CORE_ERROR("Storage Buffer with binding {} with name : {} at stage: {} not found in set2.", binding, buffer1.Name, ShaderUtils::ShaderStagesToString(buffer1.ShaderStage));
						result = false;
						continue;
					}

					const auto& buffer2 = set2.AccelerationStructures.at(binding);
					if (!CompareAccelerationStructures(buffer1, buffer2))
					{
						BEY_CORE_ERROR("Storage Buffer mismatch for binding {}.", binding);
						return false;
					}
				}

				// Compare ImageSamplers (ImageSamplers)
				for (const auto& [binding, sampler1] : set1.ImageSamplers)
				{
					if (!set2.ImageSamplers.contains(binding))
					{
						BEY_CORE_ERROR("ImageSamplers with binding {} with name : {} at stage: {} not found in set2.", binding, sampler1.Name, ShaderUtils::ShaderStagesToString(sampler1.ShaderStage));
						result = false;
						continue;
					}

					const auto& sampler2 = set2.ImageSamplers.at(binding);
					if (!CompareImageSamplers(sampler1, sampler2))
					{
						BEY_CORE_ERROR("ImageSampler (Texture) mismatch for binding {}.", binding);
						return false;
					}
				}

				// Compare ImageSamplers (SeparateTextures)
				for (const auto& [binding, sampler1] : set1.SeparateTextures)
				{
					if (!set2.SeparateTextures.contains(binding))
					{
						BEY_CORE_ERROR("SeparateTextures with binding {} with name : {} at stage: {} not found in set2.", binding, sampler1.Name, ShaderUtils::ShaderStagesToString(sampler1.ShaderStage));
						result = false;
						continue;
					}

					const auto& sampler2 = set2.SeparateTextures.at(binding);
					if (!CompareImageSamplers(sampler1, sampler2))
					{
						BEY_CORE_ERROR("ImageSampler (Texture) mismatch for binding {}.", binding);
						return false;
					}
				}

				// Compare ImageSamplers (StorageImages)
				for (const auto& [binding, sampler1] : set1.StorageImages)
				{
					if (!set2.ImageSamplers.contains(binding))
					{
						BEY_CORE_ERROR("StorageImages with binding {} with name : {} at stage: {} not found in set2.", binding, sampler1.Name, ShaderUtils::ShaderStagesToString(sampler1.ShaderStage));
						result = false;
						continue;
					}

					const auto& sampler2 = set2.StorageImages.at(binding);
					if (!CompareImageSamplers(sampler1, sampler2))
					{
						BEY_CORE_ERROR("ImageSampler (Texture) mismatch for binding {}.", binding);
						result = false;
						return false;
					}
				}

				// Compare ImageSamplers (SeparateSamplers)
				for (const auto& [binding, sampler1] : set1.SeparateSamplers)
				{
					if (!set2.ImageSamplers.contains(binding))
					{
						BEY_CORE_ERROR("SeparateSamplers with binding {} with name : {} at stage: {} not found in set2.", binding, sampler1.Name, ShaderUtils::ShaderStagesToString(sampler1.ShaderStage));
						result = false;
						continue;
					}

					const auto& sampler2 = set2.SeparateSamplers.at(binding);
					if (!CompareImageSamplers(sampler1, sampler2))
					{
						BEY_CORE_ERROR("ImageSampler (Texture) mismatch for binding {}.", binding);
						return false;
					}
				}

				// If everything matches, return true
				return result;
			}

			// Custom comparison function
			bool operator==(const ShaderDescriptorSet& other) const
			{
				return CompareShaderDescriptorSets(*this, other);
			}
		};

	}
}
