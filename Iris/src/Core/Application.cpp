#include "IrisPCH.h"
#include "Application.h"

#include "Input/Input.h"
#include "Project/Project.h"
#include "Renderer/Renderer.h"
#include "Renderer/StorageBufferSet.h"
#include "Renderer/Text/Font.h"
#include "Renderer/UniformBufferSet.h"

#include <glfw/glfw3.h>
#include <nfd.hpp>

namespace Iris {

	Application::Application(const ApplicationSpecification& spec)
		: m_Specification(spec), m_RenderThread(spec.CoreThreadingPolicy)
	{
		IR_VERIFY(!s_Instance, "No more than 1 application can be created!");
		s_Instance = this;

		s_MainThreadID = std::this_thread::get_id();

		m_RenderThread.Run();

		if (!spec.WorkingDirectory.empty())
			std::filesystem::current_path(spec.WorkingDirectory);

		WindowSpecification windowSpec = {
			.Title = spec.Name,
			.Width = spec.WindowWidth,
			.Height = spec.WindowHeight,
			.Decorated = spec.WindowDecorated,
			.VSync = spec.VSync,
			.FullScreen = spec.FullScreen,
			.IconPath = spec.IconPath
		};
		m_Window = Window::Create(windowSpec);
		m_Window->Init();
		m_Window->SetEventCallbackFunction([](Events::Event& e) { s_Instance->Application::OnEvent(e); });

		IR_VERIFY(NFD::Init() == NFD_OKAY);

		Renderer::SetConfig(spec.RendererConfig);
		Renderer::Init();

		if (spec.StartMaximized)
			m_Window->Maximize();
		else
			m_Window->CentreWindow();

		m_Window->SetResizable(spec.Resizable);

		if (m_Specification.EnableImGui)
		{
			m_ImGuiLayer = ImGuiLayer::Create();
			PushOverlay(m_ImGuiLayer);
		}

		Font::Init();

		// Render one frame
		m_RenderThread.Pump();
	}

	Application::~Application()
	{
		NFD::Quit();

		m_Window->SetEventCallbackFunction([](Events::Event& e) { (void)e; });

		m_RenderThread.Terminate();

		// Execute any remaining commands from before we clear the layers... then we do that again after we submit commands from the layer clearing
		Renderer::ExecuteAllRenderCommandQueues();

		vkDeviceWaitIdle(RendererContext::GetCurrentDevice()->GetVulkanDevice());

		for (Layer* layer : m_LayerStack)
		{
			layer->OnDetach();
			delete layer;
		}

		Project::SetActive(nullptr);
		Font::Shutdown();

		Renderer::Shutdown();

		// NOTE: We can't set the s_Instance to nullptr here since the application will still be used in other parts of the application to
		// retrieve certain data to destroy other data...
		// s_Instance = nullptr;
	}

	void Application::Run()
	{
		OnInit();
		while (m_Running)
		{
			// Wait for render completion
			{
				Timer timer;

				m_RenderThread.BlockUntilRenderComplete();

				timer.ElapsedMillis();
			}

			// static uint64_t frameCounter = 0;

			ProcessEvents();

			m_RenderThread.NextFrame();

			// Start rendering previous frame
			m_RenderThread.Kick();

			if (!m_Minimized)
			{
				Application* app = this;
				Renderer::Submit([app]()
				{
					app->m_Window->GetSwapChain().BeginFrame();
				});

				Renderer::BeginFrame();

				{
					for (Layer* layer : m_LayerStack)
						layer->OnUpdate(m_TimeStep);
				}

				if (m_Specification.EnableImGui)
				{
					Renderer::Submit([app]()
					{
						app->RenderImGui();
					});
				}

				Renderer::EndFrame();

				Renderer::Submit([app]()
				{
					app->m_Window->SwapBuffers();
				});

				m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % Renderer::GetConfig().FramesInFlight;
			}

			Input::ClearReleasedKeys();

			float time = GetTime();
			m_FrameTime = time - m_LastFrameTime;
			m_TimeStep = glm::min<float>(m_FrameTime, 0.0333f);
			m_LastFrameTime = time;

			// ++frameCounter;
		}
		OnShutdown();
	}

	void Application::Close()
	{
		m_Running = false;
	}

	void Application::ProcessEvents()
	{
		Input::TransitionPressedKeys();
		Input::TransitionPressedButtons();
		
		m_Window->ProcessEvents();

		// NOTE: In case we were to run the events on a different thread
		std::scoped_lock<std::mutex> eventLock(m_EventQueueMutex);

		while (m_EventQueue.size())
		{
			auto& event = m_EventQueue.front();
			event();
			m_EventQueue.pop();
		}
	}

	void Application::OnEvent(Events::Event& e)
	{
		Events::EventDispatcher dispatcher(e);

		dispatcher.Dispatch<Events::WindowResizeEvent>([this](Events::WindowResizeEvent& event) { return OnWindowResize(event); });
		dispatcher.Dispatch<Events::WindowCloseEvent>([this](Events::WindowCloseEvent& event) { return OnWindowClose(event); });
		dispatcher.Dispatch<Events::WindowMinimizeEvent>([this](Events::WindowMinimizeEvent& event) { return OnWindowMinimize(event); });

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();)
		{
			(*--it)->OnEvent(e);
			if (e.Handled)
				break;
		}

		if (e.Handled)
			return;
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
		m_ImGuiLayer->Begin();

		for (int i = 0; i < m_LayerStack.GetSize(); i++)
			m_LayerStack[i]->OnImGuiRender();

		m_ImGuiLayer->End();
	}

	bool Application::OnWindowResize(Events::WindowResizeEvent& e)
	{
		const uint32_t width = e.GetWidth(), height = e.GetHeight();
		if (width == 0 || height == 0)
		{
			// m_Minimized = true; No need to set this here since there is no way the window can be minimized to (0, 0) (otherwise would crash) unless it was `iconified` and that event is handled separately
			return false;
		}

		Application* app = this;
		Renderer::Submit([app, width, height]()
		{
			app->m_Window->GetSwapChain().OnResize(width, height);
		});

		return false;
	}

	bool Application::OnWindowClose(Events::WindowCloseEvent& e)
	{
		(void)e;

		m_Running = false;

		return false;
	}

	bool Application::OnWindowMinimize(Events::WindowMinimizeEvent& e)
	{
		m_Minimized = e.IsMinimized();

		return false;
	}

	float Application::GetTime() const
	{
		return static_cast<float>(glfwGetTime());
	}

	const char* Application::GetConfigurationName()
	{
#if defined(IR_CONFIG_DEBUG)
		return "Debug";
#elif defined(IR_CONFIG_RELEASE)
		return "Release";
#else
#error Unknown Configuration
#endif
	}

}