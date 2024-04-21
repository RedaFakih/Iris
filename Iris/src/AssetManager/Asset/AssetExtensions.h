#pragma once

#include "AssetManager/Asset/AssetTypes.h"

#include <unordered_map>

namespace Iris {

	inline static std::unordered_map<std::string, AssetType> s_AssetExtensionMap = {
		// Iris types
		{ ".Iscene", AssetType::Scene },
		{ ".Ismesh", AssetType::StaticMesh },
		{ ".Imaterial", AssetType::Material },
	
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