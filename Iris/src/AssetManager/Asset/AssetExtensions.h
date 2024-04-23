#pragma once

#include "AssetTypes.h"

#include <unordered_map>

namespace Iris {

	inline static std::unordered_map<std::string, AssetType> s_AssetExtensionMap = {
		// Iris types (Here they are in lower case since the AssetManager lowers the extension characters before processing it
		{ ".iscene", AssetType::Scene },
		{ ".ismesh", AssetType::StaticMesh },
		{ ".imaterial", AssetType::Material },
	
		// meshSource types
		{ ".gltf", AssetType::MeshSource },
		{ ".glb", AssetType::MeshSource },
		{ ".fbx", AssetType::MeshSource },
		{ ".obj", AssetType::MeshSource },

		// Textures
		{ ".png", AssetType::Texture },
		{ ".jpg", AssetType::Texture },
		{ ".hdr", AssetType::Texture },
		{ ".jpeg", AssetType::Texture }
	};

}