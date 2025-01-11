#pragma once

#include <map>
#include <string>
#include <filesystem>

#include "EASTL/string.h"
#include "EASTL/string_view.h"

namespace Beyond {

	class ApplicationSettings
	{
	public:
		ApplicationSettings(const std::filesystem::path& filepath);

		void Serialize();
		bool Deserialize();

		bool HasKey(eastl::string_view key) const;

		eastl::string Get(eastl::string_view name, const eastl::string& defaultValue = "") const;
		float GetFloat(eastl::string_view name, float defaultValue = 0.0f) const;
		int GetInt(eastl::string_view name, int defaultValue = 0) const;

		void Set(eastl::string_view name, eastl::string_view value);
		void SetFloat(eastl::string_view name, float value);
		void SetInt(eastl::string_view name, int value);
	private:
		std::filesystem::path m_FilePath;
		std::map<eastl::string, eastl::string> m_Settings;
	};


}
