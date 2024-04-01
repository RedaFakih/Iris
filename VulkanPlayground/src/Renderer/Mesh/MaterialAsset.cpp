#include "vkPch.h"
#include "MaterialAsset.h"

#include "Renderer/Renderer.h"
#include "Renderer/Shaders/Shader.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

namespace vkPlayground {

	static const std::string s_AlbedoColorUniform = "u_MaterialUniforms.AlbedoColor";
	static const std::string s_RoughnessUniform = "u_MaterialUniforms.Roughness";
	static const std::string s_MetalnessUniform = "u_MaterialUniforms.Metalness";
	static const std::string s_EmissionUniform = "u_MaterialUniforms.Emission";
	static const std::string s_UseNormalMapUniform = "u_MaterialUniforms.UseNormalMap";

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
		// TODO:
		//if (isTransparent)
		//	m_Material = Material::Create(Renderer::GetShadersLibrary()->Get("PlaygroundStaticTransparent"));
		//else
			m_Material = Material::Create(Renderer::GetShadersLibrary()->Get("PlaygroundStatic"));

		SetDefaults();
	}

	MaterialAsset::MaterialAsset(Ref<Material> material)
	{
		m_Material = Material::Create(material);
	}

	MaterialAsset::~MaterialAsset()
	{
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

	Ref<Texture2D> MaterialAsset::GetAlbedoMap()
	{
		return m_Material->GetTexture2D(s_AlbedoMapUniform);
	}

	void MaterialAsset::SetAlbedoMap(Ref<Texture2D> albedoMap)
	{
		m_Material->Set(s_AlbedoMapUniform, albedoMap);
	}

	void MaterialAsset::ClearAlbedoMap()
	{
		m_Material->Set(s_AlbedoMapUniform, Renderer::GetWhiteTexture());
	}

	Ref<Texture2D> MaterialAsset::GetNormalMap()
	{
		return m_Material->GetTexture2D(s_NormalMapUniform);
	}

	void MaterialAsset::SetNormalMap(Ref<Texture2D> normalMap)
	{
		m_Material->Set(s_NormalMapUniform, normalMap);
	}

	void MaterialAsset::ClearNormalMap()
	{
		m_Material->Set(s_NormalMapUniform, Renderer::GetWhiteTexture());
	}

	Ref<Texture2D> MaterialAsset::GetRoughnessMap()
	{
		return m_Material->GetTexture2D(s_RoughnessMapUniform);
	}

	void MaterialAsset::SetRoughnessMap(Ref<Texture2D> roughnessMap)
	{
		m_Material->Set(s_RoughnessMapUniform, roughnessMap);
	}

	void MaterialAsset::ClearRoughnessMap()
	{
		m_Material->Set(s_RoughnessMapUniform, Renderer::GetWhiteTexture());
	}

	Ref<Texture2D> MaterialAsset::GetMetalnessMap()
	{
		return m_Material->GetTexture2D(s_MetalnessMapUniform);
	}

	void MaterialAsset::SetMetalnessMap(Ref<Texture2D> metalnessMap)
	{
		m_Material->Set(s_MetalnessMapUniform, metalnessMap);
	}

	void MaterialAsset::ClearMetalnessMap()
	{
		m_Material->Set(s_MetalnessMapUniform, Renderer::GetWhiteTexture());
	}

	void MaterialAsset::SetDefaults()
	{
		SetAlbedoColor(glm::vec3(0.8f));
		SetRoughness(0.4f);
		SetMetalness(0.0f);
		SetEmission(0.0f);
		SetUseNormalMap(false);

		SetAlbedoMap(Renderer::GetWhiteTexture());
		SetNormalMap(Renderer::GetWhiteTexture());
		SetRoughnessMap(Renderer::GetWhiteTexture());
		SetMetalnessMap(Renderer::GetWhiteTexture());
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
	void MaterialTable::SetMaterial(uint32_t index, Ref<MaterialAsset> material)
	{
		m_Materials[index] = material;
		if (index >= m_MaterialCount)
			m_MaterialCount = index + 1;
	}

	void MaterialTable::ClearMaterial(uint32_t index)
	{
		VKPG_ASSERT(m_Materials.contains(index));
		m_Materials.erase(index);
		if (index >= m_MaterialCount)
			m_MaterialCount = index + 1;
	}

}