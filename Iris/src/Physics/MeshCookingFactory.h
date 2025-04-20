#pragma once

#include "AssetManager/Asset/MeshColliderAsset.h"
#include "MeshColliderTypes.h"
#include "PhysicsTypes.h"
#include "Renderer/Mesh/Mesh.h"

namespace Iris {

	// Fully Static
	class MeshCookingFactory
	{
	public:
		static std::pair<PhysicsCookingResult, PhysicsCookingResult> CookMesh(AssetHandle meshColliderAssetHandle, bool invalidateOld = false);
		static std::pair<PhysicsCookingResult, PhysicsCookingResult> CookMesh(Ref<MeshColliderAsset> meshColliderAsset, bool invalidateOld = false);

	private:
		static PhysicsCookingResult CookConvexMesh(const Ref<MeshColliderAsset>& meshColliderAsset, const Ref<MeshSource>& meshSource, const std::vector<uint32_t>& subMeshIndices, MeshColliderData& outData);
		static PhysicsCookingResult CookTriangleMesh(const Ref<MeshColliderAsset>& meshColliderAsset, const Ref<MeshSource>& meshSource, const std::vector<uint32_t>& subMeshIndices, MeshColliderData& outData);

		static void GenerateDebugMesh(const Ref<MeshColliderAsset>& meshColliderAsset, const std::vector<uint32_t>& subMeshIndices, const MeshColliderData& colliderData);

		static bool SerializeMeshCollider(const std::filesystem::path& filepath, MeshColliderData& meshData);
		static MeshColliderData DeserializeMeshCollider(const std::filesystem::path& filepath);

	};

}