#pragma once

#include "Core/Events/KeyEvents.h"
#include "Core/Events/MouseEvents.h"
#include "Scene/Scene.h"
#include "Project/Project.h"
#include "Editor/EditorPanel.h"

namespace Iris {

	/*
	 * TODO: For now this is just like this for a quick implementation
	 * Needs a WHOLE re-write
	 */

	class ContentBrowserPanel : public EditorPanel	
	{
	public:
		ContentBrowserPanel();
		~ContentBrowserPanel() = default;

		[[nodiscard]] inline static Ref<ContentBrowserPanel> Create()
		{
			return CreateRef<ContentBrowserPanel>();
		}

		virtual void OnImGuiRender(bool& isOpen) override;

		virtual void OnEvent(Events::Event& e) override;
		virtual void OnProjectChanged(const Ref<Project>& project) override;
		virtual void SetSceneContext(const Ref<Scene>& context) override { m_SceneContext = context; }

	private:
		bool OnKeyPressed(Events::KeyPressedEvent& e);
		bool OnMouseButtonPressed(Events::MouseButtonPressedEvent& e);

	private:
		Ref<Project> m_Project;
		Ref<Scene> m_SceneContext;

		std::filesystem::path m_BaseDirectory;
		std::filesystem::path m_CurrentDirectory;

		bool m_IsContentBrowserHovered = false;
		bool m_IsContentBrowserFocused = false;

		inline static ContentBrowserPanel* s_Instance;
	};

}