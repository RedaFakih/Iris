#pragma once

#include "Core/Base.h"
#include "Core/Events/KeyEvents.h"
#include "Core/Events/MouseEvents.h"
#include "Core/Layer.h"
#include "Editor/EditorCamera.h"
#include "Scene/Scene.h"
#include "Renderer/SceneRenderer.h"

#include <imgui/imgui_internal.h>

namespace vkPlayground {

	class UniformBufferSet;
	class RenderPass;
	class Material;
	class VertexBuffer;
	class IndexBuffer;
	class RenderCommandBuffer;

	class EditorLayer : public Layer
	{
	public:
		EditorLayer();
		virtual ~EditorLayer() override;

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(TimeStep ts) override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Events::Event& e) override;

		bool OnKeyPressed(Events::KeyPressedEvent& e);
		bool OnMouseButtonPressed(Events::MouseButtonPressedEvent& e);

	private:
		std::pair<float, float> GetMouseInViewportSpace() const;

	// Primary panels for editor
	private:
		void StartDocking();
		void EndDocking();
		//void ShowMenuBarItems();

		void ShowViewport();

	private:
		Ref<Scene> m_EditorScene;
		Ref<SceneRenderer> m_ViewportRenderer;

		EditorCamera m_EditorCamera;

		ImRect m_ViewportRect;
		ImVec2 m_ViewportSize = { 0.0f, 0.0f };

		float m_LineWidth = 2.0f;

		bool m_StartedCameraClickInViewport = false;

		bool m_ViewportPanelMouseOver = false;
		bool m_ViewportPanelFocused = false;
		bool m_AllowViewportCameraEvents = false;

		// TODO: TEMP
		Ref<RenderCommandBuffer> m_CommandBuffer;
		Ref<UniformBufferSet> m_UniformBufferSet;
		Ref<RenderPass> m_RenderingPass;
		Ref<RenderPass> m_ScreenPass;
		Ref<Material> m_ScreenPassMaterial;
		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;

	};

}