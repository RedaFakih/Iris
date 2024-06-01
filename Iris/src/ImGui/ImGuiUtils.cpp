#include "IrisPCH.h"
#include "ImGuiUtils.h"

#include "Core/Application.h"
#include "Core/Input/Input.h"
#include "Renderer/StorageBufferSet.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

#include <imgui/imgui_impl_vulkan.h>

#include <glm/gtc/type_ptr.hpp>
#include <spdlog/fmt/fmt.h>

#include <magic_enum.hpp>
using namespace magic_enum::bitwise_operators;

namespace ImGui {

	namespace Utils {

		struct InputTextCallback_UserData
		{
			std::string* String;
			ImGuiInputTextCallback ChainCallback;
			void* ChainCallbackUserData;
		};

		static int InputTextCallback(ImGuiInputTextCallbackData* data)
		{
			InputTextCallback_UserData* user_data = (InputTextCallback_UserData*)data->UserData;
			if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
			{
				// Resize string callback
				// If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
				std::string* str = user_data->String;
				IM_ASSERT(data->Buf == str->c_str());
				str->resize(data->BufTextLen);
				data->Buf = (char*)str->c_str();
			}
			else if (user_data->ChainCallback)
			{
				// Forward to user callback, if any
				data->UserData = user_data->ChainCallbackUserData;

				return user_data->ChainCallback(data);
			}

			return 0;
		}

	}

	bool DragFloat2(const char* label, glm::vec2& v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
	{
		return DragFloat2(label, glm::value_ptr(v), v_speed, v_min, v_max, format, flags);
	}

	bool DragFloat3(const char* label, glm::vec3& v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
	{
		return DragFloat4(label, glm::value_ptr(v), v_speed, v_min, v_max, format, flags);
	}

	bool DragFloat4(const char* label, glm::vec4& v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
	{
		return DragFloat4(label, glm::value_ptr(v), v_speed, v_min, v_max, format, flags);
	}

	bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
	{
		IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
		flags |= ImGuiInputTextFlags_CallbackResize;

		Utils::InputTextCallback_UserData cb_user_data = {};
		cb_user_data.String = str;
		cb_user_data.ChainCallback = callback;
		cb_user_data.ChainCallbackUserData = user_data;

		return InputText(label, (char*)str->c_str(), str->capacity() + 1, flags, Utils::InputTextCallback, &cb_user_data);
	}

	bool InputTextMultiline(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
	{
		IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
		flags |= ImGuiInputTextFlags_CallbackResize;

		Utils::InputTextCallback_UserData cb_user_data;
		cb_user_data.String = str;
		cb_user_data.ChainCallback = callback;
		cb_user_data.ChainCallbackUserData = user_data;

		return InputTextMultiline(label, (char*)str->c_str(), str->capacity() + 1, size, flags, Utils::InputTextCallback, &cb_user_data);
	}

	bool InputTextWithHint(const char* label, const char* hint, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
	{
		IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
		flags |= ImGuiInputTextFlags_CallbackResize;

		Utils::InputTextCallback_UserData cb_user_data;
		cb_user_data.String = str;
		cb_user_data.ChainCallback = callback;
		cb_user_data.ChainCallbackUserData = user_data;

		return InputTextWithHint(label, hint, (char*)str->c_str(), str->capacity() + 1, flags, Utils::InputTextCallback, &cb_user_data);
	}

}

namespace Iris::UI {

	static int s_UIContextID = 0;
	static uint32_t s_Counter = 0;
	static char s_IDBuffer[16 + 2 + 1] = "##";
	static char s_LabelIDBuffer[1024 + 1];

	const char* GenerateID()
	{
		snprintf(s_IDBuffer + 2, 16, "%u", s_Counter++);
		return s_IDBuffer;
	}

	const char* GenerateLabelID(std::string_view label)
	{
		*fmt::format_to_n(s_LabelIDBuffer, std::size(s_LabelIDBuffer), "{}##{}", label, s_Counter++).out = 0;
		return s_LabelIDBuffer;
	}

	void PushID()
	{
		ImGui::PushID(s_UIContextID++);
		s_Counter = 0;
	}

	void PopID()
	{
		ImGui::PopID();
		s_UIContextID--;
	}

	void SetToolTip(std::string_view text, float delayInSeconds, bool allowWhenDisabled, ImVec2 padding)
	{
		if (IsItemHovered(delayInSeconds, allowWhenDisabled ? ImGuiHoveredFlags_AllowWhenDisabled : 0))
		{
			UI::ImGuiScopedStyle tooltipPadding(ImGuiStyleVar_WindowPadding, padding);
			UI::ImGuiScopedColor textCol(ImGuiCol_Text, Colors::Theme::TextBrighter);
			ImGui::SetTooltip(text.data());
		}
	}

	// Prefer using this over the ToolTip
	void ShowHelpMarker(const char* description)
	{
		ImGui::SameLine();
		ImGui::TextDisabled(" (?)");
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(description);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	void ToolTip(const std::string& tip, const ImVec4& color)
	{
		ImGui::BeginTooltip();
		ImGui::TextColored(color, tip.c_str());
		ImGui::EndTooltip();
	}

	bool IsInputEnabled()
	{
		const ImGuiIO& io = ImGui::GetIO();
		return (io.ConfigFlags & ImGuiConfigFlags_NoMouse) == 0 && (io.ConfigFlags & ImGuiConfigFlags_NavNoCaptureKeyboard) == 0;
	}

	void SetInputEnabled(bool state)
	{
		ImGuiIO& io = ImGui::GetIO();

		// Since ImGui provides only the negative options (no mouse and no nav keyboard) so we have to reverse the bit ops
		// In the meaning that we OR if we want to disable and we and with the complement of it to enable (DeMorgan's)
		if (state)
		{
			io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
			io.ConfigFlags &= ~ImGuiConfigFlags_NavNoCaptureKeyboard;
		}
		else
		{
			io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
			io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
		}
	}

	bool IsMouseEnabled()
	{
		// AND with the opposite of no mouse, if both return true then the mouse is enabled
		return ImGui::GetIO().ConfigFlags & ~ImGuiConfigFlags_NoMouse;
	}

	void SetMouseEnabled(bool enabled)
	{
		if (enabled)
		{
			ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
		}
		else
		{
			ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
		}
	}

	void ShiftCursor(float x, float y)
	{
		ImVec2 cursorPos = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2{ cursorPos.x + x, cursorPos.y + y });
	}

	void ShiftCursorY(float distance)
	{
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + distance);
	}

	void ShiftCursorX(float distance)
	{
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + distance);
	}

	bool NavigatedTo()
	{
		return GImGui->NavJustMovedToId == GImGui->LastItemData.ID;
	}

	bool IsWindowFocused(const char* windowName, bool checkRootWindow)
	{
		ImGuiWindow* currentWindow = GImGui->NavWindow;

		if (checkRootWindow)
		{
			// Keep going through the windows untill they are equal then they it means that we got the root window and not some child one
			ImGuiWindow* lastWindow = nullptr;
			while (lastWindow != currentWindow)
			{
				lastWindow = currentWindow;
				currentWindow = currentWindow->RootWindow;
			}
		}

		return currentWindow == ImGui::FindWindowByName(windowName);
	}

	//////// DrawList Utils /////////

	bool PropertyColor3(const char* label, glm::vec3& color, bool showAsWheel)
	{
		ImGuiColorEditFlags flags = ImGuiColorEditFlags_AlphaBar
			| ImGuiColorEditFlags_AlphaPreview
			| ImGuiColorEditFlags_HDR
			| (showAsWheel ? ImGuiColorEditFlags_PickerHueWheel : ImGuiColorEditFlags_PickerHueBar);

		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);

		ImGui::PushItemWidth(-1);
		bool modified = ImGui::ColorEdit3(fmt::format("##{0}", label).c_str(), glm::value_ptr(color), flags);
		ImGui::PopItemWidth();

		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool PropertyColor4(const char* label, glm::vec4& color, bool showAsWheel)
	{
		ImGuiColorEditFlags flags = ImGuiColorEditFlags_AlphaBar
			| ImGuiColorEditFlags_AlphaPreview
			| ImGuiColorEditFlags_HDR
			| (showAsWheel ? ImGuiColorEditFlags_PickerHueWheel : ImGuiColorEditFlags_PickerHueBar);

		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);
		ImGui::NextColumn();
		ShiftCursorY(4.0f);

		ImGui::PushItemWidth(-1);
		bool modified = ImGui::ColorEdit4(fmt::format("##{0}", label).c_str(), glm::value_ptr(color), flags);
		ImGui::PopItemWidth();

		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	// This get the last items rect!
	ImRect GetItemRect()
	{
		ImRect res = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

		return res;
	}

	// To be used inside a Begin()/End() range
	// This returns the inner rect of the window because so far that is the most useful rect to use for UI imgui stuff
	ImRect GetWindowRect(bool clip)
	{
		if (clip)
			return GImGui->CurrentWindow->InnerClipRect;
		else
			return GImGui->CurrentWindow->InnerRect;
	}

	bool IsMouseInRectRegion(ImVec2 min, ImVec2 max, bool clip)
	{
		return ImGui::IsMouseHoveringRect(min, max, clip);
	}

	bool IsMouseInRectRegion(ImRect rect, bool clip)
	{
		return ImGui::IsMouseHoveringRect(rect.Min, rect.Max, clip);
	}

	ImRect RectExpanded(const ImRect& input, float x, float y)
	{
		ImRect res = input;
		res.Min.x -= x;
		res.Min.y -= y;
		res.Max.x += x;
		res.Max.y += y;

		return res;
	}

	ImRect RectOffset(const ImRect& input, float x, float y)
	{
		ImRect res = input;
		res.Min.x += x;
		res.Min.y += y;
		res.Max.x += x;
		res.Max.y += y;
		return res;
	}

	bool IsItemHovered(float delayInSeconds, ImGuiHoveredFlags flags)
	{
		return ImGui::IsItemHovered() && GImGui->HoveredIdTimer > delayInSeconds;
	}

	bool IsItemDisabled()
	{
		return ImGui::GetItemFlags() & ImGuiItemFlags_Disabled;
	}

	bool BeginPopup(const char* strID, ImGuiWindowFlags flags)
	{
		bool opened = false;

		if (ImGui::BeginPopup(strID, flags))
		{
			opened = true;

			// Fill background with a gradient
			const float padding = ImGui::GetStyle().WindowBorderSize;
			const ImRect windowRect = UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), -padding, -padding);
			ImGui::PushClipRect(windowRect.Min, windowRect.Max, false);

			const ImColor col1 = ImGui::GetStyleColorVec4(ImGuiCol_PopupBg);
			const ImColor col2 = UI::ColorWithMultipliedValue(col1, 0.8f);
			ImGui::GetWindowDrawList()->AddRectFilledMultiColor(windowRect.Min, windowRect.Max, col1, col1, col2, col2);
			ImGui::GetWindowDrawList()->AddRect(windowRect.Min, windowRect.Max, UI::ColorWithMultipliedValue(col1, 1.1f));
			ImGui::PopClipRect();

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 1.0f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 80));
		}

