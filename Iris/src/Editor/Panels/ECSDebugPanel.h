#pragma once

#include "Editor/EditorPanel.h"

namespace Iris {

	class ECSDebugPanel : public EditorPanel
	{
	public:
		ECSDebugPanel(Ref<Scene> context);
		~ECSDebugPanel();

		[[nodiscard]] static Ref<ECSDebugPanel> Create(Ref<Scene> context);

		virtual void OnImGuiRender(bool& open) override;

		virtual void SetSceneContext(const Ref<Scene>& context) { m_Context = context; }
	private:
		Ref<Scene> m_Context;
	};

}