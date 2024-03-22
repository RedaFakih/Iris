#include "vkPch.h"
#include "ImGuiUtils.h"

#include "Core/Application.h"

#include <imgui/imgui_impl_vulkan.h>

#include <glm/gtc/type_ptr.hpp>
#include <spdlog/fmt/fmt.h>

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

namespace vkPlayground::UI {

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

	// Prefer using this over the ToolTip
	void ShowHelpMarker(const char* description)
	{
		ImGui::TextDisabled("(?)");
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

	bool ColorEdit3Control(const char* label, glm::vec3& color, bool showAsWheel)
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

	bool ColorEdit4Control(const char* label, glm::vec4& color, bool showAsWheel)
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

	bool IsItemDisabled()
	{
		return ImGui::GetItemFlags() & ImGuiItemFlags_Disabled;
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

	void BeginPropertyGrid(uint32_t columns, bool defaultWidth)
	{
		PushID();
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 8.0f, 8.0f });
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4.0f, 4.0f });
		ImGui::Columns(columns);
		if (defaultWidth)
			ImGui::SetColumnWidth(0, 140.0f);
	}

	void EndPropertyGrid()
	{
		ImGui::Columns(1);
		UnderLine();
		ImGui::PopStyleVar(2);
		ShiftCursorY(18.0f);
		PopID();
	}

	void Separator(ImVec2 size, ImVec4 color)
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, color);
		ImGui::BeginChild("sep", size);
		ImGui::EndChild();
		ImGui::PopStyleColor();
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

	bool PropertyFloat(const char* label, float& value, float delta, float min, float max, const char* helpText)
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

	bool PropertyBool(const char* label, bool& value, const char* helpText)
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

		// TODO: Maybe useless for the checkbox
		if (!IsItemDisabled())
			DrawItemActivityOutline();

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

		if (!IsItemDisabled())
			DrawItemActivityOutline();

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		UnderLine();

		return modified;
	}

	bool PropertySliderFloat(const char* label, float& value, float min, float max, const char* format, const char* helpText)
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

	bool PropertySliderFloat2(const char* label, ImVec2& value, float min, float max, const char* format, const char* helpText)
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

	void DrawItemActivityOutline(float rounding, bool drawWhenNotActive, ImColor colorWhenActive)
	{
		if (ImGui::IsItemDeactivated())
			return;

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImRect rect = RectExpanded(GetItemRect(), 1.0f, 1.0f);
		if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
		{
			drawList->AddRect(rect.Min, rect.Max, Colors::Theme::SelectionMuted, rounding, 0, 1.5f);
		}
		else if (ImGui::IsItemActive())
		{
			drawList->AddRect(rect.Min, rect.Max, colorWhenActive, rounding, 0, 1.5f);
		}
		else if (!ImGui::IsItemHovered() && drawWhenNotActive)
		{
			drawList->AddRect(rect.Min, rect.Max, Colors::Theme::BackgroundDark, rounding, 0, 1.5f);
		}
	}

	void UnderLine(bool fullWidth, float offsetX, float offsetY)
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
		ImGui::GetWindowDrawList()->AddLine(ImVec2{ cursor.x + offsetX, cursor.y + offsetY }, ImVec2{ cursor.x + width, cursor.y + offsetY }, Colors::Theme::BackgroundDark, 1.0f);

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

	void Image(const Ref<Texture2D>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		const auto textureID = GetTextureID(image);
		ImGui::Image(textureID, size, uv0, uv1, tint_col, border_col);
	}

}