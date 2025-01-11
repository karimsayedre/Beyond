#include "pch.h"
#include "Application.h"

#include "Beyond/Renderer/UI/Font.h"

#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include "Beyond/ImGui/Colors.h"

#include "Beyond/Asset/AssetManager.h"
#include "Beyond/Audio/AudioEngine.h"
#include "Beyond/Audio/AudioEvents/AudioCommandRegistry.h"

#include "Input.h"

#include "Beyond/Physics/PhysicsSystem.h"

#include "imgui/imgui_internal.h"

#include "Beyond/Debug/Profiler.h"

#include "Beyond/Editor/EditorApplicationSettings.h"

#include <filesystem>
#include <nfd.hpp>

#include "Memory.h"
#include "Beyond/ImGui/ImGuiLayer.h"
#include "Beyond/Platform/Vulkan/VulkanSwapChain.h"
#include "Beyond/Renderer/Renderer.h"

extern bool g_ApplicationRunning;
extern ImGuiContext* GImGui;
namespace Beyond {

#define BIND_EVENT_FN(fn) std::bind(&Application::##fn, this, std::placeholders::_1)

	Application* Application::s_Instance = nullptr;

	static std::thread::id s_MainThreadID;

	Application::Application(const ApplicationSpecification& specification)
		: m_Specification(specification), m_RenderThread(specification.CoreThreadingPolicy), m_AppSettings("App.hsettings")
	{
		s_Instance = this;
		s_MainThreadID = std::this_thread::get_id();
		m_AppSettings.Deserialize();

		m_RenderThread.Run();

		if (!specification.WorkingDirectory.empty())
			std::filesystem::current_path(specification.WorkingDirectory);

		m_Profiler = hnew PerformanceProfiler();

		Renderer::SetConfig(specification.RenderConfig);

		WindowSpecification windowSpec;
		windowSpec.Title = specification.Name;
		windowSpec.Width = specification.WindowWidth;
		windowSpec.Height = specification.WindowHeight;
		windowSpec.Decorated = specification.WindowDecorated;
		windowSpec.Fullscreen = specification.Fullscreen;
		windowSpec.VSync = specification.VSync;
		windowSpec.IconPath = specification.IconPath;
		m_Window = std::unique_ptr<Window>(Window::Create(windowSpec));
		m_Window->Init();
		m_Window->SetEventCallback([this](Event& e) { OnEvent(e); });

		// Load editor settings (will generate default settings if the file doesn't exist yet)
		EditorApplicationSettingsSerializer::Init();

		BEY_CORE_VERIFY(NFD::Init() == NFD_OKAY);

		// Init renderer and execute command queue to compile all shaders
		Renderer::Init();
		// Render one frame (TODO: maybe make a func called Pump or something)
		m_RenderThread.Pump();

		if (specification.StartMaximized)
			m_Window->Maximize();
		else
			m_Window->CenterWindow();
		m_Window->SetResizable(specification.Resizable);
		
		if (m_Specification.EnableImGui)
		{
			m_ImGuiLayer = ImGuiLayer::Create();
			PushOverlay(m_ImGuiLayer);
		}

		//PhysicsSystem::Init();
		ScriptEngine::Init(specification.ScriptConfig);
		MiniAudioEngine::Init();
		Font::Init();
	}

	Application::~Application()
	{
		NFD::Quit();

		EditorApplicationSettingsSerializer::SaveSettings();

		m_Window->SetEventCallback([](Event& e) {});

		m_RenderThread.Terminate();

		for (Layer* layer : m_LayerStack)
		{
			layer->OnDetach();
			delete layer;
		}

		ScriptEngine::Shutdown();
		Project::SetActive(nullptr);
		Font::Shutdown();
		MiniAudioEngine::Shutdown();

		Renderer::Shutdown();

		delete m_Profiler;
		m_Profiler = nullptr;
	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer)
	{
		m_LayerStack.PushOverlay(layer);
		layer->OnAttach();
	}

	void Application::PopLayer(Layer* layer)
	{
		m_LayerStack.PopLayer(layer);
		layer->OnDetach();
	}

	void Application::PopOverlay(Layer* layer)
	{
		m_LayerStack.PopOverlay(layer);
		layer->OnDetach();
	}

	void Application::RenderImGui()
	{
		BEY_PROFILE_FUNC();
		BEY_SCOPE_PERF("Application::RenderImGui");
		bool changed = false;

		m_ImGuiLayer->Begin();

		for (int i = 0; i < m_LayerStack.Size(); i++)
			changed |= m_LayerStack[i]->OnImGuiRender();

		s_HasChanged = changed;
	}

