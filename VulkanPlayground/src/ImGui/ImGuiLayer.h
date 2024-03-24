#pragma once

#include "Core/Layer.h"
#include "ImGuiFonts.h"

namespace vkPlayground {

	class ImGuiFontsLibrary;

	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer();

		static ImGuiLayer* Create();

		void OnAttach() override;
		void OnDetach() override;
		void OnImGuiRender() override {}

		void Begin();
		void End();

		void SetDarkThemeColors();

		ImGuiFontsLibrary& GetFontsLibrary() { return m_FontsLibrary; }
		const ImGuiFontsLibrary& GetFontsLibrary() const { return m_FontsLibrary; }

	private:
		void LoadFonts();

	private:
		ImGuiFontsLibrary m_FontsLibrary;

	};

}