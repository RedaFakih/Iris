#include "ThumbnailCache.h"

namespace Iris {

	bool ThumbnailCache::HasThumbnail(AssetHandle assetHandle)
	{
		auto thumbnail = m_ThumbnailImages.find(assetHandle);
		if (thumbnail != m_ThumbnailImages.end() && thumbnail->second.Image != nullptr)
			return true;

		return false;
	}

	Ref<Texture2D> ThumbnailCache::GetThumbnailImage(AssetHandle assetHandle)
	{
		if (!HasThumbnail(assetHandle))
			return nullptr;

		return m_ThumbnailImages.at(assetHandle).Image;
	}

	void ThumbnailCache::SetThumbnailImage(AssetHandle assetHandle, Ref<Texture2D> image, uint64_t timestamp)
	{
		m_ThumbnailImages[assetHandle] = { image, timestamp };
	}

	void ThumbnailCache::Clear()
	{
		m_ThumbnailImages.clear();
	}

}