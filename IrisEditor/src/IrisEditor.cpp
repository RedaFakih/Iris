#include "EditorLayer.h"

#include <EntryPoint.h>

namespace Iris {

	class Editor : public Application
	{
	public:
		Editor(const ApplicationSpecification& spec, std::string_view projectPath)
			: Application(spec), m_ProjectPath(projectPath), m_UserPreferences(UserPreferences::Create())
		{
			if (m_ProjectPath.empty())
				m_ProjectPath = "SandboxProject/Sandbox.Iproj";
		}

		virtual void OnInit() override
		{
			// Persistent Storage
			{
				m_PersistentStoragePath = FileSystem::GetPersistantStoragePath() / "IrisEditor";

				if (!FileSystem::Exists(m_PersistentStoragePath))
					FileSystem::CreateDirectory(m_PersistentStoragePath);
			}

			// User Preferences
			{
				if (!FileSystem::Exists(m_PersistentStoragePath / "UserPreferences.yaml"))
					UserPreferencesSerializer::Serialize(m_UserPreferences, m_PersistentStoragePath / "UserPreferences.yaml");
				else
					UserPreferencesSerializer::Deserialize(m_UserPreferences, m_PersistentStoragePath / "UserPreferences.yaml");

				if (!m_ProjectPath.empty())
					m_UserPreferences->StartupProject = m_ProjectPath;
				else if (!m_UserPreferences->StartupProject.empty())
					m_ProjectPath = m_UserPreferences->StartupProject;
			}

			PushLayer(new EditorLayer(m_UserPreferences));
		}

	private:
		std::string m_ProjectPath;
		std::filesystem::path m_PersistentStoragePath;
		Ref<UserPreferences> m_UserPreferences;

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