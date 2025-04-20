#pragma once

#include "AssetManager/Asset/Asset.h"
#include "Renderer/Texture.h"
#include "ThumbnailGenerator.h"

namespace Iris {

	class ThumbnailCache : public RefCountedObject
	{
	public:
		ThumbnailCache() = default;
		~ThumbnailCache() = default;

		[[nodiscard]] inline static Ref<ThumbnailCache> Create()
		{
			return CreateRef<ThumbnailCache>();
		}

		bool HasThumbnail(AssetHandle assetHandle);

		// TODO: Might we use this? Since maybe if we load the engine
		//bool IsThumbnailCurrent(AssetHandle assetHandle);
		//uint64_t GetAssetTimestamp(AssetHandle assetHandle);
		//uint64_t GetThumbnailTimestamp(AssetHandle assetHandle);

		Ref<Texture2D> GetThumbnailImage(AssetHandle assetHandle);
		void SetThumbnailImage(AssetHandle assetHandle, Ref<Texture2D> image, uint64_t timestamp);

		void Clear();

	private:
		std::map<AssetHandle, ThumbnailImage> m_ThumbnailImages;

	};

}