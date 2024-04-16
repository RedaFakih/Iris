#pragma once

#include "Editor/EditorResources.h"
#include "Renderer/Texture.h"
#include "Themes.h"
#include "Utils/StringUtils.h"

#include <choc/text/choc_StringUtilities.h>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <string>

namespace ImGui {

	IMGUI_API bool DragFloat2(const char* label, glm::vec2& v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
	IMGUI_API bool DragFloat3(const char* label, glm::vec3& v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
	IMGUI_API bool DragFloat4(const char* label, glm::vec4& v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0);

	IMGUI_API bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);
	IMGUI_API bool InputTextMultiline(const char* label, std::string* str, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);
	IMGUI_API bool InputTextWithHint(const char* label, const char* hint, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);

}

namespace Iris::UI {

	struct ImGuiScopedStyle
	{
		ImGuiScopedStyle(ImGuiStyleVar var, float value) { ImGui::PushStyleVar(var, value); }
		ImGuiScopedStyle(ImGuiStyleVar var, const ImVec2& value) { ImGui::PushStyleVar(var, value); }
		~ImGuiScopedStyle() { ImGui::PopStyleVar(); }
	};

	struct ImGuiScopedColor
	{
		ImGuiScopedColor(ImGuiCol var, const ImVec4& value) { ImGui::PushStyleColor(var, value); }
		ImGuiScopedColor(ImGuiCol var, const ImU32& value) { ImGui::PushStyleColor(var, value); }
		~ImGuiScopedColor() { ImGui::PopStyleColor(); }
	};

	struct ImGuiScopedID
	{
		template<typename T>
		ImGuiScopedID(T id) { ImGui::PushID(id); }
		~ImGuiScopedID() { ImGui::PopID(); }
		ImGuiScopedID(const ImGuiScopedID&) = delete;
		ImGuiScopedID& operator=(const ImGuiScopedID&) = delete;
	};

	// Generates an ID using a simple incrementing counter
	const char* GenerateID();

	// Generates an ID using a provided string
	const char* GenerateLabelID(std::string_view label);

	// Pushes an ImGui ID and reset the incrementing counter
	void PushID();

	void PopID();

	void ShowHelpMarker(const char* description);

	void ToolTip(const std::string& tip, const ImVec4& color = ImVec4(1.0f, 1.0f, 0.529f, 0.7f));

	template<typename... Args>
	void ToolTipWithVariableArgs(const ImVec4& color, const std::string& tip, Args&&... args)
	{
		ImGui::BeginTooltip();
		ImGui::TextColored(color, tip.c_str(), std::forward<Args>(args)...);
		ImGui::EndTooltip();
	}

	bool IsInputEnabled();

	void SetInputEnabled(bool state);

	bool IsMouseEnabled();

	void SetMouseEnabled(bool enabled);

	void ShiftCursor(float x, float y);

	void ShiftCursorY(float distance);

	void ShiftCursorX(float distance);

	// Returns whether it was navigated to the previous ID/item
	bool NavigatedTo();

	bool IsWindowFocused(const char* windowName, bool checkRootWindow = true);

	//////////////////////////////////////////////////////////////////////////
	// Colors...
	//////////////////////////////////////////////////////////////////////////

	inline ImColor ColorWithValue(const ImColor& color, float value)
	{
		const ImVec4& colRaw = color.Value;
		float hue, sat, val;
		ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
		return ImColor::HSV(hue, sat, std::min(value, 1.0f));
	}

	inline ImColor ColorWithSaturation(const ImColor& color, float saturation)
	{
		const ImVec4& colRaw = color.Value;
		float hue, sat, val;
		ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
		return ImColor::HSV(hue, std::min(saturation, 1.0f), val);
	}

	inline ImColor ColorWithHue(const ImColor& color, float hue)
	{
		const ImVec4& colRaw = color.Value;
		float h, s, v;
		ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, h, s, v);
		return ImColor::HSV(std::min(hue, 1.0f), s, v);
	}

	inline ImColor ColorWithAlpha(const ImColor& color, float multiplier)
	{
		ImVec4 colRaw = color.Value;
		colRaw.w = multiplier;
		return colRaw;
	}

	inline ImColor ColorWithMultipliedValue(const ImColor& color, float multiplier)
	{
		const ImVec4& colRaw = color.Value;
		float hue, sat, val;
		ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
		return ImColor::HSV(hue, sat, std::min(val * multiplier, 1.0f));
	}

	inline ImColor ColorWithMultipliedSaturation(const ImColor& color, float multiplier)
	{
		const ImVec4& colRaw = color.Value;
		float hue, sat, val;
		ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
		return ImColor::HSV(hue, std::min(sat * multiplier, 1.0f), val);
	}

	inline ImColor ColorWithMultipliedHue(const ImColor& color, float multiplier)
	{
		const ImVec4& colRaw = color.Value;
		float hue, sat, val;
		ImGui::ColorConvertRGBtoHSV(colRaw.x, colRaw.y, colRaw.z, hue, sat, val);
		return ImColor::HSV(std::min(hue * multiplier, 1.0f), sat, val);
	}

	inline ImColor ColorWithMultipliedAlpha(const ImColor& color, float multiplier)
	{
		ImVec4 colRaw = color.Value;
		colRaw.w *= multiplier;
		return colRaw;
	}

	bool ColorEdit3Control(const char* label, glm::vec3& color, bool showAsWheel = true);

	bool ColorEdit4Control(const char* label, glm::vec4& color, bool showAsWheel = true);

	//////////////////////////////////////////////////////////////////////////
	//////// DrawList Utils /////////
	//////////////////////////////////////////////////////////////////////////
	 
