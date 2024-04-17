#pragma once

#include "Core/Events/KeyEvents.h"
#include "Editor/EditorPanel.h"

namespace Iris {

	class ShadersPanel : public EditorPanel
	{
	public:
		ShadersPanel() = default;
		~ShadersPanel() = default;

		[[nodiscard]] static Ref<ShadersPanel> Create();

		virtual void OnImGuiRender(bool& isOpen) override;
		virtual void OnEvent(Events::Event& e) override;

	private:
		bool OnKeyPressed(Events::KeyPressedEvent& e);

	};

}