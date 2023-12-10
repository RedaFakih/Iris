#include "EditorLayer.h"

#include <EntryPoint.h>

namespace vkPlayground {

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

		vkPlayground::ApplicationSpecification appSpec = {
			.Name = "Vulkan Playground",
			.WindowWidth = 1280,
			.WindowHeight = 720,
			.FullScreen = false,
			.VSync = false,
			.StartMaximized = true,
			.Resizable = true,
			.EnableImGui = true,
			.RendererConfig = { .FramesInFlight = 3 }
		};

		return new Editor(appSpec, projectPath);
	}

}