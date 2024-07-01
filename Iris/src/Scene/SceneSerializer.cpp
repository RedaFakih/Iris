#include "IrisPCH.h"
#include "SceneSerializer.h"

#include "AssetManager/AssetManager.h"
#include "Project/Project.h"
#include "Renderer/StorageBufferSet.h"
#include "Renderer/Text/Font.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"
#include "Utils/YAMLSerializationHelpers.h"

#include <yaml-cpp/yaml.h>

namespace Iris {

	void SceneSerializer::SerializeEntity(YAML::Emitter& out, Entity entity, Ref<Scene> scene)
	{
		UUID uuid = entity.GetUUID();
			
		out << YAML::BeginMap;

		out << YAML::Key << "Entity" << YAML::Value << uuid;

		if (entity.HasComponent<TagComponent>())
		{
			out << YAML::Key << "TagComponent";
			out << YAML::BeginMap;

			const std::string& tag = entity.GetComponent<TagComponent>().Tag;
			out << YAML::Key << "Tag" << YAML::Value << tag;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<RelationshipComponent>())
		{
			const RelationshipComponent& relationshipComp = entity.GetComponent<RelationshipComponent>();
			out << YAML::Key << "Parent" << YAML::Value << relationshipComp.ParentHandle;

			out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;

			for (auto child : relationshipComp.Children)
			{
				out << YAML::BeginMap;
				out << YAML::Key << "Handle" << YAML::Value << child;
				out << YAML::EndMap;
			}

			out << YAML::EndSeq;
		}

		if (entity.HasComponent<TransformComponent>())
		{
			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap;

			const TransformComponent& transformComponent = entity.Transform();
			out << YAML::Key << "Position" << YAML::Value << transformComponent.Translation;
			out << YAML::Key << "Rotation" << YAML::Value << transformComponent.RotationEuler;
			out << YAML::Key << "Scale" << YAML::Value << transformComponent.Scale;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<CameraComponent>())
		{
			out << YAML::Key << "CameraComponent";
			out << YAML::BeginMap;

			const CameraComponent& cameraComponent = entity.GetComponent<CameraComponent>();
			const SceneCamera& camera = cameraComponent.Camera;
			out << YAML::Key << "Camera" << YAML::Value;

			out << YAML::BeginMap;

			out << YAML::Key << "ProjectionType" << YAML::Value << static_cast<int>(camera.GetProjectionType());
			out << YAML::Key << "PerspectiveFOV" << YAML::Value << camera.GetDegPerspectiveVerticalFOV();
			out << YAML::Key << "PerspectiveNear" << YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFar" << YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNear" << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFar" << YAML::Value << camera.GetOrthographicFarClip();
			
			out << YAML::EndMap;
			
			out << YAML::Key << "Primary" << YAML::Value << cameraComponent.Primary;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<StaticMeshComponent>())
		{
			out << YAML::Key << "StaticMeshComponent";
			out << YAML::BeginMap;

			const StaticMeshComponent& staticMeshComp = entity.GetComponent<StaticMeshComponent>();
			out << YAML::Key << "AssetID" << YAML::Value << staticMeshComp.StaticMesh;

			Ref<MaterialTable> materialTable = staticMeshComp.MaterialTable;
			if (materialTable->GetMaterialCount() > 0)
			{
				out << YAML::Key << "MaterialTable" << YAML::Value << YAML::BeginMap;

				for (uint32_t i = 0; i < materialTable->GetMaterialCount(); i++)
				{
					AssetHandle handle = (materialTable->HasMaterial(i) ? materialTable->GetMaterial(i) : (AssetHandle)0);
					out << YAML::Key << i << YAML::Value << handle;

				}

				out << YAML::EndMap;
			}

			out << YAML::Key << "Visible" << YAML::Value << staticMeshComp.Visible;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<SpriteRendererComponent>())
		{
			out << YAML::Key << "SpriteRendererComponent";
			out << YAML::BeginMap;

			const SpriteRendererComponent& spriteRendererComponent = entity.GetComponent<SpriteRendererComponent>();
			out << YAML::Key << "Color" << YAML::Value << spriteRendererComponent.Color;

			out << YAML::Key << "Texture" << YAML::Value << spriteRendererComponent.Texture;
			out << YAML::Key << "TilingFactor" << YAML::Value << spriteRendererComponent.TilingFactor;
			out << YAML::Key << "UV0" << YAML::Value << spriteRendererComponent.UV0;
			out << YAML::Key << "UV1" << YAML::Value << spriteRendererComponent.UV1;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<TextComponent>())
		{
			out << YAML::Key << "TextComponent";
			out << YAML::BeginMap;

			const TextComponent& textComponent = entity.GetComponent<TextComponent>();
			out << YAML::Key << "TextString" << YAML::Value << textComponent.TextString;
			out << YAML::Key << "Font" << YAML::Value << textComponent.Font;
			out << YAML::Key << "Color" << YAML::Value << textComponent.Color;
			out << YAML::Key << "LineSpacing" << YAML::Value << textComponent.LineSpacing;
			out << YAML::Key << "Kerning" << YAML::Value << textComponent.Kerning;
			out << YAML::Key << "MaxWidth" << YAML::Value << textComponent.MaxWidth;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<SkyLightComponent>())
		{
			out << YAML::Key << "SkyLightComponent";
			out << YAML::BeginMap;

			const SkyLightComponent& slc = entity.GetComponent<SkyLightComponent>();
			out << YAML::Key << "EnvironmentMap" << YAML::Value << (AssetManager::IsMemoryAsset(slc.SceneEnvironment) ? static_cast<AssetHandle>(0) : slc.SceneEnvironment);
			out << YAML::Key << "Intensity" << YAML::Value << slc.Intensity;
			out << YAML::Key << "Lod" << YAML::Value << slc.Lod;
			out << YAML::Key << "DynamicSky" << YAML::Value << slc.DynamicSky;
			if(slc.DynamicSky)
				out << YAML::Key << "TurbidityAzimuthInclination" << YAML::Value << slc.TurbidityAzimuthInclination;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			out << YAML::Key << "DirectionalLightComponent";
			out << YAML::BeginMap;

			const DirectionalLightComponent& dirLightComp = entity.GetComponent<DirectionalLightComponent>();
			out << YAML::Key << "Radiance" << YAML::Value << dirLightComp.Radiance;
			out << YAML::Key << "Intensity" << YAML::Value << dirLightComp.Intensity;

			// TODO: Expand for shadow stuff...

			out << YAML::EndMap;
		}

		out << YAML::EndMap;
	}

