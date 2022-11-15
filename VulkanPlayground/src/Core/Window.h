#pragma once

#include "Base.h"
#include "AppEvents.h"
#include "Renderer/RendererContext.h"
#include "Renderer/SwapChain.h"

#include <string>
#include <functional>

struct GLFWwindow;

namespace vkPlayground {

	struct WindowSpecification
	{
		std::string WindowTitle;
		uint32_t Width;
		uint32_t Height;
		bool VSync;
	};

	class Window
	{
	public:
		using EventCallbackFn = std::function<void(Events::Event&)>;

	public:
		Window(const WindowSpecification& spec);
		~Window();

		static Ref<Window> Create(const WindowSpecification& spec);

		void Init();
		void Shutdown();

		void SetEventCallbackFunction(const EventCallbackFn& function) { m_Data.EventCallback = function; }

		Renderer::SwapChain GetSwapChain() { return m_SwapChain; }
		GLFWwindow* GetNativeWindow() const { return m_Window; }

	private:
		GLFWwindow* m_Window;
		WindowSpecification m_Specification;

		Scope<Renderer::RendererContext> m_RendererContext;
		Renderer::SwapChain m_SwapChain;

		struct WindowData
		{
			std::string Title;

			uint32_t Width;
			uint32_t Height;

			EventCallbackFn EventCallback;
		};

		WindowData m_Data;

	};

}