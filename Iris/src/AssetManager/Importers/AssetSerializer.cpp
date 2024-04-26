#include "IrisPCH.h"
#include "AssetSerializer.h"

#include "Project/Project.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"
#include "Scene/Scene.h"

namespace Iris {

	//////////////////////////////////////////////
	/// TextureSerializer
	//////////////////////////////////////////////

	bool TextureSerializer::TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const
	{
		asset = Texture2D::Create(TextureSpecification(), Project::GetEditorAssetManager()->GetFileSystemPathString(metaData));
		asset->Handle = metaData.Handle;

		bool result = asset.As<Texture2D>()->Loaded();
		if (!result)
			asset->SetFlag(AssetFlag::Invalid, true);

		return result;
	}

	//////////////////////////////////////////////
	/// MaterialAssetSerialzier
	//////////////////////////////////////////////

	void MaterialAssetSerializer::Serialize(const AssetMetaData& metaData, const Ref<Asset>& asset) const
	{
		Ref<MaterialAsset> materialAsset = asset.As<MaterialAsset>();

		std::string yamlString = SerializeToYAML(materialAsset);

		std::ofstream fout(Project::GetEditorAssetManager()->GetFileSystemPath(metaData));
		fout << yamlString;
	}

	bool MaterialAssetSerializer::TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const
	{
		std::ifstream stream(Project::GetEditorAssetManager()->GetFileSystemPath(metaData));
		if (!stream.is_open())
		{
			asset->SetFlag(AssetFlag::Missing);
			return false;
		}

		std::stringstream strStream;
		strStream << stream.rdbuf();

		Ref<MaterialAsset> materialAsset;
		bool success = DeserializeFromYAML(strStream.str(), materialAsset, metaData.Handle);
		if (!success)
		{
			asset->SetFlag(AssetFlag::Invalid);
			return false;
		}

		asset = materialAsset;
		return true;
	}

	std::string MaterialAssetSerializer::SerializeToYAML(Ref<MaterialAsset> materialAsset) const
	{
		// TODO:
		return "";
	}

	bool MaterialAssetSerializer::DeserializeFromYAML(const std::string& yamlString, Ref<MaterialAsset>& targetAsset, AssetHandle handle) const
	{
		// TODO:
		return false;
	}

	//////////////////////////////////////////////
	/// EnvironmentSerializer
	//////////////////////////////////////////////

	// TODO:
	// bool EnvironmentSerializer::TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const
	// {
	// 	auto [radiance, irradiance] = Renderer::CreateEnvironmentMap(Project::GetEditorAssetManager()->GetFileSystemPathString(metaData));
	// 
	// 	if (!radiance || !irradiance)
	// 		return false;
	// 
	// 	asset = Ref<Environment>::Create(radiance, irradiance);
	// 	asset->Handle = metaData.Handle;
	// 	return true;
	// }

	//////////////////////////////////////////////
	/// SceneAssetSerializer
	//////////////////////////////////////////////

	// TODO:
	// void SceneAssetSerializer::Serialize(const AssetMetaData& metaData, const Ref<Asset>& asset) const
	// {
	// 	SceneSerializer serializer(asset.As<Scene>());
	// 	serializer.Serialize(Project::GetEditorAssetManager()->GetFileSystemPath(metaData).string());
	// }
	// 
	// bool SceneAssetSerializer::TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const
	// {
	// 	asset = Scene::Create("SceneAsset", false);
	// 	asset->Handle = metaData.Handle;
	// 	return true;
	// }

}