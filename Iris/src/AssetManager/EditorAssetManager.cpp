#include "IrisPCH.h"
#include "EditorAssetManager.h"

#include "Asset/AssetExtensions.h"
#include "AssetManager.h"
#include "Importers/AssetImporter.h"
#include "Project/Project.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"
#include "Utils/StringUtils.h"

#include <yaml-cpp/yaml.h>

namespace Iris {

	static AssetMetaData s_NullMetaData;

	Ref<EditorAssetManager> EditorAssetManager::Create()
	{
		return CreateRef<EditorAssetManager>();
	}

	EditorAssetManager::EditorAssetManager()
	{
		m_AssetThread = EditorAssetThread::Create();
		m_AssetThread->Run();

		AssetImporter::Init();

		LoadAssetRegistry();
		ReloadAssets();
	}

	EditorAssetManager::~EditorAssetManager()
	{
		// NOTE: Called manually
		// Shutdown();
	}

	void EditorAssetManager::Shutdown()
	{
		m_AssetThread->StopAndWait(true);
		SerializeRegistry();
	}

	AssetType EditorAssetManager::GetAssetType(AssetHandle handle) const
	{
		if (!IsAssetHandleValid(handle))
			return AssetType::None;

		return GetMetaData(handle).Type;
	}

	Ref<Asset> EditorAssetManager::GetAsset(AssetHandle handle)
	{
		Ref<Asset> asset = GetAssetIncludingInvalid(handle);
		return asset && asset->IsValid() ? asset : nullptr;
	}

	AsyncAssetResult<Asset> EditorAssetManager::GetAssetAsync(AssetHandle handle)
	{
		if (IsMemoryAsset(handle))
			return { m_MemoryAssets.at(handle), true };

		auto& metaData = GetMetaDataInternal(handle);
		if (!metaData.IsValid())
			return { nullptr, false }; // TODO: Return special error asset?

		if (metaData.IsDataLoaded)
		{
			IR_VERIFY(m_LoadedAssets.contains(handle));
			return { m_LoadedAssets.at(handle), true };
		}

		// Queue load if not loaded and return a placeholder for now
		Ref<Asset> asset = nullptr;
		if (metaData.Status != AssetStatus::Loading)
		{
			metaData.Status = AssetStatus::Loading;
			m_AssetThread->QueueAssetLoad({ .MetaData = metaData });
			m_AssetThread->Set(ThreadState::Kick);
		}

		return { AssetManager::GetPlaceHolderAsset(metaData.Type), false };
	}

	void EditorAssetManager::AddMemoryOnlyAsset(Ref<Asset> asset)
	{
		AssetMetaData metaData;
		metaData.Handle = asset->Handle;
		metaData.Type = asset->GetAssetType();
		metaData.IsDataLoaded = true;
		metaData.IsMemoryAsset = true;
		m_AssetRegistry[metaData.Handle] = metaData;

		m_MemoryAssets[asset->Handle] = asset;
	}

	bool EditorAssetManager::ReloadData(AssetHandle handle)
	{
		bool result = false;
		AssetMetaData& metaData = GetMetaDataInternal(handle);
		if (!metaData.IsValid())
		{
			IR_CORE_ERROR_TAG("AssetManager", "Trying to reload invalid asset!");
			return result;
		}

		Ref<Asset> asset;
		if (metaData.IsDataLoaded)
			asset = m_LoadedAssets.at(handle);

		metaData.IsDataLoaded = AssetImporter::TryLoadData(metaData, asset);
		if (metaData.IsDataLoaded)
		{
			m_LoadedAssets[handle] = asset;
			// TODO: Dispatch immediatly application event for asset reloaded
		}
		result = metaData.IsDataLoaded;

		if (result)
		{
			// If the asset is something that refers to a MeshSource then we should reload the mesh source... (ex. StaticMesh)
			if (metaData.Type == AssetType::StaticMesh)
			{
				auto staticMesh = asset.As<StaticMesh>();
				result |= ReloadData(staticMesh->GetMeshSource());
			}
			// If the asset is a MeshSource then we should recreate all the assets that refer to the mesh source since they have data that depends on the source mesh
			else if (metaData.Type == AssetType::MeshSource)
			{
				for (auto& [loadedHandle, loadedAsset] : m_LoadedAssets)
				{
					bool reloadAsset = false;
					if (loadedAsset->GetAssetType() == AssetType::StaticMesh)
					{
						auto mesh = loadedAsset.As<StaticMesh>();
						if (mesh->GetMeshSource() == handle)
							reloadAsset = true;
					}

					if (reloadAsset)
					{
						auto& metaData2 = GetMetaDataInternal(loadedHandle);
						Ref<Asset> asset2;
						metaData2.IsDataLoaded = AssetImporter::TryLoadData(metaData2, asset2);
						if (metaData2.IsDataLoaded)
						{
							m_LoadedAssets[loadedHandle] = asset2;
							// TODO: Dispatch immediatly application event for asset reloaded
						}
					}
				}
			}
		}

		return result;
	}

