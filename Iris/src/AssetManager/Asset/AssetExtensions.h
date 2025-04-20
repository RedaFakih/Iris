#pragma once

#include "AssetTypes.h"

#include <unordered_map>

namespace Iris {

	// NOTE: Iris types (Here they are in lower case since the AssetManager lowers the extension characters before processing it
	inline static std::unordered_map<std::string, AssetType> s_AssetExtensionMap = {
		{ ".iscene", AssetType::Scene },
		{ ".ismesh", AssetType::StaticMesh },
		{ ".imaterial", AssetType::Material },
	
		// Mesh Source types
		{ ".gltf", AssetType::MeshSource },
		{ ".glb", AssetType::MeshSource },
		{ ".fbx", AssetType::MeshSource },
		{ ".obj", AssetType::MeshSource },

		// Mesh Collider types
		{ ".imc", AssetType::MeshCollider },

		// Textures
		{ ".png", AssetType::Texture },
		{ ".jpg", AssetType::Texture },
		{ ".jpeg", AssetType::Texture },

		// Envrionment Maps
		{ ".hdr", AssetType::EnvironmentMap },

		// Fonts
		{ ".ttf", AssetType::Font },
		{ ".ttc", AssetType::Font },
		{ ".otf", AssetType::Font }
	};

}