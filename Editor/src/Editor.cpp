#include "EditorLayer.h"
#include "Beyond/Utilities/FileSystem.h"
#include "Beyond/Utilities/CommandLineParser.h"

#include "Beyond/EntryPoint.h"

#ifdef BEY_PLATFORM_WINDOWS
#include <Shlobj.h>
#endif

class EditorApplication : public Beyond::Application
{
public:
	EditorApplication(const Beyond::ApplicationSpecification& specification, std::string_view projectPath)
		: Application(specification), m_ProjectPath(projectPath), m_UserPreferences(Beyond::Ref<Beyond::UserPreferences>::Create())
	{
		if (projectPath.empty())
			m_ProjectPath = "SandboxProject/Sandbox.hproj";
	}

	virtual void OnInit() override
	{
		// Persistent Storage
		{
			m_PersistentStoragePath = Beyond::FileSystem::GetPersistentStoragePath() / "Editor";

			if (!std::filesystem::exists(m_PersistentStoragePath))
				std::filesystem::create_directory(m_PersistentStoragePath);
		}

		// User Preferences
		{
			Beyond::UserPreferencesSerializer serializer(m_UserPreferences);
			if (!std::filesystem::exists(m_PersistentStoragePath / "UserPreferences.yaml"))
				serializer.Serialize(m_PersistentStoragePath / "UserPreferences.yaml");
			else
				serializer.Deserialize(m_PersistentStoragePath / "UserPreferences.yaml");

			if (!m_ProjectPath.empty())
				m_UserPreferences->StartupProject = m_ProjectPath;
			else if (!m_UserPreferences->StartupProject.empty())
				m_ProjectPath = m_UserPreferences->StartupProject;
		}

		// Update the Beyond_DIR environment variable every time we launch
		{
			std::filesystem::path workingDirectory = std::filesystem::current_path();

			if (workingDirectory.stem().string() == "Editor")
				workingDirectory = workingDirectory.parent_path();

			Beyond::FileSystem::SetEnvironmentVariable("Beyond_DIR", workingDirectory.string());
		}

		PushLayer(new Beyond::EditorLayer(m_UserPreferences));
	}

private:
	std::string m_ProjectPath;
	std::filesystem::path m_PersistentStoragePath;
	Beyond::Ref<Beyond::UserPreferences> m_UserPreferences;
};

Beyond::Application* Beyond::CreateApplication(int argc, char** argv)
{
	Beyond::CommandLineParser cli(argc, argv);

	auto raw = cli.GetRawArgs();
	if(raw.size() > 1) {
		BEY_CORE_WARN("More than one project path specified, using `{}'", raw[0]);
	}

	auto cd = cli.GetOpt("C");
	if(!cd.empty()) {
		if(_chdir(cd.data()) == -1) {
			BEY_CORE_WARN("Failed to change directory to `{}': {}", cd, strerror(errno));
		}
	}

	std::string_view projectPath;
	if(!raw.empty()) projectPath = raw[0];

	Beyond::ApplicationSpecification specification;
	specification.Name = "Editor";
	specification.WindowWidth = 1600;
	specification.WindowHeight = 900;
	specification.StartMaximized = true;
	specification.VSync = true;
	//specification.RenderConfig.ShaderPackPath = "Resources/ShaderPack.hsp";

	specification.ScriptConfig.CoreAssemblyPath = "Resources/Scripts/ScriptCore.dll";

#ifdef BEY_PLATFORM_WINDOWS
	specification.ScriptConfig.EnableDebugging = true;
	specification.ScriptConfig.EnableProfiling = true;
#else
	specification.ScriptConfig.EnableDebugging = false;
	specification.ScriptConfig.EnableProfiling = false;
#endif

	specification.CoreThreadingPolicy = ThreadingPolicy::SingleThreaded;

	return new EditorApplication(specification, projectPath);
}
