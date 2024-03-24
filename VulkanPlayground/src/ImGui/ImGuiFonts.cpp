#include "vkPch.h"
#include "ImGuiFonts.h"

#include <imgui/imgui.h>

namespace vkPlayground {

	void ImGuiFontsLibrary::SetDefaultFont(const std::string_view name)
	{
		std::string nameStr(name);
		if (m_Fonts.contains(nameStr))
		{
			ImGuiIO& io = ImGui::GetIO();
			io.FontDefault = m_Fonts.at(nameStr);

			return;
		}

		VKPG_VERIFY(false, "Failed to find the font with provided name!");
	}

	void ImGuiFontsLibrary::Load(const FontSpecification& spec, bool isDefault)
	{
		if (m_Fonts.contains(spec.FontName))
		{
			VKPG_CORE_WARN_TAG("Editor", "Tried to add font with the same name as an existing one! ({0})", spec.FontName);
			
			return;
		}

		ImGuiIO& io = ImGui::GetIO();

		ImFontConfig fontConfig;
		fontConfig.MergeMode = spec.MergeWithLast;
		ImFont* font = io.Fonts->AddFontFromFileTTF(spec.Filepath.c_str(), spec.Size, &fontConfig, spec.GlyphRanges == nullptr ? io.Fonts->GetGlyphRangesDefault() : spec.GlyphRanges);
		VKPG_VERIFY(font, "Failed to load font!");
		m_Fonts[spec.FontName] = font;

		if (isDefault)
			io.FontDefault = font;
	}

	void ImGuiFontsLibrary::PushFont(const std::string_view name) const
	{
		std::string nameStr(name);
		if (!m_Fonts.contains(nameStr))
		{
			ImGuiIO& io = ImGui::GetIO();
			ImGui::PushFont(io.FontDefault);

			return;
		}

		ImGui::PushFont(m_Fonts.at(nameStr));
	}

	void ImGuiFontsLibrary::PopFont() const
	{
		ImGui::PopFont();
	}

	const ImFont* ImGuiFontsLibrary::GetFont(const std::string_view name) const
	{
		std::string nameStr(name);
		VKPG_VERIFY(m_Fonts.contains(nameStr), "Failed to find the font with provided name!");
		return m_Fonts.at(nameStr);
	}

	const ImFont* ImGuiFontsLibrary::GetDefaultFont() const
	{
		return ImGui::GetIO().FontDefault;
	}

}