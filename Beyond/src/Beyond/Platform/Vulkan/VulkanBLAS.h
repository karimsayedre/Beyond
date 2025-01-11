#pragma once
#include "VulkanAccelerationStructure.h"
#include "Beyond/Renderer/BLAS.h"


namespace Beyond {
	class MeshSource;

	struct BuildAccelerationStructure
	{
		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> rangeInfo;
		nvvk::AccelKHR                                  as;  // result acceleration structure
		nvvk::AccelKHR                                  cleanupAS;
	};

	class VulkanBLAS : public BLAS
	{
	public:
		// Inputs used to build Bottom-level acceleration structure.
		// You manage the lifetime of the buffer(s) referenced by the VkAccelerationStructureGeometryKHRs within.
		// In particular, you must make sure they are still valid and not being modified when the BLAS is built or updated.
		struct BlasInput
		{
			// Data used to build acceleration structure geometry
			std::vector<VkAccelerationStructureGeometryKHR>       asGeometry;
			std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildOffsetInfo;
			VkBuildAccelerationStructureFlagsKHR                  flags{ 0 };
		};

	public:
		VulkanBLAS() = default;
		VulkanBLAS(const Ref<VulkanBLAS> other);
		void GetOrCreate(const MeshSource* mesh, const MaterialAsset* materialAsset, int flags);

		bool IsStaticGeometry() const { return m_IsStaticGeometry; }
		void RT_CreateBlasesInfo(const MeshSource* mesh, const MaterialAsset* materials, int flags) override;
		const std::vector<BuildAccelerationStructure>& GetBuildInfo() const { return m_SubmeshBuild; }

		const VkWriteDescriptorSetAccelerationStructureKHR& GetTlasDescriptorInfo() const { return m_DescriptorInfo; }
		void SetDescriptorInfo(const VkWriteDescriptorSetAccelerationStructureKHR& descAsInfo)
		{
			m_DescriptorInfo = descAsInfo;
		}

		~VulkanBLAS() override;
		VkDeviceAddress GetBLASAddress(const size_t submeshIndex) const noexcept;

	private:
		bool m_IsReady = false;
		std::vector<BlasInput> m_Inputs;
		eastl::vector<VkDeviceAddress> m_AddressCache;

		bool m_IsStaticGeometry = false;

		std::vector<BuildAccelerationStructure> m_SubmeshBuild;
		VkQueryPool m_QueryPool = VK_NULL_HANDLE;
		VkWriteDescriptorSetAccelerationStructureKHR m_DescriptorInfo{};
		eastl::string m_Name;
	};
}
