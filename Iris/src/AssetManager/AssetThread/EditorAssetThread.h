#pragma once

#include "AssetManager/Asset/AssetMetaData.h"
#include "Core/Base.h"
#include "Core/Thread.h"

#include <queue>

/*
 * Asset thread waits a kick from the asset manager generally to load assets
 * TODO: When having ContentBrowserPanel, we may have to kick the asset thread from there to infom the thread of any asset changes in the current directory
 * so that we can trigger a reload
 */

namespace Iris {

	struct AssetThreadData;

	class EditorAssetThread : public RefCountedObject
	{
	public:
		EditorAssetThread();
		~EditorAssetThread();

		[[nodiscard]] static Ref<EditorAssetThread> Create();

		bool IsRunning() const { return m_Running; }
		bool IsCurrentlyLoadingAssets() const;

		void QueueAssetLoad(const AssetLoadRequest& request);
		bool RetrieveReadyAssets(std::vector<AssetLoadRequest>& outAssetList);
		void UpdateAssetManagerLoadedAssetList(const std::unordered_map<AssetHandle, Ref<Asset>>& loadedAssets);

		void Run();
		void Stop();
		void StopAndWait(bool terminateThread = false);
		void AssetMonitorUpdate();

		void Wait(ThreadState stateToWait);
		void Set(ThreadState stateToSet);
		void WaitAndSet(ThreadState stateToWait, ThreadState stateToSet);

	private:
		void AssetThreadFunc();

		std::filesystem::path GetFileSystemPath(const AssetMetaData& metaData);

		bool ReloadData(AssetHandle handle);

	private:
		Thread m_Thread;
		AssetThreadData* m_Data;

		bool m_Running = false;

		std::queue<AssetLoadRequest> m_AssetLoadingQueue;
		std::mutex m_AssetLoadingQueueMutex;

		std::vector<AssetLoadRequest> m_LoadedAssets;
		std::mutex m_LoadedAssetsVectorMutex;

		std::unordered_map<AssetHandle, Ref<Asset>> m_AMLoadedAssets;
		std::mutex m_AMLoadedAssetsMapMutex;

		// TODO: What the heck?
		// Asset Monitoring
		float m_AssetUpdateDelay = 0.2f;
		float m_AssetUpdateTimer = 0.2f;
		float m_AssetUpdatePerf = 0.0f;

		inline static std::thread::id s_AssetThreadID;

	};

}