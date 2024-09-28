#include "IrisPCH.h"
#include "ProjectSerializer.h"

#include "Utils/YAMLSerializationHelpers.h"

#include <yaml-cpp/yaml.h>

namespace Iris {

	void ProjectSerializer::Serialize(Ref<Project> project, const std::filesystem::path& filePath)
	{
		YAML::Emitter out;

		out << YAML::BeginMap;

		out << YAML::Key << "Project" << YAML::Value;
		{
			out << YAML::BeginMap;

			out << YAML::Key << "Name" << YAML::Value << project->m_Config.Name;
			out << YAML::Key << "AssetDirectory" << YAML::Value << project->m_Config.AssetDirectory;
			out << YAML::Key << "AssetRegistryPath" << YAML::Value << project->m_Config.AssetRegistryPath;
			out << YAML::Key << "MeshPath" << YAML::Value << project->m_Config.MeshPath;
			out << YAML::Key << "MeshSourcePath" << YAML::Value << project->m_Config.MeshSourcePath;
			out << YAML::Key << "StartScene" << YAML::Value << project->m_Config.StartScene;
			out << YAML::Key << "AutoSave" << YAML::Value << project->m_Config.EnableAutoSave;
			out << YAML::Key << "AutoSaveInterval" << YAML::Value << project->m_Config.AutoSaveIntervalSeconds;
			out << YAML::Key << "ViewportSelectionOutlineColor" << YAML::Value << project->m_Config.ViewportSelectionOutlineColor;
			out << YAML::Key << "Viewport2DColliderOutlineColor" << YAML::Value << project->m_Config.Viewport2DColliderOutlineColor;
			out << YAML::Key << "Viewport3DColliderOutlineColor" << YAML::Value << project->m_Config.Viewport3DColliderOutlineColor;

			out << YAML::Key << "Log" << YAML::Value;
			{
				out << YAML::BeginMap;

				auto& tags = Logging::Log::GetEnabledTags();
				for (auto& [name, details] : tags)
				{
					if (name.empty())
						continue;

					out << YAML::Key << name << YAML::Value;
					out << YAML::BeginMap;
					{
						out << YAML::Key << "Enabled" << YAML::Value << details.Enabled;
						out << YAML::Key << "LevelFilter" << YAML::Value << Logging::Log::LevelToString(details.LevelFilter);
					}
					out << YAML::EndMap;
				}

				out << YAML::EndMap;
			}

			out << YAML::EndMap;
		}

		out << YAML::EndMap;

		std::ofstream ofStream(filePath);
		ofStream << out.c_str();
	}

	bool ProjectSerializer::Deserialize(Ref<Project>& project, const std::filesystem::path& filePath)
	{
		std::ifstream ifStream(filePath);
		IR_ASSERT(ifStream);
		std::stringstream ss;
		ss << ifStream.rdbuf();

		YAML::Node data = YAML::Load(ss.str());
		if (!data["Project"])
			return false;

		YAML::Node rootNode = data["Project"];
		if (!rootNode["Name"])
			return false;

		auto& config = project->m_Config;
		config.Name = rootNode["Name"].as<std::string>();

		config.AssetDirectory = rootNode["AssetDirectory"].as<std::string>();
		config.AssetRegistryPath = rootNode["AssetRegistryPath"].as<std::string>();

		config.ProjectFileName = filePath.filename().string();
		config.ProjectDirectory = filePath.parent_path().string();

		config.MeshPath = rootNode["MeshPath"].as<std::string>();
		config.MeshSourcePath = rootNode["MeshSourcePath"].as<std::string>();

		if (rootNode["StartScene"])
			config.StartScene = rootNode["StartScene"].as<std::string>();

		config.EnableAutoSave = rootNode["AutoSave"].as<bool>(false);
		config.AutoSaveIntervalSeconds = rootNode["AutoSaveInterval"].as<int>(300);
		config.ViewportSelectionOutlineColor = rootNode["ViewportSelectionOutlineColor"].as<glm::vec4>(glm::vec4{ 0.14f, 0.8f, 0.52f, 1.0f });
		config.Viewport2DColliderOutlineColor = rootNode["Viewport2DColliderOutlineColor"].as<glm::vec4>(glm::vec4{ 0.25f, 0.6f, 1.0f, 1.0f });
		config.Viewport3DColliderOutlineColor = rootNode["Viewport3DColliderOutlineColor"].as<glm::vec4>(glm::vec4{ 0.2f, 1.0f, 0.2f, 1.0f });

		YAML::Node logNode = rootNode["Log"];
		if (logNode)
		{
			auto& tags = Logging::Log::GetEnabledTags();
			for (auto node : logNode)
			{
				std::string name = node.first.as<std::string>();
				auto& details = tags[name];
				details.Enabled = node.second["Enabled"].as<bool>(true);
				details.LevelFilter = Logging::Log::LevelFromString(node.second["LevelFilter"].as<std::string>("Info"));
			}
		}

		return true;
	}

}