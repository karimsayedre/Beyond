#include "pch.h"
#include "RuntimeAssetManager.h"

#include "Beyond/Debug/Profiler.h"
#include "Beyond/Core/Application.h"
#include "Beyond/Core/Timer.h"
#include "Beyond/Scene/Scene.h"

#include "Beyond/Asset/AssetImporter.h"

namespace Beyond {

	RuntimeAssetManager::RuntimeAssetManager()
	{
		AssetImporter::Init();
	}

	RuntimeAssetManager::~RuntimeAssetManager()
	{
	}

	AssetType RuntimeAssetManager::GetAssetType(AssetHandle assetHandle)
	{
		Ref<Asset> asset = GetAsset(assetHandle);
		if (!asset)
			return AssetType::None;

		return asset->GetAssetType();
	}

	Ref<Asset> RuntimeAssetManager::GetAsset(AssetHandle assetHandle)
	{
		BEY_PROFILE_FUNC();
		BEY_SCOPE_PERF("AssetManager::GetAsset");

		if (IsMemoryAsset(assetHandle))
			return m_MemoryAssets[assetHandle];

		Ref<Asset> asset = nullptr;
		bool isLoaded = IsAssetLoaded(assetHandle);
		if (isLoaded)
		{
			asset = m_LoadedAssets[assetHandle];
		}
		else
		{
			// Needs load
			asset = m_AssetPack->LoadAsset(m_ActiveScene, assetHandle);
			if (!asset)
				return nullptr;

			m_LoadedAssets[assetHandle] = asset;
		}
		return asset;
	}

	void RuntimeAssetManager::AddMemoryOnlyAsset(Ref<Asset> asset)
	{
		m_MemoryAssets[asset->Handle] = asset;
	}

	bool RuntimeAssetManager::ReloadData(AssetHandle assetHandle)
	{
		Ref<Asset> asset = m_AssetPack->LoadAsset(m_ActiveScene, assetHandle);
		if (asset)
			m_LoadedAssets[assetHandle] = asset;
		
		return asset;
	}

	bool RuntimeAssetManager::IsAssetHandleValid(AssetHandle assetHandle)
	{
		if (assetHandle == 0)
			return false;

		return IsMemoryAsset(assetHandle) || (m_AssetPack && m_AssetPack->IsAssetHandleValid(assetHandle));
	}

	bool RuntimeAssetManager::IsMemoryAsset(AssetHandle handle)
	{
		return m_MemoryAssets.find(handle) != m_MemoryAssets.end();
	}

	bool RuntimeAssetManager::IsAssetLoaded(AssetHandle handle)
	{
		return m_LoadedAssets.find(handle) != m_LoadedAssets.end();
	}

	void RuntimeAssetManager::RemoveAsset(AssetHandle handle)
	{
		if (m_LoadedAssets.find(handle) != m_LoadedAssets.end())
			m_LoadedAssets.erase(handle);

		if (m_MemoryAssets.find(handle) != m_MemoryAssets.end())
			m_MemoryAssets.erase(handle);
	}

	std::unordered_set<Beyond::AssetHandle> RuntimeAssetManager::GetAllAssetsWithType(AssetType type)
	{
		std::unordered_set<AssetHandle> result;
		BEY_CORE_VERIFY(false, "Not implemented");
		return result;
	}

	Ref<Scene> RuntimeAssetManager::LoadScene(AssetHandle handle)
	{
		Ref<Scene> scene = m_AssetPack->LoadScene(handle);
		if (scene)
			m_ActiveScene = handle;

		return scene;
	}

}
