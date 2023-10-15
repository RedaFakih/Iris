#include "Window.h"

#include "Core/Application.h"
#include "Renderer/Core/VulkanAllocator.h"

#include <glfw/glfw3.h>

namespace vkPlayground {

	static bool s_GLFWInitialized = false;

	namespace Utils {

		static void GLFWErrorCallback(int error, const char* description)
		{
			PG_CORE_ERROR_TAG("GLFW", "{0} - Description: {1}", error, description);
		}

	}

	Ref<Window> Window::Create(const WindowSpecification& spec)
	{
		return CreateRef<Window>(spec);
	}

	Window::Window(const WindowSpecification& spec)
		: m_Specification(spec)
	{
	}

	Window::~Window()
	{
		Shutdown();
	}

	void Window::Init()
	{
		m_Data.Title = m_Specification.Title;
		m_Data.Width = m_Specification.Width;
		m_Data.Height = m_Specification.Height;

		if (!s_GLFWInitialized)
		{
			int succes = glfwInit();
			PG_ASSERT(succes, "Failed to initialize glfw");
			glfwSetErrorCallback(Utils::GLFWErrorCallback);

			s_GLFWInitialized = true;
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		if (m_Specification.FullScreen)
		{
			GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
			const GLFWvidmode* vidMode = glfwGetVideoMode(primaryMonitor);

			glfwWindowHint(GLFW_DECORATED, false); // NOTE: This removes the titlebar and everything from the window
			glfwWindowHint(GLFW_RED_BITS, vidMode->redBits);
			glfwWindowHint(GLFW_GREEN_BITS, vidMode->greenBits);
			glfwWindowHint(GLFW_BLUE_BITS, vidMode->blueBits);
			glfwWindowHint(GLFW_REFRESH_RATE, vidMode->refreshRate);

			m_Window = glfwCreateWindow(vidMode->width, vidMode->height, m_Data.Title.c_str(), primaryMonitor, nullptr);
		}
		else
		{
			m_Window = glfwCreateWindow((int)m_Data.Width, (int)m_Data.Height, m_Data.Title.c_str(), nullptr, nullptr);
		}

		// Init the context: VulkanInstance, VulkanDevice, VulkanPhysicalDevice, PipelineCache, ValidationLayers...
		m_RendererContext = RendererContext::Create();
		m_RendererContext->Init();
	
		m_SwapChain.Init(RendererContext::GetInstance(), m_RendererContext->GetDevice());
		m_SwapChain.InitSurface(m_Window);

		m_SwapChain.Create(&m_Data.Width, &m_Data.Height, m_Specification.VSync);
		glfwSetWindowUserPointer(m_Window, &m_Data);

		bool isRawMouseMotionSupported = glfwRawMouseMotionSupported();
		if (isRawMouseMotionSupported)
			glfwSetInputMode(m_Window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		else
			PG_CORE_WARN_TAG("GLFW", "Raw mouse motion not supported.");

		// TODO: Typing and clicking events...?
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

		glfwSetWindowIconifyCallback(m_Window, [](GLFWwindow* window, int iconified)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			Events::WindowMinimizeEvent event((bool)iconified);
			data.EventCallback(event);
		});

		// Update window to actual size
		{
			int width, height;
			glfwGetWindowSize(m_Window, &width, &height);
			m_Data.Width = width;
			m_Data.Height = height;
		}
	}

	void Window::Shutdown()
	{
		m_SwapChain.Destroy();

		glfwDestroyWindow(m_Window);

		// NOTE: We destroy the allocator here since it needs the device to destroy all its allocated memory
		VulkanAllocator::Shutdown();

		// NOTE: We destroy the Device here before the RendererContext gets destroyed by the Window destructor since the 
		// Device->Destroy() asks for the context
		m_RendererContext->GetDevice()->Destroy();

		glfwTerminate();
		s_GLFWInitialized = false;
	}

	void Window::ProcessEvents()
	{
		glfwPollEvents();
	}

	void Window::SwapBuffers()
	{
		m_SwapChain.Present();
	}

	void Window::SetVSync(bool vsync)
	{
		m_Specification.VSync = vsync;

		// Queue the event to happen at the beggining of the next frame since we dont want to destroy the swapchain mid-rendering stuff
		// And before we call `SwapChain::BeginFrame`.
		Application::Get().QueueEvent([&]()
		{
			m_SwapChain.SetVSync(m_Specification.VSync);
			m_SwapChain.OnResize(m_Specification.Width, m_Specification.Height);
		});
	}

	void Window::SetResizable(bool resizable) const
	{
		glfwSetWindowAttrib(m_Window, GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);
	}

	void Window::Maximize()
	{
		glfwMaximizeWindow(m_Window);
	}

	void Window::CentreWindow()
	{
		const GLFWvidmode* vidMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
		int x = (vidMode->width / 2) - (m_Data.Width / 2);
		int y = (vidMode->height / 2) - (m_Data.Height / 2);
		glfwSetWindowPos(m_Window, x, y);
	}
}