	void Application::Run()
	{
		OnInit();
		while (m_Running)
		{
			// Wait for render thread to finish frame
			{
				BEY_PROFILE_SCOPE("Wait");
				Timer timer;

				m_RenderThread.BlockUntilRenderComplete();

				m_PerformanceTimers.MainThreadWaitTime = timer.ElapsedMillis();
			}

			static uint64_t frameCounter = 0;
			static uint64_t fpsCounter = 0;
			//BEY_CORE_INFO("-- BEGIN FRAME {0}", frameCounter);
			
			ProcessEvents(); // Poll events when both threads are idle

			m_ProfilerPreviousFrameData = m_Profiler->GetPerFrameData();
			m_Profiler->Clear();

			m_RenderThread.NextFrame();

			// Start rendering previous frame
			m_RenderThread.Kick();

			if (!m_Minimized)
			{
				Timer cpuTimer;

				// On Render thread
				Renderer::Submit([&]()
				{
					m_Window->GetSwapChain().BeginFrame();
				});

				Renderer::BeginFrame();
				{
					BEY_SCOPE_PERF("Application Layer::OnUpdate");
					for (Layer* layer : m_LayerStack)
						layer->OnUpdate(m_TimeStep);
				}

				Ref<Scene> activeScene = ScriptEngine::GetSceneContext();
				if (activeScene)
				{
					m_PerformanceTimers.ScriptUpdate = activeScene->GetPerformanceTimers().ScriptUpdate;
					m_PerformanceTimers.PhysicsStepTime = activeScene->GetPerformanceTimers().PhysicsStep;
				}
			
				// Render ImGui on render thread
				Application* app = this;
				if (m_Specification.EnableImGui)
				{
					Renderer::Submit([app]() { app->RenderImGui(); });
					Renderer::Submit([=]() { m_ImGuiLayer->End(); });
				}
				Renderer::EndFrame();

				// On Render thread
				Renderer::Submit([&]()
				{
					// m_Window->GetSwapChain().BeginFrame();
					 //Renderer::WaitAndRender();
					m_Window->SwapBuffers();
				});

				m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % Renderer::GetConfig().FramesInFlight;
				m_PerformanceTimers.MainThreadWorkTime = cpuTimer.ElapsedMillis();

				fpsCounter++;
			}

			ScriptEngine::InitializeRuntimeDuplicatedEntities();
			Input::ClearReleasedKeys();

			const float time = GetTime();
			m_Frametime = time - m_LastFrameTime;
			m_TimeStep = glm::min<float>(m_Frametime, 0.0333f);
			m_LastFrameTime = time;

			float elapsedTime = time - m_LastFPSFrameTime;
			if (elapsedTime >= 0.1f)
			{
				m_FPS = static_cast<double>(fpsCounter) / elapsedTime;
				fpsCounter = 0;
				m_LastFPSFrameTime = time;
			}

			//BEY_CORE_INFO("-- END FRAME {0}", frameCounter);
			frameCounter++;

			BEY_PROFILE_MARK_FRAME;
		}
		OnShutdown();
	}

	void Application::Close()
	{
		m_Running = false;
	}

	void Application::OnShutdown()
	{
		m_EventCallbacks.clear();
		g_ApplicationRunning = false;
	}

	void Application::ProcessEvents()
	{
		Input::TransitionPressedKeys();
		Input::TransitionPressedButtons();

		m_Window->ProcessEvents();

		std::scoped_lock<std::mutex> lock(m_EventQueueMutex);

		// Process custom event queue
		while (!m_EventQueue.empty())
		{
			auto& func = m_EventQueue.front();
			func();
			m_EventQueue.pop();
		}
	}

	void Application::OnEvent(Event& event)
	{
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) { return OnWindowResize(e); });
		dispatcher.Dispatch<WindowMinimizeEvent>([this](WindowMinimizeEvent& e) { return OnWindowMinimize(e); });
		dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) { return OnWindowClose(e); });

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
		{
			(*--it)->OnEvent(event);
			if (event.Handled)
				break;
		}

		if (event.Handled)
			return;

		// TODO: Should these callbacks be called BEFORE the layers recieve events?
		//				We may actually want that since most of these callbacks will be functions REQUIRED in order for the game
		//				to work, and if a layer has already handled the event we may end up with problems
		for (auto& eventCallback : m_EventCallbacks)
		{
			eventCallback(event);

			if (event.Handled)
				break;
		}

	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		const uint32_t width = e.GetWidth(), height = e.GetHeight();
		if (width == 0 || height == 0)
		{
			//m_Minimized = true;
			return false;
		}
		//m_Minimized = false;
		
		auto& window = m_Window;
		Renderer::Submit([&window, width, height]() mutable
		{
			window->GetSwapChain().OnResize(width, height);
		});

		return false;
	}

	bool Application::OnWindowMinimize(WindowMinimizeEvent& e)
	{
		m_Minimized = e.IsMinimized();
		return false;
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		Close();
		return false; // give other things a chance to react to window close
	}

	float Application::GetTime() const
	{
		return (float)glfwGetTime();
	}

	const char* Application::GetConfigurationName()
	{
#if defined(BEY_DEBUG)
		return "Debug";
#elif defined(BEY_RELEASE)
		return "Release";
#elif defined(BEY_DIST)
		return "Dist";
#else
#error Undefined configuration?
#endif
	}

	const char* Application::GetPlatformName()
	{
#if defined(BEY_PLATFORM_WINDOWS)
		return "Windows x64";
#elif defined(BEY_PLATFORM_LINUX)
		return "Linux";
#else
		return "Unknown"
#endif
	}

	std::thread::id Application::GetMainThreadID() { return s_MainThreadID; }

}
