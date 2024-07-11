#include "IrisPCH.h"
#include "Scene.h"

#include "AssetManager/AssetManager.h"
#include "Editor/SelectionManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer/Shaders/Shader.h"
#include "Renderer/StorageBufferSet.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

namespace Iris {

	Ref<Scene> Scene::Create(const std::string& name, bool isEditorScene)
	{
		return CreateRef<Scene>(name, isEditorScene);
	}

	Ref<Scene> Scene::CreateEmpty()
	{
		return CreateRef<Scene>("Empty", false);
	}

	Scene::Scene(const std::string& name, bool isEditorScene)
		: m_Name(name), m_IsEditorScene(isEditorScene)
	{
	}

	Scene::~Scene()
	{
		m_Registry.clear();
	}


	void Scene::OnUpdateRuntime(TimeStep ts)
	{
		// NOTE: Should update some state for physics/scripting/animations but for now nothing...
	}

	void Scene::OnRenderRuntime(Ref<SceneRenderer> renderer, TimeStep ts)
	{
		// TODO:
	}

	void Scene::OnUpdateEditor(TimeStep ts)
	{
		// NOTE: Should update some state for physics/scripting/animations but for now nothing...
	}

	void Scene::OnRenderEditor(Ref<SceneRenderer> renderer, TimeStep ts, const EditorCamera& camera)
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
									.ShadowAmount = 1.0f // TODO: Should come from component
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
							.ShadowAmount = 1.0f // TODO: Should come from component
						};
					}
				}
			}
		}

		{
			renderer->SetScene(this);
			renderer->BeginScene({ camera, camera.GetViewMatrix(), camera.GetNearClip(), camera.GetFarClip(), camera.GetFOV() });

			// Render static meshes
			auto entities = GetAllEntitiesWith<StaticMeshComponent>();
			for (auto entity : entities)
			{
				const StaticMeshComponent& staticMeshComponenet = entities.get<StaticMeshComponent>(entity);
				if (!staticMeshComponenet.Visible)
					continue;

				AsyncAssetResult<StaticMesh> staticMeshResult = AssetManager::GetAssetAsync<StaticMesh>(staticMeshComponenet.StaticMesh);
				if (staticMeshResult.IsReady)
				{
					Ref<StaticMesh> staticMesh = staticMeshResult;
					AsyncAssetResult<MeshSource> meshSourceResult = AssetManager::GetAssetAsync<MeshSource>(staticMesh->GetMeshSource());
					if (meshSourceResult.IsReady)
					{
						Ref<MeshSource> meshSource = meshSourceResult;

						Entity e = { entity, this };
						glm::mat4 transform = GetWorldSpaceTransformMatrix(e);

						if (SelectionManager::IsEntityOrAncestorSelected(e))
							renderer->SubmitSelectedStaticMesh(staticMesh, meshSource, staticMeshComponenet.MaterialTable, transform);
						else
							renderer->SubmitStaticMesh(staticMesh, meshSource, staticMeshComponenet.MaterialTable, transform);
					}
				}
			}

			renderer->EndScene();

			if (renderer->GetFinalPassImage())
			{
				Ref<Renderer2D> renderer2D = renderer->GetRenderer2D();
			
				renderer2D->ResetStats();
				renderer2D->BeginScene(camera.GetViewProjection(), camera.GetViewMatrix());
			
				// Render 2D sprites
				{
					auto view = GetAllEntitiesWith<TransformComponent, SpriteRendererComponent>();
					for (auto entity : view)
					{
						Entity e = { entity, this };
			
						const auto& [transformComponent, spriteRendererComponent] = view.get<TransformComponent, SpriteRendererComponent>(entity);
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
					auto view = GetAllEntitiesWith<TransformComponent, TextComponent>();
					for (auto entity : view)
					{
						Entity e = { entity, this };

						const auto& [transformComponent, textComponent] = view.get<TransformComponent, TextComponent>(entity);
						Ref<Font> font = Font::GetFontAssetForTextComponent(textComponent.Font);
						renderer2D->DrawString(textComponent.TextString, font, GetWorldSpaceTransformMatrix(e), textComponent.MaxWidth, textComponent.Color, textComponent.LineSpacing, textComponent.Kerning);
					}
				}
			
				// Save line width since debug renderer may change that...
				float lineWidth = renderer2D->GetLineWidth();
			
				// TODO: Debug Renderer, eventhough the line width here might conflict with the renderer2D's line width since the vulkan command to change
				// it is only called once on EndScene so if the debug renderer sets it then all the lines will be rendered with that width
			
				renderer2D->EndScene();
				// Restore the line width
				renderer2D->SetLineWidth(lineWidth);
			}
		}
	}

	void Scene::SetViewportSize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;
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

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateChildEntity({}, name);
	}

	Entity Scene::CreateChildEntity(Entity parent, const std::string& name)
	{
		auto entity = Entity{ m_Registry.create(), this };
		auto& idComponent = entity.AddComponent<IDComponent>();
		idComponent.ID = {};

		if (!name.empty())
			entity.AddComponent<TagComponent>(name);

		entity.AddComponent<TransformComponent>();
		entity.AddComponent<RelationshipComponent>();

		if (parent)
			entity.SetParent(parent);

		m_EntityIDMap[idComponent.ID] = entity;

		SortEntities();

		return entity;
	}

	Entity Scene::CreateEntityWithUUID(UUID id, const std::string& name, bool shouldSort)
	{
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<IDComponent>(id);

		if (!name.empty())
			entity.AddComponent<TagComponent>(name);

		entity.AddComponent<TransformComponent>();
		entity.AddComponent<RelationshipComponent>();

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

		if (m_OnEntityDestroyedCallback)
			m_OnEntityDestroyedCallback(entity);

		if (!excludeChildren)
		{
			// Copy here since if we do not then we will have entity leaks
			auto children = entity.Children();
			for (std::size_t i = 0; i < children.size(); i++)
			{
				auto childID = children[i];
				Entity child = GetEntityWithUUID(childID);
				DestroyEntity(childID, excludeChildren, false);
			}
		}

		if (first)
		{
			Entity parent = entity.GetParent();
			if (parent)
				parent.RemoveChild(entity);
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

		DestroyEntity(it->second);
	}

	Entity Scene::DuplicateEntity(Entity entity)
	{
		auto parentNewEntity = [&entity, scene = this](Entity newEntity)
		{
			auto parent = entity.GetParent();
			if (parent)
			{
				newEntity.SetParentUUID(parent.GetUUID());
				parent.Children().push_back(newEntity.GetUUID());
			}
		};

		Entity newEntity;
		if (entity.HasComponent<TagComponent>())
			newEntity = CreateEntity(entity.GetComponent<TagComponent>().Tag);
		else
			newEntity = CreateEntity();

		CopyComponentIfExists<TransformComponent>(newEntity.m_EntityHandle, m_Registry, entity);
		CopyComponentIfExists<StaticMeshComponent>(newEntity.m_EntityHandle, m_Registry, entity);
		CopyComponentIfExists<CameraComponent>(newEntity.m_EntityHandle, m_Registry, entity);
		CopyComponentIfExists<SpriteRendererComponent>(newEntity.m_EntityHandle, m_Registry, entity);
		// CopyComponentIfExists<SkyLightComponent>(newEntity.m_EntityHandle, m_Registry, entity); // NOTE: You can not duplicate sky lights since you should only have one
		CopyComponentIfExists<DirectionalLightComponent>(newEntity.m_EntityHandle, m_Registry, entity);
		CopyComponentIfExists<TextComponent>(newEntity.m_EntityHandle, m_Registry, entity);

		// Need to copy the children here because the collection is mutated below
		std::vector<UUID> childIDs = entity.Children();
		for (UUID childID : childIDs)
		{
			Entity childDuplicate = DuplicateEntity(GetEntityWithUUID(childID));

			// Here childDuplicate is a child of entity, we need to remove it from that entity and change its parent
			UnparentEntity(childDuplicate, false);

			childDuplicate.SetParentUUID(newEntity.GetUUID());
			newEntity.Children().push_back(childDuplicate.GetUUID());
		}

		parentNewEntity(newEntity);

		return newEntity;
	}

	Entity Scene::InstantiateStaticMesh(Ref<StaticMesh> staticMesh)
	{
		AssetMetaData& assetMetaData = Project::GetEditorAssetManager()->GetMetaData(staticMesh->Handle);
		Entity rootEntity = CreateEntity(assetMetaData.FilePath.stem().string());
		Ref<MeshSource> meshSource = AssetManager::GetAssetAsync<MeshSource>(staticMesh->GetMeshSource());
		if (meshSource)
			BuildMeshEntityHierarchy(rootEntity, staticMesh, meshSource->GetRootNode());

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

	void Scene::ParentEntity(Entity entity, Entity parent)
	{
		if (parent.IsDescendantOf(entity))
		{
			UnparentEntity(parent);

			Entity newParent = TryGetEntityWithUUID(entity.GetParentUUID());
			if (newParent)
			{
				UnparentEntity(entity);
				ParentEntity(parent, newParent);
			}
		}
		else
		{
			Entity previousParent = TryGetEntityWithUUID(entity.GetParentUUID());

			if (previousParent)
				UnparentEntity(entity);
		}

		entity.SetParentUUID(parent.GetUUID());
		parent.Children().push_back(entity.GetUUID());

		ConvertToLocalSpace(entity);
	}

	void Scene::UnparentEntity(Entity entity, bool convertToWorldSpace)
	{
		Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());
		if (!parent)
			return;

		auto& parentChildren = parent.Children();
		parentChildren.erase(std::remove(parentChildren.begin(), parentChildren.end(), entity.GetUUID()), parentChildren.end());

		if (convertToWorldSpace)
			ConvertToWorldSpace(entity);

		entity.SetParentUUID(0);
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

		Entity nodeEntity = CreateChildEntity(parent, node.Name);
		nodeEntity.Transform().SetTransform(node.LocalTransform);

		if (node.SubMeshes.size() == 1)
		{
			// Node = StaticMesh in this case
			uint32_t subMeshIndex = node.SubMeshes[0];
			auto& mc = nodeEntity.AddComponent<StaticMeshComponent>(staticMesh->Handle, subMeshIndex);
		}
		else if (node.SubMeshes.size() > 1)
		{
			// Create one entity per child mesh, parented under node
			for (uint32_t i = 0; i < node.SubMeshes.size(); i++)
			{
				uint32_t subMeshIndex = node.SubMeshes[i];

				Entity childEntity = CreateChildEntity(nodeEntity, node.Name);
				childEntity.AddComponent<StaticMeshComponent>(staticMesh->Handle, subMeshIndex);
			}
		}

		for (uint32_t child : node.Children)
			BuildMeshEntityHierarchy(nodeEntity, staticMesh, nodes[child]);
	}

}