#pragma once

#include "MeshColliderTypes.h"
#include "PhysicsTypes.h"
#include "Renderer/Mesh/Mesh.h"

namespace Iris {

	// Fully Static
	class MeshCookingFactory
	{
	public:
		static std::pair<PhysicsCookingResult, PhysicsCookingResult> CookMesh(AssetHandle staticMeshHandle, bool invalidateOld = false);

	private:
		static PhysicsCookingResult CookConvexMesh(const Ref<MeshSource>& meshSource, const std::vector<uint32_t>& subMeshIndices, MeshColliderData& outData);
		static PhysicsCookingResult CookTriangleMesh(const Ref<MeshSource>& meshSource, const std::vector<uint32_t>& subMeshIndices, MeshColliderData& outData);

		static void GenerateDebugMesh(AssetHandle staticMeshHandle, const std::vector<uint32_t>& subMeshIndices, const MeshColliderData& colliderData);

		static bool SerializeMeshCollider(const std::filesystem::path& filepath, MeshColliderData& meshData);
		static MeshColliderData DeserializeMeshCollider(const std::filesystem::path& filepath);

	};

}