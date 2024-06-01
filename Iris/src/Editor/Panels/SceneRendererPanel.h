#pragma once

#include "Editor/EditorPanel.h"

namespace Iris {

	class SceneRenderer;

	class SceneRendererPanel : public EditorPanel
	{
	public:
		SceneRendererPanel() = default;
		~SceneRendererPanel() = default;

		[[nodiscard]] static Ref<SceneRendererPanel> Create()
		{
			return CreateRef<SceneRendererPanel>();
		}

		void SetRendererContext(const Ref<SceneRenderer>& renderer) { m_Context = renderer; }
		virtual void OnImGuiRender(bool& isOpen) override;

	private:
		Ref<SceneRenderer> m_Context = nullptr;

	};

}