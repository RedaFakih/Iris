#include "vkPch.h"
#include "Entity.h"

#include "Scene.h"

namespace vkPlayground {

	Entity::operator bool() const
	{
		return (m_EntityHandle != entt::null) && m_Scene && m_Scene->m_Registry.valid(m_EntityHandle);
	}

	UUID Entity::GetSceneUUID() const
	{
		return m_Scene->m_SceneID;
	}

}