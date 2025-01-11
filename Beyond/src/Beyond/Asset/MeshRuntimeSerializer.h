#pragma once

#include "Beyond/Asset/Asset.h"
#include "Beyond/Asset/AssetSerializer.h"

#include "Beyond/Serialization/AssetPack.h"
#include "Beyond/Serialization/AssetPackFile.h"
#include "Beyond/Serialization/FileStream.h"

namespace Beyond {

	class MeshRuntimeSerializer
	{
	public:
		bool SerializeToAssetPack(AssetHandle handle, FileStreamWriter& stream, AssetSerializationInfo& outInfo);
		Ref<Asset> DeserializeFromAssetPack(FileStreamReader& stream, const AssetPackFile::AssetInfo& assetInfo);
	};

}
