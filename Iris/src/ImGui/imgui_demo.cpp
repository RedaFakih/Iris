// dear imgui, v1.88
// (demo code)

// Help:
// - Read FAQ at http://dearimgui.org/faq
// - Newcomers, read 'Programmer guide' in imgui.cpp for notes on how to setup Dear ImGui in your codebase.
// - Call and read ImGui::ShowDemoWindow() in imgui_demo.cpp. All applications in examples/ are doing that.
// Read imgui.cpp for more details, documentation and comments.
// Get the latest version at https://github.com/ocornut/imgui

// Message to the person tempted to delete this file when integrating Dear ImGui into their codebase:
// Do NOT remove this file from your project! Think again! It is the most useful reference code that you and other
// coders will want to refer to and call. Have the ImGui::ShowDemoWindow() function wired in an always-available
// debug menu of your game/app! Removing this file from your project is hindering access to documentation for everyone
// in your team, likely leading you to poorer usage of the library.
// Everything in this file will be stripped out by the linker if you don't call ImGui::ShowDemoWindow().
// If you want to link core Dear ImGui in your shipped builds but want a thorough guarantee that the demo will not be
// linked, you can setup your imconfig.h with #define IMGUI_DISABLE_DEMO_WINDOWS and those functions will be empty.
// In another situation, whenever you have Dear ImGui available you probably want this to be available for reference.
// Thank you,
// -Your beloved friend, imgui_demo.cpp (which you won't delete)

// Message to beginner C/C++ programmers about the meaning of the 'static' keyword:
// In this demo code, we frequently use 'static' variables inside functions. A static variable persists across calls,
// so it is essentially like a global variable but declared inside the scope of the function. We do this as a way to
// gather code and data in the same place, to make the demo source code faster to read, faster to write, and smaller
// in size. It also happens to be a convenient way of storing simple UI related information as long as your function
// doesn't need to be reentrant or used in multiple threads. This might be a pattern you will want to use in your code,
// but most of the real data you would be editing is likely going to be stored outside your functions.

// The Demo code in this file is designed to be easy to copy-and-paste into your application!
// Because of this:
// - We never omit the ImGui:: prefix when calling functions, even though most code here is in the same namespace.
// - We try to declare static variables in the local scope, as close as possible to the code using them.
// - We never use any of the helpers/facilities used internally by Dear ImGui, unless available in the public API.
// - We never use maths operators on ImVec2/ImVec4. For our other sources files we use them, and they are provided
//   by imgui_internal.h using the IMGUI_DEFINE_MATH_OPERATORS define. For your own sources file they are optional
//   and require you either enable those, either provide your own via IM_VEC2_CLASS_EXTRA in imconfig.h.
//   Because we can't assume anything about your support of maths operators, we cannot use them in imgui_demo.cpp.

// Navigating this file:
// - In Visual Studio IDE: CTRL+comma ("Edit.GoToAll") can follow symbols in comments, whereas CTRL+F12 ("Edit.GoToImplementation") cannot.
// - With Visual Assist installed: ALT+G ("VAssistX.GoToImplementation") can also follow symbols in comments.

/*

Index of this file:

// [SECTION] Forward Declarations, Helpers
// [SECTION] Demo Window / ShowDemoWindow()
// - sub section: ShowDemoWindowWidgets()
// - sub section: ShowDemoWindowLayout()
// - sub section: ShowDemoWindowPopups()
// - sub section: ShowDemoWindowTables()
// - sub section: ShowDemoWindowMisc()
// [SECTION] About Window / ShowAboutWindow()
// [SECTION] Style Editor / ShowStyleEditor()
// [SECTION] Example App: Main Menu Bar / ShowExampleAppMainMenuBar()
// [SECTION] Example App: Debug Console / ShowExampleAppConsole()
// [SECTION] Example App: Debug Log / ShowExampleAppLog()
// [SECTION] Example App: Simple Layout / ShowExampleAppLayout()
// [SECTION] Example App: Property Editor / ShowExampleAppPropertyEditor()
// [SECTION] Example App: Long Text / ShowExampleAppLongText()
// [SECTION] Example App: Auto Resize / ShowExampleAppAutoResize()
// [SECTION] Example App: Constrained Resize / ShowExampleAppConstrainedResize()
// [SECTION] Example App: Simple overlay / ShowExampleAppSimpleOverlay()
// [SECTION] Example App: Fullscreen window / ShowExampleAppFullscreen()
// [SECTION] Example App: Manipulating window titles / ShowExampleAppWindowTitles()
// [SECTION] Example App: Custom Rendering using ImDrawList API / ShowExampleAppCustomRendering()
// [SECTION] Example App: Documents Handling / ShowExampleAppDocuments()

*/

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <imgui/imgui.h>

#include "Core/Application.h"
#include "ImGui/ImGuiUtils.h"

#ifndef IMGUI_DISABLE

// System includes
#include <ctype.h>          // toupper
#include <limits.h>         // INT_MIN, INT_MAX
#include <math.h>           // sqrtf, powf, cosf, sinf, floorf, ceilf
#include <stdio.h>          // vsnprintf, sscanf, printf
#include <stdlib.h>         // NULL, malloc, free, atoi
#if defined(_MSC_VER) && _MSC_VER <= 1500 // MSVC 2008 or earlier
#include <stddef.h>         // intptr_t
#else
#include <stdint.h>         // intptr_t
#endif

