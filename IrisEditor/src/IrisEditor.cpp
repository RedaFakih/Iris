#include "EditorLayer.h"

#include <EntryPoint.h>

namespace Iris {

	class Editor : public Application
	{
	public:
		Editor(const ApplicationSpecification& spec, std::string_view projectPath)
			: Application(spec), m_ProjectPath(projectPath)
		{
		}

		virtual void OnInit() override
		{
			PushLayer(new EditorLayer());
		}

	private:
		std::string m_ProjectPath;

	};

	Application* CreateApplication(int argc, char** argv)
	{
		std::string projectPath = "";

		Iris::ApplicationSpecification appSpec = {
			.Name = "Iris - Editor",
			.WindowWidth = 1280,
			.WindowHeight = 720,
			.FullScreen = false,
			.VSync = true,
			.StartMaximized = true,
			.Resizable = true,
			.EnableImGui = true,
			.RendererConfig = { .FramesInFlight = 3 },
			.CoreThreadingPolicy = ThreadingPolicy::SingleThreaded
		};

		return new Editor(appSpec, projectPath);
	}

}