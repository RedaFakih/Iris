#pragma once

#include "Editor/EditorPanel.h"
#include "Scene/Scene.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

namespace Iris {

	class SceneHierarchyPanel : public EditorPanel
	{
	public:
		
		[[nodiscard]] static Ref<SceneHierarchyPanel> Create(Ref<Scene>);

		virtual void OnImGuiRender(bool& isOpen) override {}

	private:

	};

}