	bool EditorAssetManager::IsAssetValid(AssetHandle handle, bool loadAsync)
	{
		Ref<Asset> asset;
		if (loadAsync)
			asset = GetAssetAsync(handle);
		else
			asset = GetAssetIncludingInvalid(handle);

		return asset && asset->IsValid();
	}

	bool EditorAssetManager::IsAssetMissing(AssetHandle handle)
	{
		if (IsMemoryAsset(handle))
			return false;

		AssetMetaData& metaData = GetMetaDataInternal(handle);
		return !FileSystem::Exists(Project::GetActive()->GetAssetDirectory() / metaData.FilePath);
	}

	void EditorAssetManager::RemoveAsset(AssetHandle handle)
	{
		if (m_LoadedAssets.contains(handle))
			m_LoadedAssets.erase(handle);

		if (m_MemoryAssets.contains(handle))
			m_MemoryAssets.erase(handle);

		if (m_AssetRegistry.Contains(handle))
			m_AssetRegistry.Remove(handle);
	}

	void EditorAssetManager::RegisterDependency(AssetHandle handle, AssetHandle dependency)
	{
		m_AssetDependencies[handle].insert(dependency);
	}

	void EditorAssetManager::SyncWithAssetThread()
	{
		std::vector<AssetLoadRequest> freshAssets;

		m_AssetThread->RetrieveReadyAssets(freshAssets);
		for (const auto& alr : freshAssets)
		{
			m_LoadedAssets[alr.Asset->Handle] = alr.Asset;

			AssetMetaData metaData = alr.MetaData;
			if (metaData.Status != AssetStatus::Invalid)
			{
				metaData.Status = AssetStatus::Ready;
				metaData.IsDataLoaded = true;
			}
			else
				metaData.IsDataLoaded = false;

			m_AssetRegistry[alr.Asset->Handle] = metaData;

			if (alr.Reloaded)
			{
				auto it = m_AssetDependencies.find(alr.Asset->Handle);
				if (it != m_AssetDependencies.end())
				{
					const auto& dependencies = it->second;
					for (AssetHandle dependencyHandle : dependencies)
					{
						Ref<Asset> asset = GetAsset(dependencyHandle);
						if (asset)
							asset->OnDependencyUpdated(alr.Asset->Handle);
					}
				}
			}
		}

		m_AssetThread->UpdateAssetManagerLoadedAssetList(m_LoadedAssets);
	}

	std::unordered_set<AssetHandle> EditorAssetManager::GetAllAssetsWithType(AssetType type) const
	{
		std::unordered_set<AssetHandle> result;
		for (const auto& [handle, metaData] : m_AssetRegistry)
		{
			if (metaData.Type == type)
				result.insert(handle);
		}

		return result;
	}

	const AssetMetaData& EditorAssetManager::GetMetaData(AssetHandle handle) const
	{
		if (m_AssetRegistry.Contains(handle))
			return m_AssetRegistry[handle];

		return s_NullMetaData;
	}

	AssetMetaData& EditorAssetManager::GetMetaData(AssetHandle handle)
	{
		if (m_AssetRegistry.Contains(handle))
			return m_AssetRegistry[handle];

		return s_NullMetaData;
	}

	const AssetMetaData& EditorAssetManager::GetMetaData(const std::filesystem::path& path) const
	{
		const std::filesystem::path relativePath = GetRelativePath(path);

		for (auto& [handle, metaData] : m_AssetRegistry)
		{
			if (relativePath == metaData.FilePath)
				return metaData;
		}

		return s_NullMetaData;
	}

	const AssetMetaData& EditorAssetManager::GetMetaData(const Ref<Asset>& asset) const
	{
		return GetMetaData(asset->Handle);
	}

