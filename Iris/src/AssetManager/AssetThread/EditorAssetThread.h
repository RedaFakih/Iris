#pragma once

#include "AssetManager/Asset/AssetMetaData.h"
#include "Core/Base.h"
#include "Core/Thread.h"

#include <queue>

namespace Iris {

	class EditorAssetThread : public RefCountedObject
	{
	public:
		EditorAssetThread();
		~EditorAssetThread();

		[[nodiscard]] static Ref<EditorAssetThread> Create();

		void QueueAssetLoad(const AssetLoadRequest& request);
		bool RetrieveReadyAssets(std::vector<AssetLoadRequest>& outAssetList);
		void UpdateAssetManagerLoadedAssetList(const std::unordered_map<AssetHandle, Ref<Asset>>& loadedAssets);

		void Stop();
		void StopAndWait();
		void AssetMonitorUpdate();

	private:
		void AssetThreadFunc();

		std::filesystem::path GetFileSystemPath(const AssetMetaData& metaData);

		bool ReloadData(AssetHandle handle);

	private:
		Thread m_Thread;
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