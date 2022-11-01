#pragma once

#include "Base.h"
#include "Window.h"

#include <vulkan/vulkan.h>

namespace vkPlayground {

	class Application
	{
	public:
		Application(/* TODO: ApplicationSpecification */);
		~Application();

		void Run();

	private:
		void Init();
		void Shutdown();

	private:
		Ref<Window> m_Window;

		float m_LastFrameTime = 0.0f;

		inline static Application* s_Instance = nullptr;

	};

}