#pragma once

#include "Asset/Asset.h"
#include "Asset/AssetTypes.h"

#include <unordered_set>

namespace Iris {

	class Texture2D;

	// Implementation is in EditorAssetManager and RuntimeAssetManager
	class AssetManagerBase : public RefCountedObject
	{
	public:
		AssetManagerBase() = default;
		virtual ~AssetManagerBase() = default;

		virtual void Shutdown() = 0;

		virtual AssetType GetAssetType(AssetHandle handle) const = 0;
		virtual Ref<Asset> GetAsset(AssetHandle handle) = 0;
		virtual AsyncAssetResult<Asset> GetAssetAsync(AssetHandle handle) = 0;

		virtual void AddMemoryOnlyAsset(Ref<Asset> asset) = 0;
		virtual bool ReloadData(AssetHandle handle) = 0;
		virtual bool IsAssetHandleValid(AssetHandle handle) const = 0; // Asset Handle being valid... (Says nothing about the asset itself)
		virtual bool IsMemoryAsset(AssetHandle handle) const = 0; // Asset exists in memory only, there is no file backing it
		virtual bool IsAssetLoaded(AssetHandle handle) = 0; // Asset has been loaded from file (it could be invalid though)
		virtual bool IsAssetValid(AssetHandle handle, bool loadAsync) = 0;  // Asset file was loaded, but is invalid for some reason
		virtual bool IsAssetMissing(AssetHandle handle) = 0; // asset file is missing
		virtual void RemoveAsset(AssetHandle handle) = 0;

		virtual void RegisterDependency(AssetHandle handle, AssetHandle dependency) = 0;

		virtual void SyncWithAssetThread() = 0;
		virtual bool IsAssetThreadCurrentlyLoadingAssets() const = 0;

		virtual std::unordered_set<AssetHandle> GetAllAssetsWithType(AssetType type) const = 0;
		virtual const std::unordered_map<AssetHandle, Ref<Asset>>& GetLoadedAssets() const = 0;

	};

}