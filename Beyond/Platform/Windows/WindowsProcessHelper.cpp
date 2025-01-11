#include "pch.h"
#include "Beyond/Utilities/ProcessHelper.h"

// NOTE: codecvt has *technically* been deprecated, but the C++ committee has said it's still safe and portable to use until a suitable replacement has been found
#include <codecvt>
#include <simdutf.h>

namespace Beyond {

	static std::unordered_map<UUID, PROCESS_INFORMATION> s_WindowsProcessStorage;

	UUID ProcessHelper::CreateProcess(const ProcessInfo& inProcessInfo)
	{
		std::filesystem::path workingDirectory = inProcessInfo.WorkingDirectory.empty() ? inProcessInfo.FilePath.parent_path() : inProcessInfo.WorkingDirectory;

		std::wstring commandLine = inProcessInfo.IncludeFilePathInCommands ? inProcessInfo.FilePath.wstring() : L"";

		if (!inProcessInfo.CommandLine.empty())
		{
			if (commandLine.size() > 0)
			{
				//std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> wstringConverter;
				std::wstring utf16 = L" ";
				utf16.resize(1 + inProcessInfo.CommandLine.size());
				simdutf::convert_utf8_to_utf16le(inProcessInfo.CommandLine.c_str(), inProcessInfo.CommandLine.size(), (char16_t*)utf16.data() + 1);
				//commandLine += L" " + wstringConverter.from_bytes(inProcessInfo.CommandLine);
				commandLine += utf16;
			}
			else
			{
				std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> wstringConverter;
				commandLine = wstringConverter.from_bytes(inProcessInfo.CommandLine);
			}
		}

		PROCESS_INFORMATION processInformation;
		ZeroMemory(&processInformation, sizeof(PROCESS_INFORMATION));

		STARTUPINFO startupInfo;
		ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
		startupInfo.cb = sizeof(STARTUPINFO);

		DWORD creationFlags = NORMAL_PRIORITY_CLASS;

		if (inProcessInfo.Detached)
			creationFlags |= DETACHED_PROCESS;

		BOOL success = ::CreateProcess(
			inProcessInfo.FilePath.c_str(), commandLine.data(),
			NULL, NULL, FALSE, creationFlags, NULL,
			workingDirectory.c_str(), &startupInfo, &processInformation);

		if (!success)
		{
			CloseHandle(processInformation.hThread);
			CloseHandle(processInformation.hProcess);
			return 0;
		}

		if(inProcessInfo.WaitToFinish)
		{
			WaitForSingleObject(processInformation.hProcess, 20000);
		}

		UUID processID = UUID();

		if (inProcessInfo.Detached)
		{
			CloseHandle(processInformation.hThread);
			CloseHandle(processInformation.hProcess);
		}
		else
		{
			s_WindowsProcessStorage[processID] = processInformation;
		}

		return processID;
	}

	void ProcessHelper::DestroyProcess(UUID inHandle, uint32_t inExitCode)
	{
		BEY_CORE_VERIFY(s_WindowsProcessStorage.find(inHandle) != s_WindowsProcessStorage.end(), "Trying to destroy untracked process!");
		const auto& processInformation = s_WindowsProcessStorage[inHandle];
		TerminateProcess(processInformation.hProcess, inExitCode);
		CloseHandle(processInformation.hThread);
		CloseHandle(processInformation.hProcess);
		s_WindowsProcessStorage.erase(inHandle);
	}

}