// Visual Studio warnings
#ifdef _MSC_VER
#pragma warning (disable: 4127)     // condition expression is constant
#pragma warning (disable: 4996)     // 'This function or variable may be unsafe': strcpy, strdup, sprintf, vsnprintf, sscanf, fopen
#pragma warning (disable: 26451)    // [Static Analyzer] Arithmetic overflow : Using operator 'xxx' on a 4 byte value and then casting the result to a 8 byte value. Cast the value to the wider type before calling operator 'xxx' to avoid overflow(io.2).
#endif

// Clang/GCC warnings with -Weverything
#if defined(__clang__)
#if __has_warning("-Wunknown-warning-option")
#pragma clang diagnostic ignored "-Wunknown-warning-option"         // warning: unknown warning group 'xxx'                     // not all warnings are known by all Clang versions and they tend to be rename-happy.. so ignoring warnings triggers new warnings on some configuration. Great!
#endif
#pragma clang diagnostic ignored "-Wunknown-pragmas"                // warning: unknown warning group 'xxx'
#pragma clang diagnostic ignored "-Wold-style-cast"                 // warning: use of old-style cast                           // yes, they are more terse.
#pragma clang diagnostic ignored "-Wdeprecated-declarations"        // warning: 'xx' is deprecated: The POSIX name for this..   // for strdup used in demo code (so user can copy & paste the code)
#pragma clang diagnostic ignored "-Wint-to-void-pointer-cast"       // warning: cast to 'void *' from smaller integer type
#pragma clang diagnostic ignored "-Wformat-security"                // warning: format string is not a string literal
#pragma clang diagnostic ignored "-Wexit-time-destructors"          // warning: declaration requires an exit-time destructor    // exit-time destruction order is undefined. if MemFree() leads to users code that has been disabled before exit it might cause problems. ImGui coding style welcomes static/globals.
#pragma clang diagnostic ignored "-Wunused-macros"                  // warning: macro is not used                               // we define snprintf/vsnprintf on Windows so they are available, but not always used.
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"  // warning: zero as null pointer constant                   // some standard header variations use #define NULL 0
#pragma clang diagnostic ignored "-Wdouble-promotion"               // warning: implicit conversion from 'float' to 'double' when passing argument to function  // using printf() is a misery with this as C++ va_arg ellipsis changes float to double.
#pragma clang diagnostic ignored "-Wreserved-id-macro"              // warning: macro name is a reserved identifier
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"  // warning: implicit conversion from 'xxx' to 'float' may lose precision
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wpragmas"                  // warning: unknown option after '#pragma GCC diagnostic' kind
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"      // warning: cast to pointer from integer of different size
#pragma GCC diagnostic ignored "-Wformat-security"          // warning: format string is not a string literal (potentially insecure)
#pragma GCC diagnostic ignored "-Wdouble-promotion"         // warning: implicit conversion from 'float' to 'double' when passing argument to function
#pragma GCC diagnostic ignored "-Wconversion"               // warning: conversion to 'xxxx' from 'xxxx' may alter its value
#pragma GCC diagnostic ignored "-Wmisleading-indentation"   // [__GNUC__ >= 6] warning: this 'if' clause does not guard this statement      // GCC 6.0+ only. See #883 on GitHub.
#endif

// Play it nice with Windows users (Update: May 2018, Notepad now supports Unix-style carriage returns!)
#ifdef _WIN32
#define IM_NEWLINE  "\r\n"
#else
#define IM_NEWLINE  "\n"
#endif

// Helpers
#if defined(_MSC_VER) && !defined(snprintf)
#define snprintf    _snprintf
#endif
#if defined(_MSC_VER) && !defined(vsnprintf)
#define vsnprintf   _vsnprintf
#endif

// Format specifiers, printing 64-bit hasn't been decently standardized...
// In a real application you should be using PRId64 and PRIu64 from <inttypes.h> (non-windows) and on Windows define them yourself.
#ifdef _MSC_VER
#define IM_PRId64   "I64d"
#define IM_PRIu64   "I64u"
#else
#define IM_PRId64   "lld"
#define IM_PRIu64   "llu"
#endif

// Helpers macros
// We normally try to not use many helpers in imgui_demo.cpp in order to make code easier to copy and paste,
// but making an exception here as those are largely simplifying code...
// In other imgui sources we can use nicer internal functions from imgui_internal.h (ImMin/ImMax) but not in the demo.
#define IM_MIN(A, B)            (((A) < (B)) ? (A) : (B))
#define IM_MAX(A, B)            (((A) >= (B)) ? (A) : (B))
#define IM_CLAMP(V, MN, MX)     ((V) < (MN) ? (MN) : (V) > (MX) ? (MX) : (V))

// Enforce cdecl calling convention for functions called by the standard library, in case compilation settings changed the default to e.g. __vectorcall
#ifndef IMGUI_CDECL
#ifdef _MSC_VER
#define IMGUI_CDECL __cdecl
#else
#define IMGUI_CDECL
#endif
#endif

//-----------------------------------------------------------------------------
// [SECTION] Forward Declarations, Helpers
//-----------------------------------------------------------------------------

#if !defined(IMGUI_DISABLE_DEMO_WINDOWS)

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

                    const float canvas_width = IM_MAX(min_widget_width, rad * 2.0f);
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

#endif // #ifndef IMGUI_DISABLE
