#pragma once

#include "Editor/AssetEditorPanel.h"
#include "Renderer/Mesh/MaterialAsset.h"
#include "Renderer/SceneRendererLite.h"

#include <imgui/imgui.h>

namespace Iris {

	class MaterialEditor : public AssetEditor
	{
	public:
		MaterialEditor();

		virtual void OnUpdate(TimeStep ts) override;
		virtual void OnEvent(Events::Event& e) override;
		virtual void SetAsset(const Ref<Asset>& asset) override;

	private:
		virtual void OnOpen() override;
		virtual void OnClose() override;
		virtual void Render() override;

		virtual ImGuiWindowFlags GetWindowFlags() override;
		virtual void OnWindowStylePush() override;
		virtual void OnWindowStylePop() override;

		void DrawDisplayImagePane();
		void DrawMaterialSettingsPane();

		bool OnKeyPressedEvent(Events::KeyPressedEvent& e);

	private:
		Ref<MaterialAsset> m_MaterialAsset;
		
		Ref<Scene> m_Scene;
		Ref<SceneRendererLite> m_SceneRenderer;
		EditorCamera m_Camera{ 45.0f, 1280.0f, 720.0f, 0.1f, 1000.0f };
		AssetHandle m_DisplayMeshHandle = 0; // TODO: Maybe allow the user to select what type of primitive mesh to display on?

		ImGuiWindowClass m_WindowClass;

		bool m_ViewportMouseOver = false;
		bool m_ViewportFocused = false;
		bool m_StartedCameraClickInViewport = false;
		bool m_AllowViewportCameraEvents = false;

	};

}