	AssetHandle EditorAssetManager::ImportAsset(const std::filesystem::path& filePath)
	{
		std::filesystem::path relativePath = GetRelativePath(filePath);

		const AssetMetaData& metaData = GetMetaData(relativePath);
		if (metaData.IsValid())
			return metaData.Handle;

		AssetType type = GetAssetTypeFromPath(relativePath);
		if (type == AssetType::None)
			return 0;

		AssetMetaData metaData2;
		metaData2.Handle = AssetHandle();
		metaData2.FilePath = relativePath;
		metaData2.Type = type;
		m_AssetRegistry[metaData2.Handle] = metaData2;

		return metaData2.Handle;
	}

	AssetType EditorAssetManager::GetAssetTypeFromExtension(const std::string& extension) const
	{
		std::string ext = Utils::ToLower(extension);
		if (s_AssetExtensionMap.contains(ext))
			return s_AssetExtensionMap.at(ext);

		return AssetType::None;
	}

	std::filesystem::path EditorAssetManager::GetFileSystemPath(const AssetMetaData& metaData) const
	{
		return Project::GetAssetDirectory() / metaData.FilePath;
	}

	std::filesystem::path EditorAssetManager::GetRelativePath(const std::filesystem::path& path) const
	{
		std::filesystem::path relativePath = path.lexically_normal();

		std::string temp = path.string();
		if (temp.find(Project::GetAssetDirectory().string()) != std::string::npos)
		{
			relativePath = std::filesystem::relative(path, Project::GetAssetDirectory());
			if (relativePath.empty())
				relativePath = path.lexically_normal();
		}
		
		return relativePath;
	}

	bool EditorAssetManager::FileExists(const AssetMetaData& metaData) const
	{
		return FileSystem::Exists(Project::GetAssetDirectory() / metaData.FilePath);
	}

	Ref<Asset> EditorAssetManager::GetAssetIncludingInvalid(AssetHandle handle)
	{
		Ref<Asset> asset = nullptr;

		if (IsMemoryAsset(handle))
			asset = m_MemoryAssets.at(handle);
		else
		{
			auto& metaData = GetMetaDataInternal(handle);
			if (metaData.IsValid())
			{
				if (!metaData.IsDataLoaded)
				{
					metaData.IsDataLoaded = AssetImporter::TryLoadData(metaData, asset);
					if (metaData.IsDataLoaded)
						m_LoadedAssets[handle] = asset;
				}
				else
					asset = m_LoadedAssets[handle];
			}
		}

		return asset;
	}

	void EditorAssetManager::LoadAssetRegistry()
	{
		IR_CORE_INFO_TAG("AssetManager", "Loading Asset Registry");

		const auto& assetRegistryPath = Project::GetAssetRegistryPath();
		if (!FileSystem::Exists(assetRegistryPath))
			return;

		std::ifstream stream(assetRegistryPath);
		IR_VERIFY(stream);
		std::stringstream strStream;
		strStream << stream.rdbuf();

		YAML::Node data = YAML::Load(strStream.str());
		auto handles = data["Assets"];
		if (!handles)
		{
			IR_CORE_ERROR_TAG("AssetManager", "Asset Registry appears to be corrupted!");
			IR_VERIFY(false);
			return;
		}

		for (auto entry : handles)
		{
			AssetMetaData metaData;

			metaData.Handle = entry["Handle"].as<uint64_t>(0);
			metaData.FilePath = entry["FilePath"].as<std::string>("");
			metaData.Type = Utils::AssetTypeFromString(entry["Type"].as<std::string>(""));

			if (metaData.Type == AssetType::None)
				continue;

			if (metaData.Type != GetAssetTypeFromPath(metaData.FilePath))
			{
				IR_CORE_WARN_TAG("AssetManager", "Mismatch between stored AssetType and extension type when reading asset registry!");
				metaData.Type = GetAssetTypeFromPath(metaData.FilePath);
			}

			if (!FileSystem::Exists(GetFileSystemPath(metaData)))
			{
				IR_CORE_WARN_TAG("AssetManager", "Missing asset '{0}' detected in registry file, trying to locate...", metaData.FilePath);

				std::string mostLikelyCandidate;
				uint32_t bestScore = 0;

				for (auto& pathEntry : std::filesystem::recursive_directory_iterator(Project::GetAssetDirectory()))
				{
					const std::filesystem::path& path = pathEntry.path();

					if (path.filename() != metaData.FilePath.filename())
						continue;

					if (bestScore > 0)
						IR_CORE_WARN_TAG("AssetManager", "Multiple candidates found...");

					std::vector<std::string> candiateParts = Utils::SplitString(path.string(), "/\\");

					uint32_t score = 0;
					for (const auto& part : candiateParts)
					{
						if (metaData.FilePath.string().find(part) != std::string::npos)
							score++;
					}

					IR_CORE_WARN_TAG("AssetManager", "'{0}' has a score of {1}, best score is {2}", path.string(), score, bestScore);

					if (bestScore > 0 && score == bestScore)
					{
						// TODO: How do we handle this?
						// Probably prompt the user at this point?
						IR_VERIFY(false);
					}

					if (score <= bestScore)
						continue;

					bestScore = score;
					mostLikelyCandidate = path.string();
				}

				if (mostLikelyCandidate.empty() && bestScore == 0)
				{
					IR_CORE_ERROR_TAG("AssetManager", "Failed to locate a potential match for '{0}'", metaData.FilePath);
					continue;
				}

				std::replace(mostLikelyCandidate.begin(), mostLikelyCandidate.end(), '\\', '/');
				metaData.FilePath = std::filesystem::relative(mostLikelyCandidate, Project::GetActive()->GetAssetDirectory());
				IR_CORE_WARN_TAG("AssetManager", "Found most likely match '{0}'", metaData.FilePath);
			}

			if (metaData.Handle == 0)
			{
				IR_CORE_WARN_TAG("AssetManager", "AssetHandle for {0} is 0, this shouldn't happen.", metaData.FilePath);
				continue;
			}

			m_AssetRegistry[metaData.Handle] = metaData;
		}

		IR_CORE_INFO_TAG("AssetManager", "Loaded {0} asset entries", m_AssetRegistry.Size());
	}

