#pragma once

#include "AssetManager/Asset/Asset.h"

namespace Iris {

	// Fully Static
	struct MeshFactory
	{
		static AssetHandle CreateBox(const glm::vec3& size);
		static AssetHandle CreateSphere(float radius);
		static AssetHandle CreateCylinder(float radius, float height);
		static AssetHandle CreateCapsule(float radius, float height);

	};

}