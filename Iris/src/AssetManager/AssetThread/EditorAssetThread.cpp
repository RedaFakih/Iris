#include "IrisPCH.h"
#include "EditorAssetThread.h"

#include "AssetManager/Importers/AssetImporter.h"
#include "Core/Application.h"
#include "Core/Timer.h"
#include "ImGui/Themes.h"
#include "Project/Project.h"
#include "Renderer/Mesh/Mesh.h"

namespace Iris {

	struct AssetThreadData
	{
		CRITICAL_SECTION CriticalSection;
		CONDITION_VARIABLE ConditionVariable;

		ThreadState State = ThreadState::Idle;
		bool KickRequested = false;
	};


	EditorAssetThread::EditorAssetThread()
		: m_Thread("Asset Thread")
	{
		m_Data = new AssetThreadData();
		InitializeCriticalSection(&m_Data->CriticalSection);
		InitializeConditionVariable(&m_Data->ConditionVariable);
	}

	EditorAssetThread::~EditorAssetThread()
	{
		if (m_Data->State != ThreadState::Joined)
			StopAndWait();

		DeleteCriticalSection(&m_Data->CriticalSection);
		delete m_Data;
	}

	bool EditorAssetThread::IsCurrentlyLoadingAssets() const
	{
		EnterCriticalSection(&m_Data->CriticalSection);
		const bool busy = m_Data->State == ThreadState::Busy;
		LeaveCriticalSection(&m_Data->CriticalSection);
		return busy;
	}

	void EditorAssetThread::QueueAssetLoad(const AssetLoadRequest& request)
	{
		std::scoped_lock<std::mutex> lock(m_AssetLoadingQueueMutex);
		m_AssetLoadingQueue.push(request);
	}

	bool EditorAssetThread::RetrieveReadyAssets(std::vector<AssetLoadRequest>& outAssetList)
	{
		std::scoped_lock<std::mutex> lock(m_LoadedAssetsVectorMutex);
		if (m_LoadedAssets.empty())
			return false;

		outAssetList.swap(m_LoadedAssets);
		return true;
	}

	void EditorAssetThread::UpdateAssetManagerLoadedAssetList(const std::unordered_map<AssetHandle, Ref<Asset>>& loadedAssets)
	{
		std::scoped_lock<std::mutex> lock(m_AMLoadedAssetsMapMutex);
		m_AMLoadedAssets = loadedAssets;
	}

	void EditorAssetThread::Run()
	{
		m_Running = true;
		m_Thread.Dispatch([this]() { AssetThreadFunc(); });
	}

	void EditorAssetThread::Stop()
	{
		m_Running = false;
	}

	void EditorAssetThread::StopAndWait()
	{
		Stop();

		Kick();
		m_Thread.Join();

		m_Data->State = ThreadState::Joined;
	}

	void EditorAssetThread::AssetMonitorUpdate()
	{
		Timer timer;
		// TODO:
		// EnsureAllLoadedCurrent();
		m_AssetThreadEnsureCurrentTime = timer.ElapsedMillis();
	}

	void EditorAssetThread::Set(ThreadState stateToSet)
	{
		EnterCriticalSection(&m_Data->CriticalSection);
		m_Data->State = stateToSet;
		WakeAllConditionVariable(&m_Data->ConditionVariable);
		LeaveCriticalSection(&m_Data->CriticalSection);
	}

	void EditorAssetThread::Kick()
	{
		EnterCriticalSection(&m_Data->CriticalSection);
		m_Data->KickRequested = true;
		WakeAllConditionVariable(&m_Data->ConditionVariable);
		LeaveCriticalSection(&m_Data->CriticalSection);
	}

	void EditorAssetThread::WaitForKick()
	{
		Timer assetThreadWaitTimer;

		EnterCriticalSection(&m_Data->CriticalSection);
		while (!m_Data->KickRequested)
		{
			SleepConditionVariableCS(&m_Data->ConditionVariable, &m_Data->CriticalSection, INFINITE);
		}

		// Consume the kick BEFORE draining the queue: a Kick() that lands while we are loading
		// sets the flag again, so the next loop iteration sees it instead of losing it
		m_Data->KickRequested = false;
		m_Data->State = ThreadState::Busy;
		LeaveCriticalSection(&m_Data->CriticalSection);

		m_AssetThreadWaitTime = assetThreadWaitTimer.ElapsedMillis();
	}

	void EditorAssetThread::AssetThreadFunc()
	{
		while (m_Running)
		{
			// Here we wait untill the Kick state is set, then we set state to Busy
			WaitForKick();

			AssetMonitorUpdate();

			// Go through the asset loding request queue if we have any
			// We use a while loop in case the user queues asset loads during an asset is being loaded
			for (;;)
			{
				std::queue<AssetLoadRequest> assetLoadQueue;
				{
					std::scoped_lock<std::mutex> lock(m_AssetLoadingQueueMutex);
					if (m_AssetLoadingQueue.empty())
						break;

					assetLoadQueue.swap(m_AssetLoadingQueue);
				}

				Application::Get().DispatchEvent<Events::TitleBarColorChangeEvent>(Colors::Theme::TitlebarRed);

				std::vector<AssetLoadRequest> loadedAssetsCopy;
				loadedAssetsCopy.reserve(assetLoadQueue.size());
				
				while (!assetLoadQueue.empty())
				{
					AssetLoadRequest& alr = assetLoadQueue.front();

					IR_CORE_WARN_TAG("AssetManager", "[AssetThread]: Loading asset: {}", alr.MetaData.FilePath.string());

					bool success = AssetImporter::TryLoadData(alr.MetaData, alr.Asset);
					if (success)
					{
						auto absolutePath = GetFileSystemPath(alr.MetaData);
						IR_CORE_INFO_TAG("AssetManager", "[AssetThread]: Finished loading asset: {}", alr.MetaData.FilePath.string());
					}
					else
					{
						alr.MetaData.Status = AssetStatus::Invalid;
						IR_CORE_INFO_TAG("AssetManager", "[AssetThread]: Failed to load asset: {} ({})", alr.MetaData.FilePath.string(), alr.MetaData.Handle);
					}

					// NOTE: In case the asset failed we still want to know what its meta data is... so we set the status to invalid
					// And if the loading failed then the asset flags will most probably say the asset is invalid/missing
					loadedAssetsCopy.push_back(std::move(alr));

					assetLoadQueue.pop();
				}

				{
					std::scoped_lock<std::mutex> lock(m_LoadedAssetsVectorMutex);
					for (AssetLoadRequest& alr : loadedAssetsCopy)
						m_LoadedAssets.push_back(std::move(alr));
				}

				Application::Get().DispatchEvent<Events::TitleBarColorChangeEvent>(Colors::Theme::TitlebarCyan);
			}

			// Return the thread state back to idle since we finished work
			Set(ThreadState::Idle);
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