	void EditorAssetManager::SerializeRegistry()
	{
		struct AssetRegistryEntry
		{
			std::string FilePath;
			AssetType Type;
		};

		std::map<UUID, AssetRegistryEntry> sortedMap;
		for (auto& [handle, metaData] : m_AssetRegistry)
		{
			if (!FileSystem::Exists(GetFileSystemPath(metaData)))
				continue;

			if (metaData.IsMemoryAsset)
				continue;

			std::string pathToSerialize = metaData.FilePath.string();
			std::replace(pathToSerialize.begin(), pathToSerialize.end(), '\\', '/');
			sortedMap[metaData.Handle] = { pathToSerialize, metaData.Type };
		}

		IR_CORE_INFO_TAG("AssetManager", "Serializing asset registry with {0} entries", sortedMap.size());

		YAML::Emitter out;
		out << YAML::BeginMap;

		out << YAML::Key << "Assets" << YAML::BeginSeq;

		for (auto& [handle, entry] : sortedMap)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Handle" << YAML::Value << handle;
			out << YAML::Key << "FilePath" << YAML::Value << entry.FilePath;
			out << YAML::Key << "Type" << YAML::Value << Utils::AssetTypeToString(entry.Type);
			out << YAML::EndMap;
		}

		out << YAML::EndSeq;

		out << YAML::EndMap;

		const std::string& assetRegistryPath = Project::GetAssetRegistryPath().string();
		std::ofstream fout(assetRegistryPath);
		fout << out.c_str();
	}

	void EditorAssetManager::ReloadAssets()
	{
		ProcessDirectory(Project::GetAssetDirectory().string());
		SerializeRegistry();
	}

	void EditorAssetManager::ProcessDirectory(const std::filesystem::path& path)
	{
		for (auto entry : std::filesystem::directory_iterator{ path })
		{
			if (entry.is_directory())
				ProcessDirectory(entry.path());
			else
				ImportAsset(entry.path());
		}
	}

	AssetMetaData& EditorAssetManager::GetMetaDataInternal(AssetHandle handle)
	{
		if (m_AssetRegistry.Contains(handle))
			return m_AssetRegistry[handle];

		return s_NullMetaData; // Make sure you check return value before changing cause you will be editing a null metaData if it is not valid
	}

	void EditorAssetManager::OnAssetRenamed(AssetHandle handle, const std::filesystem::path& newFilePath)
	{
		AssetMetaData& metaData = m_AssetRegistry[handle];
		if (!metaData.IsValid())
			return;

		metaData.FilePath = GetRelativePath(newFilePath);
		SerializeRegistry();
	}

	void EditorAssetManager::OnAssetDeleted(AssetHandle handle)
	{
		AssetMetaData& metaData = m_AssetRegistry[handle];
		if (!metaData.IsValid())
			return;

		m_AssetRegistry.Remove(handle);
		m_LoadedAssets.erase(handle);
		SerializeRegistry();
	}

}