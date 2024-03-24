#include "vkPch.h"
#include "Scene.h"

#include "Renderer/SceneRenderer.h"

namespace vkPlayground {

	Ref<Scene> Scene::Create(const std::string& name)
	{
		return CreateRef<Scene>(name);
	}

	Scene::Scene(const std::string& name)
		: m_Name(name)
	{
	}

	Scene::~Scene()
	{
		m_Registry.clear();
	}

	void Scene::SetViewportSize(uint32_t width, uint32_t height)
	{
		if (m_ViewportWidth == width && m_ViewportHeight == height)
			return;

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
		tag.Tag = name == "" ? "PlaygroundDefault" : name;

		VKPG_ASSERT(!m_EntityIDMap.contains(id));
		m_EntityIDMap[id] = entity;

		SortEntities();

		return entity;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		if (!entity)
			return;

		if (m_OnEntityDestroyedCallback)
			m_OnEntityDestroyedCallback(entity);

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

	Entity Scene::GetEntitiyWithUUID(UUID id) const
	{
		VKPG_ASSERT(m_EntityIDMap.contains(id), "Invalid Entity ID or entity does not exist!");
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