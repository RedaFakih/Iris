#pragma once

#include <string>
#include <unordered_map>

struct ImFont;
typedef unsigned short ImWchar;

namespace Iris {

	enum class DefaultFonts
	{
		Default,
		Small,
		Large,
		Bold,
		FontAwesome
	};

	struct FontSpecification
	{
		DefaultFonts FontName;
		std::string Filepath;
		float Size = 16.0f;
		const ImWchar* GlyphRanges = nullptr;
		bool MergeWithLast = false;
	};

	class ImGuiFontsLibrary
	{
	public:
		ImGuiFontsLibrary() = default;
		~ImGuiFontsLibrary() = default;

		void SetDefaultFont(DefaultFonts font);
		void Load(const FontSpecification& spec, bool isDefault = false);
		void PushFont(const char* name) const;
		void PushFont(DefaultFonts font) const;
		void PopFont() const;
		ImFont* GetFont(const char* name) const;
		ImFont* GetFont(DefaultFonts font) const;

		const ImFont* GetDefaultFont() const;
		inline const std::unordered_map<DefaultFonts, ImFont*>& GetFonts() const { return m_Fonts; }

	private:
		std::unordered_map<DefaultFonts, ImFont*> m_Fonts;

	};

	namespace Utils {

		inline constexpr const char* DefaultFontsNameToString(DefaultFonts font)
		{
			switch (font)
			{
				case DefaultFonts::Default: return "Default";
				case DefaultFonts::Small: return "Small";
				case DefaultFonts::Large: return "Large";
				case DefaultFonts::Bold: return "Bold";
				case DefaultFonts::FontAwesome: return "FontAwesome";
			}

			IR_ASSERT(false);
			return "UNKNOWN FONT";
		}

		inline constexpr DefaultFonts DefaultFontsNameFromString(const char* fontName)
		{
			if (fontName == "Default") return DefaultFonts::Default;
			else if (fontName == "Small") return DefaultFonts::Small;
			else if (fontName == "Large") return DefaultFonts::Large;
			else if (fontName == "Bold") return DefaultFonts::Bold;
			else if (fontName == "FontAwesome") return DefaultFonts::FontAwesome;
			else return DefaultFonts::Default;
		}

	}

}