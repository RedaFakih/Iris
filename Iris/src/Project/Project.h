#pragma once

#include "AssetManager/EditorAssetManager.h"

namespace Iris {

	struct ProjectConfig
	{
		std::string Name;

		std::string AssetDirectory;
		std::string AssetRegistryPath;

		// For default meshes
		std::string MeshPath;
		std::string MeshSourcePath;

		std::string StartScene;

		bool EnableAutoSave = false;
		uint32_t AutoSaveIntervalSeconds = 300; // 5 minutes

		// Not serialized
		std::string ProjectFileName;
		std::string ProjectDirectory;
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
		friend class ProjectSettingsPanel;

	};

}