#include "pch.h"
#include "EditorApplicationSettings.h"
#include "Beyond/Utilities/FileSystem.h"

#include <yaml-cpp/yaml.h>

#include <sstream>
#include <filesystem>

namespace Beyond {

	static std::filesystem::path s_EditorSettingsPath;
	
	EditorApplicationSettings& EditorApplicationSettings::Get()
	{
		static EditorApplicationSettings s_Settings;
		return s_Settings;
	}

	void EditorApplicationSettingsSerializer::Init()
	{
		s_EditorSettingsPath = std::filesystem::absolute("Config");

		if (!FileSystem::Exists(s_EditorSettingsPath))
			FileSystem::CreateDirectory(s_EditorSettingsPath);
		s_EditorSettingsPath /= "EditorApplicationSettings.yaml";

		LoadSettings();
	}

#define BEY_ENTER_GROUP(name) currentGroup = rootNode[name];
#define BEY_READ_VALUE(name, type, var, defaultValue) var = currentGroup[name].as<type>(defaultValue)

	void EditorApplicationSettingsSerializer::LoadSettings()
	{
		// Generate default settings file if one doesn't exist
		if (!FileSystem::Exists(s_EditorSettingsPath))
		{
			SaveSettings();
			return;
		}

		std::ifstream stream(s_EditorSettingsPath);
		BEY_CORE_VERIFY(stream);
		std::stringstream ss;
		ss << stream.rdbuf();

		YAML::Node data = YAML::Load(ss.str());
		if (!data["EditorApplicationSettings"])
			return;

		YAML::Node rootNode = data["EditorApplicationSettings"];
		YAML::Node currentGroup = rootNode;

		auto& settings = EditorApplicationSettings::Get();

		BEY_ENTER_GROUP("Editor");
		{
			BEY_READ_VALUE("AdvancedMode", bool, settings.AdvancedMode, false);
			BEY_READ_VALUE("HighlightUnsetMeshes", bool, settings.HighlightUnsetMeshes, true);
			BEY_READ_VALUE("TranslationSnapValue", float, settings.TranslationSnapValue, 0.5f);
			BEY_READ_VALUE("RotationSnapValue", float, settings.RotationSnapValue, 45.0f);
			BEY_READ_VALUE("ScaleSnapValue", float, settings.ScaleSnapValue, 0.5f);
		}

		BEY_ENTER_GROUP("Scripting");
		{
			BEY_READ_VALUE("ShowHiddenFields", bool, settings.ShowHiddenFields, false);
			BEY_READ_VALUE("DebuggerListenPort", int, settings.ScriptDebuggerListenPort, 2550);
		}

		BEY_ENTER_GROUP("ContentBrowser");
		{
			BEY_READ_VALUE("ShowAssetTypes", bool, settings.ContentBrowserShowAssetTypes, true);
			BEY_READ_VALUE("ThumbnailSize", int, settings.ContentBrowserThumbnailSize, 128);
		}

		stream.close();
	}

#define BEY_BEGIN_GROUP(name)\
		out << YAML::Key << name << YAML::Value << YAML::BeginMap;
#define BEY_END_GROUP() out << YAML::EndMap;

#define BEY_SERIALIZE_VALUE(name, value) out << YAML::Key << name << YAML::Value << value;

	void EditorApplicationSettingsSerializer::SaveSettings()
	{
		const auto& settings = EditorApplicationSettings::Get();

		YAML::Emitter out;
		out << YAML::BeginMap;
		BEY_BEGIN_GROUP("EditorApplicationSettings");
		{
			BEY_BEGIN_GROUP("Editor");
			{
				BEY_SERIALIZE_VALUE("AdvancedMode", settings.AdvancedMode);
				BEY_SERIALIZE_VALUE("HighlightUnsetMeshes", settings.HighlightUnsetMeshes);
				BEY_SERIALIZE_VALUE("TranslationSnapValue", settings.TranslationSnapValue);
				BEY_SERIALIZE_VALUE("RotationSnapValue", settings.RotationSnapValue);
				BEY_SERIALIZE_VALUE("ScaleSnapValue", settings.ScaleSnapValue);
			}
			BEY_END_GROUP(); 
			
			BEY_BEGIN_GROUP("Scripting");
			{
				BEY_SERIALIZE_VALUE("ShowHiddenFields", settings.ShowHiddenFields);
				BEY_SERIALIZE_VALUE("DebuggerListenPort", settings.ScriptDebuggerListenPort);
			}
			BEY_END_GROUP();

			BEY_BEGIN_GROUP("ContentBrowser");
			{
				BEY_SERIALIZE_VALUE("ShowAssetTypes", settings.ContentBrowserShowAssetTypes);
				BEY_SERIALIZE_VALUE("ThumbnailSize", settings.ContentBrowserThumbnailSize);
			}
			BEY_END_GROUP();
		}
		BEY_END_GROUP();
		out << YAML::EndMap;

		std::ofstream fout(s_EditorSettingsPath);
		fout << out.c_str();
		fout.close();
	}


}
