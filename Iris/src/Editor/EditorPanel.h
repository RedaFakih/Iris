#pragma once

#include "Core/Base.h"
#include "Core/Events/Events.h"

namespace Iris {

	class Scene;
	class Project;

	class EditorPanel : public RefCountedObject
	{
	public:
		virtual ~EditorPanel() = default;

		virtual void OnImGuiRender(bool& isOpen) = 0;
		virtual void OnEvent(Events::Event& e) {}
		virtual void SetSceneContext(const Ref<Scene>& context) {}
		virtual void OnProjectChanged(Ref<Project> project) {}
	};

}