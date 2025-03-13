#include "IrisPCH.h"
#include "ProjectSerializer.h"

#include "Physics/Physics.h"
#include "Physics/PhysicsLayer.h"
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
			out << YAML::Key << "SceneSaveOnEditorClose" << YAML::Value << project->m_Config.EnableSceneSaveOnEditorClose;
			out << YAML::Key << "ViewportSelectionOutlineColor" << YAML::Value << project->m_Config.ViewportSelectionOutlineColor;
			out << YAML::Key << "Viewport2DColliderOutlineColor" << YAML::Value << project->m_Config.Viewport2DColliderOutlineColor;
			out << YAML::Key << "ViewportSimple3DColliderOutlineColor" << YAML::Value << project->m_Config.ViewportSimple3DColliderOutlineColor;
			out << YAML::Key << "ViewportComplex3DColliderOutlineColor" << YAML::Value << project->m_Config.ViewportComplex3DColliderOutlineColor;

			// TODO: Physics
			out << YAML::Key << "Physics" << YAML::Value;
			{
				out << YAML::BeginMap;

				const PhysicsSettings& settings = PhysicsSystem::GetSettings();

				out << YAML::Key << "FixedTimestep" << YAML::Value << settings.FixedTimestep;
				out << YAML::Key << "Gravity" << YAML::Value << settings.Gravity;
				out << YAML::Key << "PositionSolverIterations" << YAML::Value << settings.PositionSolverIterations;
				out << YAML::Key << "VelocitySolverIterations" << YAML::Value << settings.VelocitySolverIterations;
				out << YAML::Key << "MaxBodies" << YAML::Value << settings.MaxBodies;

				// Physics Layers > 1 since we have 1 default layer at index 0
				if (PhysicsLayerManager::GetLayerCount() > 1)
				{
					out << YAML::Key << "Layers" << YAML::Value << YAML::BeginSeq;
					for (const PhysicsLayer& layer : PhysicsLayerManager::GetLayers())
					{
						// Default layer is not serialized
						if (layer.LayerID == 0)
							continue;

						out << YAML::BeginMap;

						out << YAML::Key << "Name" << YAML::Value << layer.Name;
						out << YAML::Key << "CollidesWithSelf" << YAML::Value << layer.CollidesWithSelf;
						out << YAML::Key << "CollidesWith" << YAML::Value << YAML::BeginSeq;
						for (const PhysicsLayer& collidingLayer : PhysicsLayerManager::GetLayerCollisions(layer.LayerID))
						{
							out << YAML::BeginMap;

							out << YAML::Key << "Name" << YAML::Value << collidingLayer.Name;

							out << YAML::EndMap;
						}
						out << YAML::EndSeq;

						out << YAML::EndMap;
					}
					out << YAML::EndSeq;
				}

				out << YAML::EndMap;
			}

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
		PhysicsLayerManager::ClearLayers();
		PhysicsLayerManager::AddLayer("Default");

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
		config.EnableSceneSaveOnEditorClose = rootNode["SceneSaveOnEditorClose"].as<bool>(false);
		config.ViewportSelectionOutlineColor = rootNode["ViewportSelectionOutlineColor"].as<glm::vec4>(glm::vec4{ 0.14f, 0.8f, 0.52f, 1.0f });
		config.Viewport2DColliderOutlineColor = rootNode["Viewport2DColliderOutlineColor"].as<glm::vec4>(glm::vec4{ 0.25f, 0.6f, 1.0f, 1.0f });
		config.ViewportSimple3DColliderOutlineColor = rootNode["ViewportSimple3DColliderOutlineColor"].as<glm::vec4>(glm::vec4{ 0.2f, 1.0f, 0.2f, 1.0f });
		config.ViewportComplex3DColliderOutlineColor = rootNode["ViewportComplex3DColliderOutlineColor"].as<glm::vec4>(glm::vec4{ 0.5f, 0.5f, 1.0f, 1.0f });

		// Physics
		YAML::Node physicsNode = rootNode["Physics"];
		if (physicsNode)
		{
			PhysicsSettings& settings = PhysicsSystem::GetSettings();

			settings.FixedTimestep = physicsNode["FixedTimestep"].as<float>(1.0f / 60.0f);
			settings.Gravity = physicsNode["Gravity"].as<glm::vec3>(glm::vec3{ 0.0f, -9.81f, 0.0f });
			settings.PositionSolverIterations = physicsNode["PositionSolverIterations"].as<uint32_t>(2);
			settings.VelocitySolverIterations = physicsNode["VelocitySolverIterations"].as<uint32_t>(8);
			settings.MaxBodies = physicsNode["MaxBodies"].as<uint32_t>(5700);

			YAML::Node layersNode = physicsNode["Layers"];
			if (layersNode)
			{
				for (auto layer : layersNode)
					PhysicsLayerManager::AddLayer(layer["Name"].as<std::string>(), false);

				for (auto layer : layersNode)
				{
					PhysicsLayer& layerInfo = PhysicsLayerManager::GetLayer(layer["Name"].as<std::string>());
					layerInfo.CollidesWithSelf = layer["CollidesWithSelf"].as<bool>(true);

					YAML::Node collidesWithNode = layersNode["CollidesWith"];
					if (collidesWithNode)
					{
						for (auto collisionLayer : collidesWithNode)
						{
							const PhysicsLayer& otherLayer = PhysicsLayerManager::GetLayer(collisionLayer["Name"].as<std::string>());
							PhysicsLayerManager::SetLayerCollision(layerInfo.LayerID, otherLayer.LayerID, true);
						}
					}
				}
			}
		}

		// Log
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