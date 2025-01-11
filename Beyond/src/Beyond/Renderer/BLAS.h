#pragma once

namespace Beyond {
	class MaterialAsset;
	class MeshSource;

	// NOTE: This is same as VkBuildAccelerationStructureFlagBitsKHR
	typedef enum BuildAccelerationStructureFlagBits : int
	{
		AllowUpdate = 0x00000001,
		AllowCompaction = 0x00000002,
		PreferFastTrace = 0x00000004,
		PreferFastBuild = 0x00000008,
		LowMemory = 0x00000010,
		Motion = 0x00000020,
		AllowOpacityMicroMapUpdate = 0x00000040,
		AllowDisableOpacityMicroMaps = 0x00000080,
		AllowDisplacementMicroMap = 0x00000200,
		AllowDataAccess = 0x00000800,
	} BuildAccelerationStructureFlagBits;

	class BLAS : public RefCounted
	{
	public:
		virtual void RT_CreateBlasesInfo(const MeshSource* mesh, const MaterialAsset* material, int flags) = 0;

		static Ref<BLAS> Create();
		static Ref<BLAS> Create(Ref<BLAS> other);
	};

}
