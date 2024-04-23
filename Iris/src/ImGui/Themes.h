#pragma once

#include <cmath>
#include <imgui/imgui.h>

namespace Colors
{
	inline static float Convert_sRGB_FromLinear(float theLinearValue)
	{
		return theLinearValue <= 0.0031308f
			? theLinearValue * 12.92f
			: std::powf(theLinearValue, 1.0f / 2.2f) * 1.055f - 0.055f;
	}

	inline static float Convert_sRGB_ToLinear(float thesRGBValue)
	{
		return thesRGBValue <= 0.04045f
			? thesRGBValue / 12.92f
			: std::powf((thesRGBValue + 0.055f) / 1.055f, 2.2f);
	}

	inline static ImVec4 ConvertFromSRGB(ImVec4 colour)
	{
		return ImVec4(Convert_sRGB_FromLinear(colour.x),
			Convert_sRGB_FromLinear(colour.y),
			Convert_sRGB_FromLinear(colour.z),
			colour.w);
	}

	inline static ImVec4 ConvertToSRGB(ImVec4 colour)
	{
		return ImVec4(std::pow(colour.x, 2.2f),
			std::pow(colour.y, 2.2f),
			std::pow(colour.z, 2.2f),
			colour.w);
	}

	// To experiment with editor theme live you can change these constexpr into static
	// members of a static "Theme" class and add a quick ImGui window to adjust the colour values
	namespace Theme {
		constexpr auto Accent				= IM_COL32(36, 158, 236, 255);
		constexpr auto AccentOrange 		= IM_COL32(236, 158, 36, 255);
		constexpr auto Highlight			= IM_COL32(39, 185, 242, 255);
		constexpr auto NiceBlue				= IM_COL32(83, 232, 254, 255);
		constexpr auto Compliment			= IM_COL32(78, 151, 166, 255);
		constexpr auto Background			= IM_COL32(36, 36, 36, 255);
		constexpr auto BackgroundDark		= IM_COL32(26, 26, 26, 255);
		constexpr auto BackgroundDarkX		= IM_COL32(10, 10, 10, 255);
		constexpr auto Titlebar				= IM_COL32(21, 21, 21, 255);
		constexpr auto TitlebarOrange		= IM_COL32(186, 66, 30, 255);
		constexpr auto TitlebarGreen		= IM_COL32(18, 88, 30, 255);
		constexpr auto TitlebarCyan			= IM_COL32(20, 125, 106, 255);
		constexpr auto TitlebarRed			= IM_COL32(185, 30, 30, 255);
		constexpr auto PropertyField		= IM_COL32(15, 15, 15, 255);
		constexpr auto Text					= IM_COL32(192, 192, 192, 255);
		constexpr auto TextBrighter			= IM_COL32(210, 210, 210, 255);
		constexpr auto TextDarker			= IM_COL32(128, 128, 128, 255);
		constexpr auto TextError			= IM_COL32(230, 51, 51, 255);
		constexpr auto TextSuccess			= IM_COL32(118, 185, 0, 255);
		constexpr auto Muted				= IM_COL32(77, 77, 77, 255);
		constexpr auto GroupHeader			= IM_COL32(47, 47, 47, 255);
		constexpr auto Selection			= IM_COL32(20, 125, 106, 255);
		constexpr auto SelectionMuted       = IM_COL32(20, 155, 136, 23);
		constexpr auto SelectionOrange		= IM_COL32(237, 172, 109, 255);
		constexpr auto SelectionOrangeMuted = IM_COL32(237, 201, 142, 23);
		constexpr auto BackgroundPopup		= IM_COL32(50, 50, 50, 255);
		constexpr auto ValidPrefab			= IM_COL32(82, 179, 222, 255);
		constexpr auto InvalidPrefab		= IM_COL32(222, 43, 43, 255);
		constexpr auto MissingMesh			= IM_COL32(230, 102, 76, 255);
		constexpr auto MeshNotSet			= IM_COL32(250, 101, 23, 255);
	}
}