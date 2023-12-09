#pragma once

#include "Core/Base.h"
#include "Core/Layer.h"
#include "Editor/EditorCamera.h"

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

	private:
		EditorCamera m_EditorCamera;

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