	// This gets the last items rect!
	ImRect GetItemRect();

	// To be used inside a Begin()/End() range
	ImRect GetWindowRect(bool clip = false);

	bool IsMouseInRectRegion(ImVec2 min, ImVec2 max, bool clip = true);

	bool IsMouseInRectRegion(ImRect rect, bool clip = true);

	ImRect RectExpanded(const ImRect& input, float x, float y);

	ImRect RectOffset(const ImRect& input, float x, float y);

	// Returns whether the last items is disabled
	bool IsItemDisabled();

	// Draw an outline for the last item
	void DrawItemActivityOutline(float rounding = GImGui->Style.FrameRounding, bool drawWhenNotActive = false, ImColor colorWhenActive = Colors::Theme::Accent);

	void UnderLine(bool fullWidth = false, float offsetX = 0.0f, float offsetY = -1.0f);

	void RenderWindowOuterBorders(ImGuiWindow* currentWindow);
	
	/////////////////////////////////////////////////////////
	// UI...
	/////////////////////////////////////////////////////////

	bool PropertyGridHeader(const std::string& name, bool openByDefault = true);

	void BeginPropertyGrid(uint32_t columns = 2, bool defaultWidth = true);

	void EndPropertyGrid();

	void Separator(ImVec2 size, ImVec4 color);

	bool PropertyString(const char* label, const char* value, bool isError = false);

	bool PropertyStringReadOnly(const char* label, const char* value, bool isErorr = false);

	bool PropertyFloat(const char* label, float& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f, const char* helpText = "");

	bool PropertyBool(const char* label, bool& value, const char* helpText = "");

	template<typename Enum, typename Type = int32_t>
	inline bool PropertyDropdown(const char* label, const char** options, int optionCount, Enum& selected, const char* helpText = "")
	{
		bool modified = false;
		Type selectedIndex = (Type)selected;
		const char* current = options[selectedIndex];

		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);

		if (std::strlen(helpText) != 0)
		{
			ImGui::SameLine();
			ShowHelpMarker(helpText);
		}

		ImGui::NextColumn();
		ShiftCursorY(4.0f);

		ImGui::PushItemWidth(-1);
		const std::string id = fmt::format("##{0}", label);
		if (ImGui::BeginCombo(id.c_str(), current))
		{
			for (int i = 0; i < optionCount; i++)
			{
				bool isSelected = (current == options[i]);
				if (ImGui::Selectable(options[i], isSelected))
				{
					current = options[i];
					*selected = (Type)i;
					modified = true;
				}

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		if (!IsItemDisabled())
			DrawItemActivityOutline();

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool PropertyDropdown(const char* label, const char** options, int optionCount, int* selected, const char* helpText = "");

	bool PropertySliderFloat(const char* label, float& value, float min = 0.0f, float max = 0.0f, const char* format = "%.3f", const char* helpText = "");

	bool PropertySliderFloat2(const char* label, ImVec2& value, float min = 0.0f, float max = 0.0f, const char* format = "%.3f", const char* helpText = "");

	bool MultiLineText(const char* label, std::string& value, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);

	//=========================================================================================
	/// Button Image
	ImTextureID GetTextureID(Ref<Texture2D> texture);

	inline void DrawButtonImage(const Ref<Texture2D>& imageNormal, const Ref<Texture2D>& imageHovered, const Ref<Texture2D>& imagePressed,
		ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed,
		ImVec2 rectMin, ImVec2 rectMax)
	{
		auto* drawList = ImGui::GetWindowDrawList();
		if (ImGui::IsItemActive())
			drawList->AddImage(GetTextureID(imagePressed), rectMin, rectMax, ImVec2(0, 0), ImVec2(1, 1), tintPressed);
		else if (ImGui::IsItemHovered())
			drawList->AddImage(GetTextureID(imageHovered), rectMin, rectMax, ImVec2(0, 0), ImVec2(1, 1), tintHovered);
		else
			drawList->AddImage(GetTextureID(imageNormal), rectMin, rectMax, ImVec2(0, 0), ImVec2(1, 1), tintNormal);
	};

	inline void DrawButtonImage(const Ref<Texture2D>& imageNormal, const Ref<Texture2D>& imageHovered, const Ref<Texture2D>& imagePressed,
		ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed,
		ImRect rectangle)
	{
		DrawButtonImage(imageNormal, imageHovered, imagePressed, tintNormal, tintHovered, tintPressed, rectangle.Min, rectangle.Max);
	};

	inline void DrawButtonImage(const Ref<Texture2D>& image,
		ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed,
		ImVec2 rectMin, ImVec2 rectMax)
	{
		DrawButtonImage(image, image, image, tintNormal, tintHovered, tintPressed, rectMin, rectMax);
	};

	inline void DrawButtonImage(const Ref<Texture2D>& image,
		ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed,
		ImRect rectangle)
	{
		DrawButtonImage(image, image, image, tintNormal, tintHovered, tintPressed, rectangle.Min, rectangle.Max);
	};


	inline void DrawButtonImage(const Ref<Texture2D>& imageNormal, const Ref<Texture2D>& imageHovered, const Ref<Texture2D>& imagePressed,
		ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed)
	{
		DrawButtonImage(imageNormal, imageHovered, imagePressed, tintNormal, tintHovered, tintPressed, ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
	};

	inline void DrawButtonImage(const Ref<Texture2D>& image,
		ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed)
	{
		DrawButtonImage(image, image, image, tintNormal, tintHovered, tintPressed, ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
	};

	// Normal image
	void Image(const Ref<Texture2D>& image, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tint_col = ImVec4(1, 1, 1, 1), const ImVec4& border_col = ImVec4(0, 0, 0, 0));

}