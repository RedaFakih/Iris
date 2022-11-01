#pragma once

#include "Base.h"
#include "Renderer/RendererContext.h"

#include <string>

struct GLFWwindow;

namespace vkPlayground {

	class Window
	{
	public:
		Window(/* TODO: WindowSpecification */);
		~Window();

		static Ref<Window> Create();

		void Init();
		void Shutdown();

		GLFWwindow* GetNativeWindow() const { return m_Window; }

	private:
		GLFWwindow* m_Window;

		Scope<Renderer::RendererContext> m_RendererContext;

		struct WindowData
		{
			std::string Title;

			uint32_t Width;
			uint32_t Height;
		};

		WindowData m_Data;

	};

}