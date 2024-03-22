#include "RuntimeLayer.h"

#include <EntryPoint.h>

namespace vkPlayground {

	class Runtime : public Application
	{
	public:
		Runtime(const ApplicationSpecification& spec, std::string_view projectPath)
			: Application(spec), m_ProjectPath(projectPath)
		{
		}

		virtual void OnInit() override
		{
			PushLayer(new RuntimeLayer());
		}

	private:
		std::string m_ProjectPath;

	};

	Application* CreateApplication(int argc, char** argv)
	{
		std::string projectPath = "";

		vkPlayground::ApplicationSpecification appSpec = {
			.Name = "Vulkan Playground",
			.WindowWidth = 1280,
			.WindowHeight = 720,
			.FullScreen = false,
			.VSync = false,
			.WorkingDirectory = "../PlaygroundEditor",
			.StartMaximized = true,
			.Resizable = true,
			.EnableImGui = false,
			.RendererConfig = {.FramesInFlight = 3 }
		};

		return new Runtime(appSpec, projectPath);
	}

}