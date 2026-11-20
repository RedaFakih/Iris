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
		auto AddComponent(Args&&... args) -> typename std::conditional<std::is_empty<T>::value, void, T&>::type;

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

		std::string& Name()
		{
			auto* tag = TryGetComponent<TagComponent>();
			if (tag)
				return tag->Tag;

			return s_NoName;
		}

		const std::string& Name() const
		{
			auto* tag = TryGetComponent<TagComponent>();
			if (tag)
				return tag->Tag;

			return s_NoName;
		}
		
		explicit operator uint32_t() const { return static_cast<uint32_t>(m_EntityHandle); }
		explicit operator entt::entity() const { return m_EntityHandle; }
		explicit operator bool() const;

		bool operator==(const Entity& other) const
		{
			return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
		}

		bool operator!=(const Entity& other) const
		{
			return !(*this == other);
		}

		Entity GetParent() const;

		UUID GetParentUUID() const { return GetComponent<RelationshipComponent>().ParentHandle; }
		const std::vector<UUID>& Children() const { return GetComponent<RelationshipComponent>().Children; }

		bool IsAncestorOf(Entity entity) const { return entity.IsDescendantOf(*this); }

		bool IsDescendantOf(Entity entity) const
		{
			if (!entity)
				return false;

			const UUID target = entity.GetUUID();

			for (Entity e = GetParent(); e; e = e.GetParent())
			{
				if (e.GetUUID() == target)
					return true;
			}

			return false;
		}

		TransformComponent& Transform() { return GetComponent<TransformComponent>(); }
		const glm::mat4& GetTransformMatrix() const { return GetComponent<TransformComponent>().GetTransform(); }

		UUID GetUUID() const { return GetComponent<IDComponent>().ID; }
		UUID GetSceneUUID() const;

	private:
		entt::entity m_EntityHandle{ entt::null };
		Scene* m_Scene = nullptr; // non owning Ref to the scene it is in

		inline static std::string s_NoName = "NoName";

		friend class Scene;
		friend class SceneHierarchyPanel;
	};

}