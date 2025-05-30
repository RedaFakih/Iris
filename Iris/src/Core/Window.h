#pragma once

#include "Base.h"
#include "Events/AppEvents.h"
#include "Events/KeyEvents.h"
#include "Events/MouseEvents.h"
#include "Renderer/Core/RendererContext.h"
#include "Renderer/Core/SwapChain.h"

#include <filesystem>
#include <functional>
#include <string>

struct GLFWwindow;
struct GLFWcursor;
struct GLFWmonitor;

namespace Iris {

	/*
	 * For custom title bar:
	 * References: 
	 *		- https://www.youtube.com/watch?v=-NJDxf4XwlQ (1)
	 *		- https://github.com/grassator/win32-window-custom-titlebar (2)
	 *		- https://kubyshkin.name/posts/win32-window-custom-title-bar-caption/ (3)
	 *		- https://www.youtube.com/watch?v=-NJDxf4XwlQ (4)
	 */

	struct WindowSpecification
	{
		std::string Title = "Iris";
		uint32_t Width = 1600;
		uint32_t Height = 900;
		bool Decorated = true;
		bool VSync = true;
		bool FullScreen = false;
		std::filesystem::path IconPath;
	};

	class Window
	{
	public:
		using EventCallbackFn = void(*)(Events::Event&);

	public:
		Window(const WindowSpecification& spec);
		~Window();

		static Scope<Window> Create(const WindowSpecification& spec);

		void Init();
		void Shutdown();

		void ProcessEvents();
		void SwapBuffers();
		void SetEventCallbackFunction(const EventCallbackFn& function) { m_Data.EventCallback = function; }

		SwapChain& GetSwapChain() { return m_SwapChain; }
		GLFWwindow* GetNativeWindow() const { return m_Window; }
		GLFWmonitor* GetNativePrimaryMonitor() const { return m_PrimaryMonitor; }
		Ref<RendererContext> GetRendererContext() { return m_RendererContext; }

		inline uint32_t GetWidth() const { return m_Data.Width; }
		inline uint32_t GetHeight() const { return m_Data.Height; }
		inline std::pair<uint32_t, uint32_t> GetSize() const { return { m_Data.Width, m_Data.Height }; }

		void SetVSync(bool vsync);
		bool IsVSync() const { return m_Specification.VSync; }
		void SetResizable(bool resizable) const;

		void Maximize();
		void CentreWindow();

		bool IsMaximized() const;
		bool IsFullScreen() const;

		const std::string& GetTitle() const { return m_Data.Title; }
		void SetTitle(const std::string& title);

	private:
		GLFWwindow* m_Window = nullptr;
		GLFWmonitor* m_PrimaryMonitor = nullptr;
		WindowSpecification m_Specification;

		GLFWcursor* m_ImGuiMouseCursors[8] = { 0 };

		struct WindowData
		{
			std::string Title;

			uint32_t Width;
			uint32_t Height;

			EventCallbackFn EventCallback;
		};

		WindowData m_Data;

		Ref<RendererContext> m_RendererContext;
		SwapChain m_SwapChain;

	};

}