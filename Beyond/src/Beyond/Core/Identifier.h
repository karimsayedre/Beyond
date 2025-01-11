#pragma once

#include "Hash.h"

#include <stdint.h>
#include <cstring>
#include <string>

namespace Beyond
{
	class Identifier
	{
	public:
		constexpr Identifier() {}

		constexpr Identifier(eastl::string_view name) noexcept
			: hash(Hash::GenerateFNVHash(name)), dbgName(name)
		{
		}

		constexpr Identifier(std::string_view name) noexcept
			: hash(Hash::GenerateFNVHash(name)), dbgName(name.data(), name.size())
		{
		}

		constexpr Identifier(const char* name) noexcept
			: hash(Hash::GenerateFNVHash(eastl::string_view(name))), dbgName(eastl::string_view(name))
		{
		}

		constexpr Identifier(uint32_t hash) noexcept
			: hash(hash)
		{
		}

		constexpr bool operator==(const Identifier& other) const noexcept { return hash == other.hash; }
		constexpr bool operator!=(const Identifier& other) const noexcept { return hash != other.hash; }

		constexpr operator uint32_t() const noexcept { return hash; }
		constexpr eastl::string_view GetDBGName() const { return dbgName; }

	private:
		friend struct std::hash<Identifier>;
		uint32_t hash = 0;
		eastl::string_view dbgName;
	};

} // namespace Beyond


namespace std
{
	template<>
	struct hash<Beyond::Identifier>
	{
		size_t operator()(const Beyond::Identifier& id) const noexcept
		{
			static_assert(noexcept(hash<uint32_t>()(id.hash)), "hash function should not throw");
			return id.hash;
		}
	};
}

namespace fmt {
	template<>
	struct fmt::formatter<Beyond::Identifier> : fmt::formatter<std::string_view>
	{
		auto format(const Beyond::Identifier& id, format_context& ctx) const
		{
			return formatter<std::string_view>::format(id.GetDBGName().data(), ctx);
		}
	};
}
