#pragma once

#include "AssetManager/AssetManager.h"
#include "Editor/AssetEditorPanel.h"
#include "Editor/EditorResources.h"
#include "Renderer/Texture.h"
#include "Themes.h"
#include "Utils/FileSystem.h"
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

	enum class VectorAxis
	{
		None = 0,
		X = BIT(0),
		Y = BIT(1),
		Z = BIT(2),
		W = BIT(3)
	};

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

	struct ImGuiScopedFont
	{
		ImGuiScopedFont(const ImGuiScopedFont&) = delete;
		ImGuiScopedFont& operator=(const ImGuiScopedFont&) = delete;
		ImGuiScopedFont(ImFont* font) { ImGui::PushFont(font); }
		~ImGuiScopedFont() { ImGui::PopFont(); }
	};

	struct ImGuiScopedID
	{
		template<typename T>
		ImGuiScopedID(T id) { ImGui::PushID(id); }
		~ImGuiScopedID() { ImGui::PopID(); }
		ImGuiScopedID(const ImGuiScopedID&) = delete;
		ImGuiScopedID& operator=(const ImGuiScopedID&) = delete;
	};

	struct PropertyAssetReferenceSettings
	{
		bool AdvanceToNextColumn = true;
		bool NoItemSpacing = false;
		float WidthOffset = 0.0f;
		bool AllowMemoryOnlyAssets = false;
		ImVec4 ButtonLabelColor = ImGui::ColorConvertU32ToFloat4(Colors::Theme::Text);
		ImVec4 ButtonLabelColorError = ImGui::ColorConvertU32ToFloat4(Colors::Theme::TextError);
		bool ShowFulLFilePath = false;
	};

	// NOTE: Move somewhere better?
	// This is for knowing what the currently referenced asset is so that if we want to modify/know what it is in other functions...
	static AssetHandle s_PropertyAssetReferenceAssetHandle;

	// Generates an ID using a simple incrementing counter
	const char* GenerateID();

	// Generates an ID using a provided string
	const char* GenerateLabelID(std::string_view label);

	// Pushes an ImGui ID and reset the incrementing counter
	void PushID();

	void PopID();

	void SetToolTip(std::string_view text, float delayInSeconds = 0.1f, bool allowWhenDisabled = true, ImVec2 padding = ImVec2(5, 5));

	void ShowHelpMarker(const char* description);

	void ToolTip(const std::string& tip, const ImVec4& color = ImVec4(1.0f, 1.0f, 0.529f, 0.7f));

	template<typename... Args>
	inline void ToolTipWithVariableArgs(const ImVec4& color, const std::string& tip, Args&&... args)
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

	bool PropertyColor3(const char* label, glm::vec3& color, bool showAsWheel = false);

	bool PropertyColor4(const char* label, glm::vec4& color, bool showAsWheel = false);

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

	// The delay won't work on texts, because the timer isn't tracked for them.
	bool IsItemHovered(float delayInSeconds = 0.1f, ImGuiHoveredFlags flags = 0);

	// Returns whether the last items is disabled
	bool IsItemDisabled();

	typedef int OutlineFlags;
	enum OutlineFlags_
	{
		OutlineFlags_None = 0,   // draw no activity outline
		OutlineFlags_WhenHovered = 1 << 1,   // draw an outline when item is hovered
		OutlineFlags_WhenActive = 1 << 2,   // draw an outline when item is active
		OutlineFlags_WhenInactive = 1 << 3,   // draw an outline when item is inactive
		OutlineFlags_HighlightActive = 1 << 4,   // when active, the outline is in highlight colour
		OutlineFlags_NoHighlightActive = OutlineFlags_WhenHovered | OutlineFlags_WhenActive | OutlineFlags_WhenInactive,
		OutlineFlags_NoOutlineInactive = OutlineFlags_WhenHovered | OutlineFlags_WhenActive | OutlineFlags_HighlightActive,
		OutlineFlags_All = OutlineFlags_WhenHovered | OutlineFlags_WhenActive | OutlineFlags_WhenInactive | OutlineFlags_HighlightActive,
	};

	// Draw an outline for the last item
	void DrawItemActivityOutline(OutlineFlags flags = OutlineFlags_All, ImColor colorHighlight = Colors::Theme::NiceBlue, float rounding = GImGui->Style.FrameRounding);

	void UnderLine(bool fullWidth = false, float offsetX = 0.0f, float offsetY = -1.0f, float thickness = 1.0f, ImU32 theme = Colors::Theme::BackgroundDark);
	void UnderLine(ImU32 theme, bool fullWidth = false, ImVec2 p1Offset = {}, ImVec2 p2Offset = {}, float thickness = 1.0f);

	void RenderWindowOuterBorders(ImGuiWindow* currentWindow);
	
	// Exposed resize behavior for native OS windows
	bool UpdateWindowManualResize(ImGuiWindow* window, ImVec2& newSize, ImVec2& newPosition);

	/////////////////////////////////////////////////////////
	// UI...
	/////////////////////////////////////////////////////////

	bool BeginPopup(const char* strID, ImGuiWindowFlags flags = 0);
	void EndPopup();

	bool BeginMenuBar(const ImRect& barRectangle);
	void EndMenuBar();

	bool PropertyGridHeader(const std::string& name, bool openByDefault = true);
	void BeginPropertyGrid(uint32_t columns = 2, float width = 0.0f, bool setWidth = true);
	void EndPropertyGrid();

	bool TreeNodeWithIcon(const std::string& label, const Ref<Texture2D>& icon, const ImVec2& size, bool openByDefault = true);

	void Separator(ImVec2 size, ImVec4 color);

	void TextWrapped(const char* value, bool isError = false);
	bool PropertyString(const char* label, const char* value, bool isError = false);
	bool PropertyStringMultiline(const char* label, std::string& value, const char* helpText = "");
	bool PropertyStringReadOnly(const char* label, const std::string& value, bool isErorr = false);
	bool PropertyStringReadOnly(const char* label, const char* value, bool isErorr = false);
	bool Property(const char* label, float& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f, const char* helpText = "");
	bool Property(const char* label, uint32_t& value, uint32_t delta = 1.0f, uint32_t min = 0, uint32_t max = 0, const char* helpText = "");
	bool Property(const char* label, bool& value, const char* helpText = "", bool underLineProperty = false);
	bool PropertySlider(const char* label, uint32_t& value, uint32_t min = 0, uint32_t max = 0, const char* format = "%.3f", const char* helpText = "");
	bool PropertySlider(const char* label, float& value, float min = 0.0f, float max = 0.0f, const char* format = "%.3f", const char* helpText = "");
	bool PropertySlider(const char* label, ImVec2& value, float min = 0.0f, float max = 0.0f, const char* format = "%.3f", const char* helpText = "");

	bool PropertyDrag(const char* label, float& value, float delta = 0.1f, float min = 0.0, float max = 0.0, const char* helpText = "");
	bool PropertyDrag(const char* label, glm::vec2& value, float delta = 0.1f, float min = 0.0, float max = 0.0, const char* helpText = "");
	bool PropertyDrag(const char* label, glm::vec3& value, float delta = 0.1f, float min = 0.0, float max = 0.0, const char* helpText = "");
	bool PropertyDrag(const char* label, glm::vec4& value, float delta = 0.1f, float min = 0.0, float max = 0.0, const char* helpText = "");

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

	bool PropertyDropdownNoLabel(const char* strID, const char** options, int optionCount, int* selected);

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

	/////////////////////////////////////////////////////////////////////////////////////////
	// Borders...

	void DrawBorder(ImVec2 rectMin, ImVec2 rectMax, const ImVec4& borderColour, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f);

	inline void DrawBorder(const ImVec4& borderColour, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
	{
		DrawBorder(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), borderColour, thickness, offsetX, offsetY);
	}

	inline void DrawBorder(float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
	{
		DrawBorder(ImGui::GetStyleColorVec4(ImGuiCol_Border), thickness, offsetX, offsetY);
	}

	inline void DrawBorder(ImVec2 rectMin, ImVec2 rectMax, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
	{
		DrawBorder(rectMin, rectMax, ImGui::GetStyleColorVec4(ImGuiCol_Border), thickness, offsetX, offsetY);
	}

	void DrawBorder(ImRect rect, float thickness = 1.0f, float rounding = 0.0f, float offsetX = 0.0f, float offsetY = 0.0f);

	void DrawBorderHorizontal(ImVec2 rectMin, ImVec2 rectMax, const ImVec4& borderColour, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f);

	inline void DrawBorderHorizontal(ImVec2 rectMin, ImVec2 rectMax, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
	{
		DrawBorderHorizontal(rectMin, rectMax, ImGui::GetStyleColorVec4(ImGuiCol_Border), thickness, offsetX, offsetY);
	}

	inline void DrawBorderHorizontal(const ImVec4& borderColour, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
	{
		DrawBorderHorizontal(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), borderColour, thickness, offsetX, offsetY);
	}

	inline void DrawBorderHorizontal(float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
	{
		DrawBorderHorizontal(ImGui::GetStyleColorVec4(ImGuiCol_Border), thickness, offsetX, offsetY);
	}

	void DrawBorderVertical(ImVec2 rectMin, ImVec2 rectMax, const ImVec4& borderColour, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f);

	inline void DrawBorderVertical(const ImVec4& borderColour, float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
	{
		DrawBorderVertical(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), borderColour, thickness, offsetX, offsetY);
	}

	inline void DrawBorderVertical(float thickness = 1.0f, float offsetX = 0.0f, float offsetY = 0.0f)
	{
		DrawBorderVertical(ImGui::GetStyleColorVec4(ImGuiCol_Border), thickness, offsetX, offsetY);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Custom Drag Scalars...
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	const char* PatchFormatStringFloatToInt(const char* fmt);

	int FormatString(char* buf, size_t buf_size, const char* fmt, ...);

	bool DragScalar(const char* label, ImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags);

	inline bool DragScalarN(const char* label, ImGuiDataType data_type, void* p_data, int components, float v_speed = 1.0f, const void* p_min = NULL, const void* p_max = NULL, const char* format = NULL, ImGuiSliderFlags flags = 0)
	{
		bool changed = ImGui::DragScalarN(label, data_type, p_data, components, v_speed, p_min, p_max, format, flags);
		DrawItemActivityOutline();
		return changed;
	}

	inline bool DragFloat(const char* label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
	{
		return DragScalar(label, ImGuiDataType_Float, v, v_speed, &v_min, &v_max, format, flags);
	}

	inline bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
	{
		bool changed = ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
		DrawItemActivityOutline();
		return changed;
	}

	inline bool DragFloat2(const char* label, float v[2], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
	{
		bool changed = ImGui::DragFloat2(label, v, v_speed, v_min, v_max, format, flags);
		DrawItemActivityOutline();
		return changed;
	}

	inline bool SliderFloat2(const char* label, float v[2], float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
	{
		bool changed = ImGui::SliderFloat2(label, v, v_min, v_max, format, flags);
		DrawItemActivityOutline();
		return changed;
	}

	inline bool DragFloat3(const char* label, float v[3], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
	{
		bool changed = ImGui::DragFloat3(label, v, v_speed, v_min, v_max, format, flags);
		DrawItemActivityOutline();
		return changed;
	}

	inline bool SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
	{
		bool changed = ImGui::SliderFloat3(label, v, v_min, v_max, format, flags);
		DrawItemActivityOutline();
		return changed;
	}

	inline bool DragFloat4(const char* label, float v[4], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
	{
		bool changed = ImGui::DragFloat4(label, v, v_speed, v_min, v_max, format, flags);
		DrawItemActivityOutline();
		return changed;
	}

	inline bool SliderFloat4(const char* label, float v[4], float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
	{
		bool changed = ImGui::SliderFloat4(label, v, v_min, v_max, format, flags);
		DrawItemActivityOutline();
		return changed;
	}

	inline bool DragDouble(const char* label, double* v, float v_speed = 1.0f, double v_min = 0.0, double v_max = 0.0, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
	{
		return DragScalar(label, ImGuiDataType_Double, v, v_speed, &v_min, &v_max, format, flags);
	}

	inline bool DragInt8(const char* label, int8_t* v, float v_speed = 1.0f, int8_t v_min = 0, int8_t v_max = 0, const char* format = nullptr, ImGuiSliderFlags flags = 0)
	{
		return DragScalar(label, ImGuiDataType_S8, v, v_speed, &v_min, &v_max, format, flags);
	}

	inline bool DragInt16(const char* label, int16_t* v, float v_speed = 1.0f, int16_t v_min = 0, int16_t v_max = 0, const char* format = nullptr, ImGuiSliderFlags flags = 0)
	{
		return DragScalar(label, ImGuiDataType_S16, v, v_speed, &v_min, &v_max, format, flags);
	}

	inline bool DragInt32(const char* label, int32_t* v, float v_speed = 1.0f, int32_t v_min = 0, int32_t v_max = 0, const char* format = nullptr, ImGuiSliderFlags flags = 0)
	{
		return DragScalar(label, ImGuiDataType_S32, v, v_speed, &v_min, &v_max, format, flags);
	}

	inline bool SliderInt32(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0)
	{
		bool changed = ImGui::SliderInt(label, v, v_min, v_max, format, flags);
		DrawItemActivityOutline();
		return changed;
	}

	inline bool DragInt64(const char* label, int64_t* v, float v_speed = 1.0f, int64_t v_min = 0, int64_t v_max = 0, const char* format = nullptr, ImGuiSliderFlags flags = 0)
	{
		return DragScalar(label, ImGuiDataType_S64, v, v_speed, &v_min, &v_max, format, flags);
	}

	inline bool DragUInt8(const char* label, uint8_t* v, float v_speed = 1.0f, uint8_t v_min = 0, uint8_t v_max = 0, const char* format = nullptr, ImGuiSliderFlags flags = 0)
	{
		return DragScalar(label, ImGuiDataType_U8, v, v_speed, &v_min, &v_max, format, flags);
	}

	inline bool DragUInt16(const char* label, uint16_t* v, float v_speed = 1.0f, uint16_t v_min = 0, uint16_t v_max = 0, const char* format = nullptr, ImGuiSliderFlags flags = 0)
	{
		return DragScalar(label, ImGuiDataType_U16, v, v_speed, &v_min, &v_max, format, flags);
	}

	inline bool DragUInt32(const char* label, uint32_t* v, float v_speed = 1.0f, uint32_t v_min = 0, uint32_t v_max = 0, const char* format = nullptr, ImGuiSliderFlags flags = 0)
	{
		return DragScalar(label, ImGuiDataType_U32, v, v_speed, &v_min, &v_max, format, flags);
	}

	inline bool DragUInt64(const char* label, uint64_t* v, float v_speed = 1.0f, uint64_t v_min = 0, uint64_t v_max = 0, const char* format = nullptr, ImGuiSliderFlags flags = 0)
	{
		return DragScalar(label, ImGuiDataType_U64, v, v_speed, &v_min, &v_max, format, flags);
	}

	inline bool InputScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_step = NULL, const void* p_step_fast = NULL, const char* format = NULL, ImGuiInputTextFlags flags = 0)
	{
		bool changed = ImGui::InputScalar(label, data_type, p_data, p_step, p_step_fast, format, flags);
		DrawItemActivityOutline();
		return changed;
	}

	inline bool InputFloat(const char* label, float* v, float step = 0.0f, float step_fast = 0.0f, const char* format = "%.3f", ImGuiInputTextFlags flags = 0)
	{
		return InputScalar(label, ImGuiDataType_Float, v, &step, &step_fast, format, flags);
	}

	inline bool InputDouble(const char* label, double* v, double step = 0.0, double step_fast = 0.0, const char* format = "%.6f", ImGuiInputTextFlags flags = 0)
	{
		return InputScalar(label, ImGuiDataType_Double, v, &step, &step_fast, format, flags);
	}

	inline bool InputInt8(const char* label, int8_t* v, int8_t step = 1, int8_t step_fast = 1, ImGuiInputTextFlags flags = 0)
	{
		return InputScalar(label, ImGuiDataType_S8, v, &step, &step_fast, nullptr, flags);
	}

	inline bool InputInt16(const char* label, int16_t* v, int16_t step = 1, int16_t step_fast = 10, ImGuiInputTextFlags flags = 0)
	{
		return InputScalar(label, ImGuiDataType_S16, v, &step, &step_fast, nullptr, flags);
	}

	inline bool InputInt32(const char* label, int32_t* v, int32_t step = 1, int32_t step_fast = 100, ImGuiInputTextFlags flags = 0)
	{
		return InputScalar(label, ImGuiDataType_S32, v, &step, &step_fast, nullptr, flags);
	}

	inline bool InputInt64(const char* label, int64_t* v, int64_t step = 1, int64_t step_fast = 1000, ImGuiInputTextFlags flags = 0)
	{
		return InputScalar(label, ImGuiDataType_S64, v, &step, &step_fast, nullptr, flags);
	}

	inline bool InputUInt8(const char* label, uint8_t* v, uint8_t step = 1, uint8_t step_fast = 1, ImGuiInputTextFlags flags = 0)
	{
		return InputScalar(label, ImGuiDataType_U8, v, &step, &step_fast, nullptr, flags);
	}

	inline bool InputUInt16(const char* label, uint16_t* v, uint16_t step = 1, uint16_t step_fast = 10, ImGuiInputTextFlags flags = 0)
	{
		return InputScalar(label, ImGuiDataType_U16, v, &step, &step_fast, nullptr, flags);
	}

	inline bool InputUInt32(const char* label, uint32_t* v, uint32_t step = 1, uint32_t step_fast = 100, ImGuiInputTextFlags flags = 0)
	{
		return InputScalar(label, ImGuiDataType_U32, v, &step, &step_fast, nullptr, flags);
	}

	inline bool InputUInt64(const char* label, uint64_t* value, ImGuiInputTextFlags flags = 0)
	{
		return InputScalar(label, ImGuiDataType_U64, value, nullptr, nullptr, nullptr, flags);
	}

	inline bool InputUInt64(const char* label, uint64_t* v, uint64_t step = 0, uint64_t step_fast = 0, ImGuiInputTextFlags flags = 0)
	{
		return InputScalar(label, ImGuiDataType_U64, v, step ? &step : nullptr, step_fast ? &step_fast : nullptr, nullptr, flags);
	}

	inline bool InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL)
	{
		bool changed = ImGui::InputText(label, buf, buf_size, flags, callback, user_data);
		DrawItemActivityOutline();
		return changed;
	}

	inline bool InputText(const char* label, std::string* value, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL)
	{
		bool changed = ImGui::InputText(label, value, flags, callback, user_data);
		DrawItemActivityOutline();
		return changed;
	}

	inline bool InputTextMultiline(const char* label, std::string* value, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL)
	{
		bool changed = ImGui::InputTextMultiline(label, value, size, flags, callback, user_data);
		DrawItemActivityOutline();
		return changed;
	}

	inline bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0)
	{
		bool changed = ImGui::ColorEdit3(label, col, flags);
		DrawItemActivityOutline();
		return changed;
	}

	inline bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0)
	{
		bool changed = ImGui::ColorEdit4(label, col, flags);
		DrawItemActivityOutline();
		return changed;
	}

	inline bool BeginCombo(const char* label, const char* preview_value, ImGuiComboFlags flags = 0)
	{
		bool opened = ImGui::BeginCombo(label, preview_value, flags);
		DrawItemActivityOutline();
		return opened;
	}

	inline void EndCombo()
	{
		ImGui::EndCombo();
	}

	inline bool Checkbox(const char* label, bool* b)
	{
		bool changed = ImGui::Checkbox(label, b);
		UI::DrawItemActivityOutline();
		return changed;
	}

	inline static bool PropertyInputU64(const char* label, uint64_t& value, uint64_t step = 0, uint64_t stepFast = 0, ImGuiInputTextFlags flags = 0, const char* helpText = "")
	{
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

		bool modified = UI::InputUInt64(GenerateID(), &value, step, stepFast, flags);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Widgets...
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool EditVec3(std::string_view label, ImVec2 size, float resetValue, bool& manuallyEdited, glm::vec3& value, VectorAxis renderMultiSelectAxes = VectorAxis::None, float speed = 1.0f, glm::vec3 v_min = glm::vec3(0.0f), glm::vec3 v_max = glm::vec3(0.0f), const char* format = "%.2f", ImGuiSliderFlags flags = 0);

	// Helper for the SearchWidget...
	bool IsMatchingSearch(const std::string& item, std::string_view searchQuery, bool caseSensitive = false, bool stripWhiteSpaces = false, bool stripUnderscores = false);

	// Search Widget
	template<uint32_t BufferSize = 256, typename StringType>
	inline bool SearchWidget(StringType& searchString, const char* hint = "Search...", bool* grabFocus = nullptr)
	{
		PushID();

		ShiftCursorY(1.0f);

		const bool layoutSuspended = []()
		{
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			if (window->DC.CurrentLayout)
			{
				ImGui::SuspendLayout();
				return true;
			}

			return false;
		}();

		bool modified = false;
		bool searching = false;

		const float areaPosX = ImGui::GetCursorPosX();
		const float framePaddingY = ImGui::GetStyle().FramePadding.y;

		UI::ImGuiScopedStyle rounding(ImGuiStyleVar_FrameRounding, 3.0f);
		UI::ImGuiScopedStyle padding(ImGuiStyleVar_FramePadding, { 28.0f, framePaddingY });

		if constexpr (std::is_same<StringType, std::string>::value)
		{
			char searchBuffer[BufferSize + 1] = { 0 };
			strncpy(searchBuffer, searchString.c_str(), BufferSize);

			if (ImGui::InputText(GenerateID(), searchBuffer, BufferSize))
			{
				searchString = searchBuffer;
				modified = true;
			}
			else if (ImGui::IsItemDeactivatedAfterEdit())
			{
				searchString = searchBuffer;
				modified = true;
			}

			searching = searchBuffer[0] != 0;
		}
		else
		{
			static_assert(std::is_same<decltype(&searchString[0]), char*>::value, "searchString paramenter must be std::string& or char*");

			if (ImGui::InputText(GenerateID(), searchString, BufferSize))
			{
				modified = true;
			}
			else if (ImGui::IsItemDeactivatedAfterEdit())
			{
				modified = true;
			}

			searching = searchString[0] != 0;
		}

		if (grabFocus && *grabFocus)
		{
			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))
			{
				ImGui::SetKeyboardFocusHere(-1);
			}

			if (ImGui::IsItemFocused())
			{
				*grabFocus = false;
			}
		}

		UI::DrawItemActivityOutline();
		ImGui::SetItemAllowOverlap();

		ImGui::SameLine(areaPosX + 5.0f);

		if (layoutSuspended)
			ImGui::ResumeLayout();

		ImGui::BeginHorizontal(GenerateID(), ImGui::GetItemRectSize());
		const ImVec2 iconSize = { ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight() };

		// Search icon
		{
			const float iconOffsetY = framePaddingY - 3.0f;
			UI::ShiftCursorY(iconOffsetY);
			UI::Image(EditorResources::SearchIcon, iconSize, { 0, 0 }, { 1, 1 }, { 1.0f, 1.0f, 1.0f, 0.2f });
			UI::ShiftCursorY(-iconOffsetY);

			// Hint...
			if (!searching)
			{
				UI::ShiftCursorY(-framePaddingY + 1.0f);
				UI::ImGuiScopedColor text(ImGuiCol_Text, Colors::Theme::TextDarker);
				UI::ImGuiScopedStyle padding(ImGuiStyleVar_FramePadding, { 0.0f, framePaddingY });
				ImGui::TextUnformatted(hint);
				UI::ShiftCursorY(-1.0f);
			}
		}

		ImGui::Spring();

		// Clear icon
		if (searching)
		{
			constexpr float spacingX = 4.0f;
			const float lineHeight = ImGui::GetItemRectSize().y - framePaddingY / 2.0f;

			if (ImGui::InvisibleButton(GenerateID(), { lineHeight, lineHeight }))
			{
				if constexpr (std::is_same<StringType, std::string>::value)
					searchString.clear();
				else
					memset(searchString, 0, BufferSize);

				modified = true;
			}

			if (ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()))
				ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

			UI::DrawButtonImage(EditorResources::ClearIcon, IM_COL32(160, 160, 160, 200), IM_COL32(170, 170, 170, 255), IM_COL32(160, 160, 160, 150), UI::RectExpanded(UI::GetItemRect(), -2.0f, -2.0f));


			ImGui::Spring(-1.0f, spacingX * 2.0f);
		}

		ImGui::EndHorizontal();
		UI::ShiftCursorY(-1.0f);
		PopID();

		return modified;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// AssetProperties...
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool AssetSearchPopup(const char* ID, AssetType type, AssetHandle& outHandle, bool* cleared = nullptr, const char* hint = "Search Assets", ImVec2 size = { 250.0f, 350.0f });

	bool AssetSearchPopup(const char* ID, AssetType type, AssetHandle& outHandle, bool allowMemoryOnlyAssets, bool* cleared = nullptr, const char* hint = "Search Assets", ImVec2 size = { 250.0f, 350.0f });

	std::pair<bool, std::string> AssetValidityAndName(AssetHandle handle, const PropertyAssetReferenceSettings& settings);

	template<typename T>
	inline static bool PropertyAssetReference(const char* label, AssetHandle& outHandle, const char* helpText = "", const PropertyAssetReferenceSettings& settings = {})
	{
		bool modified = false;

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

		ImVec2 originalButtonTextAlign = ImGui::GetStyle().ButtonTextAlign;
		{
			ImGui::GetStyle().ButtonTextAlign = { 0.0f, 0.5f };
			float width = ImGui::GetContentRegionAvail().x - settings.WidthOffset;
			constexpr float itemHeight = 28.0f;
			UI::PushID();

			auto [valid, buttonText] = AssetValidityAndName(outHandle, settings);

			if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
				buttonText = "----";

			// PropertyAssetReference could be called multiple times in same "context"
			// and so we need a unique id for the asset search popup each time.
			// notes
			// - don't use GenerateID(), that's inviting id clashes, which would be super confusing.
			// - don't store return from GenerateLabelId in a const char* here. Because its pointing to an internal
			//   buffer which may get overwritten by the time you want to use it later on.
			std::string assetSearchPopupID = GenerateLabelID("ARSP");
			{
				UI::ImGuiScopedColor buttonLabelColor(ImGuiCol_Text, valid ? settings.ButtonLabelColor : settings.ButtonLabelColorError);
				ImGui::Button(GenerateLabelID(buttonText), { width, itemHeight });

				const bool isHovered = ImGui::IsItemHovered();

				if (isHovered)
				{
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					{
						AssetEditorPanel::OpenEditor(AssetManager::GetAsset<T>(outHandle));
					}
					else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
						ImGui::OpenPopup(assetSearchPopupID.c_str());
				}
			}

			ImGui::GetStyle().ButtonTextAlign = originalButtonTextAlign;

			bool clear = false;
			if (UI::AssetSearchPopup(assetSearchPopupID.c_str(), T::GetStaticType(), outHandle, settings.AllowMemoryOnlyAssets, &clear))
			{
				if (clear)
					outHandle = 0;
				modified = true;
				s_PropertyAssetReferenceAssetHandle = outHandle;
			}
		}
		UI::PopID();

		// Implement drag/drop from other places
		if (!IsItemDisabled())
		{
			if (ImGui::BeginDragDropTarget())
			{
				const ImGuiPayload* data = ImGui::AcceptDragDropPayload("asset_payload");
				if (data)
				{
					AssetHandle assetHandle = *reinterpret_cast<AssetHandle*>(data->Data);
					s_PropertyAssetReferenceAssetHandle = assetHandle;
					Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
					if (asset && asset->GetAssetType() == T::GetStaticType())
					{
						outHandle = assetHandle;
						modified = true;
					}
				}

				ImGui::EndDragDropTarget();
			}
		}

		ImGui::PopItemWidth();
		if (settings.AdvanceToNextColumn)
		{
			ImGui::NextColumn();
			UI::UnderLine();
		}

		return modified;
	}

	template<typename T, typename Fn>
	inline static bool PropertyAssetReferenceTarget(const char* label, const char* assetName, AssetHandle& outHandle, Fn&& targetFn, const char* helpText = "", const PropertyAssetReferenceSettings& settings = {})
	{
		bool modified = false;

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
		if (settings.NoItemSpacing)
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });

		ImVec2 originalButtonTextAlign = ImGui::GetStyle().ButtonTextAlign;
		{
			ImGui::GetStyle().ButtonTextAlign = { 0.0f, 0.5f };
			float width = ImGui::GetContentRegionAvail().x - settings.WidthOffset;
			constexpr float itemHeight = 28.0f;
			
			UI::PushID();

			std::string buttonText = "Null";
			bool valid = true;

			if (AssetManager::IsAssetHandleValid(outHandle))
			{
				// It is okay to get the asset here since it will most probably be a loaded asset already
				Ref<T> source = AssetManager::GetAsset<T>(outHandle);
				valid = (source != nullptr);
				if (source)
				{
					if (assetName)
						buttonText = assetName;
					else
					{
						buttonText = Project::GetEditorAssetManager()->GetMetaData(outHandle).FilePath.stem().string();
						assetName = buttonText.c_str();
					}
				}
				else if (AssetManager::IsAssetMissing(outHandle))
				{
					buttonText = "Missing";
				}
			}

			if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
				buttonText = "----";

			// PropertyAssetReference could be called multiple times in same "context"
			// and so we need a unique id for the asset search popup each time.
			// notes
			// - don't use GenerateID(), that's inviting id clashes, which would be super confusing.
			// - don't store return from GenerateLabelId in a const char* here. Because its pointing to an internal
			//   buffer which may get overwritten by the time you want to use it later on.
			std::string assetSearchPopupID = GenerateLabelID("ARTSP");
			{
				UI::ImGuiScopedColor buttonLabelColor(ImGuiCol_Text, valid ? settings.ButtonLabelColor : settings.ButtonLabelColorError);
				ImGui::Button(GenerateLabelID(buttonText), { width, itemHeight });

				const bool isHovered = ImGui::IsItemHovered();

				if (isHovered)
				{
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					{
						AssetEditorPanel::OpenEditor(AssetManager::GetAsset<T>(outHandle));
					}
					else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
						ImGui::OpenPopup(assetSearchPopupID.c_str());
				}
			}

			ImGui::GetStyle().ButtonTextAlign = originalButtonTextAlign;

			bool clear = false;
			if (UI::AssetSearchPopup(assetSearchPopupID.c_str(), T::GetStaticType(), outHandle, &clear))
			{
				if (clear)
					outHandle = 0;

				targetFn(AssetManager::GetAsset<T>(outHandle));
				modified = true;
			}

			UI::PopID();
		}

		// Implement drag/drop from other places
		if (!IsItemDisabled())
		{
			if (ImGui::BeginDragDropTarget())
			{
				const ImGuiPayload* data = ImGui::AcceptDragDropPayload("asset_payload");
				if (data)
				{
					AssetHandle assetHandle = *reinterpret_cast<AssetHandle*>(data->Data);
					s_PropertyAssetReferenceAssetHandle = assetHandle;
					Ref<Asset> asset = AssetManager::GetAsset<Asset>(assetHandle);
					if (asset && asset->GetAssetType() == T::GetStaticType())
					{
						targetFn(asset.As<T>());
						modified = true;
					}
				}

				ImGui::EndDragDropTarget();
			}
		}

		ImGui::PopItemWidth();
		if (settings.AdvanceToNextColumn)
		{
			ImGui::NextColumn();
			UI::UnderLine();
		}

		if (settings.NoItemSpacing)
			ImGui::PopStyleVar();

		return modified;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Used by the SceneHierarchyPanel and EditorLayer for now for creating -Sections------------- of that format inside windows
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool BeginSection(const char* name, int& sectionIndex, bool columns2 = true, float column1Width = 0.0f, float column2Width = 0.0f);

	void EndSection();

	void SectionText(const char* label, const char* text);
	bool SectionCheckbox(const char* label, bool& value, const char* hint = "");
	bool SectionSlider(const char* label, float& value, float min = 0.0f, float max = 0.0f, const char* hint = "");
	bool SectionDrag(const char* label, float& value, float delta = 1.0f, float min = 0.0f, float max = 0.0f, const char* hint = "");
	bool SectionDropdown(const char* label, const char** options, int32_t optionCount, int32_t* selected, const char* hint = "");

}