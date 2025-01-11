#pragma once

#include "Beyond/Core/UUID.h"

#include <filesystem>

namespace Beyond {

	struct ProcessInfo
	{
		std::filesystem::path FilePath;
		std::filesystem::path WorkingDirectory = "";
		std::string CommandLine = "";
		bool Detached = false;
		bool IncludeFilePathInCommands = true;
		bool WaitToFinish = false;
	};

	class ProcessHelper
	{
	public:
		static UUID CreateProcess(const ProcessInfo& inProcessInfo);
		static void DestroyProcess(UUID inHandle, uint32_t inExitCode = 0);
	};

}
