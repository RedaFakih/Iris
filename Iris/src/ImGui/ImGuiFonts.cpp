#include "IrisPCH.h"
#include "ImGuiFonts.h"

#include <imgui/imgui.h>

namespace Iris {

	void ImGuiFontsLibrary::SetDefaultFont(DefaultFonts font)
	{
		if (m_Fonts.contains(font))
		{
			ImGuiIO& io = ImGui::GetIO();
			io.FontDefault = m_Fonts.at(font);

			return;
		}

		IR_VERIFY(false, "Failed to find the font with provided name!");
	}

	void ImGuiFontsLibrary::Load(const FontSpecification& spec, bool isDefault)
	{
		if (m_Fonts.contains(spec.FontName))
		{
			IR_CORE_WARN_TAG("Editor", "Tried to add font with the same name as an existing one! ({0})", spec.FontName);
			
			return;
		}

		ImGuiIO& io = ImGui::GetIO();

		ImFontConfig fontConfig;
		fontConfig.MergeMode = spec.MergeWithLast;
		ImFont* font = io.Fonts->AddFontFromFileTTF(spec.Filepath.c_str(), spec.Size, &fontConfig, spec.GlyphRanges == nullptr ? io.Fonts->GetGlyphRangesDefault() : spec.GlyphRanges);
		IR_VERIFY(font, "Failed to load font!");
		m_Fonts[spec.FontName] = font;

		if (isDefault)
			io.FontDefault = font;
	}

	void ImGuiFontsLibrary::PushFont(const char* name) const
	{
		DefaultFonts font = Utils::DefaultFontsNameFromString(name);
		if (!m_Fonts.contains(font))
		{
			ImGuiIO& io = ImGui::GetIO();
			ImGui::PushFont(io.FontDefault);

			return;
		}

		ImGui::PushFont(m_Fonts.at(font));
	}

	void ImGuiFontsLibrary::PushFont(DefaultFonts font) const
	{
		if (!m_Fonts.contains(font))
		{
			ImGuiIO& io = ImGui::GetIO();
			ImGui::PushFont(io.FontDefault);

			return;
		}

		ImGui::PushFont(m_Fonts.at(font));
	}

	void ImGuiFontsLibrary::PopFont() const
	{
		ImGui::PopFont();
	}

	ImFont* ImGuiFontsLibrary::GetFont(const char* name) const
	{
		DefaultFonts font = Utils::DefaultFontsNameFromString(name);
		IR_VERIFY(m_Fonts.contains(font), "Failed to find the font with provided name!");
		return m_Fonts.at(font);
	}

	ImFont* ImGuiFontsLibrary::GetFont(DefaultFonts font) const
	{
		IR_VERIFY(m_Fonts.contains(font), "Failed to find the font with provided name!");
		return m_Fonts.at(font);
	}

	const ImFont* ImGuiFontsLibrary::GetDefaultFont() const
	{
		return ImGui::GetIO().FontDefault;
	}

}