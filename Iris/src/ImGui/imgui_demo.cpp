// dear imgui, v1.88
// (demo code)

// Help:
// - Read FAQ at http://dearimgui.org/faq
// - Newcomers, read 'Programmer guide' in imgui.cpp for notes on how to setup Dear ImGui in your codebase.
// - Call and read ImGui::ShowDemoWindow() in imgui_demo.cpp. All applications in examples/ are doing that.
// Read imgui.cpp for more details, documentation and comments.
// Get the latest version at https://github.com/ocornut/imgui

#include "ImGui/ImGuiUtils.h"

#ifndef IMGUI_DISABLE

// Visual Studio warnings
#ifdef _MSC_VER
#pragma warning (disable: 4127)     // condition expression is constant
#pragma warning (disable: 4996)     // 'This function or variable may be unsafe': strcpy, strdup, sprintf, vsnprintf, sscanf, fopen
#pragma warning (disable: 26451)    // [Static Analyzer] Arithmetic overflow : Using operator 'xxx' on a 4 byte value and then casting the result to a 8 byte value. Cast the value to the wider type before calling operator 'xxx' to avoid overflow(io.2).
#endif

// Play it nice with Windows users (Update: May 2018, Notepad now supports Unix-style carriage returns!)
#ifdef _WIN32
#define IM_NEWLINE  "\r\n"
#else
#define IM_NEWLINE  "\n"
#endif

//-----------------------------------------------------------------------------
// [SECTION] Forward Declarations, Helpers
//-----------------------------------------------------------------------------

// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
static void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void ImGui::ShowStyleEditor(ImGuiStyle* ref, int* outThemeID)
{
    using namespace Iris;
    // You can pass in a reference ImGuiStyle structure to compare to, revert to and save to
    // (without a reference style pointer, we will use one compared locally as a reference)
    ImGuiStyle& style = ImGui::GetStyle();
    // NOTE: These are useless since we are no longer using persistant storage to export/import imgui styles
    //static ImGuiStyle ref_saved_style;

    //// Default to using internal storage as reference
    //static bool init = true;
    //if (init && ref == NULL)
    //    ref_saved_style = style;
    //init = false;
    //if (ref == NULL)
    //    ref = &ref_saved_style;
    if (ref == nullptr)
        ref = &style;

    UI::BeginPropertyGrid(2);

    static const char* availableThemes[] = { "Iris", "Dark", "Light", "Classic" };
    static int selectedThemeStyle = *outThemeID;
    if (UI::PropertyDropdown("Color Theme", availableThemes, 4, &selectedThemeStyle))
    {
        switch (selectedThemeStyle)
        {
            case 0: Application::Get().GetImGuiLayer()->SetDarkThemeColors(); break;
            case 1: ImGui::StyleColorsDark(); break;
            case 2: ImGui::StyleColorsLight(); break;
            case 3: ImGui::StyleColorsClassic(); break;
        }
    }
    *outThemeID = selectedThemeStyle; // Set the theme id to be used in serialization

    // Simplified Settings (expose floating-pointer border sizes as boolean representing 0.0f or 1.0f)
    if (UI::PropertySlider("FrameRounding", style.FrameRounding, 0.0f, 12.0f, "%.0f"))
        style.GrabRounding = style.FrameRounding; // Make GrabRounding always the same value as FrameRounding

    bool border = (style.WindowBorderSize > 0.0f);
    if (UI::Property("WindowBorder", border))
        style.WindowBorderSize = border ? 1.0f : 0.0f;
    border = (style.FrameBorderSize > 0.0f);
    if (UI::Property("FrameBorder", border))
        style.FrameBorderSize = border ? 1.0f : 0.0f;
    border = (style.PopupBorderSize > 0.0f);
    if (UI::Property("PopupBorder", border))
        style.PopupBorderSize = border ? 1.0f : 0.0f;

    // Save/Revert button
    //ImGuiUtils::ShiftCursor(10.0f, 9.0f);
    //ImGui::Text("Save/Revert");
    //ImGui::SameLine();
    //HelpMarker(
    //    "Save/Revert in local non-persistent storage. Default Colors definition are not affected. "
    //    "Use \"Export\" below to save them somewhere.");

    //ImGui::NextColumn();

    //if (ImGui::Button("Save Ref"))
    //    *ref = ref_saved_style = style;
    //ImGui::SameLine();
    //if (ImGui::Button("Revert Ref"))
    //    style = *ref;

    UI::EndPropertyGrid();
    ImGui::Separator();

    constexpr ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Sizes"))
        {
            if (ImGui::TreeNodeEx("Main", treeNodeFlags))
            {
                UI::BeginPropertyGrid(2);

                UI::PropertySlider("WindowPadding", style.WindowPadding, 0.0f, 20.0f, "%.0f");
                UI::PropertySlider("FramePadding", style.FramePadding, 0.0f, 20.0f, "%.0f");
                UI::PropertySlider("CellPadding", style.CellPadding, 0.0f, 20.0f, "%.0f");
                UI::PropertySlider("ItemSpacing", style.ItemSpacing, 0.0f, 20.0f, "%.0f");
                UI::PropertySlider("ItemInnerSpacing", style.ItemInnerSpacing, 0.0f, 20.0f, "%.0f");
                UI::PropertySlider("TouchExtraPadding", style.TouchExtraPadding, 0.0f, 10.0f, "%.0f");
                UI::PropertySlider("IndentSpacing", style.IndentSpacing, 0.0f, 30.0f, "%.0f");
                UI::PropertySlider("ScrollbarSize", style.ScrollbarSize, 1.0f, 20.0f, "%.0f");
                UI::PropertySlider("GrabMinSize", style.GrabMinSize, 1.0f, 20.0f, "%.0f");

                UI::EndPropertyGrid();
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Borders", treeNodeFlags))
            {
                UI::BeginPropertyGrid(2);

                UI::PropertySlider("WindowBorderSize", style.WindowBorderSize, 0.0f, 1.0f, "%.0f");
                UI::PropertySlider("ChildBorderSize", style.ChildBorderSize, 0.0f, 1.0f, "%.0f");
                UI::PropertySlider("PopupBorderSize", style.PopupBorderSize, 0.0f, 1.0f, "%.0f");
                UI::PropertySlider("FrameBorderSize", style.FrameBorderSize, 0.0f, 1.0f, "%.0f");
                UI::PropertySlider("TabBorderSize", style.TabBorderSize, 0.0f, 1.0f, "%.0f");

                UI::EndPropertyGrid();
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Rounding", treeNodeFlags))
            {
                UI::BeginPropertyGrid(2);

                UI::PropertySlider("WindowRounding", style.WindowRounding, 0.0f, 12.0f, "%.0f");
                UI::PropertySlider("ChildRounding", style.ChildRounding, 0.0f, 12.0f, "%.0f");
                UI::PropertySlider("FrameRounding", style.FrameRounding, 0.0f, 12.0f, "%.0f");
                UI::PropertySlider("PopupRounding", style.PopupRounding, 0.0f, 12.0f, "%.0f");
                UI::PropertySlider("ScrollbarRounding", style.ScrollbarRounding, 0.0f, 12.0f, "%.0f");
                UI::PropertySlider("GrabRounding", style.GrabRounding, 0.0f, 12.0f, "%.0f");
                UI::PropertySlider("LogSliderDeadzone", style.LogSliderDeadzone, 0.0f, 12.0f, "%.0f");
                UI::PropertySlider("TabRounding", style.TabRounding, 0.0f, 12.0f, "%.0f");

                UI::EndPropertyGrid();
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Alignment", treeNodeFlags))
            {
                UI::BeginPropertyGrid(2);

                UI::PropertySlider("WindowTitleAlign", style.WindowTitleAlign, 0.0f, 1.0f, "%.2f");

                int window_menu_button_position = style.WindowMenuButtonPosition + 1;
                static const char* windowMenuButtonPos[] = { "None", "Left", "Right" };
                if (UI::PropertyDropdown("WindowMenuButtonPosition", windowMenuButtonPos, 3, &window_menu_button_position))
                {
                    style.WindowMenuButtonPosition = window_menu_button_position - 1;
                }

                static const char* colorButtonPos[] = { "Left", "Right" };
                UI::PropertyDropdown("ColorButtonPosition", colorButtonPos, 2, &style.ColorButtonPosition);
                UI::PropertySlider("ButtonTextAlign", style.ButtonTextAlign, 0.0f, 1.0f, "%.2f", "Alignment applies when a button is larger than its text content.");
                UI::PropertySlider("SelectableTextAlign", style.SelectableTextAlign, 0.0f, 1.0f, "%.2f", "Alignment applies when a selectable is larger than its text content.");

                UI::EndPropertyGrid();
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Safe Area Padding", treeNodeFlags))
            {
                ImGui::SameLine(); HelpMarker("Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured).");
                UI::BeginPropertyGrid(2, 200.0f);

                UI::PropertySlider("DisplaySafeAreaPadding", style.DisplaySafeAreaPadding, 0.0f, 30.0f, "%.0f");

                UI::EndPropertyGrid();
                ImGui::TreePop();
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Rendering"))
        {
            UI::BeginPropertyGrid(2, 250.0f);

            UI::Property("Anti-aliased Lines", style.AntiAliasedLines, "When disabling anti-aliasing lines, you'll probably want to disable borders in your style as well.");
            UI::Property("Anti-aliased Lines use texture", style.AntiAliasedLinesUseTex, "Faster lines using texture data. Require backend to render with bilinear filtering (not point/nearest filtering).");
            UI::Property("Anti-aliased fill", style.AntiAliasedFill);
            UI::Property("Curve Tessellation Tolerance", style.CurveTessellationTol, 0.02f, 0.10f, 10.0f);
            if (style.CurveTessellationTol < 0.10f) style.CurveTessellationTol = 0.10f;

            // When editing the "Circle Segment Max Error" value, draw a preview of its effect on auto-tessellated circles.
            UI::Property("Circle Tessellation Max Error", style.CircleTessellationMaxError, 0.005f, 0.10f, 5.0f, "When drawing circle primitives with \"num_segments == 0\" tesselation will be calculated automatically.");
            if (ImGui::IsItemActive())
            {
                ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
                ImGui::BeginTooltip();
                ImGui::TextUnformatted("(R = radius, N = number of segments)");
                ImGui::Spacing();
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                const float min_widget_width = ImGui::CalcTextSize("N: MMM\nR: MMM").x;
                for (int n = 0; n < 8; n++)
                {
                    const float RAD_MIN = 5.0f;
                    const float RAD_MAX = 70.0f;
                    const float rad = RAD_MIN + (RAD_MAX - RAD_MIN) * (float)n / (8.0f - 1.0f);

                    ImGui::BeginGroup();

                    ImGui::Text("R: %.f\nN: %d", rad, draw_list->_CalcCircleAutoSegmentCount(rad));

                    const float canvas_width = std::max(min_widget_width, rad * 2.0f);
                    const float offset_x = floorf(canvas_width * 0.5f);
                    const float offset_y = floorf(RAD_MAX);

                    const ImVec2 p1 = ImGui::GetCursorScreenPos();
                    draw_list->AddCircle(ImVec2(p1.x + offset_x, p1.y + offset_y), rad, ImGui::GetColorU32(ImGuiCol_Text));
                    ImGui::Dummy(ImVec2(canvas_width, RAD_MAX * 2));

                    /*
                    const ImVec2 p2 = ImGui::GetCursorScreenPos();
                    draw_list->AddCircleFilled(ImVec2(p2.x + offset_x, p2.y + offset_y), rad, ImGui::GetColorU32(ImGuiCol_Text));
                    ImGui::Dummy(ImVec2(canvas_width, RAD_MAX * 2));
                    */

                    ImGui::EndGroup();
                    ImGui::SameLine();
                }
                ImGui::EndTooltip();
            }

            UI::Property("Global Alpha", style.Alpha, 0.005f, 0.20f, 1.0f); // Not exposing zero here so user doesn't "lose" the UI (zero alpha clips all widgets). But application code could have a toggle to switch between zero and non-zero.
            UI::Property("Disabled Alpha", style.DisabledAlpha, 0.005f, 0.0f, 1.0f, "Additional alpha multiplier for disabled items (multiply over current value of Alpha).");

            UI::EndPropertyGrid();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Colors"))
        {
            ImGui::TextUnformatted("View the color values of different fields of the currently applied theme.");

            //static int output_dest = 0;
            //static bool output_only_modified = true;
            //if (ImGui::Button("Export"))
            //{
            //    if (output_dest == 0)
            //        ImGui::LogToClipboard();
            //    else
            //        ImGui::LogToTTY();
            //    ImGui::LogText("ImVec4* colors = ImGui::GetStyle().Colors;" IM_NEWLINE);
            //    for (int i = 0; i < ImGuiCol_COUNT; i++)
            //    {
            //        const ImVec4& col = style.Colors[i];
            //        const char* name = ImGui::GetStyleColorName(i);
            //        if (!output_only_modified || memcmp(&col, &ref->Colors[i], sizeof(ImVec4)) != 0)
            //            ImGui::LogText("colors[ImGuiCol_%s]%*s= ImVec4(%.2ff, %.2ff, %.2ff, %.2ff);" IM_NEWLINE,
            //                name, 23 - (int)strlen(name), "", col.x, col.y, col.z, col.w);
            //    }
            //    ImGui::LogFinish();
            //}
            //ImGui::SameLine(); ImGui::SetNextItemWidth(120); ImGui::Combo("##output_type", &output_dest, "To Clipboard\0To TTY\0");
            //ImGui::SameLine(); ImGui::Checkbox("Only Modified Colors", &output_only_modified);

            //static ImGuiTextFilter filter;
            //filter.Draw("Filter colors", ImGui::GetFontSize() * 16);

            // TODO: Needs work...
            //static std::string searchString;
            //UI::SearchBox(searchString, "Search for specific values...");

            ImGui::Spacing();
            ImGui::Spacing();

            //static ImGuiColorEditFlags alpha_flags = 0;
            //if (ImGui::RadioButton("Opaque", alpha_flags == ImGuiColorEditFlags_None))             { alpha_flags = ImGuiColorEditFlags_None; } ImGui::SameLine();
            //if (ImGui::RadioButton("Alpha",  alpha_flags == ImGuiColorEditFlags_AlphaPreview))     { alpha_flags = ImGuiColorEditFlags_AlphaPreview; } ImGui::SameLine();
            //if (ImGui::RadioButton("Both",   alpha_flags == ImGuiColorEditFlags_AlphaPreviewHalf)) { alpha_flags = ImGuiColorEditFlags_AlphaPreviewHalf; } ImGui::SameLine();
            //HelpMarker(
            //    "In the color list:\n"
            //    "Left-click on color square to open color picker,\n"
            //    "Right-click to open edit options menu.");

            ImGui::BeginChild("##colors", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NavFlattened);
            ImGui::PushItemWidth(-160);
            UI::BeginPropertyGrid(2);
            for (int i = 0; i < ImGuiCol_COUNT; i++)
            {
                const char* name = ImGui::GetStyleColorName(i);
                //if (!filter.PassFilter(name))
                //    continue;

                // TODO: Needs work
                //if (!UI::IsMatchingSearch(name, searchString))
                    //continue;

                ImGui::PushID(i);

                glm::vec4 styleColors = { style.Colors[i].x, style.Colors[i].y, style.Colors[i].z, style.Colors[i].w };
                UI::PropertyColor4(name, styleColors);
                //ImGui::ColorEdit4("##color", (float*)&style.Colors[i], ImGuiColorEditFlags_AlphaBar | alpha_flags);
                if (memcmp(&style.Colors[i], &ref->Colors[i], sizeof(ImVec4)) != 0)
                {
                    // Tips: in a real user application, you may want to merge and use an icon font into the main font,
                    // so instead of "Save"/"Revert" you'd use icons!
                    // Read the FAQ and docs/FONTS.md about using icon fonts. It's really easy and super convenient!
                    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x); if (ImGui::Button("Save")) { ref->Colors[i] = style.Colors[i]; }
                    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x); if (ImGui::Button("Revert")) { style.Colors[i] = ref->Colors[i]; }
                }

                ImGui::PopID();
            }
            UI::EndPropertyGrid();
            ImGui::PopItemWidth();
            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        //if (ImGui::BeginTabItem("Fonts"))
        //{
        //    ImGuiIO& io = ImGui::GetIO();
        //    ImFontAtlas* atlas = io.Fonts;
        //    HelpMarker("Read FAQ and docs/FONTS.md for details on font loading.");
        //    ImGui::ShowFontAtlas(atlas);

        //    // Post-baking font scaling. Note that this is NOT the nice way of scaling fonts, read below.
        //    // (we enforce hard clamping manually as by default DragFloat/SliderFloat allows CTRL+Click text to get out of bounds).
        //    const float MIN_SCALE = 0.3f;
        //    const float MAX_SCALE = 2.0f;
        //    HelpMarker(
        //        "Those are old settings provided for convenience.\n"
        //        "However, the _correct_ way of scaling your UI is currently to reload your font at the designed size, "
        //        "rebuild the font atlas, and call style.ScaleAllSizes() on a reference ImGuiStyle structure.\n"
        //        "Using those settings here will give you poor quality results.");
        //    static float window_scale = 1.0f;
        //    ImGui::PushItemWidth(ImGui::GetFontSize() * 8);
        //    if (ImGui::DragFloat("window scale", &window_scale, 0.005f, MIN_SCALE, MAX_SCALE, "%.2f", ImGuiSliderFlags_AlwaysClamp)) // Scale only this window
        //        ImGui::SetWindowFontScale(window_scale);
        //    ImGui::DragFloat("global scale", &io.FontGlobalScale, 0.005f, MIN_SCALE, MAX_SCALE, "%.2f", ImGuiSliderFlags_AlwaysClamp); // Scale everything
        //    ImGui::PopItemWidth();

        //    ImGui::EndTabItem();
        //}

        ImGui::EndTabBar();
    }

    //ImGui::PopItemWidth();
    //ImGuiUtils::EndPropertyGrid();
}

#endif