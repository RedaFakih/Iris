#include "IrisPCH.h"
#include "MaterialAsset.h"

#include "AssetManager/AssetManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/Shaders/Shader.h"
#include "Renderer/StorageBufferSet.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

namespace Iris {

	constexpr static const char* s_AlbedoColorUniform = "u_MaterialUniforms.AlbedoColor";
	constexpr static const char* s_RoughnessUniform = "u_MaterialUniforms.Roughness";
	constexpr static const char* s_MetalnessUniform = "u_MaterialUniforms.Metalness";
	constexpr static const char* s_EmissionUniform = "u_MaterialUniforms.Emission";
	constexpr static const char* s_TilingUniform = "u_MaterialUniforms.Tiling";
	constexpr static const char* s_EnvMapRotationUniform = "u_MaterialUniforms.EnvMapRotation";
	constexpr static const char* s_UseNormalMapUniform = "u_MaterialUniforms.UseNormalMap";
	constexpr static const char* s_TransparencyUniform = "u_MaterialUniforms.Transparency";
	constexpr static const char* s_LitUniform = "u_MaterialUniforms.Lit";
	
	constexpr static const char* s_AlbedoMapUniform = "u_AlbedoTexture";
	constexpr static const char* s_NormalMapUniform = "u_NormalTexture";
	constexpr static const char* s_RoughnessMapUniform = "u_RoughnessTexture";
	constexpr static const char* s_MetalnessMapUniform = "u_MetalnessTexture";

	Ref<MaterialAsset> MaterialAsset::Create(bool isTransparent)
	{
		return CreateRef<MaterialAsset>(isTransparent);
	}

	Ref<MaterialAsset> MaterialAsset::Create(Ref<Material> material)
	{
		return CreateRef<MaterialAsset>(material);
	}

	MaterialAsset::MaterialAsset(bool isTransparent)
		: m_Transparent(isTransparent)
	{
		Handle = {};

		if (isTransparent)
			m_Material = Material::Create(Renderer::GetShadersLibrary()->Get("PlaygroundStaticTransparent"));
		else
			m_Material = Material::Create(Renderer::GetShadersLibrary()->Get("IrisPBRStatic"));

		SetDefaults();
	}

	MaterialAsset::MaterialAsset(Ref<Material> material)
	{
		Handle = {};

		m_Material = Material::Create(material);
	}

	MaterialAsset::~MaterialAsset()
	{
	}

