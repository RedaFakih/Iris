#include "IrisPCH.h"
#include "AssetSerializer.h"

#include "AssetManager/AssetManager.h"
#include "Project/Project.h"
#include "Renderer/Renderer.h"
#include "Renderer/StorageBufferSet.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"
#include "Renderer/Text/Font.h"
#include "Scene/Scene.h"
#include "Scene/SceneEnvironment.h"
#include "Utils/YAMLSerializationHelpers.h"

#include <yaml-cpp/yaml.h>

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
		YAML::Emitter out;

		out << YAML::BeginMap;

		out << YAML::Key << "Material" << YAML::Value;
		{
			out << YAML::BeginMap;

			// TODO: Here we should check if the material's shader is the same as the registered transparent shader that way we make sure if it is transparent
			bool transparent = materialAsset->IsTransparent();
			bool twoSided = materialAsset->IsDoubleSided();
			out << YAML::Key << "Transparent" << YAML::Value << transparent;
			out << YAML::Key << "TwoSided" << YAML::Value << twoSided;
			out << YAML::Key << "AlbedoColor" << YAML::Value << materialAsset->GetAlbedoColor();
			out << YAML::Key << "Emission" << YAML::Value << materialAsset->GetEmission();
			out << YAML::Key << "Tiling" << YAML::Value << materialAsset->GetTiling();
			out << YAML::Key << "EnvMapRotation" << YAML::Value << materialAsset->GetEnvMapRotation();

			if (!transparent)
			{
				out << YAML::Key << "UseNormalMap" << YAML::Value << materialAsset->IsUsingNormalMap();
				out << YAML::Key << "Roughness" << YAML::Value << materialAsset->GetRoughness();
				out << YAML::Key << "Metalness" << YAML::Value << materialAsset->GetMetalness();
			}
			else
			{
				out << YAML::Key << "Transparency" << YAML::Value << materialAsset->GetTransparency();
			}

			Ref<Texture2D> albedoMap = materialAsset->GetAlbedoMap();
			bool hasAlbedoMap = albedoMap ? !albedoMap.EqualsObject(Renderer::GetWhiteTexture()) : false;
			AssetHandle albedoMapHandle = hasAlbedoMap ? albedoMap->Handle : UUID(0);
			out << YAML::Key << "AlbedoMap" << YAML::Value << albedoMapHandle;

			if (!transparent)
			{
				{
					Ref<Texture2D> normalMap = materialAsset->GetNormalMap();
					bool hasNormalMap = normalMap ? !normalMap.EqualsObject(Renderer::GetWhiteTexture()) : false;
					AssetHandle normalMapHandle = hasNormalMap ? normalMap->Handle : UUID(0);
					out << YAML::Key << "NormalMap" << YAML::Value << normalMapHandle;
				}

				{
					Ref<Texture2D> roughnessMap = materialAsset->GetRoughnessMap();
					bool hasRoughnessMap = roughnessMap ? !roughnessMap.EqualsObject(Renderer::GetWhiteTexture()) : false;
					AssetHandle roughnessMapHandle = hasRoughnessMap ? roughnessMap->Handle : UUID(0);
					out << YAML::Key << "RoughnessMap" << YAML::Value << roughnessMapHandle;
				}

				{
					Ref<Texture2D> metalnessMap = materialAsset->GetMetalnessMap();
					bool hasMetalnessMap = metalnessMap ? !metalnessMap.EqualsObject(Renderer::GetWhiteTexture()) : false;
					AssetHandle metalnessMapHandle = hasMetalnessMap ? metalnessMap->Handle : UUID(0);
					out << YAML::Key << "MetalnessMap" << YAML::Value << metalnessMapHandle;
				}
			}

			out << YAML::Key << "MaterialFlags" << YAML::Value << materialAsset->GetMaterial()->GetMaterialFlags();

			out << YAML::EndMap;
		}

		out << YAML::EndMap;

		return std::string(out.c_str());
	}

	bool MaterialAssetSerializer::DeserializeFromYAML(const std::string& yamlString, Ref<MaterialAsset>& targetAsset, AssetHandle handle) const
	{
		YAML::Node rootNode = YAML::Load(yamlString);
		YAML::Node materialNode = rootNode["Material"];

		if (!materialNode)
			return false;

		bool transparent = false;
		transparent = materialNode["Transparent"].as<bool>(false);
		bool twoSided = materialNode["TwoSided"].as<bool>(false);

		targetAsset = MaterialAsset::Create(transparent);
		targetAsset->Handle = handle;

		glm::vec3 albedoColor = materialNode["AlbedoColor"].as<glm::vec3>(glm::vec3(0.8f));
		targetAsset->SetAlbedoColor(albedoColor);

		float emission = materialNode["Emission"].as<float>(0.0f);
		targetAsset->SetEmission(emission);

		float tiling = materialNode["Tiling"].as<float>(1.0f);
		targetAsset->SetTiling(tiling);

		float envMapRotation = materialNode["EnvMapRotation"].as<float>(0.0f);
		targetAsset->SetEnvMapRotation(envMapRotation);

		targetAsset->SetDoubleSided(twoSided);

		if (!transparent)
		{
			bool useNormalMap = materialNode["UseNormalMap"].as<bool>(false);
			targetAsset->SetUseNormalMap(useNormalMap);

			float roughness = materialNode["Roughness"].as<float>(0.5f);
			targetAsset->SetRoughness(roughness);

			float metalness = materialNode["Metalness"].as<float>(0.0f);
			targetAsset->SetMetalness(metalness);
		}
		else
		{
			float transparency = materialNode["Transparency"].as<float>(1.0f);
			targetAsset->SetTransparency(transparency);
		}

		AssetHandle albedoMap, normalMap, roughnessMap, metalnessMap;
		albedoMap = materialNode["AlbedoMap"].as<AssetHandle>(AssetHandle(0));

		if (!transparent)
		{
			normalMap = materialNode["NormalMap"].as<AssetHandle>(AssetHandle(0));
			roughnessMap = materialNode["RoughnessMap"].as<AssetHandle>(AssetHandle(0));
			metalnessMap = materialNode["MetalnessMap"].as<AssetHandle>(AssetHandle(0));
		}

		if (albedoMap)
		{
			if (AssetManager::IsAssetHandleValid(albedoMap))
				targetAsset->SetAlbedoMap(albedoMap);
		}

		if (normalMap)
		{
			if (AssetManager::IsAssetHandleValid(normalMap))
				targetAsset->SetNormalMap(normalMap);
		}

		if (roughnessMap)
		{
			if (AssetManager::IsAssetHandleValid(roughnessMap))
				targetAsset->SetRoughnessMap(roughnessMap);
		}

		if (metalnessMap)
		{
			if (AssetManager::IsAssetHandleValid(metalnessMap))
				targetAsset->SetMetalnessMap(metalnessMap);
		}

		uint32_t materialFlags = materialNode["MaterialFlags"].as<uint32_t>(0);
		targetAsset->GetMaterial()->SetFlags(materialFlags);

		return true;
	}

	//////////////////////////////////////////////
	/// FontSerializer
	//////////////////////////////////////////////

	bool FontSerializer::TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const
	{
		asset = Font::Create(Project::GetEditorAssetManager()->GetFileSystemPathString(metaData));
		asset->Handle = metaData.Handle;

		return true;
	}

	//////////////////////////////////////////////
	/// EnvironmentSerializer
	//////////////////////////////////////////////

	bool EnvironmentSerializer::TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const
	{
		auto [radiance, irradiance] = Renderer::CreateEnvironmentMap(Project::GetEditorAssetManager()->GetFileSystemPathString(metaData));
	
		if (!radiance || !irradiance)
			return false;
	
		asset = Environment::Create(radiance, irradiance);
		asset->Handle = metaData.Handle;
		return true;
	}

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