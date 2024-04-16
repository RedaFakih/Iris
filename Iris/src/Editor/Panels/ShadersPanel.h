#pragma once

#include "Editor/EditorPanel.h"

namespace Iris {

	class ShadersPanel : public EditorPanel
	{
	public:

		[[nodiscard]] static Ref<ShadersPanel> Create();

		virtual void OnImGuiRender(bool& isOpen) override {}

	private:

	};

}