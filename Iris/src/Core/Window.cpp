#include "IrisPCH.h"
#include "Window.h"

#include "Core/Application.h"
#include "Input/Input.h"
#include "Renderer/Core/VulkanAllocator.h"

#include <glfw/glfw3.h>
#include <imgui/imgui.h>
#include <stb_image/stb_image.h>

namespace Iris {

#include "Embed/IrisIcon.embed"

	static bool s_GLFWInitialized = false;

	namespace Utils {

		static void GLFWErrorCallback(int error, const char* description)
		{
			IR_CORE_ERROR_TAG("Window", "GLFW: {0} - Description: {1}", error, description);
		}

	}

	Scope<Window> Window::Create(const WindowSpecification& spec)
	{
		return CreateScope<Window>(spec);
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
			IR_VERIFY(succes, "Failed to initialize glfw");
			glfwSetErrorCallback(Utils::GLFWErrorCallback);

			s_GLFWInitialized = true;
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		if (!m_Specification.Decorated)
		{
			// This removes titlebar
			 glfwWindowHint(GLFW_TITLEBAR, false);
		}

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
			m_Window = glfwCreateWindow(static_cast<int>(m_Data.Width), static_cast<uint32_t>(m_Data.Height), m_Data.Title.c_str(), nullptr, nullptr);
		}

		// Set icon
		{
			GLFWimage icon{};
			int channels;
			if (m_Specification.IconPath.empty())
			{				
				// Embedded Iris icon
				icon.pixels = stbi_load_from_memory(g_IrisIconPNG, sizeof(g_IrisIconPNG), &icon.width, &icon.height, &channels, STBI_rgb_alpha);
				glfwSetWindowIcon(m_Window, 1, &icon);
				stbi_image_free(icon.pixels);
			}
			else
			{
				std::string iconPathStr = m_Specification.IconPath.string();
				icon.pixels = stbi_load(iconPathStr.c_str(), &icon.width, &icon.height, &channels, STBI_rgb_alpha);
				glfwSetWindowIcon(m_Window, 1, &icon);
				stbi_image_free(icon.pixels);
			}
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
			IR_CORE_WARN_TAG("Window", "Raw mouse motion not supported.");

		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
		{
			WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

			Events::WindowResizeEvent event(width, height);
			data.EventCallback(event);
			data.Width = static_cast<uint32_t>(width);
			data.Height = static_cast<uint32_t>(height);
		});

		glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
		{
			WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

			Events::WindowCloseEvent event;
		    data.EventCallback(event);
		});

		glfwSetWindowIconifyCallback(m_Window, [](GLFWwindow* window, int iconified)
		{
			WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

			Events::WindowMinimizeEvent event(static_cast<bool>(iconified));
			data.EventCallback(event);
		});

		glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			(void)scancode, (void)mods;
			WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

			switch (action)
			{
				case GLFW_PRESS:
				{
					Input::UpdateKeyState((KeyCode)key, KeyState::Pressed);
					Events::KeyPressedEvent event(static_cast<KeyCode>(key), 0);
					data.EventCallback(event);
					break;
				}
				case GLFW_RELEASE:
				{
					Input::UpdateKeyState((KeyCode)key, KeyState::Released);
					Events::KeyReleasedEvent event(static_cast<KeyCode>(key));
					data.EventCallback(event);
					break;
				}
				case GLFW_REPEAT:
				{
					Input::UpdateKeyState((KeyCode)key, KeyState::Held);
					Events::KeyPressedEvent event(static_cast<KeyCode>(key), true);
					data.EventCallback(event);
					break;
				}
			}
		});

		glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode)
		{
			WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

			Events::KeyTypedEvent event(static_cast<KeyCode>(keycode));
			
			data.EventCallback(event);
		});

		glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
		{
			(void)mods;
			WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

			switch (action)
			{
				case GLFW_PRESS:
				{
					Input::UpdateButtonState(static_cast<MouseButton>(button), KeyState::Pressed);
					Events::MouseButtonPressedEvent event(static_cast<MouseButton>(button));
					data.EventCallback(event);
					break;
				}
				case GLFW_RELEASE:
				{
					Input::UpdateButtonState(static_cast<MouseButton>(button), KeyState::Released);
					Events::MouseButtonReleasedEvent event(static_cast<MouseButton>(button));
					data.EventCallback(event);
					break;
				}
			}
		});

		glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

			Events::MouseScrolledEvent event(static_cast<float>(xOffset), static_cast<float>(yOffset));
			data.EventCallback(event);
		});

		glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos)
		{
			WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

			Events::MouseMovedEvent event(static_cast<float>(xPos), static_cast<float>(yPos));
			data.EventCallback(event);
		});

		glfwSetTitleBarHitTestCallback(m_Window, [](GLFWwindow* window, int x, int y, int* hit)
		{
			auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
		
			Events::WindowTitleBarHitTestEvent event(x, y, *hit);
			data.EventCallback(event);
		});

		m_PrimaryMonitor = glfwGetPrimaryMonitor();

		m_ImGuiMouseCursors[ImGuiMouseCursor_Arrow]      = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		m_ImGuiMouseCursors[ImGuiMouseCursor_TextInput]  = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
		m_ImGuiMouseCursors[ImGuiMouseCursor_ResizeAll]  = glfwCreateStandardCursor(GLFW_ARROW_CURSOR); // GLFW Does not have this
		m_ImGuiMouseCursors[ImGuiMouseCursor_ResizeNS]   = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
		m_ImGuiMouseCursors[ImGuiMouseCursor_ResizeEW]   = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
		m_ImGuiMouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR); // GLFW Does not have this
		m_ImGuiMouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR); // GLFW Does not have this
		m_ImGuiMouseCursors[ImGuiMouseCursor_Hand]       = glfwCreateStandardCursor(GLFW_HAND_CURSOR);

		// Update window to actual size
		{
			int width, height;
			glfwGetWindowSize(m_Window, &width, &height);
			m_Data.Width = static_cast<uint32_t>(width);
			m_Data.Height = static_cast<uint32_t>(height);
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
		const GLFWvidmode* vidMode = glfwGetVideoMode(m_PrimaryMonitor);
		int x = (vidMode->width / 2) - (m_Data.Width / 2);
		int y = (vidMode->height / 2) - (m_Data.Height / 2);
		glfwSetWindowPos(m_Window, x, y);
	}

	bool Window::IsMaximized() const
	{
		return static_cast<bool>(glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED));
	}

	bool Window::IsFullScreen() const
	{
		return static_cast<bool>(glfwGetWindowMonitor(m_Window) != nullptr);
	}

	void Window::SetTitle(const std::string& title)
	{
		m_Data.Title = title;
		glfwSetWindowTitle(m_Window, title.c_str());
	}
}