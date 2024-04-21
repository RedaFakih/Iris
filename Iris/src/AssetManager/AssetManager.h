#pragma once

#include "Asset/Asset.h"
#include "Asset/AssetTypes.h"
#include "Utils/FileSystem.h"
// TODO: Include Project

#include <unordered_set>

namespace Iris {

	class AssetManager
	{
	public:
		// Returns true if assetHandle could potentially be valid.
		static bool IsAssetHandleValid(AssetHandle handle) { return false; } // TODO:

		// Returns true if the asset referred to by assetHandle is valid.
		// Note that this will attempt to load the asset if it is not already loaded.
		// An asset is invalid if any of the following are true:
		// - The asset handle is invalid
		// - The file referred to by asset meta data is missing
		// - The asset could not be loaded from file
		static bool IsAssetValid(AssetHandle assetHandle) { return false; } // TODO:

		// Returns true if the asset referred to by assetHandle is missing.
		// Note that this checks for existence of file, but makes no attempt to load the asset from file
		// A memory-only asset cannot be missing.
		static bool IsAssetMissing(AssetHandle assetHandle) { return false; } // TODO:

		static bool ReloadData(AssetHandle assetHandle) { return false; } // TODO:

		static AssetType GetAssetType(AssetHandle assetHandle) { return AssetType::None; } // TODO:

		template<typename T>
		static Ref<T> GetAsset(AssetHandle assetHandle)
		{
			//static std::mutex mutex;
			//std::scoped_lock<std::mutex> lock(mutex);

			Ref<Asset> asset = nullptr; // TODO:
			return asset.As<T>();
		}

		template<typename T>
		static std::unordered_set<AssetHandle> GetAllAssetsWithType()
		{
			return {}; // TODO:
		}

		static const std::unordered_map<AssetHandle, Ref<Asset>>& GetLoadedAssets() { return {}; } // TODO:

		//template<typename TAsset, typename... TArgs>
		//static AssetHandle CreateMemoryOnlyAsset(TArgs&&... args)
		//{
		//	static_assert(std::is_base_of<Asset, TAsset>::value, "CreateMemoryOnlyAsset only works for types derived from Asset");

		//	Ref<TAsset> asset = Ref<TAsset>::Create(std::forward<TArgs>(args)...);
		//	asset->Handle = AssetHandle(); // NOTE(Yan): should handle generation happen here?

		//	Project::GetAssetManager()->AddMemoryOnlyAsset(asset);
		//	return asset->Handle;
		//}

		//template<typename TAsset, typename... TArgs>
		//static AssetHandle CreateMemoryOnlyRendererAsset(TArgs&&... args)
		//{
		//	static_assert(std::is_base_of<Asset, TAsset>::value, "CreateMemoryOnlyAsset only works for types derived from Asset");

		//	Ref<TAsset> asset = TAsset::Create(std::forward<TArgs>(args)...);
		//	asset->Handle = AssetHandle();

		//	Project::GetAssetManager()->AddMemoryOnlyAsset(asset);
		//	return asset->Handle;
		//}

		//template<typename TAsset, typename... TArgs>
		//static AssetHandle CreateMemoryOnlyAssetWithHandle(AssetHandle handle, TArgs&&... args)
		//{
		//	static_assert(std::is_base_of<Asset, TAsset>::value, "CreateMemoryOnlyAsset only works for types derived from Asset");

		//	Ref<TAsset> asset = Ref<TAsset>::Create(std::forward<TArgs>(args)...);
		//	asset->Handle = handle;

		//	Project::GetAssetManager()->AddMemoryOnlyAsset(asset);
		//	return asset->Handle;
		//}

		//template<typename TAsset>
		//static AssetHandle AddMemoryOnlyAsset(Ref<TAsset> asset)
		//{
		//	static_assert(std::is_base_of<Asset, TAsset>::value, "AddMemoryOnlyAsset only works for types derived from Asset");
		//	asset->Handle = AssetHandle(); // NOTE(Yan): should handle generation happen here?

		//	// Project::GetAssetManager()->AddMemoryOnlyAsset(asset); // TODO:
		//	return asset->Handle;
		//}

		static bool IsMemoryAsset(AssetHandle handle) { return false; } // TODO:

		static void RemoveAsset(AssetHandle handle)
		{
			// TODO:
		}

	};

}