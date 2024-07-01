#pragma once

#include "AssetManager/Asset/Asset.h"
#include "Renderer/Texture.h"
#include "Scene/Components.h"

#include <filesystem>

namespace Iris {

	struct MSDFData;

	class Font : public Asset
	{
	public:
		Font(const std::filesystem::path& filepath);
		Font(const std::string_view name, Buffer buffer);
		virtual ~Font();

		[[nodiscard]] static Ref<Font> Create(const std::filesystem::path& filepath)
		{
			return CreateRef<Font>(filepath);
		}

		[[nodiscard]] static Ref<Font> Create(const std::string_view name, Buffer buffer)
		{
			return CreateRef<Font>(name, buffer);
		}

		static void Init();
		static void Shutdown();

		static Ref<Font> GetDefaultFont();
		static Ref<Font> GetDefaultMonoSpacedFont();
		static Ref<Font> GetFontAssetForTextComponent(AssetHandle font);

		const std::string& GetName() const { return m_Name; }

		Ref<Texture2D> GetFontAtlas() const { return m_TextureAtlas; }
		const MSDFData* GetMSDFData() const { return m_MSDFData; }

		static AssetType GetStaticType() { return AssetType::Font; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }

	private:
		void CreateAtlas(Buffer buffer);

	private:
		std::string m_Name;
		Ref<Texture2D> m_TextureAtlas = nullptr;
		MSDFData* m_MSDFData = nullptr;

	private:
		inline static Ref<Font> s_DefaultFont, s_DefaultMonoSpacedFont;

	};

}