#pragma once

#include <EASTL/string_view.h>

namespace Beyond {

	class Hash
	{
	public:
		template <typename T>
		requires std::is_same_v<std::string_view, T> || std::is_same_v<eastl::string_view, T> || std::is_same_v<const char*, T> ||
		std::is_same_v<std::string, T> || std::is_same_v<eastl::string, T>
		static constexpr uint32_t GenerateFNVHash(const T str)
		{
			constexpr uint32_t FNV_PRIME = 16777619u;
			constexpr uint32_t OFFSET_BASIS = 2166136261u;

			size_t length;
			if constexpr (std::is_same_v<const char*, T>)
				length = std::strlen(str);
			else
				length = str.length();

			const char* data = str.data();

			uint32_t hash = OFFSET_BASIS;
			for (size_t i = 0; i < length; ++i)
			{
				hash ^= *data++;
				hash *= FNV_PRIME;
			}
			hash ^= '\0';
			hash *= FNV_PRIME;

			return hash;
		}

		static uint32_t GenerateFNVHash(const char* str)
		{
			constexpr uint32_t FNV_PRIME = 16777619u;
			constexpr uint32_t OFFSET_BASIS = 2166136261u;

			size_t length = std::strlen(str);

			const char* data = str;

			uint32_t hash = OFFSET_BASIS;
			for (size_t i = 0; i < length; ++i)
			{
				hash ^= *data++;
				hash *= FNV_PRIME;
			}
			hash ^= '\0';
			hash *= FNV_PRIME;

			return hash;
		}

		static uint32_t CRC32(const char* str);
		static uint32_t CRC32(const std::string& string);
	};
	
}
