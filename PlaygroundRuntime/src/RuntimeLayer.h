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

namespace vkPlayground {

	// TODO: TEMPORARY
	class UniformBufferSet;
	class RenderPass;
	class Material;
	class VertexBuffer;
	class IndexBuffer;
	class RenderCommandBuffer;

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

		bool OnKeyPressed(Events::KeyPressedEvent& e);
		bool OnMouseButtonPressed(Events::MouseButtonPressedEvent& e);

	private:
		Ref<Scene> m_EditorScene;
		Ref<SceneRenderer> m_ViewportRenderer;

		EditorCamera m_EditorCamera;

		float m_LineWidth = 2.0f;

		bool m_AllowViewportCameraEvents = true;

		// TODO: TEMP
		Ref<Renderer2D> m_Renderer2D;
		Ref<RenderCommandBuffer> m_CommandBuffer;
		Ref<UniformBufferSet> m_UniformBufferSet;
		Ref<RenderPass> m_RenderingPass;
		Ref<RenderPass> m_ScreenPass;
		Ref<Material> m_ScreenPassMaterial;
		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;
		Ref<Framebuffer> m_IntermediateBuffer;

	};

}