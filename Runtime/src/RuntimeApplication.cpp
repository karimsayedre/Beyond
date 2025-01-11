#include <Beyond.h>
#include <Beyond/EntryPoint.h>

#include "RuntimeLayer.h"

#include "Beyond/Renderer/RendererAPI.h"

#include "Beyond/Tiering/TieringSerializer.h"

class RuntimeApplication : public Beyond::Application
{
public:
	RuntimeApplication(const Beyond::ApplicationSpecification& specification, std::string_view projectPath)
		: Application(specification)
		, m_ProjectPath(projectPath)
	{
		s_IsRuntime = true;
	}

	virtual void OnInit() override
	{
		PushLayer(new Beyond::RuntimeLayer(m_ProjectPath));
	}

private:
	std::string m_ProjectPath;
};

Beyond::Application* Beyond::CreateApplication(int argc, char** argv)
{
	std::string_view projectPath = "SandboxProject/Sandbox.hproj";
	if (argc > 1)
	{
		projectPath = argv[1];
	}

	Beyond::ApplicationSpecification specification;
	specification.Fullscreen = true;

	specification.Name = "Beyond Runtime";
	specification.WindowWidth = 1920;
	specification.WindowHeight = 1080;
	specification.WindowDecorated = !specification.Fullscreen;
	specification.Resizable = !specification.Fullscreen;
	specification.StartMaximized = false;
	specification.EnableImGui = false;
	//specification.IconPath = "";

	// IMPORTANT: Disable for ACTUAL Dist builds'
#ifndef BEY_DIST
	//specification.WorkingDirectory = "../Editor";
#endif

	specification.ScriptConfig.CoreAssemblyPath = "Resources/Scripts/ScriptCore.dll";
	specification.ScriptConfig.EnableDebugging = false;
	specification.ScriptConfig.EnableProfiling = false;

	specification.RenderConfig.ShaderPackPath = "Resources/ShaderPack.hsp";
	specification.RenderConfig.FramesInFlight = 3;

	specification.CoreThreadingPolicy = ThreadingPolicy::SingleThreaded;

	return new RuntimeApplication(specification, projectPath);
}
