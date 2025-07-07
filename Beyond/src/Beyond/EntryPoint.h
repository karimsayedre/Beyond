#pragma once

#ifdef BEY_DEBUG
#	include <LivePP/API/x64/LPP_API_x64_CPP.h>
#endif

extern Beyond::Application* Beyond::CreateApplication(int argc, char** argv);
inline bool					g_ApplicationRunning = true;

namespace Beyond
{

	int Main(int argc, char** argv)
	{
#ifdef BEY_DEBUG
		// create a default agent, loading the Live++ agent from the given path, e.g. "ThirdParty/LivePP"
		lpp::LppProjectPreferences preferences					   = lpp::LppCreateDefaultProjectPreferences();
		preferences.hotReload.callCompileHooksForHaltedProcesses   = true;
		preferences.hotReload.callHotReloadHooksForHaltedProcesses = true;
		preferences.hotReload.callLinkHooksForHaltedProcesses	   = true;
		std::filesystem::path projectPath						   = std::filesystem::current_path() / L"..\\Beyond\\vendor\\LivePP";
		lpp::LppDefaultAgent  lppAgent							   = lpp::LppCreateDefaultAgentWithPreferences(nullptr, projectPath.c_str(), &preferences);

		// bail out in case the agent is not valid
		if (!lpp::LppIsValidDefaultAgent(&lppAgent))
		{
			BEY_CORE_FATAL("Lpp is invalid!");
			return 1;
		}

		// enable Live++ for all loaded modules
		lppAgent.EnableModule(lpp::LppGetCurrentModulePath(), lpp::LPP_MODULES_OPTION_ALL_IMPORT_MODULES, nullptr, nullptr);
#endif

		while (g_ApplicationRunning)
		{
			InitializeCore();
			Application* app = CreateApplication(argc, argv);
			BEY_CORE_ASSERT(app, "Client Application is null!");
			app->Run();
			delete app;
			ShutdownCore();
		}

#ifdef BEY_DEBUG
		// destroy the Live++ agent
		lpp::LppDestroyDefaultAgent(&lppAgent);
#endif

		return 0;
	}

} // namespace Beyond

#if BEY_DIST && BEY_PLATFORM_WINDOWS

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	return Beyond::Main(__argc, __argv);
}

#else

int main(int argc, char** argv)
{
	return Beyond::Main(argc, argv);
}

#endif // BEY_DIST
