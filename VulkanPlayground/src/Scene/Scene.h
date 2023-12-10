#pragma once

#include "Core/TimeStep.h"
#include "Core/UUID.h"
#include "Editor/EditorCamera.h"
#include "Entity.h"


namespace vkPlayground {

	class SceneRenderer;

	using EntityMap = std::unordered_map<UUID, Entity>;

	class Scene : public RefCountedObject
	{
	public:
		Scene(const std::string& name = "UntitledScene");
		~Scene();

		[[nodiscard]] static Ref<Scene> Create(const std::string& name = "UntitledScene");

		// TODO:
		//void OnUpdateRuntime(TimeStep ts);
		//void OnUpdateEditor(TimeStep ts);

		// TODO:
		//void OnRenderRuntime(/* TODO: SceneRenderer */, TimeStep ts);
		//void OnRenderEditor(/* TODO: SceneRenderer */, TimeStep ts);

		void SetViewportSize(uint32_t width, uint32_t height);

		void SetEntityDestroyedCallback(void(*callback)(Entity)) { m_OnEntityDestroyedCallback = callback; }

		Entity CreateEntity(const std::string& name = "");
		Entity CreateEntityWithUUID(UUID id, const std::string& name = "", bool shouldSort = true);
		void DestroyEntity(Entity entity);
		void DestroyEntity(UUID entityID);

		template<typename... Componenets>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<Componenets...>();
		}

		// Error if entity does not exist
		Entity GetEntitiyWithUUID(UUID id) const;
		// Empty entity if not found
		Entity TryGetEntitiyWithUUID(UUID id) const;
		Entity TryGetEntityWithTag(const std::string& tag);

		UUID GetUUID() const { return m_SceneID; }

		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		glm::vec2 GetViewportSize() const { return { m_ViewportWidth, m_ViewportHeight }; }

		const EntityMap& GetEntitiyMap() const { return m_EntityIDMap; }

	private:
		void SortEntities();

	private:
		UUID m_SceneID;
		entt::registry m_Registry;

		std::string m_Name;
		
		bool m_IsEditorScene = false;
		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;

		EntityMap m_EntityIDMap;

		void(*m_OnEntityDestroyedCallback)(Entity) = nullptr;

		friend class Entity;

	};

}

#include "EntityTemplates.inl"