#pragma once

#include "Components.h"

#include <entt/entt.hpp>

namespace Iris {

	class Scene;

	class Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity handle, Scene* scene)
			: m_EntityHandle(handle), m_Scene(scene)
		{
		}

		~Entity() = default;

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args);

		template<typename T>
		T& GetComponent();

		template<typename T>
		const T& GetComponent() const;

		// returns nullptr in case it did not find the component requested
		template<typename T>
		T* TryGetComponent();

		// returns nullptr in case it did not find the component requested
		template<typename T>
		const T* TryGetComponent() const;

		template<typename... T>
		bool HasComponent() const;

		template<typename... T>
		bool HasAny() const;

		template<typename T>
		void RemoveComponent();

		template<typename T>
		void RemoveComponentIfExists();

		std::string& Name() { return HasComponent<TagComponent>() ? GetComponent<TagComponent>().Tag : s_NoName; }
		const std::string& Name() const { return HasComponent<TagComponent>() ? GetComponent<TagComponent>().Tag : s_NoName; }
		
		operator uint32_t() const { return (uint32_t)m_EntityHandle; }
		operator entt::entity() const { return m_EntityHandle; }
		operator bool() const;

		bool operator==(const Entity& other) const
		{
			return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
		}

		bool operator!=(const Entity& other) const
		{
			return !(*this == other);
		}

		TransformComponent& Transform() { return GetComponent<TransformComponent>(); }
		const glm::mat4& Transform() const { return GetComponent<TransformComponent>().GetTransform(); }

		UUID GetUUID() const { return GetComponent<IDComponent>().ID; }
		UUID GetSceneUUID() const;

	private:
		entt::entity m_EntityHandle{ entt::null };
		Scene* m_Scene = nullptr; // non owning Ref to the scene it is in

		inline static std::string s_NoName = "NoName";

		friend class Scene;
	};

}