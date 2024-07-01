#pragma once

#include "Entity.h"

#include "Core/TimeStep.h"
#include "Core/UUID.h"
#include "Editor/EditorCamera.h"
#include "Renderer/Mesh/Mesh.h"
#include "SceneEnvironment.h"

namespace Iris {

	class SceneRenderer;

	// Just a POD type that holds all the data (from the DirectionalLightComponent and also other places) needed for a directional light in the scene
	struct SceneDirectionalLight
	{
		glm::vec3 Direction = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Radiance = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float ShadowAmount = 1.0f;

		// TODO: boolean for shadows
	};

	// Hold the data related to all lighting in the scene: Directional/Point/Spot Lights...
	struct LightEnvironment
	{
		constexpr static uint32_t MaxDirectionalLights = 1; // We can only have one directional light in the scene

		SceneDirectionalLight DirectionalLight;
		// NOTE: Other type of lights data will be stored here also...
	};

	using EntityMap = std::unordered_map<UUID, Entity>;

	class Scene : public Asset
	{
	public:
		Scene(const std::string& name = "UntitledScene", bool isEditorScene = false);
		~Scene();

		[[nodiscard]] static Ref<Scene> Create(const std::string& name = "UntitledScene", bool isEditorScene = false);
		[[nodiscard]] static Ref<Scene> CreateEmpty();

		// TODO:
		void OnUpdateRuntime(TimeStep ts);
		void OnRenderRuntime(Ref<SceneRenderer> renderer, TimeStep ts);

		// TODO:
		void OnUpdateEditor(TimeStep ts);
		void OnRenderEditor(Ref<SceneRenderer> renderer, TimeStep ts, const EditorCamera& camera);

		void SetViewportSize(uint32_t width, uint32_t height);

		void SetEntityDestroyedCallback(const std::function<void(Entity)>& callback) { m_OnEntityDestroyedCallback = callback; }

		Entity GetMainCameraEntity();

		std::vector<UUID> GetAllChildren(Entity entity) const;

		Entity CreateEntity(const std::string& name = "");
		Entity CreateChildEntity(Entity parent, const std::string& name = "");
		Entity CreateEntityWithUUID(UUID id, const std::string& name = "", bool shouldSort = true);
		// TODO: Deffers the destruction of the entity to post update
		//void SubmitToDestroyEntity(Entity entity);
		void DestroyEntity(Entity entity, bool excludeChildren = false, bool first = true);
		void DestroyEntity(UUID entityID, bool excludeChildren = false, bool first = true);

		Entity DuplicateEntity(Entity entity);

		Entity InstantiateStaticMesh(Ref<StaticMesh> staticMesh);

		template<typename... Componenets>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<Componenets...>();
		}

		void ConvertToLocalSpace(Entity entity);
		void ConvertToWorldSpace(Entity entity);
		glm::mat4 GetWorldSpaceTransformMatrix(Entity entity);
		TransformComponent GetWorldSpaceTransform(Entity entity);

		// Error if entity does not exist
		Entity GetEntityWithUUID(UUID id) const;
		// Empty entity if not found
		Entity TryGetEntityWithUUID(UUID id) const;
		Entity TryGetEntityWithTag(const std::string& tag);

		// return descendant entity with tag as specified, or empty entity if cannot be found - caller must check
		// descendant could be immediate child, or deeper in the hierachy
		Entity TryGetDescendantEntityWithTag(Entity entity, const std::string& tag);

		void ParentEntity(Entity entity, Entity parent);
		void UnparentEntity(Entity entity, bool convertToWorldSpace = true);

		UUID GetUUID() const { return m_SceneID; }

		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		glm::vec2 GetViewportSize() const { return { m_ViewportWidth, m_ViewportHeight }; }

		const EntityMap& GetEntityMap() const { return m_EntityIDMap; }
		const Ref<Environment>& GetEnvironment() const { return m_Environment; }

		template<typename TComponent>
		void CopyComponentIfExists(entt::entity dst, entt::registry& dstRegistry, entt::entity src)
		{
			if (m_Registry.has<TComponent>(src))
			{
				auto& srcComponent = m_Registry.get<TComponent>(src);
				dstRegistry.emplace_or_replace<TComponent>(dst, srcComponent);
			}
		}

		template<typename TComponent>
		static void CopyComponentFromScene(Entity dst, Ref<Scene> dstScene, Entity src, Ref<Scene> srcScene)
		{
			srcScene->CopyComponentIfExists<TComponent>((entt::entity)dst, dstScene->m_Registry, (entt::entity)src);
		}

		static AssetType GetStaticType() { return AssetType::Scene; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }

	private:
		void SortEntities();
		void BuildMeshEntityHierarchy(Entity parent, Ref<StaticMesh> staticMesh, const MeshUtils::MeshNode& node);

	private:
		UUID m_SceneID;
		entt::registry m_Registry;

		std::string m_Name;
		
		bool m_IsEditorScene = false;
		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;

		EntityMap m_EntityIDMap;

		Ref<Environment> m_Environment;
		float m_EnvironmentIntensity = 1.0f;
		float m_SkyboxLod = 0.0f;
		LightEnvironment m_LightEnvironment;

		std::function<void(Entity)> m_OnEntityDestroyedCallback;

		friend class Entity;
		friend class ECSDebugPanel;
		friend class SceneSerializer;
		friend class SceneRenderer;

	};

}

#include "EntityTemplates.inl"