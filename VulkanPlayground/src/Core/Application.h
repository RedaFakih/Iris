#pragma once

#include "Base.h"
#include "Window.h"
#include "AppEvents.h"
#include "TimeStep.h"
#include "Renderer/RendererConfiguration.h"

#include <vulkan/vulkan.h>

#include <queue>

namespace vkPlayground {

	struct ApplicationSpecification
	{
		std::string Name = "Vulkan Playground";
		uint32_t WindowWidth = 1600;
		uint32_t WindowHeight = 900;
		bool FullScreen = false; // NOTE: If set to `true` this makes the application run in FULLSCREEN without the titlebar and anything
		bool VSync = true;
		std::string WorkingDirectory;
		bool StartMaximized = true;
		bool Resizable = true;
		RendererConfiguration RendererConfig;
	};

	class Application
	{
	public:
		Application(const ApplicationSpecification& spec);
		~Application();

		void Run();
		void ProcessEvents();
		void OnEvent(Events::Event& e);
		
		inline static Application& Get() { return *s_Instance; }
		inline Window& GetWindow() { return *m_Window; }

		template<typename Func>
		inline void QueueEvent(Func&& func) { m_EventQueue.push(std::forward<Func>(func)); }
		template<typename TEvent, bool TDispatchImmediatly, typename... Args>
		void DispatchEvent(Args&&... args)
		{
			static_assert(std::is_assignable<Events::Event, TEvent>);

			std::shared_ptr<TEvent> event = std::make_shared<TEvent>(std::forward<Args>(args)...);
			if constexpr (TDispatchImmediatly)
			{
				OnEvent(event);
			}
			else
			{
				std::scoped_lock<std::mutex> lock(m_EventQueueMutex);
				QueueEvent([event]() { Application::Get().OnEvent(event); });
			}
		}

		TimeStep GetTimeStep() const { return m_TimeStep; }
		TimeStep GetFrameTime() const { return m_FrameTime; }
		float GetTime() const;

		uint32_t GetCurrentFrameIndex() const { return m_CurrentFrameIndex; }

	private:
		bool OnWindowResize(Events::WindowResizeEvent& e);
		bool OnWindowClose(Events::WindowCloseEvent& e);
		bool OnWindowMinimize(Events::WindowMinimizeEvent& e);

	private:
		//  Don't think we ever need to store the specification since all its members are used on the initialization
		// ApllicationSpecificaton m_Specification;

		Ref<Window> m_Window;
		bool m_Running = true;
		bool m_Minimized = false;

		TimeStep m_FrameTime;
		TimeStep m_TimeStep;
		float m_LastFrameTime = 0.0f;

		uint32_t m_CurrentFrameIndex = 0;

		std::mutex m_EventQueueMutex; // In case we want to have an event queue thread
		std::queue<std::function<void()>> m_EventQueue;

		inline static Application* s_Instance = nullptr;

		// friend class Renderer; NOTE: For now not needed

	};

	Application* CreateApplication(const ApplicationSpecification& spec);

}