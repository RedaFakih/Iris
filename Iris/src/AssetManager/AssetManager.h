#pragma once

#include "Asset/Asset.h"
#include "Asset/AssetTypes.h"
#include "Project/Project.h"
#include "Utils/FileSystem.h"

#include <unordered_set>

namespace Iris {

	class AssetManager
	{
	public:
		// Returns true if assetHandle could potentially be valid.
		static bool IsAssetHandleValid(AssetHandle handle) { return Project::GetAssetManager()->IsAssetHandleValid(handle); }

		// Returns true if the asset referred to by assetHandle is valid.
		// Note that this will attempt to load the asset if it is not already loaded.
		// An asset is invalid if any of the following are true:
		// - The asset handle is invalid
		// - The file referred to by asset meta data is missing
		// - The asset could not be loaded from file
		static bool IsAssetValid(AssetHandle handle, bool loadAsync = false) { return Project::GetAssetManager()->IsAssetValid(handle, loadAsync); }

		// Returns true if the asset referred to by assetHandle is missing.
		// Note that this checks for existence of file, but makes no attempt to load the asset from file
		// A memory-only asset cannot be missing.
		static bool IsAssetMissing(AssetHandle handle) { return Project::GetAssetManager()->IsAssetMissing(handle); }

		static bool ReloadData(AssetHandle handle) { return Project::GetAssetManager()->ReloadData(handle); }

		static AssetType GetAssetType(AssetHandle handle) { return Project::GetAssetManager()->GetAssetType(handle); }

		static void SyncWithAssetThread() { Project::GetAssetManager()->SyncWithAssetThread(); }

		static bool IsAssetThreadCurrentlyLoadingAssets() { return Project::GetAssetManager()->IsAssetThreadCurrentlyLoadingAssets(); }

		static Ref<Asset> GetPlaceHolderAsset(AssetType type);

		template<typename T>
		static Ref<T> GetAsset(AssetHandle handle)
		{
			Ref<Asset> asset = Project::GetAssetManager()->GetAsset(handle);
			return asset.As<T>();
		}

		template<typename T>
		static AsyncAssetResult<T> GetAssetAsync(AssetHandle handle)
		{
			AsyncAssetResult<Asset> result = Project::GetAssetManager()->GetAssetAsync(handle);
			return AsyncAssetResult<T>(result);
		}

		template<typename T>
		static std::unordered_set<AssetHandle> GetAllAssetsWithType()
		{
			return Project::GetAssetManager()->GetAllAssetsWithType(T::GetStaticType());
		}

		static const std::unordered_map<AssetHandle, Ref<Asset>>& GetLoadedAssets() { return Project::GetAssetManager()->GetLoadedAssets(); }

		template<typename T, typename... Args>
		static AssetHandle CreateMemoryOnlyAsset(Args&&... args)
		{
			static_assert(std::is_base_of<Asset, T>::value, "CreateMemoryOnlyAsset only works for types derived from Asset");
		
			Ref<T> asset = T::Create(std::forward<Args>(args)...);
			asset->Handle = AssetHandle();
		
			Project::GetAssetManager()->AddMemoryOnlyAsset(asset);
			return asset->Handle;
		}
		
		template<typename T, typename... Args>
		static AssetHandle CreateMemoryOnlyAssetWithHandle(AssetHandle handle, Args&&... args)
		{
			static_assert(std::is_base_of<Asset, T>::value, "CreateMemoryOnlyAsset only works for types derived from Asset");
		
			Ref<T> asset = T::Create(std::forward<Args>(args)...);
			asset->Handle = handle;
		
			Project::GetAssetManager()->AddMemoryOnlyAsset(asset);
			return asset->Handle;
		}
		
		template<typename T>
		static AssetHandle AddMemoryOnlyAsset(Ref<T> asset)
		{
			static_assert(std::is_base_of<Asset, T>::value, "AddMemoryOnlyAsset only works for types derived from Asset");
			asset->Handle = AssetHandle(); // NOTE: Should we generate the handle here?
		
			Project::GetAssetManager()->AddMemoryOnlyAsset(asset);
			return asset->Handle;
		}

		static bool IsMemoryAsset(AssetHandle handle) { return Project::GetAssetManager()->IsMemoryAsset(handle); }

		static void RegisterDependency(AssetHandle handle, AssetHandle dependency) { Project::GetAssetManager()->RegisterDependency(handle, dependency); }

		static void RemoveAsset(AssetHandle handle) { Project::GetAssetManager()->RemoveAsset(handle); }

	};

}