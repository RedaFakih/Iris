#include "IrisPCH.h"
#include "MeshColliderCache.h"

#include "AssetManager/AssetManager.h"
#include "Renderer/Mesh/MeshFactory.h"

namespace Iris {

	void MeshColliderCache::Init()
	{
		m_BoxMesh = MeshFactory::CreateBox(glm::vec3(1));
		m_SphereMesh = MeshFactory::CreateSphere(0.5f);
		m_CylinderMesh = MeshFactory::CreateCylinder(0.5f, 1.0f);
		m_CapsuleMesh = MeshFactory::CreateCapsule(0.5f, 1.0f);
	}

	void MeshColliderCache::Shutdown()
	{
		Clear();
	}

	bool MeshColliderCache::Exists(AssetHandle staticMeshHandle) const
	{
		return m_MeshData.contains(staticMeshHandle);
	}

	void MeshColliderCache::Clear()
	{
		m_MeshData.clear();
		m_DebugComplexStaticMeshes.clear();
		m_DebugSimpleStaticMeshes.clear();
	}

	const CachedColliderData& MeshColliderCache::GetMeshData(AssetHandle staticMeshHandle)
	{
		IR_ASSERT(m_MeshData.contains(staticMeshHandle));
		return m_MeshData.at(staticMeshHandle);
	}

	void MeshColliderCache::AddMeshData(AssetHandle staticMeshHandle, const CachedColliderData& data)
	{
		m_MeshData[staticMeshHandle] = data;
	}

	Ref<StaticMesh> MeshColliderCache::GetDebugStaticMesh(AssetHandle staticMeshHandle, PhysicsCollisionComplexity complexity)
	{
		const auto& map = (complexity == PhysicsCollisionComplexity::UseComplexAsSimple) ? m_DebugComplexStaticMeshes : m_DebugSimpleStaticMeshes;

		auto it = map.find(staticMeshHandle);
		if (it != map.end())
			return it->second;

		return nullptr;
	}

	void MeshColliderCache::AddSimpleDebugStaticMesh(AssetHandle staticMeshHandle, const Ref<StaticMesh> debugMesh)
	{
		m_DebugSimpleStaticMeshes[staticMeshHandle] = debugMesh;
	}

	void MeshColliderCache::AddComplexDebugStaticMesh(AssetHandle staticMeshHandle, const Ref<StaticMesh> debugMesh)
	{
		m_DebugComplexStaticMeshes[staticMeshHandle] = debugMesh;
	}

}