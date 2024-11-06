#include "IrisPCH.h"
#include "SceneSerializer.h"

#include "AssetManager/AssetManager.h"
#include "Project/Project.h"
#include "Renderer/Text/Font.h"
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
			if (slc.DynamicSky)
			{
				out << YAML::Key << "TurbidityAzimuthInclinationSunSize" << YAML::Value << slc.TurbidityAzimuthInclinationSunSize;
				out << YAML::Key << "DirectionalLightEntityID" << YAML::Value << slc.DirectionalLightEntityID;
				out << YAML::Key << "LinkDirectionalLightRadiance" << YAML::Value << slc.LinkDirectionalLightRadiance;
			}

			out << YAML::EndMap;
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			out << YAML::Key << "DirectionalLightComponent";
			out << YAML::BeginMap;

			const DirectionalLightComponent& dirLightComp = entity.GetComponent<DirectionalLightComponent>();
			out << YAML::Key << "Radiance" << YAML::Value << dirLightComp.Radiance;
			out << YAML::Key << "Intensity" << YAML::Value << dirLightComp.Intensity;
			out << YAML::Key << "CastShadows" << YAML::Value << dirLightComp.CastShadows;
			out << YAML::Key << "ShadowOpacity" << YAML::Value << dirLightComp.ShadowOpacity;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<RigidBodyComponent>())
		{
			out << YAML::Key << "RigidBodyComponent";
			out << YAML::BeginMap;

			const RigidBodyComponent& rigidbodyComponent = entity.GetComponent<RigidBodyComponent>();
			out << YAML::Key << "BodyType" << YAML::Value << static_cast<int>(rigidbodyComponent.BodyType);
			out << YAML::Key << "LayerID" << YAML::Value << rigidbodyComponent.LayerID;
			out << YAML::Key << "EnableDynamicTypeChange" << YAML::Value << rigidbodyComponent.EnableDynamicTypeChange;
			out << YAML::Key << "Mass" << YAML::Value << rigidbodyComponent.Mass;
			out << YAML::Key << "LinearDrag" << YAML::Value << rigidbodyComponent.LinearDrag;
			out << YAML::Key << "AngularDrag" << YAML::Value << rigidbodyComponent.AngularDrag;
			out << YAML::Key << "DisableGravity" << YAML::Value << rigidbodyComponent.DisableGravity;
			out << YAML::Key << "IsTrigger" << YAML::Value << rigidbodyComponent.IsTrigger;
			out << YAML::Key << "CollisionDetection" << YAML::Value << static_cast<int>(rigidbodyComponent.CollisionDetection);
			out << YAML::Key << "InitialLinearVelocity" << YAML::Value << rigidbodyComponent.InitialLinearVelocity;
			out << YAML::Key << "MaxLinearVelocity" << YAML::Value << rigidbodyComponent.MaxLinearVelocity;
			out << YAML::Key << "InitialAngularVelocity" << YAML::Value << rigidbodyComponent.InitialAngularVelocity;
			out << YAML::Key << "MaxAngularVelocity" << YAML::Value << rigidbodyComponent.MaxAngularVelocity;
			out << YAML::Key << "LockedAxes" << YAML::Value << static_cast<uint32_t>(rigidbodyComponent.LockedAxes);

			out << YAML::EndMap;
		}

		if (entity.HasComponent<BoxColliderComponent>())
		{
			out << YAML::Key << "BoxColliderComponent";
			out << YAML::BeginMap;

			const BoxColliderComponent& boxColliderComponent = entity.GetComponent<BoxColliderComponent>();
			out << YAML::Key << "HalfSize" << YAML::Value << boxColliderComponent.HalfSize;
			out << YAML::Key << "Offset" << YAML::Value << boxColliderComponent.Offset;
			out << YAML::Key << "Friction" << YAML::Value << boxColliderComponent.Material.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << boxColliderComponent.Material.Restitution;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<SphereColliderComponent>())
		{
			out << YAML::Key << "SphereColliderComponent";
			out << YAML::BeginMap;

			const SphereColliderComponent& sphereColliderComponent = entity.GetComponent<SphereColliderComponent>();
			out << YAML::Key << "Offset" << YAML::Value << sphereColliderComponent.Offset;
			out << YAML::Key << "Radius" << YAML::Value << sphereColliderComponent.Radius;
			out << YAML::Key << "Friction" << YAML::Value << sphereColliderComponent.Material.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << sphereColliderComponent.Material.Restitution;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<CylinderColliderComponent>())
		{
			out << YAML::Key << "CylinderColliderComponent";
			out << YAML::BeginMap;

			const CylinderColliderComponent& cylinderColliderComponent = entity.GetComponent<CylinderColliderComponent>();
			out << YAML::Key << "Offset" << YAML::Value << cylinderColliderComponent.Offset;
			out << YAML::Key << "Radius" << YAML::Value << cylinderColliderComponent.Radius;
			out << YAML::Key << "HalfHeight" << YAML::Value << cylinderColliderComponent.HalfHeight;
			out << YAML::Key << "Friction" << YAML::Value << cylinderColliderComponent.Material.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << cylinderColliderComponent.Material.Restitution;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<CapsuleColliderComponent>())
		{
			out << YAML::Key << "CapsuleColliderComponent";
			out << YAML::BeginMap;

			const CapsuleColliderComponent& capsuleColliderComponent = entity.GetComponent<CapsuleColliderComponent>();
			out << YAML::Key << "Offset" << YAML::Value << capsuleColliderComponent.Offset;
			out << YAML::Key << "Radius" << YAML::Value << capsuleColliderComponent.Radius;
			out << YAML::Key << "HalfHeight" << YAML::Value << capsuleColliderComponent.HalfHeight;
			out << YAML::Key << "Friction" << YAML::Value << capsuleColliderComponent.Material.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << capsuleColliderComponent.Material.Restitution;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<CompoundColliderComponent>())
		{
			out << YAML::Key << "CompoundColliderComponent";
			out << YAML::BeginMap;

			const CompoundColliderComponent& compoundColliderComponent = entity.GetComponent<CompoundColliderComponent>();
			out << YAML::Key << "IsImmutable" << YAML::Value << compoundColliderComponent.IsImmutable;
			out << YAML::Key << "IncludeStaticChildColliders" << YAML::Value << compoundColliderComponent.IncludeStaticChildColliders;

			out << YAML::Key << "CompoundedColliderEntities" << YAML::Value << YAML::BeginSeq;

			for (UUID uuid : compoundColliderComponent.CompoundedColliderEntities)
				out << static_cast<uint64_t>(uuid);

			out << YAML::EndSeq;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<RigidBody2DComponent>())
		{
			out << YAML::Key << "RigidBody2DComponent";
			out << YAML::BeginMap;

			const RigidBody2DComponent& rigidbody2DComponent = entity.GetComponent<RigidBody2DComponent>();
			out << YAML::Key << "BodyType" << YAML::Value << static_cast<int>(rigidbody2DComponent.BodyType);
			out << YAML::Key << "Mass" << YAML::Value << rigidbody2DComponent.Mass;
			out << YAML::Key << "LinearDrag" << YAML::Value << rigidbody2DComponent.LinearDrag;
			out << YAML::Key << "AngularDrag" << YAML::Value << rigidbody2DComponent.AngularDrag;
			out << YAML::Key << "GravityScale" << YAML::Value << rigidbody2DComponent.GravityScale;
			out << YAML::Key << "FixedRotation" << YAML::Value << rigidbody2DComponent.FixedRotation;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			out << YAML::Key << "BoxCollider2DComponent";
			out << YAML::BeginMap;

			const BoxCollider2DComponent& boxCollider2DComponent = entity.GetComponent<BoxCollider2DComponent>();
			out << YAML::Key << "HalfSize" << YAML::Value << boxCollider2DComponent.HalfSize;
			out << YAML::Key << "Offset" << YAML::Value << boxCollider2DComponent.Offset;
			out << YAML::Key << "Density" << YAML::Value << boxCollider2DComponent.Density;
			out << YAML::Key << "Friction" << YAML::Value << boxCollider2DComponent.Material.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << boxCollider2DComponent.Material.Restitution;
			out << YAML::Key << "RestitutionThreshold" << YAML::Value << boxCollider2DComponent.RestitutionThreshold;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			out << YAML::Key << "CircleCollider2DComponent";
			out << YAML::BeginMap;

			const CircleCollider2DComponent& circleCollider2DComponent = entity.GetComponent<CircleCollider2DComponent>();
			out << YAML::Key << "Offset" << YAML::Value << circleCollider2DComponent.Offset;
			out << YAML::Key << "Radius" << YAML::Value << circleCollider2DComponent.Radius;
			out << YAML::Key << "Density" << YAML::Value << circleCollider2DComponent.Density;
			out << YAML::Key << "Friction" << YAML::Value << circleCollider2DComponent.Material.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << circleCollider2DComponent.Material.Restitution;
			out << YAML::Key << "RestitutionThreshold" << YAML::Value << circleCollider2DComponent.RestitutionThreshold;

			out << YAML::EndMap;
		}

		out << YAML::EndMap;
	}

	void SceneSerializer::DeserializeEntity(YAML::Node& entitiesNode, Ref<Scene> scene, const std::vector<UUID>& visibleEntities)
	{
		for (auto entity : entitiesNode)
		{
			uint64_t uuid = entity["Entity"].as<uint64_t>(0);

			std::string name;
			YAML::Node tagComponent = entity["TagComponent"];
			if (tagComponent)
				name = tagComponent["Tag"].as<std::string>();

			// Visibility
			bool visibility = false;
			if (std::find(visibleEntities.begin(), visibleEntities.end(), uuid) != visibleEntities.end())
				visibility = true;

			Entity deserializedEntity = scene->CreateEntityWithUUID(uuid, name, visibility, false);

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
						component.TurbidityAzimuthInclinationSunSize = skyLightComp["TurbidityAzimuthInclinationSunSize"].as<glm::vec4>();
						component.DirectionalLightEntityID = skyLightComp["DirectionalLightEntityID"].as<uint64_t>(0);
						component.LinkDirectionalLightRadiance = skyLightComp["LinkDirectionalLightRadiance"].as<bool>(false);
					}
				}
			}

			YAML::Node dirLightComp = entity["DirectionalLightComponent"];
			if (dirLightComp)
			{
				DirectionalLightComponent& component = deserializedEntity.AddComponent<DirectionalLightComponent>();
				component.Radiance = dirLightComp["Radiance"].as<glm::vec3>(glm::vec3(1.0f));
				component.Intensity = dirLightComp["Intensity"].as<float>(1.0f);
				component.CastShadows = dirLightComp["CastShadows"].as<bool>(true);
				component.ShadowOpacity = dirLightComp["ShadowOpacity"].as<float>(1.0f);
			}

			YAML::Node rigidBodyComponent = entity["RigidBodyComponent"];
			if (rigidBodyComponent)
			{
				RigidBodyComponent& component = deserializedEntity.AddComponent<RigidBodyComponent>();
				component.BodyType = static_cast<PhysicsBodyType>(rigidBodyComponent["BodyType"].as<int>(0));
				component.LayerID = rigidBodyComponent["LayerID"].as<uint32_t>(0);
				component.EnableDynamicTypeChange = rigidBodyComponent["EnableDynamicTypeChange"].as<bool>(false);
				component.Mass = rigidBodyComponent["Mass"].as<float>(1.0f);
				component.LinearDrag = rigidBodyComponent["LinearDrag"].as<float>(0.01f);
				component.AngularDrag = rigidBodyComponent["AngularDrag"].as<float>(0.05f);
				component.DisableGravity = rigidBodyComponent["DisableGravity"].as<bool>(false);
				component.IsTrigger = rigidBodyComponent["IsTrigger"].as<bool>(false);
				component.CollisionDetection = static_cast<PhysicsCollisionDetectionType>(rigidBodyComponent["CollisionDetection"].as<int>(0));
				component.InitialLinearVelocity = rigidBodyComponent["InitialLinearVelocity"].as<glm::vec3>(glm::vec3{ 0.0f, 0.0f, 0.0f });
				component.MaxLinearVelocity = rigidBodyComponent["MaxLinearVelocity"].as<float>(500.0f);
				component.InitialAngularVelocity = rigidBodyComponent["InitialAngularVelocity"].as<glm::vec3>(glm::vec3{ 0.0f, 0.0f, 0.0f });
				component.MaxAngularVelocity = rigidBodyComponent["MaxAngularVelocity"].as<float>(50.0f);
				component.LockedAxes = static_cast<PhysicsActorAxis>(rigidBodyComponent["LockedAxes"].as<uint32_t>(0));
			}

			YAML::Node boxColliderComponent = entity["BoxColliderComponent"];
			if (boxColliderComponent)
			{
				BoxColliderComponent& component = deserializedEntity.AddComponent<BoxColliderComponent>();
				component.HalfSize = boxColliderComponent["HalfSize"].as<glm::vec3>(glm::vec3{ 0.5f, 0.5f, 0.5f });
				component.Offset = boxColliderComponent["Offset"].as<glm::vec3>(glm::vec3{ 0.0f, 0.0f, 0.0f });
				component.Material.Friction = boxColliderComponent["Friction"].as<float>(0.5f);
				component.Material.Restitution = boxColliderComponent["Restitution"].as<float>(0.0f);
			}

			YAML::Node sphereColliderComponent = entity["SphereColliderComponent"];
			if (sphereColliderComponent)
			{
				SphereColliderComponent& component = deserializedEntity.AddComponent<SphereColliderComponent>();
				component.Offset = sphereColliderComponent["Offset"].as<glm::vec3>(glm::vec3{ 0.0f, 0.0f, 0.0f });
				component.Radius = sphereColliderComponent["Radius"].as<float>(0.5f);
				component.Material.Friction = sphereColliderComponent["Friction"].as<float>(0.5f);
				component.Material.Restitution = sphereColliderComponent["Restitution"].as<float>(0.0f);
			}

			YAML::Node cylinderColliderComponent = entity["CylinderColliderComponent"];
			if (cylinderColliderComponent)
			{
				CylinderColliderComponent& component = deserializedEntity.AddComponent<CylinderColliderComponent>();
				component.Offset = cylinderColliderComponent["Offset"].as<glm::vec3>(glm::vec3{ 0.0f, 0.0f, 0.0f });
				component.Radius = cylinderColliderComponent["Radius"].as<float>(0.5f);
				component.HalfHeight = cylinderColliderComponent["HalfHeight"].as<float>(0.5f);
				component.Material.Friction = cylinderColliderComponent["Friction"].as<float>(0.5f);
				component.Material.Restitution = cylinderColliderComponent["Restitution"].as<float>(0.0f);
			}

			YAML::Node capsuleColliderComponent = entity["CapsuleColliderComponent"];
			if (capsuleColliderComponent)
			{
				CapsuleColliderComponent& component = deserializedEntity.AddComponent<CapsuleColliderComponent>();
				component.Offset = capsuleColliderComponent["Offset"].as<glm::vec3>(glm::vec3{ 0.0f, 0.0f, 0.0f });
				component.Radius = capsuleColliderComponent["Radius"].as<float>(0.5f);
				component.HalfHeight = capsuleColliderComponent["HalfHeight"].as<float>(0.5f);
				component.Material.Friction = capsuleColliderComponent["Friction"].as<float>(0.5f);
				component.Material.Restitution = capsuleColliderComponent["Restitution"].as<float>(0.0f);
			}

			YAML::Node compoundColliderComponent = entity["CompoundColliderComponent"];
			if (compoundColliderComponent)
			{
				CompoundColliderComponent& component = deserializedEntity.AddComponent<CompoundColliderComponent>();
				component.IsImmutable = compoundColliderComponent["IsImmutable"].as<bool>(true);
				component.IncludeStaticChildColliders = compoundColliderComponent["IncludeStaticChildColliders"].as<bool>(true);

				YAML::Node compoundedChildEntitiesNode = compoundColliderComponent["CompoundedColliderEntities"];
				if (compoundedChildEntitiesNode)
				{
					component.CompoundedColliderEntities.reserve(compoundedChildEntitiesNode.size());
					for (auto uuid : compoundedChildEntitiesNode)
						component.CompoundedColliderEntities.push_back(uuid.as<uint64_t>());
				}
			}

			YAML::Node rigidBody2DComponent = entity["RigidBody2DComponent"];
			if (rigidBody2DComponent)
			{
				RigidBody2DComponent& component = deserializedEntity.AddComponent<RigidBody2DComponent>();
				component.BodyType = static_cast<PhysicsBodyType>(rigidBody2DComponent["BodyType"].as<int>(0));
				component.FixedRotation = rigidBody2DComponent["FixedRotation"].as<bool>(false);
				component.Mass = rigidBody2DComponent["Mass"].as<float>(1.0f);
				component.LinearDrag = rigidBody2DComponent["LinearDrag"].as<float>(0.01f);
				component.AngularDrag = rigidBody2DComponent["AngularDrag"].as<float>(0.05f);
				component.GravityScale = rigidBody2DComponent["GravityScale"].as<float>(1.0f);
			}

			YAML::Node boxCollider2DComponent = entity["BoxCollider2DComponent"];
			if (boxCollider2DComponent)
			{
				BoxCollider2DComponent& component = deserializedEntity.AddComponent<BoxCollider2DComponent>();
				component.HalfSize = boxCollider2DComponent["HalfSize"].as<glm::vec2>(glm::vec2{ 0.5f, 0.5f });
				component.Offset = boxCollider2DComponent["Offset"].as<glm::vec2>(glm::vec2{ 0.0f, 0.0f });
				component.Density = boxCollider2DComponent["Density"].as<float>(1.0f);
				component.Material.Friction = boxCollider2DComponent["Friction"].as<float>(0.5f);
				component.Material.Restitution = boxCollider2DComponent["Restitution"].as<float>(0.0f);
				component.RestitutionThreshold = boxCollider2DComponent["RestitutionThreshold"].as<float>(0.5f);
			}

			YAML::Node circleCollider2DComponent = entity["CircleCollider2DComponent"];
			if (circleCollider2DComponent)
			{
				CircleCollider2DComponent& component = deserializedEntity.AddComponent<CircleCollider2DComponent>();
				component.Offset = circleCollider2DComponent["Offset"].as<glm::vec2>(glm::vec2{ 0.0f, 0.0f });
				component.Radius = circleCollider2DComponent["Radius"].as<float>(1.0f);
				component.Density = circleCollider2DComponent["Density"].as<float>(1.0f);
				component.Material.Friction = circleCollider2DComponent["Friction"].as<float>(0.5f);
				component.Material.Restitution = circleCollider2DComponent["Restitution"].as<float>(0.0f);
				component.RestitutionThreshold = circleCollider2DComponent["RestitutionThreshold"].as<float>(0.5f);
			}
		}

		scene->SortEntities();
	}

	void SceneSerializer::Serialize(Ref<Scene> scene, const std::filesystem::path& filePath)
	{
		YAML::Emitter out;

		out << YAML::BeginMap;

		out << YAML::Key << "Scene" << YAML::Value << scene->GetName();

		{
			out << YAML::Key << "VisibleEntities" << YAML::Value << YAML::BeginSeq;

			auto visibleEntities = scene->GetAllEntitiesWith<VisibleComponent>();
			for (auto e : visibleEntities)
			{
				Entity entity = { e, scene.Raw() };
				out << static_cast<uint64_t>(entity.GetUUID());
			}

			out << YAML::EndSeq;
		}

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

			// Visible Entities
			std::vector<UUID> visibleEntities;
			YAML::Node visibleEntitiesNode = data["VisibleEntities"];
			if (visibleEntitiesNode)
			{
				visibleEntities.reserve(visibleEntitiesNode.size());
				for (auto uuid : visibleEntitiesNode)
					visibleEntities.push_back(uuid.as<uint64_t>());
			}

			auto entities = data["Entities"];
			if(entities)
				DeserializeEntity(entities, scene, visibleEntities);

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