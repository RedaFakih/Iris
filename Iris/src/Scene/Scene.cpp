#include "IrisPCH.h"
#include "Scene.h"

#include "AssetManager/AssetManager.h"
#include "Editor/SelectionManager.h"
#include "Physics/Physics.h"
#include "Physics/PhysicsScene.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer/SceneRendererLite.h"
#include "Renderer/Shaders/Shader.h"

namespace Iris {

	namespace Utils {

		static b2BodyType IrisRigidBody2DTypeToB2BodyType(PhysicsBodyType bodyType)
		{
			switch (bodyType)
			{
				case PhysicsBodyType::Static:		return b2_staticBody;
				case PhysicsBodyType::Dynamic:		return b2_dynamicBody;
				case PhysicsBodyType::Kinematic:	return b2_kinematicBody;
			}

			IR_VERIFY(false, "Unreachable");
			return static_cast<b2BodyType>(0);
		}

	}

	Ref<Scene> Scene::Create(const std::string& name, bool isEditorScene)
	{
		return CreateRef<Scene>(name, isEditorScene);
	}

	Ref<Scene> Scene::CreateEmpty()
	{
		return CreateRef<Scene>("Empty", false);
	}

	Ref<Scene> Scene::GetScene(UUID uuid)
	{
		if (s_ActiveScenes.contains(uuid))
			return s_ActiveScenes.at(uuid);

		return {};
	}

	Scene::Scene(const std::string& name, bool isEditorScene)
		: m_Name(name), m_IsEditorScene(isEditorScene)
	{
		m_SceneEntity = m_Registry.create();
		s_ActiveScenes[m_SceneID] = this;

		m_Registry.on_construct<MeshColliderComponent>().connect<&Scene::OnMeshColliderComponentConstruct>(this);

		m_Registry.on_construct<StaticMeshComponent>().connect<&Scene::OnRenderableComponentChanged>(this);
		m_Registry.on_destroy <StaticMeshComponent>().connect<&Scene::OnRenderableComponentChanged>(this);
		m_Registry.on_construct<VisibleComponent>().connect<&Scene::OnRenderableComponentChanged>(this);
		m_Registry.on_destroy <VisibleComponent>().connect<&Scene::OnRenderableComponentChanged>(this);
	}

	Scene::~Scene()
	{
		// NOTE: VERY ugly hack around SkyLight GPU leaks
		// This should be handled by the AssetManager when we create some system to auto detect when it should release assets...
		// But for now this is good enough since that is a hard system to get in the engine since it requires alot of architecture
		auto lights = m_Registry.view<SkyLightComponent>();
		for (auto entity : lights)
		{
			const SkyLightComponent& skyLightComponent = lights.get<SkyLightComponent>(entity);
			if (AssetManager::IsAssetHandleValid(skyLightComponent.SceneEnvironment) && skyLightComponent.DynamicSky)
				AssetManager::RemoveAsset(skyLightComponent.SceneEnvironment);
		}

		s_ActiveScenes.erase(m_SceneID);
		m_Registry.clear();
	}

	void Scene::CopyTo(Ref<Scene>& targetScene)
	{
		// NOTE: Hack to prevent physics bodies from being created on copy via entt signals
		targetScene->m_IsEditorScene = true;

		targetScene->m_Environment = m_Environment;
		targetScene->m_EnvironmentIntensity = m_EnvironmentIntensity;

		targetScene->m_LightEnvironment = m_LightEnvironment;
		targetScene->m_SkyboxLod = m_SkyboxLod;

		std::unordered_map<UUID, entt::entity> enttMap;
		auto idComponents = m_Registry.view<IDComponent>();
		for (auto entity : idComponents)
		{
			auto uuid = m_Registry.get<IDComponent>(entity).ID;
			auto name = m_Registry.get<TagComponent>(entity).Tag;
			Entity e = targetScene->CreateEntityWithUUID(uuid, name, false);

			// Take care of entity visibility
			if (m_EntityIDMap.at(uuid).HasComponent<VisibleComponent>())
				e.AddComponent<VisibleComponent>();

			enttMap[uuid] = e.m_EntityHandle;
		}

		targetScene->SortEntities();

		CopyComponent<TagComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<RelationshipComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<TransformComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<CameraComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<SpriteRendererComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<StaticMeshComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<SkyLightComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<DirectionalLightComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<PointLightComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<SpotLightComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<TextComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<RigidBodyComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<BoxColliderComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<SphereColliderComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<CylinderColliderComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<CapsuleColliderComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<CompoundColliderComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<MeshColliderComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<RigidBody2DComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<BoxCollider2DComponent>(targetScene->m_Registry, m_Registry, enttMap);
		CopyComponent<CircleCollider2DComponent>(targetScene->m_Registry, m_Registry, enttMap);

		targetScene->SetPhysics2DGravity(m_Box2DGravity);


		targetScene->m_ViewportWidth = m_ViewportWidth;
		targetScene->m_ViewportHeight = m_ViewportHeight;

		targetScene->m_IsEditorScene = false;
	}

	void Scene::OnUpdateRuntime(TimeStep ts)
	{
		// Box2D Physics
		Scope<b2World>& world2D = m_Registry.get<Box2DWorldComponent>(m_SceneEntity).World;

		{
			Timer timer;
			world2D->Step(ts, PhysicsSystem::GetSettings().VelocitySolverIterations, PhysicsSystem::GetSettings().PositionSolverIterations);
		}

		{
			auto view = m_Registry.view<RigidBody2DComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				RigidBody2DComponent& rigidBodyComp = e.GetComponent<RigidBody2DComponent>();

				if (!rigidBodyComp.RuntimeBody)
					continue;

				b2Body* body = static_cast<b2Body*>(rigidBodyComp.RuntimeBody);

				const b2Vec2& position = body->GetPosition();
				TransformComponent& transformComp = e.Transform();

				transformComp.Translation.x = position.x;
				transformComp.Translation.y = position.y;

				glm::vec3 rotation = transformComp.GetRotationEuler();
				rotation.z = body->GetAngle();
				transformComp.SetRotationEuler(rotation);
			}
		}