	void SceneSerializer::DeserializeEntity(YAML::Node& entitiesNode, Ref<Scene> scene)
	{
		for (auto entity : entitiesNode)
		{
			uint64_t uuid = entity["Entity"].as<uint64_t>(0);

			std::string name;
			YAML::Node tagComponent = entity["TagComponent"];
			if (tagComponent)
				name = tagComponent["Tag"].as<std::string>();

			Entity deserializedEntity = scene->CreateEntityWithUUID(uuid, name, false);

			RelationshipComponent& relationComp = deserializedEntity.GetComponent<RelationshipComponent>();
			uint64_t parentHandle = entity["Parent"] ? entity["Parent"].as<uint64_t>() : 0;
			relationComp.ParentHandle = parentHandle;

			YAML::Node children = entity["Children"];
			for (auto child : children)
			{
				uint64_t childID = child["Handle"].as<uint64_t>();
				relationComp.Children.push_back(childID);
			}

			YAML::Node transformComp = entity["TransformComponent"];
			if (transformComp)
			{
				TransformComponent& transform = deserializedEntity.GetComponent<TransformComponent>();
				transform.Translation = transformComp["Position"].as<glm::vec3>();
				transform.SetRotationEuler(transformComp["Rotation"].as<glm::vec3>(glm::vec3(0.0f)));
				transform.Scale = transformComp["Scale"].as<glm::vec3>();
			}

			YAML::Node cameraComp = entity["CameraComponent"];
			if (cameraComp)
			{
				CameraComponent& component = deserializedEntity.AddComponent<CameraComponent>();
				const auto& cameraNode = cameraComp["Camera"];

				component.Camera = SceneCamera();
				auto& camera = component.Camera;

				if (cameraNode.IsMap())
				{
					if (cameraNode["ProjectionType"])
						camera.SetProjectionType((SceneCamera::ProjectionType)cameraNode["ProjectionType"].as<int>());
					if (cameraNode["PerspectiveFOV"])
						camera.SetDegPerspectiveVerticalFOV(cameraNode["PerspectiveFOV"].as<float>());
					if (cameraNode["PerspectiveNear"])
						camera.SetPerspectiveNearClip(cameraNode["PerspectiveNear"].as<float>());
					if (cameraNode["PerspectiveFar"])
						camera.SetPerspectiveFarClip(cameraNode["PerspectiveFar"].as<float>());
					if (cameraNode["OrthographicSize"])
						camera.SetOrthographicSize(cameraNode["OrthographicSize"].as<float>());
					if (cameraNode["OrthographicNear"])
						camera.SetOrthographicNearClip(cameraNode["OrthographicNear"].as<float>());
					if (cameraNode["OrthographicFar"])
						camera.SetOrthographicFarClip(cameraNode["OrthographicFar"].as<float>());
				}

				component.Primary = cameraComp["Primary"].as<bool>();
			}

			YAML::Node staticMeshComp = entity["StaticMeshComponent"];
			if (staticMeshComp)
			{
				StaticMeshComponent& component = deserializedEntity.AddComponent<StaticMeshComponent>();

				AssetHandle assetHandle = staticMeshComp["AssetID"].as<uint64_t>();
				if (AssetManager::IsAssetHandleValid(assetHandle))
					component.StaticMesh = assetHandle;

				if (staticMeshComp["MaterialTable"])
				{
					YAML::Node materialTableNode = staticMeshComp["MaterialTable"];
					for (auto materialEntry : materialTableNode)
					{
						uint32_t index = materialEntry.first.as<uint32_t>();
						AssetHandle materialAsset = materialEntry.second.as<AssetHandle>(0);
						if (materialAsset && AssetManager::IsAssetHandleValid(materialAsset))
							component.MaterialTable->SetMaterial(index, materialAsset);
					}
				}

				if (staticMeshComp["Visible"])
					component.Visible = staticMeshComp["Visible"].as<bool>();
			}

			YAML::Node spriteRendererComp = entity["SpriteRendererComponent"];
			if (spriteRendererComp)
			{
				SpriteRendererComponent& component = deserializedEntity.AddComponent<SpriteRendererComponent>();
				component.Color = spriteRendererComp["Color"].as<glm::vec4>();
				if (spriteRendererComp["Texture"])
					component.Texture = spriteRendererComp["Texture"].as<AssetHandle>();
				component.TilingFactor = spriteRendererComp["TilingFactor"].as<float>();
				if (spriteRendererComp["UV0"])
					component.UV0 = spriteRendererComp["UV0"].as<glm::vec2>();
				if (spriteRendererComp["UV1"])
					component.UV1 = spriteRendererComp["UV1"].as<glm::vec2>();
			}

			YAML::Node textComp = entity["TextComponent"];
			if (textComp)
			{
				TextComponent& component = deserializedEntity.AddComponent<TextComponent>();
				component.TextString = textComp["TextString"].as<std::string>();
				component.TextHash = std::hash<std::string>{}(component.TextString);

				AssetHandle font = textComp["Font"].as<AssetHandle>(0);
				if (AssetManager::IsAssetHandleValid(font))
					component.Font = font;
				else
					component.Font = Font::GetDefaultFont()->Handle;

				component.Color = textComp["Color"].as<glm::vec4>();
				component.LineSpacing = textComp["LineSpacing"].as<float>();
				component.Kerning = textComp["Kerning"].as<float>();
				component.MaxWidth = textComp["MaxWidth"].as<float>();
			}

			YAML::Node skyLightComp = entity["SkyLightComponent"];
			if (skyLightComp)
			{
				SkyLightComponent& component = deserializedEntity.AddComponent<SkyLightComponent>();

				if (skyLightComp["EnvironmentMap"])
				{
					AssetHandle envMapHandle = skyLightComp["EnvironmentMap"].as<AssetHandle>(0);
					if (AssetManager::IsAssetHandleValid(envMapHandle))
						component.SceneEnvironment = envMapHandle;
				}

				component.Intensity = skyLightComp["Intensity"].as<float>(1.0f);
				component.Lod = skyLightComp["Lod"].as<float>(0.0f);

				if (skyLightComp["DynamicSky"])
				{
					component.DynamicSky = skyLightComp["DynamicSky"].as<bool>();
					if (component.DynamicSky)
					{
						component.TurbidityAzimuthInclination = skyLightComp["TurbidityAzimuthInclination"].as<glm::vec3>();
					}
				}
			}

			YAML::Node dirLightComp = entity["DirectionalLightComponent"];
			if (dirLightComp)
			{
				DirectionalLightComponent& component = deserializedEntity.AddComponent<DirectionalLightComponent>();
				component.Radiance = dirLightComp["Radiance"].as<glm::vec3>(glm::vec3(1.0f));
				component.Intensity = dirLightComp["Intensity"].as<float>(1.0f);
				// TODO: Expand for shadow stuff...
			}

		}

		scene->SortEntities();
	}

