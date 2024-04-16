#pragma once

#include "Core/Base.h"
#include "Core/Events/KeyEvents.h"
#include "Core/Events/MouseEvents.h"
#include "Core/Layer.h"
#include "Editor/EditorCamera.h"
#include "Scene/Scene.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer/Renderer2D.h"

#include <imgui/imgui_internal.h>

namespace Iris {

	class RuntimeLayer : public Layer
	{
	public:
		RuntimeLayer();
		virtual ~RuntimeLayer() override;

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(TimeStep ts) override;
		virtual void OnImGuiRender() override {}
		virtual void OnEvent(Events::Event& e) override;

		void OnRender2D();

		bool OnKeyPressed(Events::KeyPressedEvent& e);
		bool OnMouseButtonPressed(Events::MouseButtonPressedEvent& e);

	private:
		Ref<Scene> m_RuntimeScene;
		Ref<SceneRenderer> m_ViewportRenderer;
		Ref<Renderer2D> m_Renderer2D;

		EditorCamera m_EditorCamera;

		float m_LineWidth = 2.0f;
		uint32_t m_Width = 1280;
		uint32_t m_Height = 920;

		bool m_AllowViewportCameraEvents = true;

		Ref<RenderPass> m_SwapChainRP;
		Ref<Material> m_SwapChainMaterial;
		Ref<RenderCommandBuffer> m_CommandBuffer;

	};

}