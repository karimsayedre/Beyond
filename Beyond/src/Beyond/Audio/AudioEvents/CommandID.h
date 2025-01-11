#pragma once

#include "Beyond/Core/Hash.h"

namespace Beyond::Audio
{
	class CommandID
	{
	private:
		uint32_t ID;

	private:

		friend struct std::hash<CommandID>;
		friend struct fmt::formatter<CommandID>;
		uint32_t GetID() const noexcept { return ID; };

	public:
		CommandID() : CommandID("") {}
		explicit CommandID(const char* str) { ID = Hash::CRC32(str); }
		static CommandID FromString(const char* sourceString) { return CommandID(sourceString); }

		bool operator==(const CommandID& other) const { return ID == other.ID; }
		bool operator!=(const CommandID& other) const { return !(*this == other); }
		operator uint32_t() const { return GetID(); }

		static CommandID FromUnsignedInt(uint32_t ID)
		{
			CommandID commandID; 
			commandID.ID = ID; 
			return commandID;
		}

		static CommandID InvalidID()
		{ 
			return CommandID("");
		}
	};
} // namespace Beyond::Audio

namespace std
{
	template<>
	struct hash<Beyond::Audio::CommandID>
	{
		size_t operator()(const Beyond::Audio::CommandID& comm) const noexcept
		{
			static_assert(noexcept(hash<uint32_t>()(comm.GetID())), "hash fuction should not throw");
			return hash<uint32_t>()(comm.GetID());
		}
	};
}

namespace fmt
{
	template <>
	struct fmt::formatter<Beyond::Audio::CommandID>
	{
		constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin())
		{
			return ctx.begin();
		}

		template <typename FormatContext>
		auto format(const Beyond::Audio::CommandID& cmd, FormatContext& ctx) -> decltype(ctx.out())
		{
			return fmt::format_to(ctx.out(), "{}", cmd.ID);
		}
	};

}
