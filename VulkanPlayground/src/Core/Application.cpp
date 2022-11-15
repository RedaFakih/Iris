#include "Application.h"

#include <glfw/glfw3.h>

namespace vkPlayground {

	Application::Application()
	{
		PG_ASSERT(!s_Instance, "No more than 1 application can be created!");
		s_Instance = this;

		Init();
	}

	Application::~Application()
	{
		// TODO: Some other things maybe?

		Shutdown();

		s_Instance = nullptr;
	}

	void Application::Init()
	{
		WindowSpecification spec = {};
		spec.WindowTitle = "VulkanPlayground";
		spec.Width = 1280;
		spec.Height = 720;
		spec.VSync = false; // Fiddle around with this
		m_Window = Window::Create(spec);
		m_Window->SetEventCallbackFunction([this](Events::Event& e) {Application::OnEvent(e); });
	}

	void Application::Run()
	{
		while (m_Running)
		{
			glfwPollEvents();
		}
	}

	void Application::OnEvent(Events::Event& e)
	{
		Events::EventDispatcher dispatcher(e);

		dispatcher.Dispatch<Events::WindowResizeEvent>([this](Events::WindowResizeEvent& event)
		{
			const uint32_t width = event.GetWidth(), height = event.GetHeight();
			if (width == 0 || height == 0)
			{
				//m_Minimized = true;
				return false;
			}
			//m_Minimized = false;

			m_Window->GetSwapChain().OnResize(width, height);

			return false;
		});

		dispatcher.Dispatch<Events::WindowCloseEvent>([this](Events::WindowCloseEvent&)
		{
			m_Running = false;

			return false;
		});
	}

	void Application::Shutdown()
	{
	}

}