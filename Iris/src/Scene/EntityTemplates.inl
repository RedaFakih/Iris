#pragma once

namespace Iris {

	template<typename T, typename... Args>
	auto Entity::AddComponent(Args&&... args) -> typename std::conditional<std::is_empty<T>::value, void, T&>::type
	{
		IR_ASSERT(!HasComponent<T>(), "Entity already has component!");
		return m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
	}

	template<typename T>
	T& Entity::GetComponent()
	{
		IR_ASSERT(HasComponent<T>(), "Entity doesn't have component!");
		return m_Scene->m_Registry.get<T>(m_EntityHandle);
	}

	template<typename T>
	const T& Entity::GetComponent() const
	{
		IR_ASSERT(HasComponent<T>(), "Entity doesn't have component!");
		return m_Scene->m_Registry.get<T>(m_EntityHandle);
	}

	template<typename T>
	T* Entity::TryGetComponent()
	{
		return m_Scene->m_Registry.try_get<T>(m_EntityHandle);
	}

	template<typename T>
	const T* Entity::TryGetComponent() const
	{
		return m_Scene->m_Registry.try_get<T>(m_EntityHandle);
	}

	template<typename... T>
	bool Entity::HasComponent() const
	{
		return m_Scene->m_Registry.has<T...>(m_EntityHandle);
	}

	template<typename... T>
	bool Entity::HasAny() const
	{
		return m_Scene->m_Registry.any<T...>(m_EntityHandle);
	}

	template<typename T>
	void Entity::RemoveComponent()
	{
		IR_ASSERT(HasComponent<T>(), "Entity doesn't have component!");
		m_Scene->m_Registry.remove<T>(m_EntityHandle);
	}

	template<typename T>
	void Entity::RemoveComponentIfExists()
	{
		m_Scene->m_Registry.remove_if_exists<T>(m_EntityHandle);
	}

}