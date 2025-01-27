#pragma once

#include "Beyond/Core/Ref.h"
#include "Beyond/Core/Log.h"
#include "Beyond/Physics/PhysicsAPI.h"
#include "Beyond/Asset/AssetManager/EditorAssetManager.h"
#include "Beyond/Asset/AssetManager/RuntimeAssetManager.h"

#include <filesystem>

namespace Beyond {

	class AudioCommandRegistry;

	struct ProjectConfig
	{
		std::string Name;

		std::string AssetDirectory;
		std::string AssetRegistryPath;

		std::string AudioCommandsRegistryPath = "Assets/AudioCommandsRegistry.hzr";

		std::string MeshPath;
		std::string MeshSourcePath;

		std::string AnimationPath;

		std::string ScriptModulePath = "Assets/Scripts/Binaries";
		std::string DefaultNamespace;

		std::string StartScene;

		bool AutomaticallyReloadAssembly;

		bool EnableAutoSave = false;
		int AutoSaveIntervalSeconds = 300;

		PhysicsAPIType CurrentPhysicsAPI = PhysicsAPIType::Jolt;

		// Not serialized
		std::string ProjectFileName;
		std::string ProjectDirectory;
	};

	class Project : public RefCounted
	{
	public:
		Project();
		~Project();

		const ProjectConfig& GetConfig() const { return m_Config; }

		static Ref<Project> GetActive() { return s_ActiveProject; }
		static void SetActive(Ref<Project> project);
		static void SetActiveRuntime(Ref<Project> project, Ref<AssetPack> assetPack);

		inline static Ref<AssetManagerBase> GetAssetManager() { return s_AssetManager; }
		inline static Ref<EditorAssetManager> GetEditorAssetManager() { return s_AssetManager.As<EditorAssetManager>(); }
		inline static Ref<RuntimeAssetManager> GetRuntimeAssetManager() { return s_AssetManager.As<RuntimeAssetManager>(); }

		static const std::string& GetProjectName()
		{
			BEY_CORE_ASSERT(s_ActiveProject);
			return s_ActiveProject->GetConfig().Name;
		}

		static std::filesystem::path GetProjectDirectory()
		{
			BEY_CORE_ASSERT(s_ActiveProject);
			return s_ActiveProject->GetConfig().ProjectDirectory;
		}

		static std::filesystem::path GetAssetDirectory()
		{
			BEY_CORE_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / s_ActiveProject->GetConfig().AssetDirectory;
		}

		static std::filesystem::path GetAssetRegistryPath()
		{
			BEY_CORE_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / s_ActiveProject->GetConfig().AssetRegistryPath;
		}

		static std::filesystem::path GetMeshPath()
		{
			BEY_CORE_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / s_ActiveProject->GetConfig().MeshPath;
		}

		static std::filesystem::path GetAnimationPath()
		{
			BEY_CORE_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / s_ActiveProject->GetConfig().AnimationPath;
		}

		static std::filesystem::path GetAudioCommandsRegistryPath()
		{
			BEY_CORE_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / s_ActiveProject->GetConfig().AudioCommandsRegistryPath;
		}

		static std::filesystem::path GetScriptModulePath()
		{
			BEY_CORE_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / s_ActiveProject->GetConfig().ScriptModulePath;
		}

		static std::filesystem::path GetScriptModuleFilePath()
		{
			BEY_CORE_ASSERT(s_ActiveProject);
			return GetScriptModulePath() / fmt::format("{}.dll", GetProjectName());
		}

		static std::filesystem::path GetCacheDirectory()
		{
			BEY_CORE_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / "Cache";
		}
	private:
		void OnSerialized();
		void OnDeserialized();

	private:
		ProjectConfig m_Config;
		Ref<AudioCommandRegistry> m_AudioCommands;
		inline static Ref<AssetManagerBase> s_AssetManager;

		friend class ProjectSettingsWindow;
		friend class ProjectSerializer;

		inline static Ref<Project> s_ActiveProject;
	};

}
