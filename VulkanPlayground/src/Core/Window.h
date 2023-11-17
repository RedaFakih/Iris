#pragma once

#include "Base.h"
#include "AppEvents.h"
#include "Renderer/Core/RendererContext.h"
#include "Renderer/Core/SwapChain.h"

#include <string>
#include <functional>

struct GLFWwindow;

namespace vkPlayground {

	struct WindowSpecification
	{
		std::string Title = "Vulkan Playground";
		uint32_t Width = 1600;
		uint32_t Height = 900;
		bool VSync = true;
		bool FullScreen = false;
	};

	class Window
	{
	public:
		using EventCallbackFn = std::function<void(Events::Event&)>;

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
		Ref<RendererContext> GetRendererContext() { return m_RendererContext; }

		inline uint32_t GetWidth() const { return m_Data.Width; }
		inline uint32_t GetHeight() const { return m_Data.Height; }
		inline std::pair<uint32_t, uint32_t> GetSize() const { return { m_Data.Width, m_Data.Height }; }

		void SetVSync(bool vsync);
		bool IsVSync() const { return m_Specification.VSync; }
		void SetResizable(bool resizable) const;

		void Maximize();
		void CentreWindow();

		const std::string& GetTitle() const { return m_Data.Title; }
		const void SetTitle(const std::string& title) { m_Data.Title = title; }

	private:
		GLFWwindow* m_Window = nullptr;
		WindowSpecification m_Specification;

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