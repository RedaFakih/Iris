#pragma once

#include "Core/Base.h"

namespace Iris {

	enum class AssetFlag : uint8_t
	{
		None = 0,
		Missing = BIT(0),
		Invalid = BIT(1)
	};

	enum class AssetType : uint8_t
	{
		None = 0,
		Scene,
		StaticMesh,
		MeshSource,
		Material,
		Texture,
		EnvironmentMap
	};

	namespace Utils {

		inline AssetType AssetTypeFromString(std::string_view assetType)
		{
			if (assetType == "None")                return AssetType::None;
			if (assetType == "Scene")               return AssetType::Scene;
			if (assetType == "StaticMesh")          return AssetType::StaticMesh;
			if (assetType == "MeshSource")          return AssetType::MeshSource;
			if (assetType == "Material")            return AssetType::Material;
			if (assetType == "Texture")             return AssetType::Texture;
			if (assetType == "EnvironmentMap")      return AssetType::EnvironmentMap;

			IR_VERIFY(false, "Unknown Asset Type");
			return AssetType::None;
		}

		inline const char* AssetTypeToString(AssetType assetType)
		{
			switch (assetType)
			{
				case AssetType::None:                return "None";
				case AssetType::Scene:               return "Scene";
				case AssetType::StaticMesh:          return "StaticMesh";
				case AssetType::MeshSource:          return "MeshSource";
				case AssetType::Material:            return "Material";
				case AssetType::Texture:             return "Texture";
				case AssetType::EnvironmentMap:      return "EnvironmentMap";
			}

			IR_VERIFY(false, "Unknown Asset Type");
			return "None";
		}

	}

}