	void SceneSerializer::Serialize(Ref<Scene> scene, const std::filesystem::path& filePath)
	{
		YAML::Emitter out;

		out << YAML::BeginMap;

		out << YAML::Key << "Scene" << YAML::Value << scene->GetName();
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

		std::map<UUID, entt::entity> sortedEntityMap;
		auto idComponentView = scene->m_Registry.view<IDComponent>();
		for (auto entity : idComponentView)
			sortedEntityMap[idComponentView.get<IDComponent>(entity).ID] = entity;

		for (auto [id, entity] : sortedEntityMap)
			SerializeEntity(out, { entity, scene.Raw() }, scene);

		out << YAML::EndSeq;

		out << YAML::EndMap;

		std::ofstream fout(filePath);
		fout << out.c_str();
		fout.close();
	}

	bool SceneSerializer::Deserialize(Ref<Scene>& scene, const std::filesystem::path& filePath)
	{
		std::ifstream ifStream(filePath);
		IR_VERIFY(ifStream);
		std::stringstream strStream;
		strStream << ifStream.rdbuf();

		try {
			YAML::Node data = YAML::Load(strStream.str());
			if (!data["Scene"])
				return false;

			std::string sceneName = data["Scene"].as<std::string>();
			IR_CORE_INFO_TAG("AssetManager", "Deserializing scene '{0}'", sceneName);
			scene->SetName(sceneName);

			auto entities = data["Entities"];
			if(entities)
				DeserializeEntity(entities, scene);

			scene->m_Registry.sort<IDComponent>([scene](const auto lhs, const auto rhs)
			{
				auto lhsEntity = scene->m_EntityIDMap.find(lhs.ID);
				auto rhsEntity = scene->m_EntityIDMap.find(rhs.ID);
				return static_cast<uint32_t>(lhsEntity->second) < static_cast<uint32_t>(rhsEntity->second);
			});
		}
		catch ([[maybe_unused]] const YAML::Exception& e) {
			IR_CORE_ERROR_TAG("AssetManager", "Could not deserialize scene with filepath: {}", filePath);
			return false;
		}

		const AssetMetaData& metaData = Project::GetEditorAssetManager()->GetMetaData(filePath);
		scene->Handle = metaData.Handle;

		if (scene->GetName() == "UntitledScene")
		{
			std::string filename = filePath.filename().string();
			scene->SetName(filename.substr(0, filename.find_last_of('.')));
		}

		return true;
	}

}