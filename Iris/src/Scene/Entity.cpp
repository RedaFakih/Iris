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

	UUID Entity::GetSceneUUID() const
	{
		return m_Scene->m_SceneID;
	}

}