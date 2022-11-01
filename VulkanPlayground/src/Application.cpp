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
		m_Window = Window::Create();
	}

	void Application::Run()
	{
		while (!glfwWindowShouldClose(m_Window->GetNativeWindow()))
		{
			glfwPollEvents();
		}
	}

	void Application::Shutdown()
	{
	}

}