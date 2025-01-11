#include "ApplicationSettings.h"

#include "yaml-cpp/yaml.h"

#include <fstream>
#include <iostream>

#include "EASTL/string.h"
#include "EASTL/string_view.h"

namespace Beyond {

	static void CreateDirectoriesIfNeeded(const std::filesystem::path& path)
	{
		std::filesystem::path directory = path.parent_path();
		if (!std::filesystem::exists(directory))
			std::filesystem::create_directories(directory);
	}

	ApplicationSettings::ApplicationSettings(const std::filesystem::path& filepath)
		: m_FilePath(filepath)
	{
		Deserialize();
	}

	void ApplicationSettings::Serialize()
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Beyond Application Settings";
		out << YAML::Value;

		out << YAML::BeginMap;
		for (const auto& [key, value] : m_Settings)
			out << YAML::Key << key.c_str() << YAML::Value << value.c_str();

		out << YAML::EndMap;

		out << YAML::EndSeq;

		CreateDirectoriesIfNeeded(m_FilePath);
		std::ofstream fout(m_FilePath);
		fout << out.c_str();

		fout.close();
	}

	bool ApplicationSettings::Deserialize()
	{
		std::ifstream stream(m_FilePath);
		if (!stream.good())
			return false;

		std::stringstream strStream;
		strStream << stream.rdbuf();

		YAML::Node data = YAML::Load(strStream.str());

		auto settings = data["Beyond Application Settings"];
		if (!settings)
			return false;

		for (auto it = settings.begin(); it != settings.end(); it++)
		{
			const auto& key = it->first.as<std::string>().c_str();
			const auto& value = it->second.as<std::string>().c_str();
			m_Settings[key] = value;
		}

		stream.close();
		return true;
	}

	bool ApplicationSettings::HasKey(eastl::string_view key) const
	{
		return m_Settings.find(eastl::string(key)) != m_Settings.end();
	}

	eastl::string ApplicationSettings::Get(eastl::string_view name, const eastl::string& defaultValue) const
	{
		if (!HasKey(name))
			return defaultValue;

		return m_Settings.at(eastl::string(name));
	}

	float ApplicationSettings::GetFloat(eastl::string_view name, float defaultValue) const
	{
		if (!HasKey(name))
			return defaultValue;

		const eastl::string& string = m_Settings.at(eastl::string(name));
		return std::stof(string.c_str());
	}

	int ApplicationSettings::GetInt(eastl::string_view name, int defaultValue) const
	{
		if (!HasKey(name))
			return defaultValue;

		const eastl::string& string = m_Settings.at(eastl::string(name));
		return std::stoi(string.c_str());
	}

	void ApplicationSettings::Set(eastl::string_view name, eastl::string_view value)
	{
		m_Settings[eastl::string(name)] = value;
	}

	void ApplicationSettings::SetFloat(eastl::string_view name, float value)
	{
		m_Settings[eastl::string(name)] = eastl::to_string(value);
	}

	void ApplicationSettings::SetInt(eastl::string_view name, int value)
	{
		m_Settings[eastl::string(name)] = eastl::to_string(value);
	}

}
