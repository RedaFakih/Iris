#pragma once

#include "Asset.h"

#include <filesystem>

namespace Iris {

	enum class AssetStatus : uint8_t
	{
		None = 0,
		Ready,
		Loading,
		Invalid
	};

	struct AssetMetaData
	{
		AssetHandle Handle = 0;
		AssetType Type;
		std::filesystem::path FilePath;

		AssetStatus Status = AssetStatus::None;

		bool IsDataLoaded = false;
		bool IsMemoryAsset = false;

		bool IsValid() const { return Handle != 0 && !IsMemoryAsset; }
	};

	struct AssetLoadRequest
	{
		AssetMetaData MetaData;
		Ref<Asset> Asset;
		bool Reloaded = false;
	};

	struct RuntimeAssetLoadRequest
	{
		AssetHandle SceneHandle;
		AssetHandle Hanlde;
		Ref<Asset> Asset;
	};

}