		return opened;
	}

	void EndPopup()
	{
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		ImGui::EndPopup();
	}

	bool BeginMenuBar(const ImRect& barRectangle)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		IR_ASSERT(!window->DC.MenuBarAppending);
		ImGui::BeginGroup(); // Backup position on layer 0
		ImGui::PushID("##menubar");

		const ImVec2 padding = window->WindowPadding;

		// We don't clip with current window clipping rectangle as it is already set to the area below. However we clip with window full rect.
		// We remove 1 worth of rounding to Max.x so that text in long menus and small windows don't tend to display over the lower-right rounded area,
		// which looks particularly glitchy.
		ImRect barRect = UI::RectOffset(barRectangle, 0.0f, padding.y); // window->MenuBarRect();
		ImRect clipRect = ImRect(IM_ROUND(ImMax(window->Pos.x, barRect.Min.x + window->WindowBorderSize + window->Pos.x - 10.0f)),
								 IM_ROUND(barRect.Min.y + window->WindowBorderSize + window->Pos.y),
								 IM_ROUND(ImMax(barRect.Min.x + window->Pos.x, barRect.Max.x - ImMax(window->WindowRounding, window->WindowBorderSize))),
								 IM_ROUND(barRect.Max.y + window->Pos.y)
		);

		clipRect.ClipWith(window->OuterRectClipped);
		ImGui::PushClipRect(clipRect.Min, clipRect.Max, false);

		// We overwrite CursorMaxPos because BeginGroup sets it to CursorPos (essentially the .EmitItem hack in EndMenuBar() would need something analogous here
		window->DC.CursorPos = ImVec2(barRect.Min.x + window->Pos.x, barRect.Min.y + window->Pos.y);
		window->DC.CursorMaxPos = ImVec2(barRect.Min.x + window->Pos.x, barRect.Min.y + window->Pos.y);
		window->DC.LayoutType = ImGuiLayoutType_Horizontal;
		window->DC.NavLayerCurrent = ImGuiNavLayer_Menu;
		window->DC.MenuBarAppending = true;
		ImGui::AlignTextToFramePadding();

		return true;
	}

	void EndMenuBar()
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return;
		ImGuiContext& g = *GImGui;

		// Nav: When a move request within one of our child menu failed, capture the request to navigate among our siblings.
		if (ImGui::NavMoveRequestButNoResultYet() && (g.NavMoveDir == ImGuiDir_Left || g.NavMoveDir == ImGuiDir_Right) && (g.NavWindow->Flags & ImGuiWindowFlags_ChildMenu))
		{
			// Try to find out if the request is for one of our child menu
			ImGuiWindow* nav_earliest_child = g.NavWindow;
			while (nav_earliest_child->ParentWindow && (nav_earliest_child->ParentWindow->Flags & ImGuiWindowFlags_ChildMenu))
				nav_earliest_child = nav_earliest_child->ParentWindow;
			if (nav_earliest_child->ParentWindow == window && nav_earliest_child->DC.ParentLayoutType == ImGuiLayoutType_Horizontal && (g.NavMoveFlags & ImGuiNavMoveFlags_Forwarded) == 0)
			{
				// To do so we claim focus back, restore NavId and then process the movement request for yet another frame.
				// This involve a one-frame delay which isn't very problematic in this situation. We could remove it by scoring in advance for multiple window (probably not worth bothering)
				const ImGuiNavLayer layer = ImGuiNavLayer_Menu;
				IM_ASSERT(window->DC.NavLayersActiveMaskNext & (1 << layer)); // Sanity check
				ImGui::FocusWindow(window);
				ImGui::SetNavID(window->NavLastIds[layer], layer, 0, window->NavRectRel[layer]);
				g.NavDisableHighlight = true; // Hide highlight for the current frame so we don't see the intermediary selection.
				g.NavDisableMouseHover = g.NavMousePosDirty = true;
				ImGui::NavMoveRequestForward(g.NavMoveDir, g.NavMoveClipDir, g.NavMoveFlags, g.NavMoveScrollFlags); // Repeat
			}
		}

		IM_MSVC_WARNING_SUPPRESS(6011); // Static Analysis false positive "warning C6011: Dereferencing NULL pointer 'window'"
		// IM_ASSERT(window->Flags & ImGuiWindowFlags_MenuBar);
		IM_ASSERT(window->DC.MenuBarAppending);
		ImGui::PopClipRect();
		ImGui::PopID();
		window->DC.MenuBarOffset.x = window->DC.CursorPos.x - window->Pos.x; // Save horizontal position so next append can reuse it. This is kinda equivalent to a per-layer CursorPos.
		g.GroupStack.back().EmitItem = false;
		ImGui::EndGroup(); // Restore position on layer 0
		window->DC.LayoutType = ImGuiLayoutType_Vertical;
		window->DC.IsSameLine = false;
		window->DC.NavLayerCurrent = ImGuiNavLayer_Main;
		window->DC.MenuBarAppending = false;
	}

	bool PropertyGridHeader(const std::string& name, bool openByDefault)
	{
		ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_Framed
			| ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_AllowItemOverlap
			| ImGuiTreeNodeFlags_FramePadding;

		if (openByDefault)
			treeNodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;

		constexpr float framePaddingX = 6.0f;
		constexpr float framePaddingY = 6.0f;

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ framePaddingX, framePaddingY });
		ImGui::PushID(name.c_str());
		bool open = ImGui::TreeNodeEx("##dummy_id", treeNodeFlags, Utils::ToUpper(name).c_str());
		ImGui::PopID();
		ImGui::PopStyleVar(2);

		return open;
	}

	void BeginPropertyGrid(uint32_t columns, float width, bool setWidth)
	{
		PushID();
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 8.0f, 8.0f });
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4.0f, 4.0f });
		ImGui::Columns(columns);
		if (setWidth)
			ImGui::SetColumnWidth(0, width == 0.0f ? 140.0f : width);
	}

	void EndPropertyGrid()
	{
		ImGui::Columns(1);
		UnderLine();
		ImGui::PopStyleVar(2);
		ShiftCursorY(7.0f);
		PopID();
	}

	bool TreeNodeWithIcon(const std::string& label, const Ref<Texture2D>& icon, const ImVec2& size, bool openByDefault)
	{
		ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_Framed
			| ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_AllowItemOverlap
			| ImGuiTreeNodeFlags_FramePadding;

		if (openByDefault)
			treeNodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;

		bool open = false;
		const float framePaddingX = 6.0f;
		const float framePaddingY = 6.0f; // affects height of the header

		UI::ImGuiScopedStyle headerRounding(ImGuiStyleVar_FrameRounding, 0.0f);
		UI::ImGuiScopedStyle headerPaddingAndHeight(ImGuiStyleVar_FramePadding, ImVec2{ framePaddingX, framePaddingY });

		ImGui::PushID(label.c_str());
		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
		open = ImGui::TreeNodeEx("##dummy_id", treeNodeFlags);

		float lineHeight = ImGui::GetItemRectMax().y - ImGui::GetItemRectMin().y;
		ImGui::SameLine();
		UI::ShiftCursorY(size.y / 2.0f - 6.5f);
		UI::Image(icon, size);
		ImGui::SameLine();
		UI::ShiftCursorY(-(size.y / 2.0f) + 10.5f);
		ImGui::TextUnformatted(Utils::ToUpper(label).c_str());

		ImGui::PopID();

		return open;
	}

	void Separator(ImVec2 size, ImVec4 color)
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, color);
		ImGui::BeginChild("sep", size);
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}

	void TextWrapped(const char* value, bool isError)
	{
		if (isError)
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(204, 51, 76.5, 255));
		ImGui::TextWrapped(value);
		if (isError)
			ImGui::PopStyleColor();

		UnderLine();
	}

	bool PropertyString(const char* label, const char* value, bool isError)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);

		ImGui::NextColumn();
		ShiftCursorY(4.0f);

		bool modified = false;
		char buffer[256];
		strcpy_s(buffer, sizeof(buffer), value);

		s_IDBuffer[0] = '#';
		s_IDBuffer[1] = '#';
		memset(s_IDBuffer + 2, 0, 14);
		sprintf_s(s_IDBuffer + 2, 14, "%o", s_Counter++); // %o converts to octal (base 8)

		ImGui::PushItemWidth(-1);
		if (isError)
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(204, 51, 76.5, 255));
		if (ImGui::InputText(s_IDBuffer, (char*)value, strlen(value)))
		{
			value = buffer;
			modified = true;
		}
		if (isError)
			ImGui::PopStyleColor();

		ImGui::PopItemWidth();

		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool PropertyStringReadOnly(const char* label, const std::string& value, bool isErorr)
	{
		return PropertyStringReadOnly(label, value.c_str(), isErorr);
	}

	bool PropertyStringReadOnly(const char* label, const char* value, bool isErorr)
	{
		ShiftCursor(10.0f, 9.0f);
		ImGui::Text(label);

		ImGui::NextColumn();
		ShiftCursorY(4.0f);

		s_IDBuffer[0] = '#';
		s_IDBuffer[1] = '#';
		memset(s_IDBuffer + 2, 0, 14);
		sprintf_s(s_IDBuffer + 2, 14, "%o", s_Counter++); // %o converts to octal (base 8)

		ImGui::PushItemWidth(-1);
		if (isErorr)
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(204, 51, 76.5, 255));
		bool modified = ImGui::InputText(s_IDBuffer, (char*)value, strlen(value), ImGuiInputTextFlags_ReadOnly);
		if (isErorr)
			ImGui::PopStyleColor();

		ImGui::PopItemWidth();

		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool Property(const char* label, float& value, float delta, float min, float max, const char* helpText)
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
		bool modified = ImGui::DragFloat(fmt::format("##{0}", label).c_str(), &value, delta, min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline();
		ImGui::PopItemWidth();

		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool Property(const char* label, uint32_t& value, uint32_t delta, uint32_t min, uint32_t max, const char* helpText)
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
		bool modified = ImGui::DragInt(fmt::format("##{0}", label).c_str(), reinterpret_cast<int*>(&value), static_cast<float>(delta), min, max);

		if (!IsItemDisabled())
			DrawItemActivityOutline();
		ImGui::PopItemWidth();

		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool Property(const char* label, bool& value, const char* helpText, bool underLineProperty)
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

		bool modified = ImGui::Checkbox(fmt::format("##{0}", label).c_str(), &value);

		if (!IsItemDisabled())
			DrawItemActivityOutline();

		if (underLineProperty)
			UnderLine();

		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool PropertySlider(const char* label, uint32_t& value, uint32_t min, uint32_t max, const char* format, const char* helpText)
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
		bool modified = ImGui::SliderInt(fmt::format("##{0}", label).c_str(), reinterpret_cast<int*>(&value), min, max, format);

		if (!IsItemDisabled())
			DrawItemActivityOutline();
		ImGui::PopItemWidth();

		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool PropertySlider(const char* label, float& value, float min, float max, const char* format, const char* helpText)
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
		bool modified = ImGui::SliderFloat(fmt::format("##{0}", label).c_str(), &value, min, max, format);

		if (!IsItemDisabled())
			DrawItemActivityOutline();
		ImGui::PopItemWidth();

		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool PropertySlider(const char* label, ImVec2& value, float min, float max, const char* format, const char* helpText)
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
		bool modified = ImGui::SliderFloat2(fmt::format("##{0}", label).c_str(), (float*)&value, min, max, format);

		if (!IsItemDisabled())
			DrawItemActivityOutline();
		ImGui::PopItemWidth();

		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool PropertyDrag(const char* label, float& value, float delta, float min, float max, const char* helpText)
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

		bool modified = UI::DragDouble(fmt::format("##{0}", label).c_str(), reinterpret_cast<double*>(&value), delta, min, max);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool PropertyDrag(const char* label, glm::vec2& value, float delta, float min, float max, const char* helpText)
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

		bool modified = UI::DragFloat2(fmt::format("##{0}", label).c_str(), glm::value_ptr(value), delta, min, max);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool PropertyDrag(const char* label, glm::vec3& value, float delta, float min, float max, const char* helpText)
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

		bool modified = UI::DragFloat3(fmt::format("##{0}", label).c_str(), glm::value_ptr(value), delta, min, max);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool PropertyDrag(const char* label, glm::vec4& value, float delta, float min, float max, const char* helpText)
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

		bool modified = UI::DragFloat4(fmt::format("##{0}", label).c_str(), glm::value_ptr(value), delta, min, max);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool PropertyDropdown(const char* label, const char** options, int optionCount, int* selected, const char* helpText)
	{
		bool modified = false;
		const char* current = options[*selected];

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
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 5.0f, 5.0f });
		if (ImGui::BeginCombo(id.c_str(), current))
		{
			for (int i = 0; i < optionCount; i++)
			{
				bool isSelected = (current == options[i]);
				if (ImGui::Selectable(options[i], isSelected))
				{
					current = options[i];
					*selected = i;
					modified = true;
				}

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}
		ImGui::PopStyleVar();

		if (!IsItemDisabled())
			DrawItemActivityOutline();

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool PropertyDropdownNoLabel(const char* strID, const char** options, int optionCount, int* selected)
	{
		bool modified = false;
		const char* current = options[*selected];
		ShiftCursorY(4.0f);

		ImGui::PushItemWidth(-1);
		const std::string id = fmt::format("##{0}", strID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 5.0f, 5.0f });
		if (ImGui::BeginCombo(id.c_str(), current))
		{
			for (int i = 0; i < optionCount; i++)
			{
				bool isSelected = (current == options[i]);
				if (ImGui::Selectable(options[i], isSelected))
				{
					current = options[i];
					*selected = i;
					modified = true;
				}

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}
		ImGui::PopStyleVar();

		if (!IsItemDisabled())
			DrawItemActivityOutline();

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool MultiLineText(const char* label, std::string& value, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
	{
		bool changed = ImGui::InputTextMultiline(label, &value, size, flags, callback, user_data);
		DrawItemActivityOutline();
		return changed;
	}

	ImTextureID GetTextureID(Ref<Texture2D> texture)
	{
		const VkDescriptorImageInfo& info = texture->GetDescriptorImageInfo();
		if (!info.imageView)
			return nullptr;

		return ImGui_ImplVulkan_AddTexture(info.sampler, info.imageView, info.imageLayout);
	}

	void DrawItemActivityOutline(OutlineFlags flags, ImColor colorHighlight, float rounding)
	{
		if (IsItemDisabled())
			return;

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImRect rect = RectExpanded(GetItemRect(), 1.0f, 1.0f);
		if ((flags & OutlineFlags_WhenActive) && ImGui::IsItemActive())
		{
			if (flags & OutlineFlags_HighlightActive)
			{
				drawList->AddRect(rect.Min, rect.Max, colorHighlight, rounding, 0, 1.5f);
			}
			else
			{
				drawList->AddRect(rect.Min, rect.Max, ImColor(60, 60, 60), rounding, 0, 1.5f);
			}
		}
		else if ((flags & OutlineFlags_WhenHovered) && ImGui::IsItemHovered() && !ImGui::IsItemActive())
		{
			drawList->AddRect(rect.Min, rect.Max, ImColor(60, 60, 60), rounding, 0, 1.5f);
		}
		else if ((flags & OutlineFlags_WhenInactive) && !ImGui::IsItemHovered() && !ImGui::IsItemActive())
		{
			drawList->AddRect(rect.Min, rect.Max, ImColor(50, 50, 50), rounding, 0, 1.0f);
		}
	}

	void UnderLine(bool fullWidth, float offsetX, float offsetY, float thickness, ImU32 theme)
	{
		if (fullWidth)
		{
			if (ImGui::GetCurrentWindow()->DC.CurrentColumns != nullptr)
				ImGui::PushColumnsBackground();
			else if (ImGui::GetCurrentTable() != nullptr)
				ImGui::TablePushBackgroundChannel();
		}

		const float width = fullWidth ? ImGui::GetWindowWidth() : ImGui::GetContentRegionAvail().x;
		const ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddLine(ImVec2{ cursor.x + offsetX, cursor.y + offsetY }, ImVec2{ cursor.x + width, cursor.y + offsetY }, theme, thickness);

		if (fullWidth)
		{
			if (ImGui::GetCurrentWindow()->DC.CurrentColumns != nullptr)
				ImGui::PopColumnsBackground();
			else if (ImGui::GetCurrentTable() != nullptr)
				ImGui::TablePopBackgroundChannel();
		}
	}

	void UnderLine(ImU32 theme, bool fullWidth, ImVec2 p1Offset, ImVec2 p2Offset, float thickness)
	{
		if (fullWidth)
		{
			if (ImGui::GetCurrentWindow()->DC.CurrentColumns != nullptr)
				ImGui::PushColumnsBackground();
			else if (ImGui::GetCurrentTable() != nullptr)
				ImGui::TablePushBackgroundChannel();
		}

		const float width = fullWidth ? ImGui::GetWindowWidth() : p2Offset.x - p1Offset.x;
		const ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddLine(ImVec2{ cursor.x + p1Offset.x, cursor.y + p1Offset.y }, ImVec2{ cursor.x + width, cursor.y + p2Offset.y}, theme, thickness);

		if (fullWidth)
		{
			if (ImGui::GetCurrentWindow()->DC.CurrentColumns != nullptr)
				ImGui::PopColumnsBackground();
			else if (ImGui::GetCurrentTable() != nullptr)
				ImGui::TablePopBackgroundChannel();
		}
	}

	// Exposed to be used for window with disabled decorations
	// This border is going to be drawn even if window border size is set to 0.0f
	void RenderWindowOuterBorders(ImGuiWindow* currentWindow)
	{
		struct ImGuiResizeBorderDef
		{
			ImVec2 InnerDir;
			ImVec2 SegmentN1, SegmentN2;
			float  OuterAngle;
		};

		static const ImGuiResizeBorderDef resize_border_def[4] =
		{
			{ ImVec2(+1, 0), ImVec2(0, 1), ImVec2(0, 0), IM_PI * 1.00f }, // Left
			{ ImVec2(-1, 0), ImVec2(1, 0), ImVec2(1, 1), IM_PI * 0.00f }, // Right
			{ ImVec2(0, +1), ImVec2(0, 0), ImVec2(1, 0), IM_PI * 1.50f }, // Up
			{ ImVec2(0, -1), ImVec2(1, 1), ImVec2(0, 1), IM_PI * 0.50f }  // Down
		};

		auto GetResizeBorderRect = [](ImGuiWindow* window, int border_n, float perp_padding, float thickness)
			{
				ImRect rect = window->Rect();
				if (thickness == 0.0f)
				{
					rect.Max.x -= 1;
					rect.Max.y -= 1;
				}
				if (border_n == ImGuiDir_Left) { return ImRect(rect.Min.x - thickness, rect.Min.y + perp_padding, rect.Min.x + thickness, rect.Max.y - perp_padding); }
				if (border_n == ImGuiDir_Right) { return ImRect(rect.Max.x - thickness, rect.Min.y + perp_padding, rect.Max.x + thickness, rect.Max.y - perp_padding); }
				if (border_n == ImGuiDir_Up) { return ImRect(rect.Min.x + perp_padding, rect.Min.y - thickness, rect.Max.x - perp_padding, rect.Min.y + thickness); }
				if (border_n == ImGuiDir_Down) { return ImRect(rect.Min.x + perp_padding, rect.Max.y - thickness, rect.Max.x - perp_padding, rect.Max.y + thickness); }
				IM_ASSERT(0);
				return ImRect();
			};


		ImGuiContext& g = *GImGui;
		float rounding = currentWindow->WindowRounding;
		float border_size = 1.0f; // window->WindowBorderSize;
		if (border_size > 0.0f && !(currentWindow->Flags & ImGuiWindowFlags_NoBackground))
			currentWindow->DrawList->AddRect(currentWindow->Pos, { currentWindow->Pos.x + currentWindow->Size.x,  currentWindow->Pos.y + currentWindow->Size.y }, ImGui::GetColorU32(ImGuiCol_Border), rounding, 0, border_size);

		int border_held = currentWindow->ResizeBorderHeld;
		if (border_held != -1)
		{
			const ImGuiResizeBorderDef& def = resize_border_def[border_held];
			ImRect border_r = GetResizeBorderRect(currentWindow, border_held, rounding, 0.0f);
			ImVec2 p1 = ImLerp(border_r.Min, border_r.Max, def.SegmentN1);
			const float offsetX = def.InnerDir.x * rounding;
			const float offsetY = def.InnerDir.y * rounding;
			p1.x += 0.5f + offsetX;
			p1.y += 0.5f + offsetY;

			ImVec2 p2 = ImLerp(border_r.Min, border_r.Max, def.SegmentN2);
			p2.x += 0.5f + offsetX;
			p2.y += 0.5f + offsetY;

			currentWindow->DrawList->PathArcTo(p1, rounding, def.OuterAngle - IM_PI * 0.25f, def.OuterAngle);
			currentWindow->DrawList->PathArcTo(p2, rounding, def.OuterAngle, def.OuterAngle + IM_PI * 0.25f);
			currentWindow->DrawList->PathStroke(ImGui::GetColorU32(ImGuiCol_SeparatorActive), 0, ImMax(2.0f, border_size)); // Thicker than usual
		}
		if (g.Style.FrameBorderSize > 0 && !(currentWindow->Flags & ImGuiWindowFlags_NoTitleBar) && !currentWindow->DockIsActive)
		{
			float y = currentWindow->Pos.y + currentWindow->TitleBarHeight() - 1;
			currentWindow->DrawList->AddLine(ImVec2(currentWindow->Pos.x + border_size, y), ImVec2(currentWindow->Pos.x + currentWindow->Size.x - border_size, y), ImGui::GetColorU32(ImGuiCol_Border), g.Style.FrameBorderSize);
		}
	}

	bool UpdateWindowManualResize(ImGuiWindow* window, ImVec2& newSize, ImVec2& newPosition)
	{
		auto CalcWindowSizeAfterConstraint = [](ImGuiWindow* window, const ImVec2& size_desired)
		{
			ImGuiContext& g = *GImGui;
			ImVec2 new_size = size_desired;
			if (g.NextWindowData.Flags & ImGuiNextWindowDataFlags_HasSizeConstraint)
			{
				// Using -1,-1 on either X/Y axis to preserve the current size.
				ImRect cr = g.NextWindowData.SizeConstraintRect;
				new_size.x = (cr.Min.x >= 0 && cr.Max.x >= 0) ? ImClamp(new_size.x, cr.Min.x, cr.Max.x) : window->SizeFull.x;
				new_size.y = (cr.Min.y >= 0 && cr.Max.y >= 0) ? ImClamp(new_size.y, cr.Min.y, cr.Max.y) : window->SizeFull.y;
				if (g.NextWindowData.SizeCallback)
				{
					ImGuiSizeCallbackData data;
					data.UserData = g.NextWindowData.SizeCallbackUserData;
					data.Pos = window->Pos;
					data.CurrentSize = window->SizeFull;
					data.DesiredSize = new_size;
					g.NextWindowData.SizeCallback(&data);
					new_size = data.DesiredSize;
				}
				new_size.x = IM_FLOOR(new_size.x);
				new_size.y = IM_FLOOR(new_size.y);
			}

			// Minimum size
			if (!(window->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_AlwaysAutoResize)))
			{
				ImGuiWindow* window_for_height = (window->DockNodeAsHost && window->DockNodeAsHost->VisibleWindow) ? window->DockNodeAsHost->VisibleWindow : window;
				const float decoration_up_height = window_for_height->TitleBarHeight() + window_for_height->MenuBarHeight();
				new_size = ImMax(new_size, g.Style.WindowMinSize);
				new_size.y = ImMax(new_size.y, decoration_up_height + ImMax(0.0f, g.Style.WindowRounding - 1.0f)); // Reduce artifacts with very small windows
			}
			return new_size;
		};

		auto CalcWindowAutoFitSize = [CalcWindowSizeAfterConstraint](ImGuiWindow* window, const ImVec2& size_contents)
		{
			ImGuiContext& g = *GImGui;
			ImGuiStyle& style = g.Style;
			const float decoration_up_height = window->TitleBarHeight() + window->MenuBarHeight();
			ImVec2 size_pad{ window->WindowPadding.x * 2.0f, window->WindowPadding.y * 2.0f };
			ImVec2 size_desired = { size_contents.x + size_pad.x + 0.0f, size_contents.y + size_pad.y + decoration_up_height };
			if (window->Flags & ImGuiWindowFlags_Tooltip)
			{
				// Tooltip always resize
				return size_desired;
			}
			else
			{
				// Maximum window size is determined by the viewport size or monitor size
				const bool is_popup = (window->Flags & ImGuiWindowFlags_Popup) != 0;
				const bool is_menu = (window->Flags & ImGuiWindowFlags_ChildMenu) != 0;
				ImVec2 size_min = style.WindowMinSize;
				if (is_popup || is_menu) // Popups and menus bypass style.WindowMinSize by default, but we give then a non-zero minimum size to facilitate understanding problematic cases (e.g. empty popups)
					size_min = ImMin(size_min, ImVec2(4.0f, 4.0f));

				// FIXME-VIEWPORT-WORKAREA: May want to use GetWorkSize() instead of Size depending on the type of windows?
				ImVec2 avail_size = window->Viewport->Size;
				if (window->ViewportOwned)
					avail_size = ImVec2(FLT_MAX, FLT_MAX);
				const int monitor_idx = window->ViewportAllowPlatformMonitorExtend;
				if (monitor_idx >= 0 && monitor_idx < g.PlatformIO.Monitors.Size)
					avail_size = g.PlatformIO.Monitors[monitor_idx].WorkSize;
				ImVec2 size_auto_fit = ImClamp(size_desired, size_min, ImMax(size_min, { avail_size.x - style.DisplaySafeAreaPadding.x * 2.0f,
																						avail_size.y - style.DisplaySafeAreaPadding.y * 2.0f }));

				// When the window cannot fit all contents (either because of constraints, either because screen is too small),
				// we are growing the size on the other axis to compensate for expected scrollbar. FIXME: Might turn bigger than ViewportSize-WindowPadding.
				ImVec2 size_auto_fit_after_constraint = CalcWindowSizeAfterConstraint(window, size_auto_fit);
				bool will_have_scrollbar_x = (size_auto_fit_after_constraint.x - size_pad.x - 0.0f < size_contents.x && !(window->Flags & ImGuiWindowFlags_NoScrollbar) && (window->Flags & ImGuiWindowFlags_HorizontalScrollbar)) || (window->Flags & ImGuiWindowFlags_AlwaysHorizontalScrollbar);
				bool will_have_scrollbar_y = (size_auto_fit_after_constraint.y - size_pad.y - decoration_up_height < size_contents.y && !(window->Flags & ImGuiWindowFlags_NoScrollbar)) || (window->Flags & ImGuiWindowFlags_AlwaysVerticalScrollbar);
				if (will_have_scrollbar_x)
					size_auto_fit.y += style.ScrollbarSize;
				if (will_have_scrollbar_y)
					size_auto_fit.x += style.ScrollbarSize;
				return size_auto_fit;
			}
		};

		ImGuiContext& g = *GImGui;

		// Decide if we are going to handle borders and resize grips
		const bool handle_borders_and_resize_grips = (window->DockNodeAsHost || !window->DockIsActive);

		if (!handle_borders_and_resize_grips || window->Collapsed)
			return false;

		const ImVec2 size_auto_fit = CalcWindowAutoFitSize(window, window->ContentSizeIdeal);

		// Handle manual resize: Resize Grips, Borders, Gamepad
		int border_held = -1;
		ImU32 resize_grip_col[4] = {};
		const int resize_grip_count = g.IO.ConfigWindowsResizeFromEdges ? 2 : 1; // Allow resize from lower-left if we have the mouse cursor feedback for it.
		const float resize_grip_draw_size = IM_FLOOR(ImMax(g.FontSize * 1.10f, window->WindowRounding + 1.0f + g.FontSize * 0.2f));
		window->ResizeBorderHeld = (signed char)border_held;

		//const ImRect& visibility_rect;

		struct ImGuiResizeBorderDef
		{
			ImVec2 InnerDir;
			ImVec2 SegmentN1, SegmentN2;
			float  OuterAngle;
		};
		static const ImGuiResizeBorderDef resize_border_def[4] =
		{
			{ ImVec2(+1, 0), ImVec2(0, 1), ImVec2(0, 0), IM_PI * 1.00f }, // Left
			{ ImVec2(-1, 0), ImVec2(1, 0), ImVec2(1, 1), IM_PI * 0.00f }, // Right
			{ ImVec2(0, +1), ImVec2(0, 0), ImVec2(1, 0), IM_PI * 1.50f }, // Up
			{ ImVec2(0, -1), ImVec2(1, 1), ImVec2(0, 1), IM_PI * 0.50f }  // Down
		};

		// Data for resizing from corner
		struct ImGuiResizeGripDef
		{
			ImVec2  CornerPosN;
			ImVec2  InnerDir;
			int     AngleMin12, AngleMax12;
		};
		static const ImGuiResizeGripDef resize_grip_def[4] =
		{
			{ ImVec2(1, 1), ImVec2(-1, -1), 0, 3 },  // Lower-right
			{ ImVec2(0, 1), ImVec2(+1, -1), 3, 6 },  // Lower-left
			{ ImVec2(0, 0), ImVec2(+1, +1), 6, 9 },  // Upper-left (Unused)
			{ ImVec2(1, 0), ImVec2(-1, +1), 9, 12 }  // Upper-right (Unused)
		};

		auto CalcResizePosSizeFromAnyCorner = [CalcWindowSizeAfterConstraint](ImGuiWindow* window, const ImVec2& corner_target, const ImVec2& corner_norm, ImVec2* out_pos, ImVec2* out_size)
		{
			ImVec2 pos_min = ImLerp(corner_target, window->Pos, corner_norm);                // Expected window upper-left
			ImVec2 pos_max = ImLerp({ window->Pos.x + window->Size.x, window->Pos.y + window->Size.y }, corner_target, corner_norm); // Expected window lower-right
			ImVec2 size_expected = { pos_max.x - pos_min.x,  pos_max.y - pos_min.y };
			ImVec2 size_constrained = CalcWindowSizeAfterConstraint(window, size_expected);
			*out_pos = pos_min;
			if (corner_norm.x == 0.0f)
				out_pos->x -= (size_constrained.x - size_expected.x);
			if (corner_norm.y == 0.0f)
				out_pos->y -= (size_constrained.y - size_expected.y);
			*out_size = size_constrained;
		};

		auto GetResizeBorderRect = [](ImGuiWindow* window, int border_n, float perp_padding, float thickness)
		{
			ImRect rect = window->Rect();
			if (thickness == 0.0f)
			{
				rect.Max.x -= 1;
				rect.Max.y -= 1;
			}
			if (border_n == ImGuiDir_Left) { return ImRect(rect.Min.x - thickness, rect.Min.y + perp_padding, rect.Min.x + thickness, rect.Max.y - perp_padding); }
			if (border_n == ImGuiDir_Right) { return ImRect(rect.Max.x - thickness, rect.Min.y + perp_padding, rect.Max.x + thickness, rect.Max.y - perp_padding); }
			if (border_n == ImGuiDir_Up) { return ImRect(rect.Min.x + perp_padding, rect.Min.y - thickness, rect.Max.x - perp_padding, rect.Min.y + thickness); }
			if (border_n == ImGuiDir_Down) { return ImRect(rect.Min.x + perp_padding, rect.Max.y - thickness, rect.Max.x - perp_padding, rect.Max.y + thickness); }
			IM_ASSERT(0);
			return ImRect();
		};

		static const float WINDOWS_HOVER_PADDING = 4.0f;                        // Extend outside window for hovering/resizing (maxxed with TouchPadding) and inside windows for borders. Affect FindHoveredWindow().
		static const float WINDOWS_RESIZE_FROM_EDGES_FEEDBACK_TIMER = 0.04f;    // Reduce visual noise by only highlighting the border after a certain time.

		auto& style = g.Style;
		ImGuiWindowFlags flags = window->Flags;

		if (/*(flags & ImGuiWindowFlags_NoResize) || */(flags & ImGuiWindowFlags_AlwaysAutoResize) || window->AutoFitFramesX > 0 || window->AutoFitFramesY > 0)
			return false;
		if (window->WasActive == false) // Early out to avoid running this code for e.g. an hidden implicit/fallback Debug window.
			return false;

		bool ret_auto_fit = false;
		const int resize_border_count = g.IO.ConfigWindowsResizeFromEdges ? 4 : 0;
		const float grip_draw_size = IM_FLOOR(ImMax(g.FontSize * 1.35f, window->WindowRounding + 1.0f + g.FontSize * 0.2f));
		const float grip_hover_inner_size = IM_FLOOR(grip_draw_size * 0.75f);
		const float grip_hover_outer_size = g.IO.ConfigWindowsResizeFromEdges ? WINDOWS_HOVER_PADDING : 0.0f;

		ImVec2 pos_target(FLT_MAX, FLT_MAX);
		ImVec2 size_target(FLT_MAX, FLT_MAX);

		// Calculate the range of allowed position for that window (to be movable and visible past safe area padding)
		// When clamping to stay visible, we will enforce that window->Pos stays inside of visibility_rect.
		ImRect viewport_rect(window->Viewport->GetMainRect());
		ImRect viewport_work_rect(window->Viewport->GetWorkRect());
		ImVec2 visibility_padding = ImMax(style.DisplayWindowPadding, style.DisplaySafeAreaPadding);
		ImRect visibility_rect({ viewport_work_rect.Min.x + visibility_padding.x, viewport_work_rect.Min.y + visibility_padding.y },
			{ viewport_work_rect.Max.x - visibility_padding.x, viewport_work_rect.Max.y - visibility_padding.y });

		// Clip mouse interaction rectangles within the viewport rectangle (in practice the narrowing is going to happen most of the time).
		// - Not narrowing would mostly benefit the situation where OS windows _without_ decoration have a threshold for hovering when outside their limits.
		//   This is however not the case with current backends under Win32, but a custom borderless window implementation would benefit from it.
		// - When decoration are enabled we typically benefit from that distance, but then our resize elements would be conflicting with OS resize elements, so we also narrow.
		// - Note that we are unable to tell if the platform setup allows hovering with a distance threshold (on Win32, decorated window have such threshold).
		// We only clip interaction so we overwrite window->ClipRect, cannot call PushClipRect() yet as DrawList is not yet setup.
		const bool clip_with_viewport_rect = !(g.IO.BackendFlags & ImGuiBackendFlags_HasMouseHoveredViewport) || (g.IO.MouseHoveredViewport != window->ViewportId) || !(window->Viewport->Flags & ImGuiViewportFlags_NoDecoration);
		if (clip_with_viewport_rect)
			window->ClipRect = window->Viewport->GetMainRect();

		// Resize grips and borders are on layer 1
		window->DC.NavLayerCurrent = ImGuiNavLayer_Menu;

		// Manual resize grips
		ImGui::PushID("#RESIZE");
		for (int resize_grip_n = 0; resize_grip_n < resize_grip_count; resize_grip_n++)
		{
			const ImGuiResizeGripDef& def = resize_grip_def[resize_grip_n];

			const ImVec2 corner = ImLerp(window->Pos, { window->Pos.x + window->Size.x, window->Pos.y + window->Size.y }, def.CornerPosN);

			// Using the FlattenChilds button flag we make the resize button accessible even if we are hovering over a child window
			bool hovered, held;
			const ImVec2 min = { corner.x - def.InnerDir.x * grip_hover_outer_size, corner.y - def.InnerDir.y * grip_hover_outer_size };
			const ImVec2 max = { corner.x + def.InnerDir.x * grip_hover_outer_size, corner.y + def.InnerDir.y * grip_hover_outer_size };
			ImRect resize_rect(min, max);

			if (resize_rect.Min.x > resize_rect.Max.x) ImSwap(resize_rect.Min.x, resize_rect.Max.x);
			if (resize_rect.Min.y > resize_rect.Max.y) ImSwap(resize_rect.Min.y, resize_rect.Max.y);
			ImGuiID resize_grip_id = window->GetID(resize_grip_n); // == GetWindowResizeCornerID()
			ImGui::ButtonBehavior(resize_rect, resize_grip_id, &hovered, &held, ImGuiButtonFlags_FlattenChildren | ImGuiButtonFlags_NoNavFocus);
			//GetForegroundDrawList(window)->AddRect(resize_rect.Min, resize_rect.Max, IM_COL32(255, 255, 0, 255));
			if (hovered || held)
				g.MouseCursor = (resize_grip_n & 1) ? ImGuiMouseCursor_ResizeNESW : ImGuiMouseCursor_ResizeNWSE;

			if (held && g.IO.MouseDoubleClicked[0] && resize_grip_n == 0)
			{
				// Manual auto-fit when double-clicking
				size_target = CalcWindowSizeAfterConstraint(window, size_auto_fit);
				ret_auto_fit = true;
				ImGui::ClearActiveID();
			}
			else if (held)
			{
				// Resize from any of the four corners
				// We don't use an incremental MouseDelta but rather compute an absolute target size based on mouse position
				ImVec2 clamp_min = ImVec2(def.CornerPosN.x == 1.0f ? visibility_rect.Min.x : -FLT_MAX, def.CornerPosN.y == 1.0f ? visibility_rect.Min.y : -FLT_MAX);
				ImVec2 clamp_max = ImVec2(def.CornerPosN.x == 0.0f ? visibility_rect.Max.x : +FLT_MAX, def.CornerPosN.y == 0.0f ? visibility_rect.Max.y : +FLT_MAX);

				const float x = g.IO.MousePos.x - g.ActiveIdClickOffset.x + ImLerp(def.InnerDir.x * grip_hover_outer_size, def.InnerDir.x * -grip_hover_inner_size, def.CornerPosN.x);
				const float y = g.IO.MousePos.y - g.ActiveIdClickOffset.y + ImLerp(def.InnerDir.y * grip_hover_outer_size, def.InnerDir.y * -grip_hover_inner_size, def.CornerPosN.y);

				ImVec2 corner_target(x, y); // Corner of the window corresponding to our corner grip
				corner_target = ImClamp(corner_target, clamp_min, clamp_max);
				CalcResizePosSizeFromAnyCorner(window, corner_target, def.CornerPosN, &pos_target, &size_target);
			}

			// Only lower-left grip is visible before hovering/activating
			if (resize_grip_n == 0 || held || hovered)
				resize_grip_col[resize_grip_n] = ImGui::GetColorU32(held ? ImGuiCol_ResizeGripActive : hovered ? ImGuiCol_ResizeGripHovered : ImGuiCol_ResizeGrip);
		}
		for (int border_n = 0; border_n < resize_border_count; border_n++)
		{
			const ImGuiResizeBorderDef& def = resize_border_def[border_n];
			const ImGuiAxis axis = (border_n == ImGuiDir_Left || border_n == ImGuiDir_Right) ? ImGuiAxis_X : ImGuiAxis_Y;

			bool hovered, held;
			ImRect border_rect = GetResizeBorderRect(window, border_n, grip_hover_inner_size, WINDOWS_HOVER_PADDING);
			ImGuiID border_id = window->GetID(border_n + 4); // == GetWindowResizeBorderID()
			ImGui::ButtonBehavior(border_rect, border_id, &hovered, &held, ImGuiButtonFlags_FlattenChildren);
			//GetForegroundDrawLists(window)->AddRect(border_rect.Min, border_rect.Max, IM_COL32(255, 255, 0, 255));
			if ((hovered && g.HoveredIdTimer > WINDOWS_RESIZE_FROM_EDGES_FEEDBACK_TIMER) || held)
			{
				g.MouseCursor = (axis == ImGuiAxis_X) ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS;
				if (held)
					border_held = border_n;
			}
			if (held)
			{
				ImVec2 clamp_min(border_n == ImGuiDir_Right ? visibility_rect.Min.x : -FLT_MAX, border_n == ImGuiDir_Down ? visibility_rect.Min.y : -FLT_MAX);
				ImVec2 clamp_max(border_n == ImGuiDir_Left ? visibility_rect.Max.x : +FLT_MAX, border_n == ImGuiDir_Up ? visibility_rect.Max.y : +FLT_MAX);
				ImVec2 border_target = window->Pos;
				border_target[axis] = g.IO.MousePos[axis] - g.ActiveIdClickOffset[axis] + WINDOWS_HOVER_PADDING;
				border_target = ImClamp(border_target, clamp_min, clamp_max);
				CalcResizePosSizeFromAnyCorner(window, border_target, ImMin(def.SegmentN1, def.SegmentN2), &pos_target, &size_target);
			}
		}
		ImGui::PopID();

		bool changed = false;
		newSize = window->Size;
		newPosition = window->Pos;

		// Apply back modified position/size to window
		if (size_target.x != FLT_MAX)
		{
			//window->SizeFull = size_target;
			//MarkIniSettingsDirty(window);
			newSize = size_target;
			changed = true;
		}
		if (pos_target.x != FLT_MAX)
		{
			//window->Pos = ImFloor(pos_target);
			//MarkIniSettingsDirty(window);
			newPosition = pos_target;
			changed = true;
		}

		//window->Size = window->SizeFull;
		return changed;
	}

	void Image(const Ref<Texture2D>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		const auto textureID = GetTextureID(image);
		ImGui::Image(textureID, size, uv0, uv1, tint_col, border_col);
	}

	void DrawBorder(ImVec2 rectMin, ImVec2 rectMax, const ImVec4& borderColour, float thickness, float offsetX, float offsetY)
	{
		auto min = rectMin;
		min.x -= thickness;
		min.y -= thickness;
		min.x += offsetX;
		min.y += offsetY;
		auto max = rectMax;
		max.x += thickness;
		max.y += thickness;
		max.x += offsetX;
		max.y += offsetY;

		auto* drawList = ImGui::GetWindowDrawList();
		drawList->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(borderColour), 0.0f, 0, thickness);
	}

	void DrawBorder(ImRect rect, float thickness, float rounding, float offsetX, float offsetY)
	{
		auto min = rect.Min;
		min.x -= thickness;
		min.y -= thickness;
		min.x += offsetX;
		min.y += offsetY;
		auto max = rect.Max;
		max.x += thickness;
		max.y += thickness;
		max.x += offsetX;
		max.y += offsetY;

		auto* drawList = ImGui::GetWindowDrawList();
		drawList->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Border)), rounding, 0, thickness);
	}

	void DrawBorderHorizontal(ImVec2 rectMin, ImVec2 rectMax, const ImVec4& borderColour, float thickness, float offsetX, float offsetY)
	{
		auto min = rectMin;
		min.y -= thickness;
		min.x += offsetX;
		min.y += offsetY;
		auto max = rectMax;
		max.y += thickness;
		max.x += offsetX;
		max.y += offsetY;

		auto* drawList = ImGui::GetWindowDrawList();
		const auto colour = ImGui::ColorConvertFloat4ToU32(borderColour);
		drawList->AddLine(min, ImVec2(max.x, min.y), colour, thickness);
		drawList->AddLine(ImVec2(min.x, max.y), max, colour, thickness);
	}

	void DrawBorderVertical(ImVec2 rectMin, ImVec2 rectMax, const ImVec4& borderColour, float thickness, float offsetX, float offsetY)
	{
		auto min = rectMin;
		min.x -= thickness;
		min.x += offsetX;
		min.y += offsetY;
		auto max = rectMax;
		max.x += thickness;
		max.x += offsetX;
		max.y += offsetY;

		auto* drawList = ImGui::GetWindowDrawList();
		const auto colour = ImGui::ColorConvertFloat4ToU32(borderColour);
		drawList->AddLine(min, ImVec2(min.x, max.y), colour, thickness);
		drawList->AddLine(ImVec2(max.x, min.y), max, colour, thickness);
	}

	const char* PatchFormatStringFloatToInt(const char* fmt)
	{
		if (fmt[0] == '%' && fmt[1] == '.' && fmt[2] == '0' && fmt[3] == 'f' && fmt[4] == 0) // Fast legacy path for "%.0f" which is expected to be the most common case.
			return "%d";
		const char* fmt_start = ImParseFormatFindStart(fmt);    // Find % (if any, and ignore %%)
		const char* fmt_end = ImParseFormatFindEnd(fmt_start);  // Find end of format specifier, which itself is an exercise of confidence/recklessness (because snprintf is dependent on libc or user).
		if (fmt_end > fmt_start && fmt_end[-1] == 'f')
		{
			if (fmt_start == fmt && fmt_end[0] == 0)
				return "%d";
			ImGuiContext& g = *GImGui;
			ImFormatString(g.TempBuffer.Data, IM_ARRAYSIZE(g.TempBuffer.Data), "%.*s%%d%s", (int)(fmt_start - fmt), fmt, fmt_end); // Honor leading and trailing decorations, but lose alignment/precision.
			return g.TempBuffer.Data;
		}
		return fmt;
	}

	int FormatString(char* buf, size_t buf_size, const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		int w = vsnprintf(buf, buf_size, fmt, args);
		va_end(args);
		if (buf == NULL)
			return w;
		if (w == -1 || w >= (int)buf_size)
			w = (int)buf_size - 1;
		buf[w] = 0;
		return w;
	}

	bool DragScalar(const char* label, ImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;
		const ImGuiID id = window->GetID(label);
		const float w = ImGui::CalcItemWidth();

		const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
		const ImRect frame_bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + w, window->DC.CursorPos.y + (label_size.y + style.FramePadding.y * 2.0f)));
		const ImRect total_bb(frame_bb.Min, ImVec2(frame_bb.Max.x + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), frame_bb.Max.y));

		const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
		ImGui::ItemSize(total_bb, style.FramePadding.y);
		if (!ImGui::ItemAdd(total_bb, id, &frame_bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
			return false;

		// Default format string when passing NULL
		if (format == NULL)
			format = ImGui::DataTypeGetInfo(data_type)->PrintFmt;
		else if (data_type == ImGuiDataType_S32 && strcmp(format, "%d") != 0) // (FIXME-LEGACY: Patch old "%.0f" format string to use "%d", read function more details.)
			format = PatchFormatStringFloatToInt(format);

		// Tabbing or CTRL-clicking on Drag turns it into an InputText
		const bool hovered = ImGui::ItemHoverable(frame_bb, id);
		bool temp_input_is_active = temp_input_allowed && ImGui::TempInputIsActive(id);
		if (!temp_input_is_active)
		{
			const bool input_requested_by_tabbing = temp_input_allowed && (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_FocusedByTabbing) != 0;
			const bool clicked = (hovered && g.IO.MouseClicked[0]);
			const bool double_clicked = (hovered && g.IO.MouseClickedCount[0] == 2);
			if (input_requested_by_tabbing || clicked || double_clicked || g.NavActivateId == id || g.NavActivateInputId == id)
			{
				ImGui::SetActiveID(id, window);
				ImGui::SetFocusID(id, window);
				ImGui::FocusWindow(window);
				g.ActiveIdUsingNavDirMask = (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
				if (temp_input_allowed)
					if (input_requested_by_tabbing || (clicked && g.IO.KeyCtrl) || double_clicked || g.NavActivateInputId == id)
						temp_input_is_active = true;
			}

			// Experimental: simple click (without moving) turns Drag into an InputText
			if (g.IO.ConfigDragClickToInputText && temp_input_allowed && !temp_input_is_active)
				if (g.ActiveId == id && hovered && g.IO.MouseReleased[0] && !ImGui::IsMouseDragPastThreshold(0, g.IO.MouseDragThreshold * 0.5f))
				{
					g.NavActivateId = g.NavActivateInputId = id;
					g.NavActivateFlags = ImGuiActivateFlags_PreferInput;
					temp_input_is_active = true;
				}
		}

		if (temp_input_is_active)
		{
			// Only clamp CTRL+Click input when ImGuiSliderFlags_AlwaysClamp is set
			const bool is_clamp_input = (flags & ImGuiSliderFlags_AlwaysClamp) != 0 && (p_min == NULL || p_max == NULL || ImGui::DataTypeCompare(data_type, p_min, p_max) < 0);
			return ImGui::TempInputScalar(frame_bb, id, label, data_type, p_data, format, is_clamp_input ? p_min : NULL, is_clamp_input ? p_max : NULL);
		}

		// Draw frame
		const ImU32 frame_col = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
		ImGui::RenderNavHighlight(frame_bb, id);
		ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, style.FrameRounding);

		// Drag behavior
		const bool value_changed = ImGui::DragBehavior(id, data_type, p_data, v_speed, p_min, p_max, format, flags);
		if (value_changed)
			ImGui::MarkItemEdited(id);

		const bool mixed_value = (g.CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0;

		// Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
		char value_buf[64];
		const char* value_buf_end = value_buf + (mixed_value ? FormatString(value_buf, IM_ARRAYSIZE(value_buf), "---") : ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format));
		if (g.LogEnabled)
			ImGui::LogSetNextTextDecoration("{", "}");
		ImGui::RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.5f, 0.5f));

		if (label_size.x > 0.0f)
			ImGui::RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

		DrawItemActivityOutline();

		IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
		return value_changed;
	}

	bool EditVec3(std::string_view label, ImVec2 size, float resetValue, bool& manuallyEdited, glm::vec3& value, VectorAxis renderMultiSelectAxes, float speed, glm::vec3 v_min, glm::vec3 v_max, const char* format, ImGuiSliderFlags flags)
	{
		ImGui::BeginVertical(fmt::format("{0}fr", label).c_str());
		bool changed = false;

		{
			constexpr float spacingX = 8.0f;

			UI::ImGuiScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, { spacingX, 0.0f });
			UI::ImGuiScopedStyle padding(ImGuiStyleVar_WindowPadding, { 0.0f, 2.0f });
			constexpr float framePadding = 2.0f;
			constexpr float outlineSpacing = 1.0f;
			const float lineHeight = GImGui->Font->FontSize + framePadding * 2.0f;
			const ImVec2 buttonSize = { lineHeight + 2.0f, lineHeight + 4.3f };
			const float inputItemWidth = size.x / 3.0f - buttonSize.x;

			UI::ShiftCursorY(framePadding);

			const ImGuiFontsLibrary& fontsLibrary = Application::Get().GetImGuiLayer()->GetFontsLibrary();

			auto drawControl = [&](const std::string& label, float& value, const ImVec4& colorN, const ImVec4& colorH, const ImVec4& colorP, bool renderMultiSelect, float speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
			{
				UI::ShiftCursorY(-0.1f);

				{
					UI::ImGuiScopedStyle buttomFrame(ImGuiStyleVar_FramePadding, { framePadding, 0.0f });
					UI::ImGuiScopedStyle buttomRounding(ImGuiStyleVar_FrameRounding, 1.0f);
					UI::ImGuiScopedColor buttonColor(ImGuiCol_Button, colorN);
					UI::ImGuiScopedColor buttonColorHovered(ImGuiCol_ButtonHovered, colorH);
					UI::ImGuiScopedColor buttonColorActive(ImGuiCol_ButtonActive, colorP);
					UI::ImGuiScopedFont font(fontsLibrary.GetFont("RobotoBold"));

					if (ImGui::Button(label.c_str(), buttonSize))
					{
						value = resetValue;
						changed = true;
					}
				}

				ImGui::SameLine(0.0f, outlineSpacing);
				ImGui::SetNextItemWidth(inputItemWidth);
				//UI::ShiftCursorY(-framePadding / 2.0f);

				bool wasTempInputActive = ImGui::TempInputIsActive(ImGui::GetID(("##" + label).c_str()));
				changed |= UI::DragFloat(("##" + label).c_str(), &value, speed, v_min, v_max, format, flags);

				if (changed && Input::IsKeyDown(KeyCode::Tab))
					manuallyEdited = true;

				if (ImGui::TempInputIsActive(ImGui::GetID(("##" + label).c_str())))
					changed = false;

				if (wasTempInputActive)
					manuallyEdited |= ImGui::IsItemDeactivatedAfterEdit();
			};

			drawControl("X", value.x, { 0.8f, 0.1f, 0.15f, 1.0f }, { 0.9f, 0.2f, 0.2f, 1.0f }, { 0.8f, 0.1f, 0.15f, 1.0f }, (renderMultiSelectAxes & VectorAxis::X) == VectorAxis::X, speed, v_min.x, v_max.x, format, flags);

			ImGui::SameLine(0.0f, outlineSpacing);
			drawControl("Y", value.y, { 0.2f, 0.7f, 0.2f, 1.0f }, { 0.3f, 0.8f, 0.3f, 1.0f }, { 0.2f, 0.7f, 0.2f, 1.0f }, (renderMultiSelectAxes & VectorAxis::Y) == VectorAxis::Y, speed, v_min.y, v_max.y, format, flags);

			ImGui::SameLine(0.0f, outlineSpacing);
			drawControl("Z", value.z, { 0.1f, 0.25f, 0.8f, 1.0f }, { 0.2f, 0.35f, 0.9f, 1.0f }, { 0.1f, 0.25f, 0.8f, 1.0f }, (renderMultiSelectAxes & VectorAxis::Z) == VectorAxis::Z, speed, v_min.z, v_max.z, format, flags);

			ImGui::EndVertical();
		}

		return changed || manuallyEdited;
	}

	bool IsMatchingSearch(const std::string& item, std::string_view searchQuery, bool caseSensitive, bool stripWhiteSpaces, bool stripUnderscores)
	{
		if (searchQuery.empty())
			return true;

		if (item.empty())
			return false;

		std::string itemSanitized = stripUnderscores ? choc::text::replace(item, "_", " ") : item;

		if (stripWhiteSpaces)
			itemSanitized = choc::text::replace(itemSanitized, " ", "");

		std::string searchString = stripWhiteSpaces ? choc::text::replace(searchQuery, " ", "") : std::string(searchQuery);

		if (!caseSensitive)
		{
			itemSanitized = Utils::ToLower(itemSanitized);
			searchString = Utils::ToLower(searchString);
		}

		bool result = false;
		if (choc::text::contains(searchString, " "))
		{
			std::vector<std::string> searchTerms = choc::text::splitAtWhitespace(searchString);
			for (const auto& searchTerm : searchTerms)
			{
				if (!searchTerm.empty() && choc::text::contains(itemSanitized, searchTerm))
					result = true;
				else
				{
					result = false;
					break;
				}
			}
		}
		else
		{
			result = choc::text::contains(itemSanitized, searchString);
		}

		return result;
	}

	bool AssetSearchPopup(const char* ID, AssetType type, AssetHandle& outHandle, bool* cleared, const char* hint, ImVec2 size)
	{
		UI::ImGuiScopedColor popupBG(ImGuiCol_PopupBg, UI::ColorWithMultipliedValue(Colors::Theme::Background, 1.6f).Value);

		bool modified = false;

		const AssetRegistry& assetRegistry = Project::GetEditorAssetManager()->GetAssetRegistry();
		AssetHandle current = outHandle;

		ImGui::SetNextWindowSize({ size.x, 0.0f });

		static bool grabFocus = true;

		if (UI::BeginPopup(ID, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
		{
			static std::string searchString;

			if (ImGui::GetCurrentWindow()->Appearing)
			{
				grabFocus = true;
				searchString.clear();
			}

			// Search widget
			UI::ShiftCursor(3.0f, 2.0f);
			ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - ImGui::GetCursorPosX() * 2.0f);
			UI::SearchWidget(searchString, hint, &grabFocus);

			const bool searching = !searchString.empty();

			// Clear property button
			if (cleared != nullptr)
			{
				UI::ImGuiScopedColor buttonColours(ImGuiCol_Button, UI::ColorWithMultipliedValue(Colors::Theme::Background, 1.0f).Value);
				UI::ImGuiScopedColor buttonHovered(ImGuiCol_ButtonHovered, UI::ColorWithMultipliedValue(Colors::Theme::Background, 1.2f).Value);
				UI::ImGuiScopedColor buttonActive(ImGuiCol_ButtonActive, UI::ColorWithMultipliedValue(Colors::Theme::Background, 0.9f).Value);
				UI::ImGuiScopedStyle border(ImGuiStyleVar_FrameBorderSize, 0.0f);

				ImGui::SetCursorPosX(0);

				ImGui::PushItemFlag(ImGuiItemFlags_NoNav, searching);

				if (ImGui::Button("CLEAR", { ImGui::GetWindowWidth(), 0.0f }))
				{
					*cleared = true;
					modified = true;
				}

				ImGui::PopItemFlag();
			}

			// List of assets
			{
				UI::ImGuiScopedColor listBoxBg(ImGuiCol_FrameBg, IM_COL32_DISABLE);
				UI::ImGuiScopedColor listBoxBorder(ImGuiCol_Border, IM_COL32_DISABLE);

				ImGuiID listID = ImGui::GetID("##SearchListBox");
				if (ImGui::BeginListBox("##SearchListBox", ImVec2(-FLT_MIN, 0.0f)))
				{
					bool forwardFocus = false;

					ImGuiContext& g = *GImGui;
					if (g.NavJustMovedToId != 0)
					{
						if (g.NavJustMovedToId == listID)
						{
							forwardFocus = true;
							// ActivateItem moves keyboard navigation focuse inside of the window
							ImGui::ActivateItem(listID);
							ImGui::SetKeyboardFocusHere(1);
						}
					}

					for (const auto& [path, metaData] : assetRegistry)
					{
						if (metaData.Type != type)
							continue;

						if (metaData.IsMemoryAsset)
							continue;

						const std::string assetName = metaData.FilePath.stem().string();

						if (!searchString.empty() && !UI::IsMatchingSearch(assetName, searchString))
							continue;

						bool isSelected = (current == metaData.Handle);
						if (ImGui::Selectable(assetName.c_str(), isSelected))
						{
							current = metaData.Handle;
							outHandle = metaData.Handle;
							modified = true;
						}

						if (forwardFocus)
							forwardFocus = false;
						else if (isSelected)
							ImGui::SetItemDefaultFocus();
					}

					ImGui::EndListBox();
				}
			}

			if (modified)
				ImGui::CloseCurrentPopup();

			UI::EndPopup();
		}

		return modified;
	}

	bool AssetSearchPopup(const char* ID, AssetType type, AssetHandle& outHandle, bool allowMemoryOnlyAssets, bool* cleared, const char* hint, ImVec2 size)
	{
		UI::ImGuiScopedColor popupBG(ImGuiCol_PopupBg, UI::ColorWithMultipliedValue(Colors::Theme::Background, 1.6f).Value);

		bool modified = false;

		const AssetRegistry& assetRegistry = Project::GetEditorAssetManager()->GetAssetRegistry();
		AssetHandle current = outHandle;

		ImGui::SetNextWindowSize({ size.x, 0.0f });

		static bool grabFocus = true;

		if (UI::BeginPopup(ID, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
		{
			static std::string searchString;

			if (ImGui::GetCurrentWindow()->Appearing)
			{
				grabFocus = true;
				searchString.clear();
			}

			// Search widget
			UI::ShiftCursor(3.0f, 2.0f);
			ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - ImGui::GetCursorPosX() * 2.0f);
			UI::SearchWidget(searchString, hint, &grabFocus);

			const bool searching = !searchString.empty();

			// Clear property button
			if (cleared != nullptr)
			{
				UI::ImGuiScopedColor buttonColours(ImGuiCol_Button, UI::ColorWithMultipliedValue(Colors::Theme::Background, 1.0f).Value);
				UI::ImGuiScopedColor buttonHovered(ImGuiCol_ButtonHovered, UI::ColorWithMultipliedValue(Colors::Theme::Background, 1.2f).Value);
				UI::ImGuiScopedColor buttonActive(ImGuiCol_ButtonActive, UI::ColorWithMultipliedValue(Colors::Theme::Background, 0.9f).Value);
				UI::ImGuiScopedStyle border(ImGuiStyleVar_FrameBorderSize, 0.0f);

				ImGui::SetCursorPosX(0);

				ImGui::PushItemFlag(ImGuiItemFlags_NoNav, searching);

				if (ImGui::Button("CLEAR", { ImGui::GetWindowWidth(), 0.0f }))
				{
					*cleared = true;
					modified = true;
				}

				ImGui::PopItemFlag();
			}

			// List of assets
			{
				UI::ImGuiScopedColor listBoxBg(ImGuiCol_FrameBg, IM_COL32_DISABLE);
				UI::ImGuiScopedColor listBoxBorder(ImGuiCol_Border, IM_COL32_DISABLE);

				ImGuiID listID = ImGui::GetID("##SearchListBox");
				if (ImGui::BeginListBox("##SearchListBox", ImVec2(-FLT_MIN, 0.0f)))
				{
					bool forwardFocus = false;

					ImGuiContext& g = *GImGui;
					if (g.NavJustMovedToId != 0)
					{
						if (g.NavJustMovedToId == listID)
						{
							forwardFocus = true;
							// ActivateItem moves keyboard navigation focuse inside of the window
							ImGui::ActivateItem(listID);
							ImGui::SetKeyboardFocusHere(1);
						}
					}

					for (const auto& [path, metaData] : assetRegistry)
					{
						if (metaData.Type != type)
							continue;

						if (allowMemoryOnlyAssets != metaData.IsMemoryAsset)
							continue;

						const std::string assetName = metaData.IsMemoryAsset ? metaData.FilePath.string() : metaData.FilePath.stem().string();

						if (!searchString.empty() && !UI::IsMatchingSearch(assetName, searchString))
							continue;

						bool isSelected = (current == metaData.Handle);
						if (ImGui::Selectable(assetName.c_str(), isSelected))
						{
							current = metaData.Handle;
							outHandle = metaData.Handle;
							modified = true;
						}

						if (forwardFocus)
							forwardFocus = false;
						else if (isSelected)
							ImGui::SetItemDefaultFocus();
					}

					ImGui::EndListBox();
				}
			}

			if (modified)
				ImGui::CloseCurrentPopup();

			UI::EndPopup();
		}

		return modified;
	}

	std::pair<bool, std::string> AssetValidityAndName(AssetHandle handle, const PropertyAssetReferenceSettings& settings)
	{
		std::string name = "Null";

		bool valid = AssetManager::IsAssetHandleValid(handle);
		if (valid)
		{
			if (settings.ShowFulLFilePath)
				name = Project::GetEditorAssetManager()->GetMetaData(handle).FilePath.string();
			else
				name = Project::GetEditorAssetManager()->GetMetaData(handle).FilePath.stem().string();

			if (AssetManager::IsAssetMissing(handle))
			{
				valid = false;
				name += "(Missing)";
			}
		}

		return { valid, name };
	}

}