		if (m_ShouldSimulate)
		{
			Ref<PhysicsScene> physicsScene = GetPhysicsScene();

			{
				Timer timer;
				physicsScene->Simulate(ts);
			}
		}
	}

	void Scene::OnRenderRuntime(Ref<SceneRenderer> renderer)
	{
		Entity cameraEntity = GetMainCameraEntity();
		if (!cameraEntity)
		{
			IR_CORE_ERROR("*******NO CAMERA FOUND**********");
			return;
		}

		// Process all lighting data in the scene
		{
			m_LightEnvironment = LightEnvironment();
			bool foundLinkedDirLight = false;
			bool usingDynamicSky = false;

			// Skylights (NOTE: Should only really have one in the scene and no more!)
			{
				auto view = GetAllEntitiesWith<SkyLightComponent>();

				for (auto entity : view)
				{
					SkyLightComponent& skyLightComponent = view.get<SkyLightComponent>(entity);

					if (!AssetManager::IsAssetHandleValid(skyLightComponent.SceneEnvironment) && skyLightComponent.DynamicSky)
					{
						Ref<TextureCube> preethamEnv = Renderer::CreatePreethamSky(skyLightComponent.TurbidityAzimuthInclinationSunSize.x, skyLightComponent.TurbidityAzimuthInclinationSunSize.y, skyLightComponent.TurbidityAzimuthInclinationSunSize.z, skyLightComponent.TurbidityAzimuthInclinationSunSize.w);
						skyLightComponent.SceneEnvironment = AssetManager::CreateMemoryOnlyAsset<Environment>(preethamEnv, preethamEnv);
					}

					m_Environment = AssetManager::GetAssetAsync<Environment>(skyLightComponent.SceneEnvironment);
					m_EnvironmentIntensity = skyLightComponent.Intensity;
					m_SkyboxLod = skyLightComponent.Lod;

					usingDynamicSky = skyLightComponent.DynamicSky;
					if (usingDynamicSky)
					{
						// Check if we have a linked directional light
						Entity dirLightEntity = TryGetEntityWithUUID(skyLightComponent.DirectionalLightEntityID);
						if (dirLightEntity)
						{
							DirectionalLightComponent* dirLightComp = dirLightEntity.TryGetComponent<DirectionalLightComponent>();
							if (dirLightComp)
							{
								foundLinkedDirLight = true;

								glm::vec3 direction = glm::normalize(glm::vec3(
									glm::sin(skyLightComponent.TurbidityAzimuthInclinationSunSize.z) * glm::cos(skyLightComponent.TurbidityAzimuthInclinationSunSize.y),
									glm::cos(skyLightComponent.TurbidityAzimuthInclinationSunSize.z),
									glm::sin(skyLightComponent.TurbidityAzimuthInclinationSunSize.z) * glm::sin(skyLightComponent.TurbidityAzimuthInclinationSunSize.y)
								));

								constexpr glm::vec3 horizonColor = { 1.0f, 0.5f, 0.0f }; // Reddish color at the horizon
								constexpr glm::vec3 zenithColor = { 1.0f, 1.0f, 0.9f }; // Whitish color at the zenith

								m_LightEnvironment.DirectionalLight = SceneDirectionalLight{
									.Direction = direction,
									.Radiance = skyLightComponent.LinkDirectionalLightRadiance ? glm::mix(horizonColor, zenithColor, glm::cos(skyLightComponent.TurbidityAzimuthInclinationSunSize.z)) : dirLightComp->Radiance,
									.Intensity = dirLightComp->Intensity,
									.ShadowOpacity = dirLightComp->ShadowOpacity,
									.LightSize = dirLightComp->LightSize,
									.CastShadows = dirLightComp->CastShadows
								};

								if (skyLightComponent.LinkDirectionalLightRadiance)
									dirLightComp->Radiance = m_LightEnvironment.DirectionalLight.Radiance;
							}
						}
					}

					if ((!usingDynamicSky || !foundLinkedDirLight) && skyLightComponent.LinkDirectionalLightRadiance == true)
						skyLightComponent.LinkDirectionalLightRadiance = false;
				}

				if (!m_Environment || view.empty()) // Invalid skylight (We dont have one or the handle was invalid)
				{
					m_Environment = Environment::Create(Renderer::GetBlackCubeTexture(), Renderer::GetBlackCubeTexture());
					m_EnvironmentIntensity = 1.0f;
					m_SkyboxLod = 0.0f;
				}
			}

			// Directional Light
			{
				if (!usingDynamicSky || (usingDynamicSky && !foundLinkedDirLight))
				{
					auto view = GetAllEntitiesWith<TransformComponent, DirectionalLightComponent>();

					uint32_t dirLightCountIndex = 0;
					for (auto entity : view)
					{
						if (dirLightCountIndex >= 1)
							break;
						IR_VERIFY(dirLightCountIndex++ < LightEnvironment::MaxDirectionalLights, "We can't have more than {} directional lights in one scene!", LightEnvironment::MaxDirectionalLights);

						const auto& [transformComp, dirLightComp] = view.get<TransformComponent, DirectionalLightComponent>(entity);
						glm::vec3 direction = -glm::normalize(glm::mat3(transformComp.GetTransform()) * glm::vec3(1.0f));
						m_LightEnvironment.DirectionalLight = SceneDirectionalLight{
							.Direction = direction,
							.Radiance = dirLightComp.Radiance,
							.Intensity = dirLightComp.Intensity,
							.ShadowOpacity = dirLightComp.ShadowOpacity,
							.LightSize = dirLightComp.LightSize,
							.CastShadows = dirLightComp.CastShadows
						};
					}
				}
			}

			// Point Lights
			{
				auto view = GetAllEntitiesWith<TransformComponent, PointLightComponent, VisibleComponent>();

				m_LightEnvironment.PointLights.resize(view.size());

				uint32_t pointLightIndex = 0;
				for (auto entity : view)
				{
					const auto& [transformComp, pointLightComp] = view.get<TransformComponent, PointLightComponent>(entity);
					TransformComponent transform = GetWorldSpaceTransform({ entity, this });
					m_LightEnvironment.PointLights[pointLightIndex++] = ScenePointLight{
						.Position = transform.Translation,
						.Intensity = pointLightComp.Intensity,
						.Radiance = pointLightComp.Radiance,
						.Radius = pointLightComp.Radius,
						.Falloff = pointLightComp.FallOff
					};
				}
			}

			// Spot Lights
			{
				auto view = GetAllEntitiesWith<TransformComponent, SpotLightComponent, VisibleComponent>();

				m_LightEnvironment.SpotLights.resize(view.size());

				uint32_t spotLightIndex = 0;
				for (auto entity : view)
				{
					Entity iEntity = { entity, this };

					const auto& [transformComp, spotLightComp] = view.get<TransformComponent, SpotLightComponent>(entity);
					TransformComponent transform = iEntity.HasComponent<RigidBodyComponent>() ? iEntity.Transform() : GetWorldSpaceTransform(iEntity);

					m_LightEnvironment.SpotLights[spotLightIndex++] = SceneSpotLight{
						.Position = transform.Translation,
						.Intensity = spotLightComp.Intensity,
						.Direction = glm::normalize(glm::rotate(transform.GetRotation(), { 1.0f, 0.0f, 0.0f })),
						.AngleAttenuation = spotLightComp.AngleAttenuation,
						.Radiance = spotLightComp.Radiance,
						.Range = spotLightComp.Range,
						.Angle = spotLightComp.Angle,
						.Falloff = spotLightComp.FallOff
					};
				}
			}
		}

		glm::mat4 cameraViewMatrix = glm::inverse(GetWorldSpaceTransformMatrix(cameraEntity));
		SceneCamera& camera = cameraEntity.GetComponent<CameraComponent>().Camera;
		camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);

		{
			renderer->SetScene(this);
			renderer->BeginScene({ camera, cameraViewMatrix, camera.GetPerspectiveNearClip(), camera.GetPerspectiveFarClip(), camera.GetRadPerspectiveVerticalFOV() });

			// Render static meshes
			auto entities = GetAllEntitiesWith<StaticMeshComponent, VisibleComponent>();
			for (auto entity : entities)
			{
				const StaticMeshComponent& staticMeshComponenet = entities.get<StaticMeshComponent>(entity);

				AsyncAssetResult<StaticMesh> staticMeshResult = AssetManager::GetAssetAsync<StaticMesh>(staticMeshComponenet.StaticMesh);
				if (!staticMeshResult.IsReady)
					continue;
			
				Ref<StaticMesh> staticMesh = staticMeshResult;
				AsyncAssetResult<MeshSource> meshSourceResult = AssetManager::GetAssetAsync<MeshSource>(staticMesh->GetMeshSource());
				if (!meshSourceResult.IsReady)
					continue;
			
				Ref<MeshSource> meshSource = meshSourceResult;

				Entity e = { entity, this };
				glm::mat4 transform = GetWorldSpaceTransformMatrix(e);

				if (SelectionManager::IsEntityOrAncestorSelected(e))
					renderer->SubmitSelectedStaticMesh(staticMesh, meshSource, staticMeshComponenet.MaterialTable, transform);
				else
					renderer->SubmitStaticMesh(staticMesh, meshSource, staticMeshComponenet.MaterialTable, transform);
			}

			RenderPhysicsDebug(renderer);

			renderer->EndScene();

			if (renderer->GetFinalPassImage())
			{
				Ref<Renderer2D> renderer2D = renderer->GetRenderer2D();

				renderer2D->ResetStats();
				renderer2D->BeginScene(camera.GetProjectionMatrix() * cameraViewMatrix, cameraViewMatrix);

				// Render 2D sprites
				{
					auto view = GetAllEntitiesWith<SpriteRendererComponent, VisibleComponent>();
					for (auto entity : view)
					{
						Entity e = { entity, this };

						const SpriteRendererComponent& spriteRendererComponent = view.get<SpriteRendererComponent>(entity);

						if (spriteRendererComponent.Texture)
						{
							if (AssetManager::IsAssetHandleValid(spriteRendererComponent.Texture))
							{
								Ref<Texture2D> texture = AssetManager::GetAssetAsync<Texture2D>(spriteRendererComponent.Texture);
								renderer2D->DrawQuad(
									GetWorldSpaceTransformMatrix(e),
									texture,
									spriteRendererComponent.TilingFactor,
									spriteRendererComponent.Color,
									spriteRendererComponent.UV0,
									spriteRendererComponent.UV1
								);
							}
						}
						else
						{
							renderer2D->DrawQuad(GetWorldSpaceTransformMatrix(e), spriteRendererComponent.Color);
						}
					}
				}

				// Render Text
				{
					auto view = GetAllEntitiesWith<TextComponent, VisibleComponent>();
					for (auto entity : view)
					{
						Entity e = { entity, this };

						const TextComponent& textComponent = view.get<TextComponent>(entity);

						Ref<Font> font = Font::GetFontAssetForTextComponent(textComponent.Font);
						renderer2D->DrawString(textComponent.TextString, font, GetWorldSpaceTransformMatrix(e), textComponent.MaxWidth, textComponent.Color, textComponent.LineSpacing, textComponent.Kerning);
					}
				}

				// Save line width since debug renderer may change that...
				float lineWidth = renderer2D->GetLineWidth();

				// TODO: Debug Renderer, eventhough the line width here might conflict with the renderer2D's line width since the vulkan command to change
				// it is only called once on EndScene so if the debug renderer sets it then all the lines will be rendered with that width

				// Render 2D Physics Debug
				Render2DPhysicsDebug(renderer, renderer2D);

				renderer2D->EndScene();
				// Restore the line width
				renderer2D->SetLineWidth(lineWidth);
			}
		}
	}

	void Scene::OnRenderRuntime(Ref<SceneRendererLite> renderer)
	{
		Entity cameraEntity = GetMainCameraEntity();
		if (!cameraEntity)
		{
			IR_CORE_ERROR("*******NO CAMERA FOUND**********");
			return;
		}

		// Process all lighting data in the scene
		{
			m_LightEnvironment = LightEnvironment();
			bool foundLinkedDirLight = false;
			bool usingDynamicSky = false;

			// Skylights (NOTE: Should only really have one in the scene and no more!)
			{
				auto view = GetAllEntitiesWith<SkyLightComponent>();

				for (auto entity : view)
				{
					SkyLightComponent& skyLightComponent = view.get<SkyLightComponent>(entity);

					if (!AssetManager::IsAssetHandleValid(skyLightComponent.SceneEnvironment) && skyLightComponent.DynamicSky)
					{
						Ref<TextureCube> preethamEnv = Renderer::CreatePreethamSky(skyLightComponent.TurbidityAzimuthInclinationSunSize.x, skyLightComponent.TurbidityAzimuthInclinationSunSize.y, skyLightComponent.TurbidityAzimuthInclinationSunSize.z, skyLightComponent.TurbidityAzimuthInclinationSunSize.w);
						skyLightComponent.SceneEnvironment = AssetManager::CreateMemoryOnlyAsset<Environment>(preethamEnv, preethamEnv);
					}

					m_Environment = AssetManager::GetAssetAsync<Environment>(skyLightComponent.SceneEnvironment);
					m_EnvironmentIntensity = skyLightComponent.Intensity;
					m_SkyboxLod = skyLightComponent.Lod;

					usingDynamicSky = skyLightComponent.DynamicSky;
					if (usingDynamicSky)
					{
						// Check if we have a linked directional light
						Entity dirLightEntity = TryGetEntityWithUUID(skyLightComponent.DirectionalLightEntityID);
						if (dirLightEntity)
						{
							DirectionalLightComponent* dirLightComp = dirLightEntity.TryGetComponent<DirectionalLightComponent>();
							if (dirLightComp)
							{
								foundLinkedDirLight = true;

								glm::vec3 direction = glm::normalize(glm::vec3(
									glm::sin(skyLightComponent.TurbidityAzimuthInclinationSunSize.z) * glm::cos(skyLightComponent.TurbidityAzimuthInclinationSunSize.y),
									glm::cos(skyLightComponent.TurbidityAzimuthInclinationSunSize.z),
									glm::sin(skyLightComponent.TurbidityAzimuthInclinationSunSize.z) * glm::sin(skyLightComponent.TurbidityAzimuthInclinationSunSize.y)
								));

								constexpr glm::vec3 horizonColor = { 1.0f, 0.5f, 0.0f }; // Reddish color at the horizon
								constexpr glm::vec3 zenithColor = { 1.0f, 1.0f, 0.9f }; // Whitish color at the zenith

								m_LightEnvironment.DirectionalLight = SceneDirectionalLight{
									.Direction = direction,
									.Radiance = skyLightComponent.LinkDirectionalLightRadiance ? glm::mix(horizonColor, zenithColor, glm::cos(skyLightComponent.TurbidityAzimuthInclinationSunSize.z)) : dirLightComp->Radiance,
									.Intensity = dirLightComp->Intensity,
									.ShadowOpacity = dirLightComp->ShadowOpacity,
									.LightSize = dirLightComp->LightSize,
									.CastShadows = dirLightComp->CastShadows
								};

								if (skyLightComponent.LinkDirectionalLightRadiance)
									dirLightComp->Radiance = m_LightEnvironment.DirectionalLight.Radiance;
							}
						}
					}

					if ((!usingDynamicSky || !foundLinkedDirLight) && skyLightComponent.LinkDirectionalLightRadiance == true)
						skyLightComponent.LinkDirectionalLightRadiance = false;
				}

				if (!m_Environment || view.empty()) // Invalid skylight (We dont have one or the handle was invalid)
				{
					m_Environment = Environment::Create(Renderer::GetBlackCubeTexture(), Renderer::GetBlackCubeTexture());
					m_EnvironmentIntensity = 1.0f;
					m_SkyboxLod = 0.0f;
				}
			}

			// Directional Light
			{
				if (!usingDynamicSky || (usingDynamicSky && !foundLinkedDirLight))
				{
					auto view = GetAllEntitiesWith<TransformComponent, DirectionalLightComponent>();

					uint32_t dirLightCountIndex = 0;
					for (auto entity : view)
					{
						if (dirLightCountIndex >= 1)
							break;
						IR_VERIFY(dirLightCountIndex++ < LightEnvironment::MaxDirectionalLights, "We can't have more than {} directional lights in one scene!", LightEnvironment::MaxDirectionalLights);

						const auto& [transformComp, dirLightComp] = view.get<TransformComponent, DirectionalLightComponent>(entity);
						glm::vec3 direction = -glm::normalize(glm::mat3(transformComp.GetTransform()) * glm::vec3(1.0f));
						m_LightEnvironment.DirectionalLight = SceneDirectionalLight{
							.Direction = direction,
							.Radiance = dirLightComp.Radiance,
							.Intensity = dirLightComp.Intensity,
							.ShadowOpacity = dirLightComp.ShadowOpacity,
							.LightSize = dirLightComp.LightSize,
							.CastShadows = dirLightComp.CastShadows
						};
					}
				}
			}
		}

		glm::mat4 cameraViewMatrix = glm::inverse(GetWorldSpaceTransformMatrix(cameraEntity));
		SceneCamera& camera = cameraEntity.GetComponent<CameraComponent>().Camera;
		camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);

		{
			renderer->SetScene(this);
			renderer->BeginScene({ camera, cameraViewMatrix, camera.GetPerspectiveNearClip(), camera.GetPerspectiveFarClip(), camera.GetRadPerspectiveVerticalFOV() });

			// Render static meshes
			auto entities = GetAllEntitiesWith<StaticMeshComponent, VisibleComponent>();
			for (auto entity : entities)
			{
				const StaticMeshComponent& staticMeshComponenet = entities.get<StaticMeshComponent>(entity);

				AsyncAssetResult<StaticMesh> staticMeshResult = AssetManager::GetAssetAsync<StaticMesh>(staticMeshComponenet.StaticMesh);
				if (!staticMeshResult.IsReady)
					continue;
				
				Ref<StaticMesh> staticMesh = staticMeshResult;
				AsyncAssetResult<MeshSource> meshSourceResult = AssetManager::GetAssetAsync<MeshSource>(staticMesh->GetMeshSource());
				if (!meshSourceResult.IsReady)
					continue;

				Ref<MeshSource> meshSource = meshSourceResult;

				Entity e = { entity, this };
				glm::mat4 transform = GetWorldSpaceTransformMatrix(e);

				renderer->SubmitStaticMesh(staticMesh, meshSource, staticMeshComponenet.MaterialTable, transform);
			}

			renderer->EndScene();
		}
	}

	void Scene::OnUpdateEditor(TimeStep ts)
	{
		// NOTE: Should update some state for physics/scripting/animations but for now nothing...
	}

	void Scene::OnRenderEditor(Ref<SceneRenderer> renderer, const EditorCamera& camera, SelectionHighlightMask selctionHighlightMask)
	{
		// Render the scene

		// Process all lighting data in the scene
		{
			m_LightEnvironment = LightEnvironment();
			bool foundLinkedDirLight = false;
			bool usingDynamicSky = false;

			// Skylights (NOTE: Should only really have one in the scene and no more!)
			{
				auto view = GetAllEntitiesWith<SkyLightComponent>();

				for (auto entity : view)
				{
					SkyLightComponent& skyLightComponent = view.get<SkyLightComponent>(entity);

					if (!AssetManager::IsAssetHandleValid(skyLightComponent.SceneEnvironment) && skyLightComponent.DynamicSky)
					{
						Ref<TextureCube> preethamEnv = Renderer::CreatePreethamSky(skyLightComponent.TurbidityAzimuthInclinationSunSize.x, skyLightComponent.TurbidityAzimuthInclinationSunSize.y, skyLightComponent.TurbidityAzimuthInclinationSunSize.z, skyLightComponent.TurbidityAzimuthInclinationSunSize.w);
						skyLightComponent.SceneEnvironment = AssetManager::CreateMemoryOnlyAsset<Environment>(preethamEnv, preethamEnv);
					}

					m_Environment = AssetManager::GetAssetAsync<Environment>(skyLightComponent.SceneEnvironment);
					m_EnvironmentIntensity = skyLightComponent.Intensity;
					m_SkyboxLod = skyLightComponent.Lod;

					usingDynamicSky = skyLightComponent.DynamicSky;
					if (usingDynamicSky)
					{
						// Check if we have a linked directional light
						Entity dirLightEntity = TryGetEntityWithUUID(skyLightComponent.DirectionalLightEntityID);
						if (dirLightEntity)
						{
							DirectionalLightComponent* dirLightComp = dirLightEntity.TryGetComponent<DirectionalLightComponent>();
							if (dirLightComp)
							{
								foundLinkedDirLight = true;

								glm::vec3 direction = glm::normalize(glm::vec3(
									glm::sin(skyLightComponent.TurbidityAzimuthInclinationSunSize.z) * glm::cos(skyLightComponent.TurbidityAzimuthInclinationSunSize.y),
									glm::cos(skyLightComponent.TurbidityAzimuthInclinationSunSize.z),
									glm::sin(skyLightComponent.TurbidityAzimuthInclinationSunSize.z) * glm::sin(skyLightComponent.TurbidityAzimuthInclinationSunSize.y)
								));

								constexpr glm::vec3 horizonColor = { 1.0f, 0.5f, 0.0f }; // Reddish color at the horizon
								constexpr glm::vec3 zenithColor = { 1.0f, 1.0f, 0.9f }; // Whitish color at the zenith

								m_LightEnvironment.DirectionalLight = SceneDirectionalLight{
									.Direction = direction,
									.Radiance = skyLightComponent.LinkDirectionalLightRadiance ? glm::mix(horizonColor, zenithColor, glm::cos(skyLightComponent.TurbidityAzimuthInclinationSunSize.z)) : dirLightComp->Radiance,
									.Intensity = dirLightComp->Intensity,
									.ShadowOpacity = dirLightComp->ShadowOpacity,
									.LightSize = dirLightComp->LightSize,
									.CastShadows = dirLightComp->CastShadows
								};

								if (skyLightComponent.LinkDirectionalLightRadiance)
									dirLightComp->Radiance = m_LightEnvironment.DirectionalLight.Radiance;
							}
						}
					}

					if ((!usingDynamicSky || !foundLinkedDirLight) && skyLightComponent.LinkDirectionalLightRadiance == true)
						skyLightComponent.LinkDirectionalLightRadiance = false;
				}

				if (!m_Environment || view.empty()) // Invalid skylight (We dont have one or the handle was invalid)
				{
					m_Environment = Environment::Create(Renderer::GetBlackCubeTexture(), Renderer::GetBlackCubeTexture());
					m_EnvironmentIntensity = 1.0f;
					m_SkyboxLod = 0.0f;
				}
			}

			// Directional Light
			{
				if (!usingDynamicSky || (usingDynamicSky && !foundLinkedDirLight))
				{
					auto view = GetAllEntitiesWith<TransformComponent, DirectionalLightComponent>();

					uint32_t dirLightCountIndex = 0;
					for (auto entity : view)
					{
						IR_VERIFY(dirLightCountIndex < LightEnvironment::MaxDirectionalLights, "We can't have more than {} directional lights in one scene!", LightEnvironment::MaxDirectionalLights);
						if (dirLightCountIndex++ >= 1)
							break;

						const auto& [transformComp, dirLightComp] = view.get<TransformComponent, DirectionalLightComponent>(entity);
						glm::vec3 direction = glm::normalize(glm::mat3(transformComp.GetTransform()) * glm::vec3(1.0f));
						m_LightEnvironment.DirectionalLight = SceneDirectionalLight{
							.Direction = direction,
							.Radiance = dirLightComp.Radiance,
							.Intensity = dirLightComp.Intensity,
							.ShadowOpacity = dirLightComp.ShadowOpacity,
							.LightSize = dirLightComp.LightSize,
							.CastShadows = dirLightComp.CastShadows
						};
					}
				}
			}

			// Point Lights
			{
				auto view = GetAllEntitiesWith<TransformComponent, PointLightComponent, VisibleComponent>();

				if (view.size() > Renderer::GetConfig().MaxNumberOfPointLights)
					m_LightEnvironment.PointLights.resize(Renderer::GetConfig().MaxNumberOfPointLights);
				else
					m_LightEnvironment.PointLights.resize(view.size());

				uint32_t pointLightIndex = 0;
				for (auto entity : view)
				{
					const auto& [transformComp, pointLightComp] = view.get<TransformComponent, PointLightComponent>(entity);
					TransformComponent transform = GetWorldSpaceTransform({ entity, this });
					m_LightEnvironment.PointLights[pointLightIndex++] = ScenePointLight{
						.Position = transform.Translation,
						.Intensity = pointLightComp.Intensity,
						.Radiance = pointLightComp.Radiance,
						.Radius = pointLightComp.Radius,
						.Falloff = pointLightComp.FallOff
					};

					if (pointLightIndex >= Renderer::GetConfig().MaxNumberOfPointLights)
						break;
				}
			}

			// Spot Lights
			{
				auto view = GetAllEntitiesWith<TransformComponent, SpotLightComponent, VisibleComponent>();

				if (view.size() > Renderer::GetConfig().MaxNumberOfSpotLights)
					m_LightEnvironment.SpotLights.resize(Renderer::GetConfig().MaxNumberOfSpotLights);
				else
					m_LightEnvironment.SpotLights.resize(view.size());


				uint32_t spotLightIndex = 0;
				for (auto entity : view)
				{
					Entity iEntity = { entity, this };

					const auto& [transformComp, spotLightComp] = view.get<TransformComponent, SpotLightComponent>(entity);
					TransformComponent transform = iEntity.HasComponent<RigidBodyComponent>() ? iEntity.Transform() : GetWorldSpaceTransform(iEntity);

					m_LightEnvironment.SpotLights[spotLightIndex++] = SceneSpotLight{
						.Position = transform.Translation,
						.Intensity = spotLightComp.Intensity,
						.Direction = glm::normalize(glm::rotate(transform.GetRotation(), { 1.0f, 0.0f, 0.0f })),
						.AngleAttenuation = spotLightComp.AngleAttenuation,
						.Radiance = spotLightComp.Radiance,
						.Range = spotLightComp.Range,
						.Angle = spotLightComp.Angle,
						.Falloff = spotLightComp.FallOff
					};

					if (spotLightIndex >= Renderer::GetConfig().MaxNumberOfSpotLights)
						break;
				}
			}
		}

		{
			renderer->SetScene(this);
			renderer->BeginScene({ camera, camera.GetViewMatrix(), camera.GetNearClip(), camera.GetFarClip(), camera.GetFOV() });

			// Render static meshes

			const bool anythingSelected = selctionHighlightMask.AnyHighlighted;

			auto visibleStaticMeshes = GetAllEntitiesWith<StaticMeshComponent, VisibleComponent>();
			for (auto entity : visibleStaticMeshes)
			{
				const StaticMeshComponent& staticMeshComponenet = visibleStaticMeshes.get<StaticMeshComponent>(entity);

				AsyncAssetResult<StaticMesh> staticMeshResult = AssetManager::GetAssetAsync<StaticMesh>(staticMeshComponenet.StaticMesh);
				if (!staticMeshResult.IsReady)
					continue;
				
				Ref<StaticMesh> staticMesh = staticMeshResult;
				AsyncAssetResult<MeshSource> meshSourceResult = AssetManager::GetAssetAsync<MeshSource>(staticMesh->GetMeshSource());
				if (!meshSourceResult.IsReady)
					continue;

				Ref<MeshSource> meshSource = meshSourceResult;

				Entity e = { entity, this };
				glm::mat4 transform = GetWorldSpaceTransformMatrix(e);

				if (anythingSelected && selctionHighlightMask.Test(entity))
					renderer->SubmitSelectedStaticMesh(staticMesh, meshSource, staticMeshComponenet.MaterialTable, transform);
				else
					renderer->SubmitStaticMesh(staticMesh, meshSource, staticMeshComponenet.MaterialTable, transform);
			}

			RenderPhysicsDebug(renderer);

			renderer->EndScene();

			if (renderer->GetFinalPassImage())
			{
				Ref<Renderer2D> renderer2D = renderer->GetRenderer2D();
			
				renderer2D->ResetStats();
				renderer2D->BeginScene(camera.GetViewProjection(), camera.GetViewMatrix());

				// Render 2D sprites
				{
					auto view = GetAllEntitiesWith<SpriteRendererComponent, VisibleComponent>();
					for (auto entity : view)
					{
						Entity e = { entity, this };
			
						const SpriteRendererComponent& spriteRendererComponent = view.get<SpriteRendererComponent>(entity);

						if (spriteRendererComponent.Texture)
						{
							if (AssetManager::IsAssetHandleValid(spriteRendererComponent.Texture))
							{
								Ref<Texture2D> texture = AssetManager::GetAssetAsync<Texture2D>(spriteRendererComponent.Texture);
								renderer2D->DrawQuad(
									GetWorldSpaceTransformMatrix(e),
									texture,
									spriteRendererComponent.TilingFactor,
									spriteRendererComponent.Color,
									spriteRendererComponent.UV0,
									spriteRendererComponent.UV1
								);
							}
						}
						else
						{
							renderer2D->DrawQuad(GetWorldSpaceTransformMatrix(e), spriteRendererComponent.Color);
						}
					}
				}

				// Render Text
				{
					auto view = GetAllEntitiesWith<TextComponent, VisibleComponent>();
					for (auto entity : view)
					{
						Entity e = { entity, this };

						const TextComponent& textComponent = view.get<TextComponent>(entity);

						Ref<Font> font = Font::GetFontAssetForTextComponent(textComponent.Font);
						renderer2D->DrawString(textComponent.TextString, font, GetWorldSpaceTransformMatrix(e), textComponent.MaxWidth, textComponent.Color, textComponent.LineSpacing, textComponent.Kerning);
					}
				}
			
				// Save line width since debug renderer may change that...
				float lineWidth = renderer2D->GetLineWidth();
			
				// TODO: Debug Renderer, eventhough the line width here might conflict with the renderer2D's line width since the vulkan command to change
				// it is only called once on EndScene so if the debug renderer sets it then all the lines will be rendered with that width
			
				// Render 2D Physics Debug
				Render2DPhysicsDebug(renderer, renderer2D);

				renderer2D->EndScene();
				// Restore the line width
				renderer2D->SetLineWidth(lineWidth);
			}
		}
	}

	void Scene::OnRenderEditor(Ref<SceneRendererLite> renderer, const EditorCamera& camera)
	{
		// Render the scene

		// Process all lighting data in the scene
		{
			m_LightEnvironment = LightEnvironment();
			bool foundLinkedDirLight = false;
			bool usingDynamicSky = false;

			// Skylights (NOTE: Should only really have one in the scene and no more!)
			{
				auto view = GetAllEntitiesWith<SkyLightComponent>();

				for (auto entity : view)
				{
					SkyLightComponent& skyLightComponent = view.get<SkyLightComponent>(entity);

					if (!AssetManager::IsAssetHandleValid(skyLightComponent.SceneEnvironment) && skyLightComponent.DynamicSky)
					{
						Ref<TextureCube> preethamEnv = Renderer::CreatePreethamSky(skyLightComponent.TurbidityAzimuthInclinationSunSize.x, skyLightComponent.TurbidityAzimuthInclinationSunSize.y, skyLightComponent.TurbidityAzimuthInclinationSunSize.z, skyLightComponent.TurbidityAzimuthInclinationSunSize.w);
						skyLightComponent.SceneEnvironment = AssetManager::CreateMemoryOnlyAsset<Environment>(preethamEnv, preethamEnv);
					}

					m_Environment = AssetManager::GetAssetAsync<Environment>(skyLightComponent.SceneEnvironment);
					m_EnvironmentIntensity = skyLightComponent.Intensity;
					m_SkyboxLod = skyLightComponent.Lod;

					usingDynamicSky = skyLightComponent.DynamicSky;
					if (usingDynamicSky)
					{
						// Check if we have a linked directional light
						Entity dirLightEntity = TryGetEntityWithUUID(skyLightComponent.DirectionalLightEntityID);
						if (dirLightEntity)
						{
							DirectionalLightComponent* dirLightComp = dirLightEntity.TryGetComponent<DirectionalLightComponent>();
							if (dirLightComp)
							{
								foundLinkedDirLight = true;

								glm::vec3 direction = glm::normalize(glm::vec3(
									glm::sin(skyLightComponent.TurbidityAzimuthInclinationSunSize.z) * glm::cos(skyLightComponent.TurbidityAzimuthInclinationSunSize.y),
									glm::cos(skyLightComponent.TurbidityAzimuthInclinationSunSize.z),
									glm::sin(skyLightComponent.TurbidityAzimuthInclinationSunSize.z) * glm::sin(skyLightComponent.TurbidityAzimuthInclinationSunSize.y)
								));

								constexpr glm::vec3 horizonColor = { 1.0f, 0.5f, 0.0f }; // Reddish color at the horizon
								constexpr glm::vec3 zenithColor = { 1.0f, 1.0f, 0.9f }; // Whitish color at the zenith

								m_LightEnvironment.DirectionalLight = SceneDirectionalLight{
									.Direction = direction,
									.Radiance = skyLightComponent.LinkDirectionalLightRadiance ? glm::mix(horizonColor, zenithColor, glm::cos(skyLightComponent.TurbidityAzimuthInclinationSunSize.z)) : dirLightComp->Radiance,
									.Intensity = dirLightComp->Intensity,
									.ShadowOpacity = dirLightComp->ShadowOpacity,
									.LightSize = dirLightComp->LightSize,
									.CastShadows = dirLightComp->CastShadows
								};

								if (skyLightComponent.LinkDirectionalLightRadiance)
									dirLightComp->Radiance = m_LightEnvironment.DirectionalLight.Radiance;
							}
						}
					}

					if ((!usingDynamicSky || !foundLinkedDirLight) && skyLightComponent.LinkDirectionalLightRadiance == true)
						skyLightComponent.LinkDirectionalLightRadiance = false;
				}

				if (!m_Environment || view.empty()) // Invalid skylight (We dont have one or the handle was invalid)
				{
					m_Environment = Environment::Create(Renderer::GetBlackCubeTexture(), Renderer::GetBlackCubeTexture());
					m_EnvironmentIntensity = 1.0f;
					m_SkyboxLod = 0.0f;
				}
			}

			// Directional Light
			{
				if (!usingDynamicSky || (usingDynamicSky && !foundLinkedDirLight))
				{
					auto view = GetAllEntitiesWith<TransformComponent, DirectionalLightComponent>();

					uint32_t dirLightCountIndex = 0;
					for (auto entity : view)
					{
						IR_VERIFY(dirLightCountIndex < LightEnvironment::MaxDirectionalLights, "We can't have more than {} directional lights in one scene!", LightEnvironment::MaxDirectionalLights);
						if (dirLightCountIndex++ >= 1)
							break;

						const auto& [transformComp, dirLightComp] = view.get<TransformComponent, DirectionalLightComponent>(entity);
						glm::vec3 direction = glm::normalize(glm::mat3(transformComp.GetTransform()) * glm::vec3(1.0f));
						m_LightEnvironment.DirectionalLight = SceneDirectionalLight{
							.Direction = direction,
							.Radiance = dirLightComp.Radiance,
							.Intensity = dirLightComp.Intensity,
							.ShadowOpacity = dirLightComp.ShadowOpacity,
							.LightSize = dirLightComp.LightSize,
							.CastShadows = dirLightComp.CastShadows
						};
					}
				}
			}
		}

		{
			renderer->SetScene(this);
			renderer->BeginScene({ camera, camera.GetViewMatrix(), camera.GetNearClip(), camera.GetFarClip(), camera.GetFOV() });

			// Render static meshes
			auto entities = GetAllEntitiesWith<StaticMeshComponent, VisibleComponent>();
			for (auto entity : entities)
			{
				const StaticMeshComponent& staticMeshComponenet = entities.get<StaticMeshComponent>(entity);

				AsyncAssetResult<StaticMesh> staticMeshResult = AssetManager::GetAssetAsync<StaticMesh>(staticMeshComponenet.StaticMesh);
				if (!staticMeshResult.IsReady)
					continue;
				
				Ref<StaticMesh> staticMesh = staticMeshResult;
				AsyncAssetResult<MeshSource> meshSourceResult = AssetManager::GetAssetAsync<MeshSource>(staticMesh->GetMeshSource());
				if (!meshSourceResult.IsReady)
					continue;

				Ref<MeshSource> meshSource = meshSourceResult;

				Entity e = { entity, this };
				glm::mat4 transform = GetWorldSpaceTransformMatrix(e);

				renderer->SubmitStaticMesh(staticMesh, meshSource, staticMeshComponenet.MaterialTable, transform);
			}

			renderer->EndScene();
		}
	}

	void Scene::SetViewportSize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;
	}

	void Scene::OnRuntimeStart()
	{
		m_ShouldSimulate = true;
		m_IsPlaying = true;

		// Create Physics 2D world
		Box2DWorldComponent& b2WorldComp = m_Registry.emplace<Box2DWorldComponent>(m_SceneEntity);
		b2WorldComp.World = CreateScope<b2World>(b2Vec2{ m_Box2DGravity.x, m_Box2DGravity.y });

		{
			auto view = m_Registry.group<RigidBody2DComponent>(entt::get<TransformComponent>);
			for (auto entity : view)
			{
				Entity e = { entity, this };
				UUID entityID = e.GetUUID();
				TransformComponent& transformComp = e.Transform();
				RigidBody2DComponent& rigidBodyComp = e.GetComponent<RigidBody2DComponent>();

				b2BodyDef bodyDef;
				bodyDef.type = Utils::IrisRigidBody2DTypeToB2BodyType(rigidBodyComp.BodyType);
				
				bodyDef.position.Set(transformComp.Translation.x, transformComp.Translation.y);
				bodyDef.angle = transformComp.GetRotationEuler().z;

				b2MassData massData;

				b2Body* body = b2WorldComp.World->CreateBody(&bodyDef);
				body->GetMassData(&massData);
				massData.mass = rigidBodyComp.Mass;
				body->SetMassData(&massData);
				body->SetFixedRotation(rigidBodyComp.FixedRotation);
				body->SetGravityScale(rigidBodyComp.GravityScale);
				body->SetLinearDamping(rigidBodyComp.LinearDrag);
				body->SetAngularDamping(rigidBodyComp.AngularDrag);
				body->GetUserData().pointer = static_cast<uintptr_t>(entityID);
				rigidBodyComp.RuntimeBody = body;
			}
		}

		{
			auto view = m_Registry.view<BoxCollider2DComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				TransformComponent& transformComp = e.Transform();

				BoxCollider2DComponent& boxColliderComp = e.GetComponent<BoxCollider2DComponent>();
				if (e.HasComponent<RigidBody2DComponent>())
				{
					RigidBody2DComponent& rigidBodyComp = e.GetComponent<RigidBody2DComponent>();
					IR_ASSERT(rigidBodyComp.RuntimeBody);

					b2Body* body = static_cast<b2Body*>(rigidBodyComp.RuntimeBody);
					
					b2PolygonShape polygonShape;
					polygonShape.SetAsBox(
						transformComp.Scale.x * boxColliderComp.HalfSize.x,
						transformComp.Scale.y * boxColliderComp.HalfSize.y,
						b2Vec2{ boxColliderComp.Offset.x, boxColliderComp.Offset.y },
						0.0f
					);

					b2FixtureDef fixtureDef;
					fixtureDef.shape = &polygonShape;
					fixtureDef.friction = boxColliderComp.Material.Friction;
					fixtureDef.density = boxColliderComp.Density;
					fixtureDef.restitution = boxColliderComp.Material.Restitution;
					fixtureDef.restitutionThreshold = boxColliderComp.RestitutionThreshold;
					body->CreateFixture(&fixtureDef);
				}
			}
		}

		{
			auto view = m_Registry.view<CircleCollider2DComponent>();
			for (auto entity : view)
			{
				Entity e = { entity, this };
				TransformComponent& transformComp = e.Transform();

				CircleCollider2DComponent& circleColliderComp = m_Registry.get<CircleCollider2DComponent>(entity);
				if (e.HasComponent<RigidBody2DComponent>())
				{
					RigidBody2DComponent& rigidBodyComp = e.GetComponent<RigidBody2DComponent>();
					IR_ASSERT(rigidBodyComp.RuntimeBody);
					b2Body* body = static_cast<b2Body*>(rigidBodyComp.RuntimeBody);

					b2CircleShape circleShape;
					circleShape.m_p = b2Vec2(circleColliderComp.Offset.x, circleColliderComp.Offset.y);
					circleShape.m_radius = transformComp.Scale.x * circleColliderComp.Radius;

					b2FixtureDef fixtureDef;
					fixtureDef.shape = &circleShape;
					fixtureDef.density = circleColliderComp.Density;
					fixtureDef.friction = circleColliderComp.Material.Friction;
					fixtureDef.restitution = circleColliderComp.Material.Restitution;
					fixtureDef.restitutionThreshold = circleColliderComp.RestitutionThreshold;
					body->CreateFixture(&fixtureDef);
				}
			}
		}

		JoltWorldComponent& joltWorldComp = m_Registry.emplace<JoltWorldComponent>(m_SceneEntity);
		joltWorldComp.PhysicsWorld = PhysicsSystem::CreatePhysicsScene(this);
	}

	void Scene::OnRuntimeStop()
	{
		// B2World destructor automatically does all necessary cleanup
		Box2DWorldComponent& b2WorldComp = m_Registry.get<Box2DWorldComponent>(m_SceneEntity);
		b2WorldComp.World = nullptr;
		
		JoltWorldComponent& joltWorldComp = m_Registry.get<JoltWorldComponent>(m_SceneEntity);
		joltWorldComp.PhysicsWorld->Destroy();
		joltWorldComp.PhysicsWorld = nullptr;
		m_Registry.remove<JoltWorldComponent>(m_SceneEntity);

		m_IsPlaying = false;
		m_ShouldSimulate = false;
	}

	Entity Scene::GetMainCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			CameraComponent& comp = view.get<CameraComponent>(entity);
			if (comp.Primary)
			{
				IR_ASSERT(comp.Camera.GetOrthographicSize() || comp.Camera.GetDegPerspectiveVerticalFOV(), "Camera is not fully initialized");

				return { entity, this };
			}
		}

		return {};
	}

	std::vector<UUID> Scene::GetAllChildren(Entity entity) const
	{
		std::vector<UUID> result;

		for (auto childID : entity.Children())
		{
			result.push_back(childID);

			std::vector<UUID> childChildrenIDs = GetAllChildren(GetEntityWithUUID(childID));
			result.reserve(result.size() + childChildrenIDs.size());
			result.insert(result.end(), childChildrenIDs.begin(), childChildrenIDs.end());
		}

		return result;
	}

	Entity Scene::CreateEntity(const std::string& name, bool visible)
	{
		return CreateChildEntity({}, name, visible);
	}

	Entity Scene::CreateChildEntity(Entity parent, const std::string& name, bool visible)
	{
		auto entity = Entity{ m_Registry.create(), this };
		++m_HierarchyVersion;

		auto& idComponent = entity.AddComponent<IDComponent>();
		idComponent.ID = {};

		if (!name.empty())
			entity.AddComponent<TagComponent>(name);

		entity.AddComponent<TransformComponent>();
		entity.AddComponent<RelationshipComponent>();

		if (visible)
			entity.AddComponent<VisibleComponent>();

		if (parent)
			AttachChild(parent, entity);

		m_EntityIDMap[idComponent.ID] = entity;

		SortEntities();

		return entity;
	}

	Entity Scene::CreateEntityWithUUID(UUID id, const std::string& name, bool visible, bool shouldSort)
	{
		Entity entity = { m_Registry.create(), this };
		++m_HierarchyVersion;

		entity.AddComponent<IDComponent>(id);

		if (!name.empty())
			entity.AddComponent<TagComponent>(name);

		entity.AddComponent<TransformComponent>();
		entity.AddComponent<RelationshipComponent>();

		if (visible)
			entity.AddComponent<VisibleComponent>();

		IR_ASSERT(!m_EntityIDMap.contains(id));
		m_EntityIDMap[id] = entity;

		if (shouldSort)
			SortEntities();

		return entity;
	}

	void Scene::DestroyEntity(Entity entity, bool excludeChildren, bool first)
	{
		if (!entity)
			return;

		++m_HierarchyVersion;

		if (!m_IsEditorScene)
		{
			Ref<PhysicsScene> physicsScene = GetPhysicsScene();

			if (entity.HasComponent<RigidBodyComponent>())
				physicsScene->DestroyBody(entity);

			if (entity.HasComponent<RigidBody2DComponent>())
			{
				Scope<b2World>& b2DWorld = m_Registry.get<Box2DWorldComponent>(m_SceneEntity).World;
				b2Body* body = reinterpret_cast<b2Body*>(entity.GetComponent<RigidBody2DComponent>().RuntimeBody);
				b2DWorld->DestroyBody(body);
			}
		}

		if (m_OnEntityDestroyedCallback)
			m_OnEntityDestroyedCallback(entity);

		if (!excludeChildren)
		{
			// Copy here since if we do not then we will have entity leaks since the collection is mutated during recursion
			std::vector<UUID> children = entity.Children();
			for (UUID childId : children)
			{
				Entity child = TryGetEntityWithUUID(childId);
				if (!child)
					continue;

				// Detach FIRST: no live entity's Children list may reference a dead
				// UUID while callbacks / SortEntities run during the cascade.
				DetachChild(entity, child);
				DestroyEntity(child, excludeChildren, false);
			}
		}
		else
		{
			// children survive a targeted destroy — detach them so their
			// ParentHandle doesn't dangle at a dead UUID. They become roots
			std::vector<UUID> children = entity.Children();
			for (UUID childId : children)
			{
				Entity child = TryGetEntityWithUUID(childId);
				if (child)
					DetachChild(entity, child);
			}
		}

		if (first)
		{
			Entity parent = entity.GetParent();
			if (parent)
				DetachChild(parent, entity);
		}

		UUID id = entity.GetUUID();

		if (SelectionManager::IsSelected(id))
			SelectionManager::Deselect(id);

		m_Registry.destroy(entity.m_EntityHandle);
		m_EntityIDMap.erase(id);

		SortEntities();
	}

	void Scene::DestroyEntity(UUID entityID, bool excludeChildren, bool first)
	{
		auto it = m_EntityIDMap.find(entityID);
		if (it == m_EntityIDMap.end())
			return;

		DestroyEntity(it->second, excludeChildren, first);
	}

	void Scene::ParentEntity(Entity entity, Entity parent)
	{
		if (!entity || !parent || entity == parent)
			return;

		if (parent.IsDescendantOf(entity))
		{
			UnparentEntity(parent);

			Entity grandParent = TryGetEntityWithUUID(entity.GetParentUUID());
			if (grandParent)
			{
				UnparentEntity(entity);
				ParentEntity(parent, grandParent);
			}
		}
		else
		{
			Entity previousParent = TryGetEntityWithUUID(entity.GetParentUUID());

			if (previousParent)
				UnparentEntity(entity);
		}

		AttachChild(parent, entity);

		ConvertToLocalSpace(entity);
	}

	void Scene::UnparentEntity(Entity entity, bool convertToWorldSpace)
	{
		Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());
		if (!parent)
			return;

		DetachChild(parent, entity);

		if (convertToWorldSpace)
			ConvertToWorldSpace(entity);
	}

	Entity Scene::DuplicateEntity(Entity entity)
	{
		Entity newEntity;
		if (entity.HasComponent<TagComponent>())
			newEntity = CreateEntity(entity.GetComponent<TagComponent>().Tag);
		else
			newEntity = CreateEntity();

		CopyComponentIfExists<TransformComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<StaticMeshComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<CameraComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<SpriteRendererComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		// CopyComponentIfExists<SkyLightComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity)); // NOTE: You can not duplicate sky lights since you should only have one
		CopyComponentIfExists<DirectionalLightComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<PointLightComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<SpotLightComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<TextComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<RigidBodyComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<BoxColliderComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<SphereColliderComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<CylinderColliderComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<CapsuleColliderComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<CompoundColliderComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<MeshColliderComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<RigidBody2DComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<BoxCollider2DComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));
		CopyComponentIfExists<CircleCollider2DComponent>(newEntity.m_EntityHandle, m_Registry, static_cast<entt::entity>(entity));

		// Need to copy the children here because the collection is mutated below
		std::vector<UUID> childIDs = entity.Children();
		for (UUID childID : childIDs)
		{
			Entity childDuplicate = DuplicateEntity(GetEntityWithUUID(childID));

			// Here childDuplicate is a child of entity, we need to remove it from that entity and change its parent
			UnparentEntity(childDuplicate, false);
			AttachChild(newEntity,  childDuplicate);
		}

		Entity parent = entity.GetParent();
		if (parent)
			AttachChild(parent, newEntity);

		return newEntity;
	}

	Entity Scene::InstantiateStaticMesh(Ref<StaticMesh> staticMesh)
	{
		AssetMetaData& assetMetaData = Project::GetEditorAssetManager()->GetMetaData(staticMesh->Handle);
		Entity rootEntity = CreateEntity(assetMetaData.FilePath.stem().string());
		rootEntity.AddComponent<StaticMeshComponent>(staticMesh->Handle);

		if (staticMesh->ShouldGenerateColliders())
		{
			rootEntity.AddComponent<MeshColliderComponent>();
			rootEntity.AddComponent<RigidBodyComponent>();
			
			PhysicsSystem::EnsureMeshColliderReady(rootEntity);
		}

		return rootEntity;
	}

	void Scene::ConvertToLocalSpace(Entity entity)
	{
		Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());

		if (!parent)
			return;

		auto& transform = entity.Transform();
		glm::mat4 parentTransform = GetWorldSpaceTransformMatrix(parent);
		glm::mat4 localTransform = glm::inverse(parentTransform) * transform.GetTransform();
		transform.SetTransform(localTransform);
	}

	void Scene::ConvertToWorldSpace(Entity entity)
	{
		Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());

		if (!parent)
			return;

		glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);
		auto& entityTransform = entity.Transform();
		entityTransform.SetTransform(transform);
	}

	glm::mat4 Scene::GetWorldSpaceTransformMatrix(Entity entity)
	{
		glm::mat4 transform(1.0f);

		Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());
		if (parent)
			transform = GetWorldSpaceTransformMatrix(parent);

		return transform * entity.Transform().GetTransform();
	}

	TransformComponent Scene::GetWorldSpaceTransform(Entity entity)
	{
		glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);
		TransformComponent transformComponent;
		transformComponent.SetTransform(transform);
		return transformComponent;
	}

	Entity Scene::GetEntityWithUUID(UUID id) const
	{
		IR_ASSERT(m_EntityIDMap.contains(id), "Invalid Entity ID or entity does not exist!");
		return m_EntityIDMap.at(id);
	}

	Entity Scene::TryGetEntityWithUUID(UUID id) const
	{
		const auto it = m_EntityIDMap.find(id);
		if (it != m_EntityIDMap.end())
			return it->second;

		return Entity{};
	}

	Entity Scene::TryGetEntityWithTag(const std::string& tag)
	{
		auto view = GetAllEntitiesWith<TagComponent>();
		for(auto entity : view)
		{
			if (m_Registry.get<TagComponent>(entity).Tag == tag)
				return Entity{ entity, const_cast<Scene*>(this) };
		}

		return Entity{};
	}

	Entity Scene::TryGetDescendantEntityWithTag(Entity entity, const std::string& tag)
	{
		if (entity)
		{
			if (entity.GetComponent<TagComponent>().Tag == tag)
				return entity;

			for (const auto childID : entity.Children())
			{
				Entity descendant = TryGetDescendantEntityWithTag(GetEntityWithUUID(childID), tag);
				if (descendant)
					return descendant;
			}
		}

		return {};
	}

	Ref<PhysicsScene> Scene::GetPhysicsScene() const
	{
		if (!m_Registry.has<JoltWorldComponent>(m_SceneEntity))
			return nullptr;

		return m_Registry.get<JoltWorldComponent>(m_SceneEntity).PhysicsWorld;
	}

	void Scene::SortEntities()
	{
		m_Registry.sort<IDComponent>([&](const auto lhs, const auto rhs)
		{
			auto lhsEntity = m_EntityIDMap.find(lhs.ID);
			auto rhsEntity = m_EntityIDMap.find(rhs.ID);
			return static_cast<uint32_t>(lhsEntity->second) < static_cast<uint32_t>(rhsEntity->second);
		});
	}

	// TODO: This will only be used when we have normal meshes that have skeletal animation
	void Scene::BuildMeshEntityHierarchy(Entity parent, Ref<StaticMesh> staticMesh, const MeshUtils::MeshNode& node)
	{
		Ref<MeshSource> meshSource = AssetManager::GetAsset<MeshSource>(staticMesh->GetMeshSource());
		const std::vector<MeshUtils::MeshNode>& nodes = meshSource->GetNodes();

		// Skip empty root node
		if (node.IsRoot() && node.SubMeshes.size() == 0)
		{
			for (uint32_t child : node.Children)
				BuildMeshEntityHierarchy(parent, staticMesh, nodes[child]);

			return;
		}

		Entity nodeEntity = CreateChildEntity(parent, node.Name, true);
		nodeEntity.Transform().SetTransform(node.LocalTransform);

		if (node.SubMeshes.size() == 1)
		{
			// Node = StaticMesh in this case
			uint32_t subMeshIndex = node.SubMeshes[0];
			StaticMeshComponent& mc = nodeEntity.AddComponent<StaticMeshComponent>(staticMesh->Handle, subMeshIndex);

			if (staticMesh->ShouldGenerateColliders())
			{
				nodeEntity.AddComponent<MeshColliderComponent>();
				nodeEntity.AddComponent<RigidBodyComponent>();
				
				PhysicsSystem::EnsureMeshColliderReady(nodeEntity);
			}
		}
		else if (node.SubMeshes.size() > 1)
		{
			// Create one entity per child mesh, parented under node
			for (uint32_t i = 0; i < node.SubMeshes.size(); i++)
			{
				uint32_t subMeshIndex = node.SubMeshes[i];

				Entity childEntity = CreateChildEntity(nodeEntity, node.Name, true);
				childEntity.AddComponent<StaticMeshComponent>(staticMesh->Handle, subMeshIndex);

				if (staticMesh->ShouldGenerateColliders())
				{
					childEntity.AddComponent<MeshColliderComponent>();
					childEntity.AddComponent<RigidBodyComponent>();
					
					PhysicsSystem::EnsureMeshColliderReady(childEntity);
				}
			}
		}

		for (uint32_t child : node.Children)
			BuildMeshEntityHierarchy(nodeEntity, staticMesh, nodes[child]);
	}

	void Scene::RenderPhysicsDebug(Ref<SceneRenderer> renderer)
	{
		if (!renderer->GetOptions().ShowPhysicsColliders)
			return;

		auto submitBoxCollider = [this, &renderer](Entity entity, Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource)
		{
			glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);
			const BoxColliderComponent& colliderComp = entity.GetComponent<BoxColliderComponent>();
			glm::mat4 colliderTransform = glm::translate(glm::mat4(1.0f), colliderComp.Offset) * glm::scale(glm::mat4(1.0f), colliderComp.HalfSize * 2.0f);
			renderer->SubmitPhysicsStaticDebugMesh(staticMesh, meshSource, transform * colliderTransform);
		};

		auto submitSphereCollider = [this, &renderer](Entity entity, Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource)
		{
			glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);
			const SphereColliderComponent& colliderComp = entity.GetComponent<SphereColliderComponent>();
			glm::mat4 colliderTransform = glm::translate(glm::mat4(1.0f), colliderComp.Offset) * glm::scale(glm::mat4(1.0f), glm::vec3(colliderComp.Radius * 2.0f));
			renderer->SubmitPhysicsStaticDebugMesh(staticMesh, meshSource, transform * colliderTransform);
		};

		auto submitCapsuleCollider = [this, &renderer](Entity entity, Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource)
		{
			glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);
			const CapsuleColliderComponent& colliderComp = entity.GetComponent<CapsuleColliderComponent>();
			glm::mat4 colliderTransform = glm::translate(glm::mat4(1.0f), colliderComp.Offset) * glm::scale(glm::mat4(1.0f), glm::vec3(colliderComp.Radius * 2.0f, colliderComp.HalfHeight * 2.0f, colliderComp.Radius * 2.0f));
			renderer->SubmitPhysicsStaticDebugMesh(staticMesh, meshSource, transform * colliderTransform);
		};

		auto submitCylinderCollider = [this, &renderer](Entity entity, Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource)
		{
			glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);
			const CylinderColliderComponent& colliderComp = entity.GetComponent<CylinderColliderComponent>();
			glm::mat4 colliderTransform = glm::translate(glm::mat4(1.0f), colliderComp.Offset) * glm::scale(glm::mat4(1.0f), glm::vec3(colliderComp.Radius * 2.0f, colliderComp.HalfHeight * 2.0f, colliderComp.Radius * 2.0f));
			renderer->SubmitPhysicsStaticDebugMesh(staticMesh, meshSource, transform * colliderTransform);
		};

		auto submitMeshCollider = [this, &renderer](Entity entity)
		{
			if (!entity.HasComponent<StaticMeshComponent>() || !entity.HasComponent<MeshColliderComponent>())
				return;

			glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);
			const MeshColliderComponent& colliderComp = entity.GetComponent<MeshColliderComponent>();
			AssetHandle staticMeshHandle = entity.GetComponent<StaticMeshComponent>().StaticMesh;

			bool isSimpleCollider = colliderComp.CollisionComplexity == PhysicsCollisionComplexity::UseComplexAsSimple ? false : true;

			Ref<StaticMesh> staticMesh = PhysicsSystem::GetMesheColliderCache()->GetDebugStaticMesh(staticMeshHandle, colliderComp.CollisionComplexity);
			if (staticMesh)
			{
				if (Ref<MeshSource> meshSource = AssetManager::GetAsset<MeshSource>(staticMesh->GetMeshSource()); meshSource)
				{
					renderer->SubmitPhysicsStaticDebugMesh(staticMesh, meshSource, transform, isSimpleCollider);
				}
			}
		};

		if (renderer->GetOptions().PhysicsColliderViewMode == SceneRendererOptions::PhysicsColliderViewOptions::SelectedEntity)
		{
			if (SelectionManager::GetSelectionCount(SelectionContext::Scene) == 0)
				return;

			for (UUID entityId : SelectionManager::GetSelections(SelectionContext::Scene))
			{
				Entity entity = GetEntityWithUUID(entityId);

				if (entity.HasComponent<BoxColliderComponent>())
				{
					Ref<StaticMesh> staticMesh = AssetManager::GetAsset<StaticMesh>(PhysicsSystem::GetMesheColliderCache()->GetBoxDebugMesh());
					if (staticMesh)
					{
						Ref<MeshSource> meshSource = AssetManager::GetAsset<MeshSource>(staticMesh->GetMeshSource());
						if (meshSource)
							submitBoxCollider(entity, staticMesh, meshSource);
					}
				}

				if (entity.HasComponent<SphereColliderComponent>())
				{
					Ref<StaticMesh> staticMesh = AssetManager::GetAsset<StaticMesh>(PhysicsSystem::GetMesheColliderCache()->GetSphereDebugMesh());
					if (staticMesh)
					{
						Ref<MeshSource> meshSource = AssetManager::GetAsset<MeshSource>(staticMesh->GetMeshSource());
						if (meshSource)
							submitSphereCollider(entity, staticMesh, meshSource);
					}
				}

				if (entity.HasComponent<CapsuleColliderComponent>())
				{
					Ref<StaticMesh> staticMesh = AssetManager::GetAsset<StaticMesh>(PhysicsSystem::GetMesheColliderCache()->GetCapsuleDebugMesh());
					if (staticMesh)
					{
						Ref<MeshSource> meshSource = AssetManager::GetAsset<MeshSource>(staticMesh->GetMeshSource());
						if (meshSource)
							submitCapsuleCollider(entity, staticMesh, meshSource);
					}
				}

				if (entity.HasComponent<CylinderColliderComponent>())
				{
					Ref<StaticMesh> staticMesh = AssetManager::GetAsset<StaticMesh>(PhysicsSystem::GetMesheColliderCache()->GetCylinderDebugMesh());
					if (staticMesh)
					{
						Ref<MeshSource> meshSource = AssetManager::GetAsset<MeshSource>(staticMesh->GetMeshSource());
						if (meshSource)
							submitCylinderCollider(entity, staticMesh, meshSource);
					}
				}

				if (entity.HasComponent<MeshColliderComponent>())
					submitMeshCollider(entity);
			}
		}
		else
		{
			Ref<StaticMesh> boxStaticMesh = AssetManager::GetAsset<StaticMesh>(PhysicsSystem::GetMesheColliderCache()->GetBoxDebugMesh());
			if (boxStaticMesh)
			{
				Ref<MeshSource> boxMeshSource = AssetManager::GetAsset<MeshSource>(boxStaticMesh->GetMeshSource());
				if (boxMeshSource)
				{
					for (auto entity : GetAllEntitiesWith<BoxColliderComponent>())
						submitBoxCollider({ entity, this }, boxStaticMesh, boxMeshSource);
				}
			}

			Ref<StaticMesh> sphereStaticMesh = AssetManager::GetAsset<StaticMesh>(PhysicsSystem::GetMesheColliderCache()->GetSphereDebugMesh());
			if (sphereStaticMesh)
			{
				Ref<MeshSource> sphereMeshSource = AssetManager::GetAsset<MeshSource>(sphereStaticMesh->GetMeshSource());
				if (sphereMeshSource)
				{
					for (auto entity : GetAllEntitiesWith<SphereColliderComponent>())
						submitSphereCollider({ entity, this }, sphereStaticMesh, sphereMeshSource);
				}
			}

			Ref<StaticMesh> capsuleStaticMesh = AssetManager::GetAsset<StaticMesh>(PhysicsSystem::GetMesheColliderCache()->GetCapsuleDebugMesh());
			if (capsuleStaticMesh)
			{
				Ref<MeshSource> capsuleMeshSource = AssetManager::GetAsset<MeshSource>(capsuleStaticMesh->GetMeshSource());
				if (capsuleMeshSource)
				{
					for (auto entity : GetAllEntitiesWith<CapsuleColliderComponent>())
						submitCapsuleCollider({ entity, this }, capsuleStaticMesh, capsuleMeshSource);
				}
			}

			Ref<StaticMesh> cylinderStaticMesh = AssetManager::GetAsset<StaticMesh>(PhysicsSystem::GetMesheColliderCache()->GetCylinderDebugMesh());
			if (cylinderStaticMesh)
			{
				Ref<MeshSource> cylinderMeshSource = AssetManager::GetAsset<MeshSource>(cylinderStaticMesh->GetMeshSource());
				if (cylinderMeshSource)
				{
					for (auto entity : GetAllEntitiesWith<CylinderColliderComponent>())
						submitCylinderCollider({ entity, this }, cylinderStaticMesh, cylinderMeshSource);
				}
			}

			for (auto entity : GetAllEntitiesWith<MeshColliderComponent>())
				submitMeshCollider({ entity, this });
		}
	}

	void Scene::Render2DPhysicsDebug(Ref<SceneRenderer> renderer, Ref<Renderer2D> renderer2D)
	{
		if (!renderer->GetOptions().ShowPhysicsColliders)
			return;

		if (renderer->GetOptions().PhysicsColliderViewMode == SceneRendererOptions::PhysicsColliderViewOptions::SelectedEntity)
		{
			if (SelectionManager::GetSelectionCount(SelectionContext::Scene) == 0)
				return;

			for (UUID entityID : SelectionManager::GetSelections(SelectionContext::Scene))
			{
				Entity entity = GetEntityWithUUID(entityID);

				if (entity.HasComponent<BoxCollider2DComponent>())
				{
					const TransformComponent& tc = GetWorldSpaceTransform(entity);
					const BoxCollider2DComponent& boxCollider = entity.GetComponent<BoxCollider2DComponent>();
					renderer2D->DrawRotatedRect(
						glm::vec2{ tc.Translation.x, tc.Translation.y } + boxCollider.Offset,
						(2.0f * boxCollider.HalfSize) * glm::vec2(tc.Scale),
						tc.GetRotationEuler().z,
						Project::GetActive()->GetConfig().Viewport2DColliderOutlineColor
					);
				}

				if (entity.HasComponent<CircleCollider2DComponent>())
				{
					const TransformComponent& tc = GetWorldSpaceTransform(entity);
					const CircleCollider2DComponent& circleCollider = entity.GetComponent<CircleCollider2DComponent>();
					renderer2D->DrawCircle(
						glm::vec3{ tc.Translation.x, tc.Translation.y, 0.0f } + glm::vec3(circleCollider.Offset, 0.0f),
						glm::vec3(0.0f),
						circleCollider.Radius * tc.Scale.x,
						Project::GetActive()->GetConfig().Viewport2DColliderOutlineColor
					);
				}
			}
		}
		else
		{
			{
				auto view = m_Registry.view<BoxCollider2DComponent>();
				for (auto entity : view)
				{
					Entity e = { entity, this };
					const TransformComponent& tc = GetWorldSpaceTransform(e);
					BoxCollider2DComponent& boxCollider = e.GetComponent<BoxCollider2DComponent>();
					renderer2D->DrawRotatedRect(
						glm::vec2{ tc.Translation.x, tc.Translation.y } + boxCollider.Offset,
						(2.0f * boxCollider.HalfSize) * glm::vec2(tc.Scale),
						tc.GetRotationEuler().z,
						Project::GetActive()->GetConfig().Viewport2DColliderOutlineColor
					);
				}
			}

			{
				auto view = m_Registry.view<CircleCollider2DComponent>();
				for (auto entity : view)
				{
					Entity e = { entity, this };
					const TransformComponent& tc = GetWorldSpaceTransform(e);
					CircleCollider2DComponent& circleCollider = e.GetComponent<CircleCollider2DComponent>();
					renderer2D->DrawCircle(
						glm::vec3{ tc.Translation.x, tc.Translation.y, 0.0f } + glm::vec3(circleCollider.Offset, 0.0f),
						glm::vec3(0.0f),
						circleCollider.Radius * tc.Scale.x,
						Project::GetActive()->GetConfig().Viewport2DColliderOutlineColor
					);
				}
			}
		}
	}

	void Scene::OnMeshColliderComponentConstruct(entt::registry& registry, entt::entity entity)
	{
		Entity e = { entity, this };
		PhysicsSystem::EnsureMeshColliderReady(e);
	}

	void Scene::AttachChild(Entity parent, Entity child)
	{
		IR_ASSERT(parent && child);
		IR_ASSERT(parent != child);

		auto& childRel = m_Registry.get<RelationshipComponent>(static_cast<entt::entity>(child));
		IR_ASSERT(childRel.ParentHandle == 0); // caller must detach first — no double-parents, ever

		childRel.ParentHandle = parent.GetUUID();
		m_Registry.get<RelationshipComponent>(static_cast<entt::entity>(parent)).Children.push_back(child.GetUUID());

		++m_HierarchyVersion;
	}

	void Scene::DetachChild(Entity parent, Entity child)
	{
		IR_ASSERT(parent && child);

		auto& children = m_Registry.get<RelationshipComponent>(static_cast<entt::entity>(parent)).Children;
		UUID childID = child.GetUUID();
		auto it = std::find(children.begin(), children.end(), childID);
		IR_ASSERT(it != children.end()); // link must exist in both directions
		children.erase(it);

		m_Registry.get<RelationshipComponent>(static_cast<entt::entity>(child)).ParentHandle = 0;

		++m_HierarchyVersion;
	}

}