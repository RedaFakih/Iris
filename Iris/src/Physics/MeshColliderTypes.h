#pragma once

#include "Core/Buffer.h"

#include <glm/glm.hpp>

namespace Iris {

	enum class MeshColliderType : uint8_t
	{
		None = 0,
		Triangle,
		Convex
	};

	struct SubMeshColliderData
	{
		Buffer ColliderData;
		glm::mat4 Transform;
	};

	struct MeshColliderData
	{
		std::vector<SubMeshColliderData> SubMeshes;
		MeshColliderType Type;
	};

	struct CachedColliderData
	{
		MeshColliderData SimpleColliderData; // Convex Shape
		MeshColliderData ComplexColliderData; // Triangle Shape
	};

	// .Imc file format Header
	struct IrisPhysicsMeshHeader
	{
		const char Header[11] = "IrisJoltMC";
		MeshColliderType Type;
		uint32_t SubMeshCount;
	};

}