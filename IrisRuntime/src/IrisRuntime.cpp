#include "RuntimeLayer.h"

#include <EntryPoint.h>

namespace Iris {

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

		Iris::ApplicationSpecification appSpec = {
			.Name = "Iris - Runtime",
			.WindowWidth = 1280,
			.WindowHeight = 720,
			.FullScreen = true,
			.VSync = true,
			.WorkingDirectory = "../IrisEditor",
			.StartMaximized = false,
			.Resizable = true,
			.EnableImGui = false,
			.RendererConfig = {.FramesInFlight = 3 }
		};

		appSpec.WindowDecorated = !appSpec.FullScreen;
		appSpec.Resizable = !appSpec.FullScreen;

		return new Runtime(appSpec, projectPath);
	}

}