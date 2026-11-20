#pragma once

#include "AssetSerializer.h"
#include "Renderer/Mesh/Mesh.h"

namespace Iris {

	struct MeshColliderAsset;

	struct MeshSourceSerializer : public AssetSerializer
	{
		virtual void Serialize(const AssetMetaData& metaData, const Ref<Asset>& asset) const override { (void)metaData, (void)asset; }
		virtual bool TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const override;
	};

	struct StaticMeshSerializer : public AssetSerializer
	{
		virtual void Serialize(const AssetMetaData& metaData, const Ref<Asset>& asset) const override;
		virtual bool TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const override;

	private:
		std::string SerializeToYAML(Ref<StaticMesh> staticMesh) const;
		bool DeserializeFromYAML(const std::string& yamlString, Ref<StaticMesh>& targetStaticMesh) const;
	};

}