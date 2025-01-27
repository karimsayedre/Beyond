#pragma once

#include <string>

#include "Vulkan.h"
#include "VulkanDevice.h"
#include "VulkanMemoryAllocator/vk_mem_alloc.h"

#include "Beyond/Renderer/GPUStats.h"
#include "nvvk/resourceallocator_vk.hpp"

namespace Beyond {

	class VulkanAllocator
	{
	public:
		VulkanAllocator() = default;
		VulkanAllocator(const eastl::string& tag);
		~VulkanAllocator();

		//void Allocate(VkMemoryRequirements requirements, VkDeviceMemory* dest, VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VmaAllocation AllocateBuffer(VkBufferCreateInfo bufferCreateInfo, VmaMemoryUsage usage, VkBuffer& outBuffer);
		VmaAllocation AllocateImage(VkImageCreateInfo imageCreateInfo, VmaMemoryUsage usage, VkImage& outImage, VkDeviceSize* allocatedSize = nullptr);
		void Free(VmaAllocation allocation);
		void DestroyImage(VkImage image, VmaAllocation allocation);
		void DestroyBuffer(VkBuffer& buffer, VmaAllocation& allocation);

		template<typename T>
		T* MapMemory(VmaAllocation allocation)
		{
			T* mappedMemory {};
			vmaMapMemory(VulkanAllocator::GetVMAAllocator(), allocation, (void**)&mappedMemory);
			return mappedMemory;
		}

		void UnmapMemory(VmaAllocation allocation);
		void DestroyAS(nvvk::AccelKHR& accel);
		uint64_t GetAllocationSize(const VmaAllocation memHandle);

		static void DumpStats();
		static GPUMemoryStats GetStats();

		static void Init(Ref<VulkanDevice> device);
		static void Shutdown();

		static VmaAllocator& GetVMAAllocator();
	private:
		eastl::string m_Tag;
	};


}
