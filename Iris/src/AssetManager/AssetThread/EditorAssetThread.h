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

		[[nodiscard]] inline static Ref<EditorAssetThread> Create()
		{
			return CreateRef<EditorAssetThread>();
		}

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

		constexpr inline float GetAssetThreadSleepTime() const { return m_AssetThreadSleepTime; }
		constexpr inline float GetAssetThreadWaitTime() const { return m_AssetThreadWaitTime; }
		constexpr inline float GetAssetThreadEnsureCurrentTime() const { return m_AssetThreadEnsureCurrentTime; }

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

		std::vector<AssetLoadRequest> m_LoadedAssets; // These are local to the thread and are not yet visible to the engine untill the next sync between AssetManager and AssetThread is done.
		std::mutex m_LoadedAssetsVectorMutex;

		std::unordered_map<AssetHandle, Ref<Asset>> m_AMLoadedAssets;
		std::mutex m_AMLoadedAssetsMapMutex;

		// Asset Monitoring
		float m_AssetThreadSleepTime = 0.0f;
		float m_AssetThreadWaitTime = 0.0f;
		float m_AssetThreadEnsureCurrentTime = 0.0f;

		inline static std::thread::id s_AssetThreadID;

	};

}