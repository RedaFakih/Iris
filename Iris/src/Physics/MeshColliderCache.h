#pragma once

#include "MeshColliderTypes.h"
#include "Physics/PhysicsTypes.h"
#include "Renderer/Mesh/Mesh.h"

namespace Iris {

	class MeshColliderCache : public RefCountedObject
	{
	public:
		virtual ~MeshColliderCache() { Shutdown(); }

		void Init();
		void Shutdown();

		[[nodiscard]] inline static Ref<MeshColliderCache> Create()
		{
			return CreateRef<MeshColliderCache>();
		}

		bool Exists(AssetHandle staticMeshHandle) const;
		
		void Clear();

		AssetHandle GetBoxDebugMesh() const { return m_BoxMesh; }
		AssetHandle GetSphereDebugMesh() const { return m_SphereMesh; }
		AssetHandle GetCylinderDebugMesh() const { return m_CylinderMesh; }
		AssetHandle GetCapsuleDebugMesh() const { return m_CapsuleMesh; }

		const CachedColliderData& GetMeshData(AssetHandle staticMeshHandle);
		void AddMeshData(AssetHandle staticMeshHandle, const CachedColliderData& data);

		Ref<StaticMesh> GetDebugStaticMesh(AssetHandle staticMeshHandle, PhysicsCollisionComplexity complexity);

	private:
		void AddSimpleDebugStaticMesh(AssetHandle staticMeshHandle, const Ref<StaticMesh> debugMesh);
		void AddComplexDebugStaticMesh(AssetHandle staticMeshHandle, const Ref<StaticMesh> debugMesh);

	private:
		std::unordered_map<AssetHandle, CachedColliderData> m_MeshData;

		std::unordered_map<AssetHandle, Ref<StaticMesh>> m_DebugSimpleStaticMeshes;
		std::unordered_map<AssetHandle, Ref<StaticMesh>> m_DebugComplexStaticMeshes;

		AssetHandle m_BoxMesh = 0;
		AssetHandle m_SphereMesh = 0;
		AssetHandle m_CylinderMesh = 0;
		AssetHandle m_CapsuleMesh = 0;

		friend class MeshCookingFactory;

	};

}