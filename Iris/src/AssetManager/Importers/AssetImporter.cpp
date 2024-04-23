#include "IrisPCH.h"
#include "AssetImporter.h"

#include "MeshSerializer.h"
#include "Project/Project.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

namespace Iris {

	std::unordered_map<AssetType, Scope<AssetSerializer>> AssetImporter::s_Serializers;

	void AssetImporter::Init()
	{
		s_Serializers.clear();
		s_Serializers[AssetType::Texture] = CreateScope<TextureSerializer>();
		s_Serializers[AssetType::StaticMesh] = CreateScope<StaticMeshSerializer>();
		s_Serializers[AssetType::MeshSource] = CreateScope<MeshSourceSerializer>();
		s_Serializers[AssetType::Material] = CreateScope<MaterialAssetSerializer>();
		// s_Serializers[AssetType::EnvironmentMap] = CreateScope<EnvironmentSerializer>();
		// s_Serializers[AssetType::Scene] = CreateScope<SceneAssetSerializer>();
	}

	void AssetImporter::Serialize(const AssetMetaData& metadata, const Ref<Asset>& asset)
	{
		if (!s_Serializers.contains(metadata.Type))
		{
			IR_CORE_WARN_TAG("AssetManager", "There's currently no importer for assets of type {0}", metadata.FilePath.stem().string());
			return;
		}

		s_Serializers[asset->GetAssetType()]->Serialize(metadata, asset);
	}

	void AssetImporter::Serialize(const Ref<Asset>& asset)
	{
		const AssetMetaData& metadata = Project::GetEditorAssetManager()->GetMetaData(asset->Handle);
		Serialize(metadata, asset);
	}

	bool AssetImporter::TryLoadData(const AssetMetaData& metadata, Ref<Asset>& asset)
	{
		if (s_Serializers.find(metadata.Type) == s_Serializers.end())
		{
			IR_CORE_WARN_TAG("AssetManager", "There's currently no importer for assets of type {0}", metadata.FilePath.stem().string());
			return false;
		}

		return s_Serializers[metadata.Type]->TryLoadData(metadata, asset);
	}

}