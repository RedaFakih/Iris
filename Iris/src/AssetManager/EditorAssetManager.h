#pragma once

#include "AssetManager/AssetRegistry.h"
#include "AssetManagerBase.h"
#include "AssetThread/EditorAssetThread.h"
#include "Core/Hash.h"
#include "Importers/AssetImporter.h"

namespace Iris {

	class EditorAssetManager : public AssetManagerBase
	{
	public:
		EditorAssetManager();
		virtual ~EditorAssetManager();

		[[nodiscard]] static Ref<EditorAssetManager> Create();

		virtual void Shutdown() override;

		virtual AssetType GetAssetType(AssetHandle handle) const override;
		virtual Ref<Asset> GetAsset(AssetHandle handle) override;
		virtual AsyncAssetResult<Asset> GetAssetAsync(AssetHandle handle) override;

		virtual void AddMemoryOnlyAsset(Ref<Asset> asset) override;
		virtual bool ReloadData(AssetHandle handle) override;
		virtual bool IsAssetHandleValid(AssetHandle handle) const override { return IsMemoryAsset(handle) || GetMetaData(handle).IsValid(); }
		virtual bool IsMemoryAsset(AssetHandle handle) const override { return m_MemoryAssets.contains(handle); }
		virtual bool IsAssetLoaded(AssetHandle handle) override { return m_LoadedAssets.contains(handle); }
		virtual bool IsAssetValid(AssetHandle handle) override;
		virtual bool IsAssetMissing(AssetHandle handle) override;
		virtual void RemoveAsset(AssetHandle handle) override;

		virtual void RegisterDependency(AssetHandle handle, AssetHandle dependency) override;

		virtual void SyncWithAssetThread() override;

		virtual std::unordered_set<AssetHandle> GetAllAssetsWithType(AssetType type) const override;
		virtual const std::unordered_map<AssetHandle, Ref<Asset>>& GetLoadedAssets() const override { return m_LoadedAssets; }

		// Editor only
		const AssetMetaData& GetMetaData(AssetHandle handle) const;
		AssetMetaData& GetMetaData(AssetHandle handle);
		const AssetMetaData& GetMetaData(const std::filesystem::path& path) const;
		const AssetMetaData& GetMetaData(const Ref<Asset>& asset) const;

		AssetHandle ImportAsset(const std::filesystem::path& filePath);

		AssetHandle GetAssetHandleFromFilePath(const std::filesystem::path& filePath) const { return GetMetaData(filePath).Handle; }

		AssetType GetAssetTypeFromExtension(const std::string& extension) const;
		AssetType GetAssetTypeFromPath(const std::filesystem::path& path) const { return GetAssetTypeFromExtension(path.extension().string()); }

		std::filesystem::path GetFileSystemPath(AssetHandle handle) const { return GetFileSystemPath(GetMetaData(handle)); }
		std::filesystem::path GetFileSystemPath(const AssetMetaData& metaData) const;
		std::string GetFileSystemPathString(const AssetMetaData& metaData) const { return GetFileSystemPath(metaData).string(); }
		std::filesystem::path GetRelativePath(const std::filesystem::path& path) const;

		bool FileExists(const AssetMetaData& metaData) const;

		const AssetRegistry& GetAssetRegistry() const { return m_AssetRegistry; }

		template<typename T, typename... Args>
		Ref<T> CreateNewAsset(const std::string& filename, const std::filesystem::path& directorypath, Args&&... args)
		{
			static_assert(std::is_base_of<Asset, T>::value, "CreateNewAsset only works for types derived from Asset");

			AssetMetaData metaData;
			metaData.Handle = AssetHandle();
			if (directorypath.empty() || directorypath == ".")
				metaData.FilePath = filename;
			else
				metaData.FilePath = GetRelativePath(directorypath / filename);
			metaData.IsDataLoaded = true;
			metaData.Type = T::GetStaticType();

			m_AssetRegistry[metaData.Handle] = metaData;

			SerializeRegistry();

			Ref<T> asset = T::Create(std::forward<Args>(args)...);
			asset->Handle = metaData.Handle;
			m_LoadedAssets[asset->Handle] = asset;
			AssetImporter::Serialize(metaData, asset);

			return asset;
		}

		template<typename T, typename... Args>
		AssetHandle CreateNamedMemoryOnlyAsset(const std::string& name, Args&&... args)
		{
			static_assert(std::is_base_of<Asset, T>::value, "CreateMemoryOnlyAsset only works for types derived from Asset");

			Ref<T> asset = Ref<T>::Create(std::forward<Args>(args)...);
			asset->Handle = Hash::GenerateFNVHash(name);

			AssetMetaData metadata;
			metadata.Handle = asset->Handle;
			metadata.FilePath = name;
			metadata.IsDataLoaded = true;
			metadata.Type = T::GetStaticType();
			metadata.IsMemoryAsset = true;

			m_AssetRegistry[metadata.Handle] = metadata;
			m_MemoryAssets[asset->Handle] = asset;

			return asset->Handle;
		}

		void ReplaceLoadedAsset(AssetHandle handle, Ref<Asset> asset) { m_LoadedAssets[handle] = asset; }

	private:
		Ref<Asset> GetAssetIncludingInvalid(AssetHandle handle);

		void LoadAssetRegistry();
		void SerializeRegistry();
		void ReloadAssets();
		void ProcessDirectory(const std::filesystem::path& path);

		AssetMetaData& GetMetaDataInternal(AssetHandle handle);

		void OnAssetRenamed(AssetHandle handle, const std::filesystem::path& newFilePath);
		void OnAssetDeleted(AssetHandle handle);

	private:
		// TODO: Move to AssetSystem
		std::unordered_map<AssetHandle, Ref<Asset>> m_LoadedAssets;
		std::unordered_map<AssetHandle, Ref<Asset>> m_MemoryAssets;

		std::unordered_map<AssetHandle, std::unordered_set<AssetHandle>> m_AssetDependencies;

		Ref<EditorAssetThread> m_AssetThread;
		AssetRegistry m_AssetRegistry;

		friend class EditorAssetThread;

	};

}