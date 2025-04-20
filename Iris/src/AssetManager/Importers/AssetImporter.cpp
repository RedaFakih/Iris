#include "IrisPCH.h"
#include "AssetImporter.h"

#include "MeshSerializer.h"
#include "Project/Project.h"

namespace Iris {

	std::unordered_map<AssetType, Scope<AssetSerializer>> AssetImporter::s_Serializers;

	void AssetImporter::Init()
	{
		s_Serializers.clear();
		s_Serializers[AssetType::Texture] = CreateScope<TextureSerializer>();
		s_Serializers[AssetType::StaticMesh] = CreateScope<StaticMeshSerializer>();
		s_Serializers[AssetType::MeshSource] = CreateScope<MeshSourceSerializer>();
		s_Serializers[AssetType::MeshCollider] = CreateScope<MeshColliderSerializer>();
		s_Serializers[AssetType::Material] = CreateScope<MaterialAssetSerializer>();
		s_Serializers[AssetType::Font] = CreateScope<FontSerializer>();
		s_Serializers[AssetType::EnvironmentMap] = CreateScope<EnvironmentSerializer>();
		s_Serializers[AssetType::Scene] = CreateScope<SceneAssetSerializer>();
	}

	void AssetImporter::Serialize(const AssetMetaData& metaData, const Ref<Asset>& asset)
	{
		if (!s_Serializers.contains(metaData.Type))
		{
			IR_CORE_WARN_TAG("AssetManager", "There's currently no importer for assets of type {0}", metaData.FilePath.stem().string());
			return;
		}

		s_Serializers[asset->GetAssetType()]->Serialize(metaData, asset);
	}

	void AssetImporter::Serialize(const Ref<Asset>& asset)
	{
		const AssetMetaData& metaData = Project::GetEditorAssetManager()->GetMetaData(asset->Handle);
		Serialize(metaData, asset);
	}

	bool AssetImporter::TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset)
	{
		if (s_Serializers.find(metaData.Type) == s_Serializers.end())
		{
			IR_CORE_WARN_TAG("AssetManager", "There's currently no importer for assets of type {0}", metaData.FilePath.stem().string());
			return false;
		}

		return s_Serializers[metaData.Type]->TryLoadData(metaData, asset);
	}

}