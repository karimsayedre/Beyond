#pragma once


#include "Beyond/Renderer/AccelerationStructure.h"
#include "VulkanAllocator.h"
#include "VulkanContext.h"



namespace Beyond {
	class VulkanBLAS;


	//class VulkanAccelerationStructure : public AccelerationStructure
	//{
	//public:
	//	VulkanAccelerationStructure();
	//	virtual ~VulkanAccelerationStructure() override;
	//	void RT_CreateBlas(const std::vector<uint32_t>& indices, VkDeviceSize asSize, VkDeviceAddress scratchAddress);
	//	void RT_Build(VkCommandBuffer cmdBuf, const VkAccelerationStructureBuildGeometryInfoKHR& buildInfo,
	//	              const VkAccelerationStructureBuildRangeInfoKHR* buildRangeInfos);
	//
	//
	//	Ref<VulkanBLAS> GetBLASes() const;
	//	const nvvk::AccelKHR& GetTLAS() const { return m_TLAS; }
	//	nvvk::AccelKHR& GetTLAS() { return m_TLAS; }
	//
	//	const VkWriteDescriptorSetAccelerationStructureKHR& GetTlasDescriptorInfo() const { return m_DescriptorInfo; }
	//	void SetDescriptorInfo(const VkWriteDescriptorSetAccelerationStructureKHR& descAsInfo)
	//	{
	//		m_DescriptorInfo = descAsInfo;
	//	}
	//
	//private:
	//	void Release();
	//	void RT_Invalidate();
	//
	//public:
	//	bool IsReady() const override { return m_TLAS.buffer.buffer != VK_NULL_HANDLE; }
	//
	//private:
	//	nvvk::AccelKHR              m_TLAS;  // Top-level acceleration structure
	//
	//	Ref<VulkanBLAS> m_BLASes;  // Bottom-level acceleration structure
	//
	//	VkQueryPool m_QueryPool = VK_NULL_HANDLE;
	//	VkWriteDescriptorSetAccelerationStructureKHR m_DescriptorInfo{};
	//	eastl::string m_Name;
	//	VkShaderStageFlagBits m_ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	//};
}
