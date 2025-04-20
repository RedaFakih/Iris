#pragma once

#include "AssetManager/Asset/AssetMetaData.h"

namespace Iris {

	class MaterialAsset;
	class Scene;

	struct AssetSerializer
	{
		virtual void Serialize(const AssetMetaData& metaData, const Ref<Asset>& asset) const = 0;
		virtual bool TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const = 0;
	};

	struct TextureSerializer : public AssetSerializer
	{
		virtual void Serialize(const AssetMetaData& metaData, const Ref<Asset>& asset) const override { (void)metaData, (void)asset; }
		virtual bool TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const override;
	};

	struct MaterialAssetSerializer : public AssetSerializer
	{
		virtual void Serialize(const AssetMetaData& metaData, const Ref<Asset>& asset) const override;
		virtual bool TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const override;

	private:
		std::string SerializeToYAML(Ref<MaterialAsset> materialAsset) const;
		bool DeserializeFromYAML(const std::string& yamlString, Ref<MaterialAsset>& targetAsset, AssetHandle handle) const;
	};

	struct FontSerializer : public AssetSerializer
	{
		virtual void Serialize(const AssetMetaData& metaData, const Ref<Asset>& asset) const override { (void)metaData, (void)asset; }
		virtual bool TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const override;
	};

	struct EnvironmentSerializer : public AssetSerializer
	{
		virtual void Serialize(const AssetMetaData& metaData, const Ref<Asset>& asset) const override { (void)metaData, (void)asset; }
		virtual bool TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const override;
	};

	struct SceneAssetSerializer : public AssetSerializer
	{
		virtual void Serialize(const AssetMetaData& metaData, const Ref<Asset>& asset) const override;
		virtual bool TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const override;
	};

}