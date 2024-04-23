#pragma once

#include "AssetSerializer.h"
#include "Serialization/FileStream.h"

namespace Iris {

	class AssetImporter
	{
	public:
		static void Init();
		static void Serialize(const AssetMetaData& metadata, const Ref<Asset>& asset);
		static void Serialize(const Ref<Asset>& asset);
		static bool TryLoadData(const AssetMetaData& metadata, Ref<Asset>& asset);

	private:
		static std::unordered_map<AssetType, Scope<AssetSerializer>> s_Serializers;
	};

}