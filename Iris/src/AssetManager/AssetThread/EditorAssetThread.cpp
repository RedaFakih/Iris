#include "IrisPCH.h"
#include "EditorAssetThread.h"

#include "AssetManager/Importers/AssetImporter.h"
#include "Core/Application.h"
#include "Core/Timer.h"
#include "ImGui/Themes.h"
#include "Project/Project.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

namespace Iris {

	Ref<EditorAssetThread> EditorAssetThread::Create()
	{
		return CreateRef<EditorAssetThread>();
	}

	EditorAssetThread::EditorAssetThread()
		: m_Thread("Asset Thread")
	{
		s_AssetThreadID = m_Thread.GetID();

		m_Thread.Dispatch([this]() {AssetThreadFunc(); });
	}

	EditorAssetThread::~EditorAssetThread()
	{
		StopAndWait();
		s_AssetThreadID = std::thread::id();
	}

	void EditorAssetThread::QueueAssetLoad(const AssetLoadRequest& request)
	{
		std::scoped_lock<std::mutex> lock(m_AssetLoadingQueueMutex);
		m_AssetLoadingQueue.push(request);
	}

	bool EditorAssetThread::RetrieveReadyAssets(std::vector<AssetLoadRequest>& outAssetList)
	{
		if (m_LoadedAssets.empty())
			return false;

		std::scoped_lock<std::mutex> lock(m_LoadedAssetsVectorMutex);
		outAssetList = m_LoadedAssets;
		// Clear so that we do not have duplicates and leak memory
		m_LoadedAssets.clear();
		return true;
	}

	void EditorAssetThread::UpdateAssetManagerLoadedAssetList(const std::unordered_map<AssetHandle, Ref<Asset>>& loadedAssets)
	{
		std::scoped_lock<std::mutex> lock(m_AMLoadedAssetsMapMutex);
		m_AMLoadedAssets = loadedAssets;
	}

	void EditorAssetThread::Stop()
	{
		m_Running = false;
	}

	void EditorAssetThread::StopAndWait()
	{
		Stop();
		m_Thread.Join();
	}

	void EditorAssetThread::AssetMonitorUpdate()
	{
		Timer timer;
		// TODO:
		// EnsureAllLoadedCurrent();
		m_AssetUpdatePerf = timer.ElapsedMillis();
	}

	void EditorAssetThread::AssetThreadFunc()
	{
		m_Running = true;
		while (m_Running)
		{
			AssetMonitorUpdate();

			bool loadingAssets = false;
			if (!m_AssetLoadingQueue.empty())
			{
				loadingAssets = true;
				Application::Get().DispatchEvent<Events::TitleBarColorChangeEvent>(Colors::Theme::TitlebarOrange);
			}

			// Go through the asset loding request queue if we have any
			if (!m_AssetLoadingQueue.empty())
			{
				m_AssetLoadingQueueMutex.lock();
				// Copy the queue and clear the original one so that it does not stay locked through out the whole asset loading
				std::queue<AssetLoadRequest> assetLoadQeuue = m_AssetLoadingQueue;
				m_AssetLoadingQueue = {};
				m_AssetLoadingQueueMutex.unlock();

				while (!assetLoadQeuue.empty())
				{
					AssetLoadRequest& alr = assetLoadQeuue.front();

					IR_CORE_WARN_TAG("AssetManager", "[AssetThread]: Loading asset: {}", alr.MetaData.FilePath.string());

					// TODO: Here we still can not load textures since it may clash with the other thread submitting to the same queue
					bool success = AssetImporter::TryLoadData(alr.MetaData, alr.Asset);
					if (success)
					{
						auto absolutePath = GetFileSystemPath(alr.MetaData);

						std::scoped_lock<std::mutex> lock(m_LoadedAssetsVectorMutex);
						m_LoadedAssets.emplace_back(alr);
						IR_CORE_INFO_TAG("AssetManager", "[AssetThread]: Finished loading asset: {}", alr.MetaData.FilePath.string());
					}
					else
						IR_CORE_INFO_TAG("AssetManager", "[AssetThread]: Failed to load asset: {} ({})", alr.MetaData.FilePath.string(), alr.MetaData.Handle);

					assetLoadQeuue.pop();
				}
			}

			if (loadingAssets)
			{
				Application::Get().DispatchEvent<Events::TitleBarColorChangeEvent>(Colors::Theme::TitlebarCyan);
			}

			// TEMP (replace with condition var)
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(5ms);
		}
	}

	std::filesystem::path EditorAssetThread::GetFileSystemPath(const AssetMetaData& metaData)
	{
		return Project::GetAssetDirectory() / metaData.FilePath;
	}

	bool EditorAssetThread::ReloadData(AssetHandle handle)
	{
		bool result = false;
		AssetMetaData& metaData = Project::GetEditorAssetManager()->GetMetaDataInternal(handle);
		if (!metaData.IsValid())
		{
			IR_CORE_ERROR_TAG("AssetManager", "Trying to reload invalid asset!");
			return result;
		}

		IR_CORE_INFO_TAG("AssetManager", "[AssetThread]: RELOADING ASSET - {}", metaData.FilePath.string());

		Ref<Asset> asset;
		if (metaData.IsDataLoaded)
			asset = m_AMLoadedAssets.at(handle);

		metaData.IsDataLoaded = AssetImporter::TryLoadData(metaData, asset);
		if (metaData.IsDataLoaded)
		{
			m_LoadedAssets.emplace_back(metaData, asset, true);
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
				for (auto& [loadedHandle, loadedAsset] : m_AMLoadedAssets)
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
						auto& metaData2 = Project::GetEditorAssetManager()->GetMetaDataInternal(loadedHandle);
						Ref<Asset> asset2;
						metaData2.IsDataLoaded = AssetImporter::TryLoadData(metaData2, asset2);
						if (metaData2.IsDataLoaded)
						{
							m_LoadedAssets.emplace_back(metaData2, asset2, true);
							// TODO: Dispatch immediatly application event for asset reloaded
						}
					}
				}
			}
		}

		return result;
	}

}