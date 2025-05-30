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
		virtual void OnEvent(Events::Event& e) { (void)e; }
		virtual void SetSceneContext(const Ref<Scene>& context) { (void)context; }
		virtual void OnProjectChanged(const Ref<Project>& project) { (void)project; }
	};

}