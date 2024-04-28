#pragma once

#include "AssetManager/Asset/Asset.h"
#include "Core/Base.h"
#include "Material.h"

namespace Iris {

	/*
	 * This is basically a PBR material for loaded meshes to use for rendering...
	 * The other material type is just a general material for general uses such as creating a custom renderpass for a custom shader and you need a material to
	 * handle some material resources like textures... e.g. the renderer2D quad passes uses a quad material to set textures for quads...
	 */

	class MaterialAsset : public Asset
	{
	public:
		MaterialAsset(bool isTransparent = false);
		MaterialAsset(Ref<Material> material);
		virtual ~MaterialAsset();

		[[nodiscard]] static Ref<MaterialAsset> Create(bool isTransparent = false);
		[[nodiscard]] static Ref<MaterialAsset> Create(Ref<Material> material);

		virtual void OnDependencyUpdated(AssetHandle handle) override;

		glm::vec3& GetAlbedoColor();
		void SetAlbedoColor(const glm::vec3& color);

		bool IsUsingNormalMap();
		void SetUseNormalMap(bool value);

		float& GetRoughness();
		void SetRoughness(float roughness);

		float& GetMetalness();
		void SetMetalness(float metalness);

		float& GetEmission();
		void SetEmission(float emission);

		float& GetTransparency();
		void SetTransparency(float tranparency);

		Ref<Texture2D> GetAlbedoMap();
		void SetAlbedoMap(AssetHandle albedoMap);
		void ClearAlbedoMap();

		Ref<Texture2D> GetNormalMap();
		void SetNormalMap(AssetHandle  normalMap);
		void ClearNormalMap();

		Ref<Texture2D> GetRoughnessMap();
		void SetRoughnessMap(AssetHandle  roughnessMap);
		void ClearRoughnessMap();

		Ref<Texture2D> GetMetalnessMap();
		void SetMetalnessMap(AssetHandle  metalnessMap);
		void ClearMetalnessMap();

		Ref<Material> GetMaterial() const { return m_Material; }
		void SetMaterial(Ref<Material> material) { m_Material = material; }

		bool IsTransparent() const { return m_Transparent; }

		static AssetType GetStaticType() { return AssetType::Material; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }

	private:
		void SetDefaults();

	private:
		Ref<Material> m_Material;
		bool m_Transparent = false;

		struct MapAssets
		{
			AssetHandle AlbedoMap = 0;
			AssetHandle NormalMap = 0;
			AssetHandle RoughnessMap = 0;
			AssetHandle MetalnessMap = 0;
		} m_Maps;

		friend class MaterialEditor;

	};

	class MaterialTable : public RefCountedObject
	{
	public:
		MaterialTable(uint32_t materialCount = 1);
		MaterialTable(Ref<MaterialTable> other);
		virtual ~MaterialTable() = default;

		[[nodiscard]] static Ref<MaterialTable> Create(uint32_t materialCount = 1);
		[[nodiscard]] static Ref<MaterialTable> Create(Ref<MaterialTable> other);

		inline bool HasMaterial(uint32_t materialIndex) const { return m_Materials.contains(materialIndex); }
		void SetMaterial(uint32_t index, AssetHandle material);
		void ClearMaterial(uint32_t index);

		inline AssetHandle GetMaterial(uint32_t index) const
		{
			IR_ASSERT(m_Materials.contains(index));
			return m_Materials.at(index);
		}

		inline std::map<uint32_t, AssetHandle>& GetMaterials() { return m_Materials; }
		inline const std::map<uint32_t, AssetHandle>& GetMaterials() const { return m_Materials; }

		inline uint32_t GetMaterialCount() const { return m_MaterialCount; }
		inline void SetMaterialCount(uint32_t count) { m_MaterialCount = count; }

		inline void Clear() { m_Materials.clear(); }

	private:
		std::map<uint32_t, AssetHandle> m_Materials;
		uint32_t m_MaterialCount;

	};

}