#pragma once

#include <string>
#include <unordered_map>

struct ImFont;
typedef unsigned short ImWchar;

namespace Iris {

	struct FontSpecification
	{
		std::string FontName;
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

		void SetDefaultFont(const std::string_view name);
		void Load(const FontSpecification& spec, bool isDefault = false);
		void PushFont(const std::string_view name) const;
		void PopFont() const;
		const ImFont* GetFont(const std::string_view name) const;

		const ImFont* GetDefaultFont() const;
		inline const std::unordered_map<std::string, ImFont*>& GetFonts() const { return m_Fonts; }

	private:
		std::unordered_map<std::string, ImFont*> m_Fonts;

	};

}