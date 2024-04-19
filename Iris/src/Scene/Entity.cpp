#include "IrisPCH.h"
#include "Entity.h"

#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"
#include "Scene.h"

namespace Iris {

	Entity::operator bool() const
	{
		return (m_EntityHandle != entt::null) && m_Scene && m_Scene->m_Registry.valid(m_EntityHandle);
	}

	Entity Entity::GetParent() const
	{
		return m_Scene->TryGetEntityWithUUID(GetParentUUID());
	}

	bool Entity::IsAncestorOf(Entity entity) const
	{
		const auto& children = Children();

		if (children.empty())
			return false;

		for (UUID child : children)
			if (child == entity.GetUUID())
				return true;

		for (UUID child : children)
			if (m_Scene->GetEntityWithUUID(child).IsAncestorOf(entity))
				return true;

		return false;
	}

	UUID Entity::GetSceneUUID() const
	{
		return m_Scene->m_SceneID;
	}

}