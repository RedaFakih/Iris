#include "IrisPCH.h"
#include "MaterialAsset.h"

#include "AssetManager/AssetManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/Shaders/Shader.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

namespace Iris {

	static const std::string s_AlbedoColorUniform = "u_MaterialUniforms.AlbedoColor";
	static const std::string s_RoughnessUniform = "u_MaterialUniforms.Roughness";
	static const std::string s_MetalnessUniform = "u_MaterialUniforms.Metalness";
	static const std::string s_EmissionUniform = "u_MaterialUniforms.Emission";
	static const std::string s_UseNormalMapUniform = "u_MaterialUniforms.UseNormalMap";
	static const std::string s_TransparencyUniform = "u_MaterialUniforms.Transparency";

	static const std::string s_AlbedoMapUniform = "u_AlbedoTexture";
	static const std::string s_NormalMapUniform = "u_NormalTexture";
	static const std::string s_RoughnessMapUniform = "u_RoughnessTexture";
	static const std::string s_MetalnessMapUniform = "u_MetalnessTexture";

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

	float& MaterialAsset::GetTransparency()
	{
		return m_Material->GetFloat(s_TransparencyUniform);
	}

	void MaterialAsset::SetTransparency(float transparency)
	{
		m_Material->Set(s_TransparencyUniform, transparency);
	}

	Ref<Texture2D> MaterialAsset::GetAlbedoMap()
	{
		return m_Material->GetTexture2D(s_AlbedoMapUniform);
	}

	void MaterialAsset::SetAlbedoMap(AssetHandle albedoMap)
	{
		m_Maps.AlbedoMap = albedoMap;

		if (albedoMap)
		{
			Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(albedoMap);
			m_Material->Set(s_AlbedoMapUniform, texture);
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

	void MaterialAsset::SetNormalMap(AssetHandle normalMap)
	{
		m_Maps.NormalMap = normalMap;

		if (normalMap)
		{
			Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(normalMap);
			m_Material->Set(s_NormalMapUniform, texture);
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

	void MaterialAsset::SetRoughnessMap(AssetHandle roughnessMap)
	{
		m_Maps.RoughnessMap = roughnessMap;

		if (roughnessMap)
		{
			Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(roughnessMap);
			m_Material->Set(s_RoughnessMapUniform, texture);
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

	void MaterialAsset::SetMetalnessMap(AssetHandle metalnessMap)
	{
		m_Maps.MetalnessMap = metalnessMap;

		if (metalnessMap)
		{
			Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(metalnessMap);
			m_Material->Set(s_MetalnessMapUniform, texture);
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
			SetUseNormalMap(false);

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