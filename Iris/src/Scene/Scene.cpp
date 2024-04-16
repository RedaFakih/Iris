#include "IrisPCH.h"
#include "Scene.h"

#include "Renderer/Shaders/Shader.h"
#include "Renderer/UniformBufferSet.h"
#include "Renderer/Texture.h"
#include "Renderer/SceneRenderer.h"

namespace Iris {

	Ref<Scene> Scene::Create(const std::string& name, bool isEditorScene)
	{
		return CreateRef<Scene>(name, isEditorScene);
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

		{
			renderer->SetScene(this);
			renderer->BeginScene({ camera, camera.GetViewMatrix(), camera.GetNearClip(), camera.GetFarClip(), camera.GetFOV() });

			// Render static meshes
			auto entities = GetAllEntitiesWith<StaticMeshComponent>();
			for (auto entity : entities)
			{
				auto& staticMeshComponenet = entities.get<StaticMeshComponent>(entity);
				if (!staticMeshComponenet.Visible)
					continue;

				Ref<StaticMesh> staticMesh = staticMeshComponenet.StaticMesh;
				if (staticMesh)
				{
					Ref<MeshSource> meshSource = staticMesh->GetMeshSource();
					if (meshSource)
					{
						Entity e = { entity, this };
						glm::mat4 transform = GetWorldSpaceTransformMatrix(e);

						// TODO: Selected Mesh separation into drawlists happens here...
						//if (SelectionManager::IsEntitySelected(e))
						//	renderer->SubmitSelectedStaticMesh(staticMesh, meshSource, staticMeshComponenet.MaterialTable, transform);
						//else
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
				renderer2D->SetTargetFramebuffer(renderer->GetExternalCompositeFramebuffer());

				// Render 2D sprites
				{
					auto view = GetAllEntitiesWith<TransformComponent, SpriteRendererComponent>();
					for (auto entity : view)
					{
						Entity e = { entity, this };

						const auto& [transformComponent, spriteRendererComponent] = view.get<TransformComponent, SpriteRendererComponent>(entity);
						if(spriteRendererComponent.Texture)
						{
							renderer2D->DrawQuad(
								GetWorldSpaceTransformMatrix(e),
								spriteRendererComponent.Texture,
								spriteRendererComponent.TilingFactor,
								spriteRendererComponent.Color,
								spriteRendererComponent.UV0,
								spriteRendererComponent.UV1
							);
						}
						else
						{
							renderer2D->DrawQuad(GetWorldSpaceTransformMatrix(e), spriteRendererComponent.Color);
						}
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

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID id, const std::string& name, bool shouldSort)
	{
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<IDComponent>(id);
		entity.AddComponent<TransformComponent>();
		TagComponent& tag = entity.AddComponent<TagComponent>(name);
		tag.Tag = name.empty() ? "PlaygroundDefault" : name;

		IR_ASSERT(!m_EntityIDMap.contains(id));
		m_EntityIDMap[id] = entity;

		if (shouldSort)
			SortEntities();

		return entity;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		if (!entity)
			return;

		if (m_OnEntityDestroyedCallback)
			m_OnEntityDestroyedCallback(entity);

		// TODO: Deselect

		UUID id = entity.GetUUID();
		m_Registry.destroy(entity.m_EntityHandle);
		m_EntityIDMap.erase(id);

		SortEntities();
	}

	void Scene::DestroyEntity(UUID entityID)
	{
		auto it = m_EntityIDMap.find(entityID);
		if (it == m_EntityIDMap.end())
			return;

		DestroyEntity(it->second);
	}

	glm::mat4 Scene::GetWorldSpaceTransformMatrix(Entity entity)
	{
		glm::mat4 transform(1.0f);

		// TODO: Get Parent transform so that we get the correct transform

		return transform * entity.Transform().GetTransform();
	}

	Entity Scene::GetEntitiyWithUUID(UUID id) const
	{
		IR_ASSERT(m_EntityIDMap.contains(id), "Invalid Entity ID or entity does not exist!");
		return m_EntityIDMap.at(id);
	}

	Entity Scene::TryGetEntitiyWithUUID(UUID id) const
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

	void Scene::SortEntities()
	{
		m_Registry.sort<IDComponent>([&](const auto lhs, const auto rhs)
		{
			auto lhsEntity = m_EntityIDMap.find(lhs.ID);
			auto rhsEntity = m_EntityIDMap.find(rhs.ID);
			return static_cast<uint32_t>(lhsEntity->second) < static_cast<uint32_t>(rhsEntity->second);
		});
	}

}