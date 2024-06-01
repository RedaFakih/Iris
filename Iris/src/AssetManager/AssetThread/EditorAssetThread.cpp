#include "IrisPCH.h"
#include "EditorAssetThread.h"

#include "AssetManager/Importers/AssetImporter.h"
#include "Core/Application.h"
#include "Core/Timer.h"
#include "ImGui/Themes.h"
#include "Project/Project.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/StorageBufferSet.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

namespace Iris {

	struct AssetThreadData
	{
		CRITICAL_SECTION CriticalSection;
		CONDITION_VARIABLE ConditionVariable;

		ThreadState State = ThreadState::Idle;
	};

	Ref<EditorAssetThread> EditorAssetThread::Create()
	{
		return CreateRef<EditorAssetThread>();
	}

	EditorAssetThread::EditorAssetThread()
		: m_Thread("Asset Thread")
	{
		s_AssetThreadID = m_Thread.GetID();

		m_Data = new AssetThreadData();
		InitializeCriticalSection(&m_Data->CriticalSection);
		InitializeConditionVariable(&m_Data->ConditionVariable);
	}

	EditorAssetThread::~EditorAssetThread()
	{
		if (m_Data->State != ThreadState::Joined)
			StopAndWait(true);

		DeleteCriticalSection(&m_Data->CriticalSection);
		s_AssetThreadID = std::thread::id();
	}

	bool EditorAssetThread::IsCurrentlyLoadingAssets() const
	{
		return m_Data->State == ThreadState::Busy;
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

	void EditorAssetThread::Run()
	{
		m_Running = true;
		m_Thread.Dispatch([this]() { AssetThreadFunc(); });
	}

	void EditorAssetThread::Stop()
	{
		m_Running = false;
	}

	void EditorAssetThread::StopAndWait(bool terminateThread)
	{
		Stop();

		Set(ThreadState::Kick);
		Wait(ThreadState::Idle);

		if (terminateThread)
		{
			Set(ThreadState::Joined);
			m_Thread.Join();

			s_AssetThreadID = std::thread::id();
		}
	}

	void EditorAssetThread::AssetMonitorUpdate()
	{
		Timer timer;
		// TODO:
		// EnsureAllLoadedCurrent();
		m_AssetUpdatePerf = timer.ElapsedMillis();
	}

	void EditorAssetThread::Wait(ThreadState stateToWait)
	{
		// Nothing to wait for
		if (m_Data->State == stateToWait)
			return;

		EnterCriticalSection(&m_Data->CriticalSection);
		while (m_Data->State != stateToWait)
		{
			SleepConditionVariableCS(&m_Data->ConditionVariable, &m_Data->CriticalSection, INFINITE);
		}
		LeaveCriticalSection(&m_Data->CriticalSection);
	}

	void EditorAssetThread::Set(ThreadState stateToSet)
	{
		// Already set
		if (m_Data->State == stateToSet)
			return;

		EnterCriticalSection(&m_Data->CriticalSection);
		m_Data->State = stateToSet;
		WakeAllConditionVariable(&m_Data->ConditionVariable);
		LeaveCriticalSection(&m_Data->CriticalSection);
	}

	void EditorAssetThread::WaitAndSet(ThreadState stateToWait, ThreadState stateToSet)
	{
		// Nothing to wait for
		if (m_Data->State == stateToWait)
			return;

		EnterCriticalSection(&m_Data->CriticalSection);
		while (m_Data->State != stateToWait)
		{
			SleepConditionVariableCS(&m_Data->ConditionVariable, &m_Data->CriticalSection, INFINITE);
		}
		m_Data->State = stateToSet;
		WakeAllConditionVariable(&m_Data->ConditionVariable);
		LeaveCriticalSection(&m_Data->CriticalSection);
	}

	void EditorAssetThread::AssetThreadFunc()
	{
		while (m_Running)
		{
			// Here we wait untill the Kick state is set, then we set state to Busy
			{
				Timer assetThreadWaitTimer;

				WaitAndSet(ThreadState::Kick, ThreadState::Busy);

				assetThreadWaitTimer.ElapsedMillis();
			}

			AssetMonitorUpdate();

			// Go through the asset loding request queue if we have any
			// We use a while loop in case the user queues asset loads during an asset is being loaded
			while (!m_AssetLoadingQueue.empty())
			{
				Application::Get().DispatchEvent<Events::TitleBarColorChangeEvent, true>(Colors::Theme::TitlebarRed);

				m_AssetLoadingQueueMutex.lock();
				// Copy the queue and clear the original one so that it does not stay locked through out the whole asset loading
				std::queue<AssetLoadRequest> assetLoadQeuue = m_AssetLoadingQueue;
				m_AssetLoadingQueue = {};
				m_AssetLoadingQueueMutex.unlock();

				std::vector<AssetLoadRequest> loadedAssetsCopy(assetLoadQeuue.size());
				uint32_t index = 0;
				while (!assetLoadQeuue.empty())
				{
					AssetLoadRequest& alr = assetLoadQeuue.front();

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
					loadedAssetsCopy[index++] = alr;

					assetLoadQeuue.pop();
				}

				std::scoped_lock<std::mutex> lock(m_LoadedAssetsVectorMutex);
				// Extend the vector to hold the extra assets
				m_LoadedAssets.reserve(m_LoadedAssets.size() +  loadedAssetsCopy.size());
				for (uint32_t i = 0; i < loadedAssetsCopy.size(); i++)
				{
					m_LoadedAssets.push_back(loadedAssetsCopy[i]);
				}

				Application::Get().DispatchEvent<Events::TitleBarColorChangeEvent, true>(Colors::Theme::TitlebarCyan);
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