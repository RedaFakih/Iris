#pragma once

#include "AssetManager/EditorAssetManager.h"

namespace Iris {

	struct ProjectConfig
	{
		std::string Name;

		// TODO: The values that are set here are temp and they should not be set here
		std::string AssetDirectory = "Assets";
		std::string AssetRegistryPath = "Assets/AssetRegistry.Iar";

		std::string MeshPath;
		std::string MeshSourcePath;

		std::string StartScene;

		// TODO: Not implemented yet...
		bool EnableAutoSave = false;
		int AutoSaveIntervalSeconds = 300; // 5 minutes

		// Not serialized
		std::string ProjectFileName;
		// TODO: The value set here is temp and it should not be set here
		std::string ProjectDirectory = "SandboxProject";
	};

	class Project : public RefCountedObject
	{
	public:
		Project() = default;
		~Project() = default;

		[[nodiscard]] static Ref<Project> Create();

		const ProjectConfig& GetConfig() const { return m_Config; }

		static Ref<Project> GetActive() { return s_ActiveProject; }
		static void SetActive(Ref<Project> project);

		inline static Ref<AssetManagerBase> GetAssetManager() { return s_AssetManager; }
		inline static Ref<EditorAssetManager> GetEditorAssetManager() { return s_AssetManager.As<EditorAssetManager>(); }

		inline static const std::string& GetProjectName()
		{
			IR_ASSERT(s_ActiveProject);
			return s_ActiveProject->GetConfig().Name;
		}

		inline static std::filesystem::path GetProjectDirectory()
		{
			IR_ASSERT(s_ActiveProject);
			return s_ActiveProject->GetConfig().ProjectDirectory;
		}

		inline static std::filesystem::path GetAssetDirectory()
		{
			IR_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / s_ActiveProject->GetConfig().AssetDirectory;
		}

		inline static std::filesystem::path GetAssetRegistryPath()
		{
			IR_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / s_ActiveProject->GetConfig().AssetRegistryPath;
		}

		inline static std::filesystem::path GetMeshPath()
		{
			IR_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / s_ActiveProject->GetConfig().MeshPath;
		}

		inline static std::filesystem::path GetCacheDirectory()
		{
			IR_ASSERT(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / "Cache";
		}

	private:
		ProjectConfig m_Config;
		inline static Ref<AssetManagerBase> s_AssetManager;

		inline static Ref<Project> s_ActiveProject;

		friend struct ProjectSerializer;

	};

}