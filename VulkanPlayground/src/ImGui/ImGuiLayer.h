#pragma once

#include "Core/Layer.h"

namespace vkPlayground {

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
	};

}