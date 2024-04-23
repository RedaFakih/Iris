#pragma once

#include "AssetManager/Asset/AssetMetaData.h"

namespace Iris {

	class MaterialAsset;
	class Scene;

	struct AssetSerializer
	{
		virtual void Serialize(const AssetMetaData& metadata, const Ref<Asset>& asset) const = 0;
		virtual bool TryLoadData(const AssetMetaData& metadata, Ref<Asset>& asset) const = 0;
	};

	struct TextureSerializer : public AssetSerializer
	{
		virtual void Serialize(const AssetMetaData& metadata, const Ref<Asset>& asset) const override {}
		virtual bool TryLoadData(const AssetMetaData& metadata, Ref<Asset>& asset) const override;
	};

	struct MaterialAssetSerializer : public AssetSerializer
	{
		virtual void Serialize(const AssetMetaData& metadata, const Ref<Asset>& asset) const override;
		virtual bool TryLoadData(const AssetMetaData& metadata, Ref<Asset>& asset) const override;

	private:
		std::string SerializeToYAML(Ref<MaterialAsset> materialAsset) const;
		bool DeserializeFromYAML(const std::string& yamlString, Ref<MaterialAsset>& targetAsset, AssetHandle handle) const;
	};

	// TODO:
	// struct EnvironmentSerializer : public AssetSerializer
	// {
	// 	virtual void Serialize(const AssetMetaData& metadata, const Ref<Asset>& asset) const override {}
	// 	virtual bool TryLoadData(const AssetMetaData& metadata, Ref<Asset>& asset) const override;
	// };

	// TODO:
	// struct SceneAssetSerializer : public AssetSerializer
	// {
	// 	virtual void Serialize(const AssetMetaData& metadata, const Ref<Asset>& asset) const override;
	// 	virtual bool TryLoadData(const AssetMetaData& metadata, Ref<Asset>& asset) const override;
	// };

}