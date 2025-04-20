#pragma once

#include "Asset.h"
#include "Physics/ColliderMaterial.h"
#include "Physics/PhysicsTypes.h"

namespace Iris {

	struct MeshColliderAsset : public Asset
	{
		AssetHandle ColliderMesh = 0;
		ColliderMaterial Material;
		bool EnableVertexWelding = true;
		float VertexWeldTolerance = 0.1f;
		bool CheckZeroAreaTriangles = true;
		float AreaTestEpsilon = 0.06f;
		bool FlipNormals = false;
		bool ShiftVerticesToOrigin = false;
		bool AlwaysShareShape = false;
		PhysicsCollisionComplexity CollisionComplexity = PhysicsCollisionComplexity::Default;
		glm::vec3 ColliderScale = { 1.0f, 1.0f, 1.0f };

		MeshColliderAsset() = default;
		MeshColliderAsset(AssetHandle colliderMesh, ColliderMaterial material = {})
			: ColliderMesh(colliderMesh), Material(material) {}

		[[nodiscard]] inline static Ref<MeshColliderAsset> Create()
		{
			return CreateRef<MeshColliderAsset>();
		}

		[[nodiscard]] inline static Ref<MeshColliderAsset> Create(AssetHandle colliderMesh, ColliderMaterial material = {})
		{
			return CreateRef<MeshColliderAsset>(colliderMesh, material);
		}

		static AssetType GetStaticType() { return AssetType::MeshCollider; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }

	};

}