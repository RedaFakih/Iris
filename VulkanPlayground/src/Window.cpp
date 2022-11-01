#include "Window.h"

#include <glfw/glfw3.h>

namespace vkPlayground {

	static bool s_GLFWInitialized = false;

	static void GLFWErrorCallback(int error, const char* description)
	{
		std::cerr << error << "\n" << description << std::endl;
	}

	Ref<Window> Window::Create()
	{
		return CreateRef<Window>();
	}

	Window::Window(/* TODO: WindowSpecification */)
	{
		Init();
	}

	Window::~Window()
	{
		Shutdown();
	}

	void Window::Init()
	{
		m_Data.Title = "vkPlayground";
		m_Data.Width = 1280;
		m_Data.Height = 720;

		if (!s_GLFWInitialized)
		{
			int succes = glfwInit();
			PG_ASSERT(succes, "Failed to initialize glfw");
			glfwSetErrorCallback(GLFWErrorCallback);

			s_GLFWInitialized = true;
		}

		// TODO: Handle other hints via struct
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // TODO: False for now, since SwapChain is not resizable yet!

		m_Window = glfwCreateWindow(m_Data.Width, m_Data.Height, m_Data.Title.c_str(), nullptr, nullptr);

		// Init the context: VulkanInstance, VulkanDevice, VulkanPhysicalDevice, PipelineCache, ValidationLayers...
		m_RendererContext = Renderer::RendererContext::Create();
		m_RendererContext->Init();
	}

	void Window::Shutdown()
	{
		glfwDestroyWindow(m_Window);

		glfwTerminate();
	}
}