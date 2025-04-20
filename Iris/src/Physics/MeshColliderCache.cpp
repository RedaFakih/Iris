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

	bool MeshColliderCache::Exists(Ref<MeshColliderAsset> meshColliderAsset) const
	{
		AssetHandle collisionMesh = meshColliderAsset->ColliderMesh;

		if (!m_MeshData.contains(collisionMesh))
			return false;

		const std::map<AssetHandle, CachedColliderData>& meshDataMap = m_MeshData.at(collisionMesh);
		if (AssetManager::IsMemoryAsset(collisionMesh))
			return meshDataMap.find(0) != meshDataMap.end();

		return meshDataMap.contains(collisionMesh);
	}

	void MeshColliderCache::Clear()
	{
		m_MeshData.clear();
		m_DebugComplexStaticMeshes.clear();
		m_DebugSimpleStaticMeshes.clear();
	}

	const CachedColliderData& MeshColliderCache::GetMeshData(Ref<MeshColliderAsset> meshColliderAsset)
	{
		AssetHandle collisionMesh = meshColliderAsset->ColliderMesh;

		if (m_MeshData.contains(collisionMesh))
		{
			const std::map<AssetHandle, CachedColliderData>& meshDataMap = m_MeshData.at(collisionMesh);
			if (meshDataMap.contains(meshColliderAsset->Handle))
				return meshDataMap.at(meshColliderAsset->Handle);

			return meshDataMap.at(0);
		}

		const std::map<AssetHandle, CachedColliderData>& meshDataMap = m_MeshData.at(collisionMesh);
		IR_ASSERT(meshDataMap.contains(meshColliderAsset->Handle));
		return meshDataMap.at(meshColliderAsset->Handle);
	}

	Ref<StaticMesh> MeshColliderCache::GetDebugStaticMesh(Ref<MeshColliderAsset> meshColliderAsset)
	{
		AssetHandle collisionMesh = meshColliderAsset->ColliderMesh;
		const auto& map = meshColliderAsset->CollisionComplexity == PhysicsCollisionComplexity::UseComplexAsSimple ? m_DebugComplexStaticMeshes : m_DebugSimpleStaticMeshes;

		auto debugMeshes = map.find(collisionMesh);
		if (debugMeshes != map.end())
		{
			auto debugMesh = debugMeshes->second.find(meshColliderAsset->Handle);
			if (debugMesh != debugMeshes->second.end())
				return debugMesh->second;

			debugMesh = debugMeshes->second.find(0);
			if (debugMesh != debugMeshes->second.end())
				return debugMesh->second;
		}

		return nullptr;
	}

	void MeshColliderCache::AddSimpleDebugStaticMesh(Ref<MeshColliderAsset> meshColliderAsset, const Ref<StaticMesh> debugMesh)
	{
		if (AssetManager::IsMemoryAsset(meshColliderAsset->Handle))
			m_DebugSimpleStaticMeshes[meshColliderAsset->ColliderMesh][0] = debugMesh;
		else
			m_DebugSimpleStaticMeshes[meshColliderAsset->ColliderMesh][meshColliderAsset->Handle] = debugMesh;
	}

	void MeshColliderCache::AddComplexDebugStaticMesh(Ref<MeshColliderAsset> meshColliderAsset, const Ref<StaticMesh> debugMesh)
	{
		if (AssetManager::IsMemoryAsset(meshColliderAsset->Handle))
			m_DebugComplexStaticMeshes[meshColliderAsset->ColliderMesh][0] = debugMesh;
		else
			m_DebugComplexStaticMeshes[meshColliderAsset->ColliderMesh][meshColliderAsset->Handle] = debugMesh;
	}

}