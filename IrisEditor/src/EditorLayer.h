#pragma once

#include "Core/Base.h"
#include "Core/Events/AppEvents.h"
#include "Core/Events/KeyEvents.h"
#include "Core/Events/MouseEvents.h"
#include "Core/Layer.h"
#include "Editor/EditorCamera.h"
#include "Editor/PanelsManager.h"
#include "Project/Project.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Scene.h"

#include <imgui/imgui_internal.h>

namespace Iris {

	class EditorLayer : public Layer
	{
	public:
		EditorLayer();
		virtual ~EditorLayer() override;

		static EditorLayer* Get() { return s_EditorLayerInstance; }

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(TimeStep ts) override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Events::Event& e) override;

		void OnRender2D();

		bool OnKeyPressed(Events::KeyPressedEvent& e);
		bool OnMouseButtonPressed(Events::MouseButtonPressedEvent& e);
		bool OnTitleBarColorChange(Events::TitleBarColorChangeEvent& e);

		// Project
		void OpenProject();
		void OpenProject(std::filesystem::path& filePath);
		void CreateProject(std::filesystem::path projectPath);
		void EmptyProject();
		void SaveProject();
		void CloseProject(bool unloadProject = true);

		// Scene
		void NewScene(const std::string& name = "UntitledScene");
		bool OpenScene();
		bool OpenScene(const std::filesystem::path filePath);
		bool OpenScene(const AssetMetaData& metaData);
		void SaveScene();
		void SaveSceneAs();

		void SceneHierarchySetEditorCameraTransform(Entity entity);

	private:
		std::pair<float, float> GetMouseInViewportSpace() const;
		std::pair<glm::vec3, glm::vec3> CastRay(const EditorCamera& camera, float x, float y);

		void OnEntityDeleted(Entity e);

	// Primary panels for editor
	private:
		void UI_StartDocking();
		void UI_EndDocking();

		// Return title bar height
		float UI_DrawTitleBar();
		void UI_HandleManualWindowResize();
		bool UI_TitleBarHitTest(int x, int y) const;
		void UI_DrawMainMenuBar();
		float GetSnapValue();
		void UI_DrawGizmos();

		void DeleteEntity(Entity entity);

		void UI_ShowViewport();
		void UI_ShowShadersPanel();
		void UI_ShowFontsPanel();

	private:
		struct SelectionData
		{
			Entity Entity;
			MeshUtils::SubMesh* Mesh;
			float Distance = 0.0f;
		};

	private:
		Ref<Scene> m_CurrentScene;
		Ref<Scene> m_EditorScene;
		Ref<SceneRenderer> m_ViewportRenderer;
		Ref<Renderer2D> m_Renderer2D;

		Scope<PanelsManager> m_PanelsManager;

		std::string m_SceneFilePath;

		EditorCamera m_EditorCamera;

		ImRect m_ViewportRect;
		ImVec2 m_ViewportSize = { 0.0f, 0.0f };

		int m_GizmoType = -1; // -1 = No gizmo
		int m_GizmoMode = 0; // 0 = local

		float m_LineWidth = 2.0f;

		bool m_TitleBarHovered = false;
		uint32_t m_TitleBarTargetColor;
		uint32_t m_TitleBarActiveColor;
		uint32_t m_TitleBarPreviousColor;
		bool m_AnimateTitleBarColor = true;

		bool m_StartedCameraClickInViewport = false;

		bool m_ShowBoundingBoxes = false;
		bool m_ShowBoundingBoxSelectedMeshOnly = false;
		bool m_ShowBoundingBoxSubMeshes = false;

		bool m_ShowGizmos = true;

		bool m_ViewportPanelMouseOver = false;
		bool m_ViewportPanelFocused = false;
		bool m_AllowViewportCameraEvents = false;

		// UI...
		bool m_ShowImGuiStackToolWindow = false;
		bool m_ShowImGuiMetricsWindow = false;
		bool m_ShowImGuiStyleEditor = false;

		enum class TransformationTarget { MedianPoint, IndividualOrigins };
		TransformationTarget m_MultiTransformTarget = TransformationTarget::MedianPoint;

		// We can only have on instance of the EditorLayer
		inline static EditorLayer* s_EditorLayerInstance = nullptr;

	};

}