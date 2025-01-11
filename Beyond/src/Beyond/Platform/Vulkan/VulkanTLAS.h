#pragma once
#include <nvvk/resourceallocator_vk.hpp>

#include "VulkanRenderCommandBuffer.h"
#include "VulkanStorageBuffer.h"
#include "Beyond/Renderer/TLAS.h"

namespace Beyond {

	class VulkanTLAS : public TLAS
	{
	public:
		VulkanTLAS(bool motion, eastl::string name);
		void RT_CreateAccelerationStructure(const VkAccelerationStructureMotionInfoNV& motionInfo, const VkAccelerationStructureBuildSizesInfoKHR& sizeInfo);
		void RT_BuildTlas(Ref<VulkanRenderCommandBuffer> commandBuffer, const eastl::vector<VkAccelerationStructureInstanceKHR>& instances, VkBuildAccelerationStructureFlagsKHR flags);
		void RT_BuildTlas(Ref<VulkanRenderCommandBuffer> renderCommandBuffer, Ref<VulkanStorageBuffer> storageBuffer, VkBuildAccelerationStructureFlagsKHR flags);

		const VkWriteDescriptorSetAccelerationStructureKHR& GetVulkanDescriptorInfo() const { return m_DescriptorInfo; }

		const nvvk::AccelKHR& GetTLAS() const { return m_TLAS; }
		nvvk::AccelKHR& GetTLAS() { return m_TLAS; }
		bool IsReady() override { return m_TLAS.buffer.buffer != VK_NULL_HANDLE; }
		void Release();
		~VulkanTLAS() override;

	private:
		void RT_BuildTLAS(Ref<VulkanRenderCommandBuffer> commandBuffer, nvvk::Buffer instancesBuffer, uint32_t instancesCount, VkBuildAccelerationStructureFlagsKHR flags);

		bool m_Motion = false;

		nvvk::AccelKHR m_TLAS;  // Top-level acceleration structure

		nvvk::Buffer m_ScratchBuffer;

		// At the class level, add a new member variable
		bool m_IsBuilt = false;

		//uint32_t m_LastInstanceCount = 0;

		VkQueryPool m_QueryPool = VK_NULL_HANDLE;
		VkWriteDescriptorSetAccelerationStructureKHR m_DescriptorInfo{};
		eastl::string m_Name;
		VkDeviceSize m_Size {};
		uint32_t m_LastInstanceCount;
		VkSemaphore m_TlasSemaphore[3];
	};
}
