#pragma once
#include "MaterialAsset.h"

namespace Beyond
{

	struct MeshKey
	{
		AssetHandle MeshHandle;
		AssetHandle MaterialHandle;
		uint32_t	SubmeshIndex;
		bool		IsSelected;

		MeshKey(AssetHandle meshHandle, AssetHandle materialHandle, uint32_t submeshIndex, bool isSelected)
			: MeshHandle(meshHandle), MaterialHandle(materialHandle), SubmeshIndex(submeshIndex), IsSelected(isSelected)
		{
		}

		auto operator<=>(const MeshKey&) const = default;
	};

	struct DrawCommand
	{
		Mesh*		   Mesh;
		uint32_t	   SubmeshIndex = 0;
		MaterialTable* MaterialTable;
		Material*	   OverrideMaterial;

		uint32_t InstanceCount	= 0;
		uint32_t InstanceOffset = 0;
		bool	 IsRigged		= false;
	};

	struct StaticDrawCommand
	{
		Ref<StaticMesh>	   StaticMesh;
		uint32_t		   SubmeshIndex = 0;
		Ref<MaterialTable> MaterialTable;
		Ref<Material>	   OverrideMaterial;

		uint32_t InstanceCount	= 0;
		uint32_t InstanceOffset = 0;
	};
} // namespace Beyond

namespace eastl
{
	template<>
	struct hash<Beyond::MeshKey>
	{
		size_t operator()(const Beyond::MeshKey& key) const
		{
			std::size_t h1 = key.MeshHandle;
			std::size_t h2 = key.MaterialHandle;
			std::size_t h3 = std::hash<uint32_t>()(key.SubmeshIndex);
			std::size_t h4 = std::hash<bool>()(key.IsSelected);
			return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3); // Combine the hashes
		}
	};
} // namespace eastl

namespace std
{
	template<>
	struct hash<Beyond::MeshKey>
	{
		size_t operator()(const Beyond::MeshKey& key) const noexcept
		{
			std::size_t h1 = key.MeshHandle;
			std::size_t h2 = key.MaterialHandle;
			std::size_t h3 = std::hash<uint32_t>()(key.SubmeshIndex);
			std::size_t h4 = std::hash<bool>()(key.IsSelected);
			return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3); // Combine the hashes
		}
	};
} // namespace std
