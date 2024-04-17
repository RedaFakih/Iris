#pragma once

#include "Core/Base.h"
#include "Core/Events/Events.h"

namespace Iris {

	class Scene;

	class EditorPanel : public RefCountedObject
	{
	public:
		virtual ~EditorPanel() = default;

		virtual void OnImGuiRender(bool& isOpen) = 0;
		virtual void OnEvent(Events::Event& e) {}
		virtual void SetSceneContext(Ref<Scene> context) {}
	};

}