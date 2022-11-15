#include "Window.h"

#include <glfw/glfw3.h>

namespace vkPlayground {

	static bool s_GLFWInitialized = false;

	namespace Utils {

		static void GLFWErrorCallback(int error, const char* description)
		{
			std::cerr << "[GLFW] Error: " << error << " - Description: " << description << std::endl;
		}

	}

	Ref<Window> Window::Create(const WindowSpecification& spec)
	{
		return CreateRef<Window>(spec);
	}

	Window::Window(const WindowSpecification& spec)
		: m_Specification(spec)
	{
		Init();
	}

	Window::~Window()
	{
		Shutdown();
	}

	void Window::Init()
	{
		m_Data.Title = m_Specification.WindowTitle;
		m_Data.Width = m_Specification.Width;
		m_Data.Height = m_Specification.Height;

		if (!s_GLFWInitialized)
		{
			int succes = glfwInit();
			PG_ASSERT(succes, "Failed to initialize glfw");
			glfwSetErrorCallback(Utils::GLFWErrorCallback);

			s_GLFWInitialized = true;
		}

		// TODO: Handle other hints via struct
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // TODO: False for now, since SwapChain is not resizable yet!

		m_Window = glfwCreateWindow(m_Data.Width, m_Data.Height, m_Data.Title.c_str(), nullptr, nullptr);

		// Init the context: VulkanInstance, VulkanDevice, VulkanPhysicalDevice, PipelineCache, ValidationLayers...
		m_RendererContext = Renderer::RendererContext::Create();
		m_RendererContext->Init();
	
		m_SwapChain.Init(Renderer::RendererContext::GetInstance(), m_RendererContext->GetDevice());
		m_SwapChain.InitSurface(m_Window);

		m_SwapChain.Create(&m_Data.Width, &m_Data.Height, m_Specification.VSync);
		glfwSetWindowUserPointer(m_Window, &m_Data);

		bool isRawMouseMotionSupported = glfwRawMouseMotionSupported();
		if (isRawMouseMotionSupported)
			glfwSetInputMode(m_Window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		else
			std::cerr << "[Platform] Raw mouse motion not supported.\n";

		// TODO: GLFW Callbacks for some reason???
		// TODO: Maybe handle a simple event system...
		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
		{
			WindowData& data = *((WindowData*)glfwGetWindowUserPointer(window));

			Events::WindowResizeEvent event(width, height);
			data.EventCallback(event);
			data.Width = (uint32_t)width;
			data.Height = (uint32_t)height;
		});

		glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			Events::WindowCloseEvent event;
		    data.EventCallback(event);
		});
	}

	void Window::Shutdown()
	{
		m_SwapChain.Destroy();

		glfwDestroyWindow(m_Window);

		glfwTerminate();
	}
}