	void MaterialAsset::OnDependencyUpdated(AssetHandle handle)
	{
		if (handle == m_Maps.AlbedoMap)
		{
			Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle);
			IR_VERIFY(texture);
			m_Material->Set(s_AlbedoMapUniform, texture);
		}
		else if (handle == m_Maps.NormalMap)
		{
			Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle);
			IR_VERIFY(texture);
			m_Material->Set(s_NormalMapUniform, texture);
		}
		else if (handle == m_Maps.RoughnessMap)
		{
			Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle);
			IR_VERIFY(texture);
			m_Material->Set(s_RoughnessMapUniform, texture);
		}
		else if(handle == m_Maps.MetalnessMap)
		{
			Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle);
			IR_VERIFY(texture);
			m_Material->Set(s_MetalnessMapUniform, texture);
		}
	}

	glm::vec3& MaterialAsset::GetAlbedoColor()
	{
		return m_Material->GetVector3(s_AlbedoColorUniform);
	}

	void MaterialAsset::SetAlbedoColor(const glm::vec3& color)
	{
		m_Material->Set(s_AlbedoColorUniform, color);
	}

	bool MaterialAsset::IsUsingNormalMap()
	{
		return m_Material->GetBool(s_UseNormalMapUniform);
	}

	void MaterialAsset::SetUseNormalMap(bool value)
	{
		m_Material->Set(s_UseNormalMapUniform, value);
	}

	float& MaterialAsset::GetRoughness()
	{
		return m_Material->GetFloat(s_RoughnessUniform);
	}

	void MaterialAsset::SetRoughness(float roughness)
	{
		m_Material->Set(s_RoughnessUniform, roughness);
	}

	float& MaterialAsset::GetMetalness()
	{
		return m_Material->GetFloat(s_MetalnessUniform);
	}

	void MaterialAsset::SetMetalness(float metalness)
	{
		m_Material->Set(s_MetalnessUniform, metalness);
	}

	float& MaterialAsset::GetEmission()
	{
		return m_Material->GetFloat(s_EmissionUniform);
	}

	void MaterialAsset::SetEmission(float emission)
	{
		m_Material->Set(s_EmissionUniform, emission);
	}

	float& MaterialAsset::GetTiling()
	{
		return m_Material->GetFloat(s_TilingUniform);
	}

	void MaterialAsset::SetTiling(float tiling)
	{
		m_Material->Set(s_TilingUniform, tiling);
	}

	float& MaterialAsset::GetTransparency()
	{
		return m_Material->GetFloat(s_TransparencyUniform);
	}

	void MaterialAsset::SetTransparency(float transparency)
	{
		m_Material->Set(s_TransparencyUniform, transparency);
	}

	bool MaterialAsset::IsLit()
	{
		return m_Material->GetBool(s_LitUniform);
	}

	void MaterialAsset::SetLit()
	{
		m_Material->Set(s_LitUniform, true);
	}

	void MaterialAsset::SetUnlit()
	{
		m_Material->Set(s_LitUniform, false);
	}

	Ref<Texture2D> MaterialAsset::GetAlbedoMap()
	{
		return m_Material->GetTexture2D(s_AlbedoMapUniform);
	}

	void MaterialAsset::SetAlbedoMap(AssetHandle albedoMap, bool setImmediatly)
	{
		m_Maps.AlbedoMap = albedoMap;

		if (albedoMap)
		{
			if (setImmediatly)
			{
				Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(albedoMap);
				m_Material->Set(s_AlbedoMapUniform, texture);
			}
			else
			{
				Project::GetEditorAssetManager()->AddPostSyncTask([this, albedoMap]() -> bool
				{
					AsyncAssetResult<Texture2D> result = AssetManager::GetAssetAsync<Texture2D>(albedoMap);
					if (result.IsReady)
					{
						m_Material->Set(s_AlbedoMapUniform, result.Asset);
						return true;
					}

					return false;
				}, true);
			}

			AssetManager::RegisterDependency(albedoMap, Handle);
		}
		else
		{
			ClearAlbedoMap();
		}
	}

	void MaterialAsset::ClearAlbedoMap()
	{
		m_Material->Set(s_AlbedoMapUniform, Renderer::GetWhiteTexture());
	}

	Ref<Texture2D> MaterialAsset::GetNormalMap()
	{
		return m_Material->GetTexture2D(s_NormalMapUniform);
	}

	void MaterialAsset::SetNormalMap(AssetHandle normalMap, bool setImmediatly)
	{
		m_Maps.NormalMap = normalMap;

		if (normalMap)
		{
			if (setImmediatly)
			{
				Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(normalMap);
				m_Material->Set(s_NormalMapUniform, texture);
			}
			else
			{
				Project::GetEditorAssetManager()->AddPostSyncTask([this, normalMap]() -> bool
				{
					AsyncAssetResult<Texture2D> result = AssetManager::GetAssetAsync<Texture2D>(normalMap);
					if (result.IsReady)
					{
						m_Material->Set(s_NormalMapUniform, result.Asset);
						SetUseNormalMap(true);
						return true;
					}

					// Otherwise we set defaults until the map is loaded
					SetUseNormalMap(false);
					return false;
				}, true);
			}

			AssetManager::RegisterDependency(normalMap, Handle);
		}
		else
		{
			ClearNormalMap();
		}
	}

	void MaterialAsset::ClearNormalMap()
	{
		m_Material->Set(s_NormalMapUniform, Renderer::GetWhiteTexture());
	}

	Ref<Texture2D> MaterialAsset::GetRoughnessMap()
	{
		return m_Material->GetTexture2D(s_RoughnessMapUniform);
	}

	void MaterialAsset::SetRoughnessMap(AssetHandle roughnessMap, bool setImmediatly)
	{
		m_Maps.RoughnessMap = roughnessMap;

		if (roughnessMap)
		{
			if (setImmediatly)
			{
				Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(roughnessMap);
				m_Material->Set(s_RoughnessMapUniform, texture);
			}
			else
			{
				Project::GetEditorAssetManager()->AddPostSyncTask([this, roughnessMap]() -> bool
				{
					AsyncAssetResult<Texture2D> result = AssetManager::GetAssetAsync<Texture2D>(roughnessMap);
					if (result.IsReady)
					{
						m_Material->Set(s_RoughnessMapUniform, result.Asset);
						SetRoughness(1.0f);
						return true;
					}

					// Otherwise we set defaults until the map is loaded
					SetRoughness(0.4f);
					return false;
				}, true);
			}

			AssetManager::RegisterDependency(roughnessMap, Handle);
		}
		else
		{
			ClearRoughnessMap();
		}
	}

	void MaterialAsset::ClearRoughnessMap()
	{
		m_Material->Set(s_RoughnessMapUniform, Renderer::GetWhiteTexture());
	}

	Ref<Texture2D> MaterialAsset::GetMetalnessMap()
	{
		return m_Material->GetTexture2D(s_MetalnessMapUniform);
	}

	void MaterialAsset::SetMetalnessMap(AssetHandle metalnessMap, bool setImmediatly)
	{
		m_Maps.MetalnessMap = metalnessMap;

		if (metalnessMap)
		{
			if (setImmediatly)
			{
				Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(metalnessMap);
				m_Material->Set(s_MetalnessMapUniform, texture);
			}
			else
			{
				Project::GetEditorAssetManager()->AddPostSyncTask([this, metalnessMap]() -> bool
				{
					AsyncAssetResult<Texture2D> result = AssetManager::GetAssetAsync<Texture2D>(metalnessMap);
					if (result.IsReady)
					{
						m_Material->Set(s_MetalnessMapUniform, result.Asset);
						SetMetalness(1.0f);
						return true;
					}

					// Otherwise we set defaults until the map is loaded
					SetMetalness(0.0f);
					return false;
				}, true);
			}

			AssetManager::RegisterDependency(metalnessMap, Handle);
		}
		else
		{
			ClearMetalnessMap();
		}
	}

	void MaterialAsset::ClearMetalnessMap()
	{
		m_Material->Set(s_MetalnessMapUniform, Renderer::GetWhiteTexture());
	}

	void MaterialAsset::SetDefaults()
	{
		if (m_Transparent)
		{
			// Set defaults
			SetAlbedoColor(glm::vec3(0.8f));

			// Maps
			ClearAlbedoMap();
		}
		else
		{
			SetAlbedoColor(glm::vec3(0.8f));
			SetRoughness(0.4f);
			SetMetalness(0.0f);
			SetEmission(0.0f);
			SetTiling(1.0f);
			SetUseNormalMap(false);
			SetLit();

			ClearAlbedoMap();
			ClearNormalMap();
			ClearRoughnessMap();
			ClearMetalnessMap();
		}
	}

	Ref<MaterialTable> MaterialTable::Create(uint32_t materialCount)
	{
		return CreateRef<MaterialTable>(materialCount);
	}

	Ref<MaterialTable> MaterialTable::Create(Ref<MaterialTable> other)
	{
		return CreateRef<MaterialTable>(other);
	}

	MaterialTable::MaterialTable(uint32_t materialCount)
		: m_MaterialCount(materialCount)
	{
	}

	MaterialTable::MaterialTable(Ref<MaterialTable> other)
		: m_MaterialCount(other->m_MaterialCount)
	{
		for (auto [index, material] : other->GetMaterials())
			SetMaterial(index, material);
	}
	void MaterialTable::SetMaterial(uint32_t index, AssetHandle material)
	{
		m_Materials[index] = material;
		if (index >= m_MaterialCount)
			m_MaterialCount = index + 1;
	}

	void MaterialTable::ClearMaterial(uint32_t index)
	{
		IR_ASSERT(m_Materials.contains(index));
		m_Materials.erase(index);
		if (index >= m_MaterialCount)
			m_MaterialCount = index + 1;
	}

}