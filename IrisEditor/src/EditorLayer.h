#pragma once

#include "Core/Base.h"
#include "Core/Events/AppEvents.h"
#include "Core/Events/KeyEvents.h"
#include "Core/Events/MouseEvents.h"
#include "Core/Layer.h"
#include "Editor/EditorCamera.h"
#include "Editor/PanelsManager.h"
#include "Project/Project.h"
#include "Project/UserPreferences.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer/StorageBufferSet.h"
#include "Scene/Scene.h"
#include "Utils/FileSystem.h"

#include <imgui/imgui_internal.h>

namespace Iris {

	class EditorLayer : public Layer
	{
	public:
		EditorLayer(const Ref<UserPreferences>& userPrefs);
		virtual ~EditorLayer() override;

		static EditorLayer* Get() { return s_EditorLayerInstance; }

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(TimeStep ts) override;
		virtual void OnRender(TimeStep ts) override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Events::Event& e) override;

		void OnRender2D();

		bool OnKeyPressed(Events::KeyPressedEvent& e);
		bool OnMouseButtonPressed(Events::MouseButtonPressedEvent& e);
		bool OnTitleBarColorChange(Events::TitleBarColorChangeEvent& e);

		// Project
		void OpenProject();
		void OpenProject(const std::filesystem::path& filePath);
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
		void SaveSceneAuto();
		void SaveSceneAs();

		void SceneHierarchySetEditorCameraTransform(Entity entity);

	private:
		std::pair<float, float> GetMouseInViewportSpace() const;
		std::pair<glm::vec3, glm::vec3> CastRay(float x, float y);

		void OnEntityDeleted(Entity e);

	// Primary panels for editor
	private:
		void OnScenePlay();
		void OnSceneStop();

		void UI_StartDocking();
		void UI_EndDocking();

		// Return title bar height
		float UI_DrawTitleBar();
		void UI_HandleManualWindowResize();
		bool UI_TitleBarHitTest(int x, int y) const;
		void UI_DrawMainMenuBar();
		float GetSnapValue();
		glm::vec3& GetSnapValues() { return m_GizmoSnapValues; }
		void SetSnapValue(float value);
		void UI_DrawViewportIcons();
		void UI_DrawViewportOverlays();
		void UI_DrawGizmos();
		void UI_ShowNewSceneModal();
		void UI_ShowNewProjectModal();

		void DeleteEntity(Entity entity);

		void UI_ShowViewport();
		void UI_ShowFontsPanel();

		// Shortcuts
		void SC_Shift1();
		void SC_Shift2();
		void SC_Shift3();
		void SC_F();
		void SC_H();
		void SC_Delete();
		void SC_CtrlD();
		void SC_CtrlF();
		void SC_CtrlH(bool useSlashState = false);
		void SC_Slash();
		void SC_AltC();

	private:
		struct SelectionData
		{
			Entity Entity;
			MeshUtils::SubMesh* Mesh;
			float Distance = 0.0f;
		};

	private:
		Ref<UserPreferences> m_UserPreferences;

		Ref<Scene> m_CurrentScene;
		Ref<Scene> m_EditorScene;
		Ref<Scene> m_RuntimeScene;

		Ref<SceneRenderer> m_ViewportRenderer;
		Ref<Renderer2D> m_Renderer2D;

		Scope<PanelsManager> m_PanelsManager;

		std::string m_SceneFilePath;

		EditorCamera m_EditorCamera;

		ImRect m_ViewportRect;
		ImVec2 m_ViewportSize = { 0.0f, 0.0f };

		int m_GizmoType = -1; // -1 = No gizmo
		int m_GizmoMode = 0; // 0 = local
		glm::vec3 m_GizmoSnapValues = { 1.0f, 45.0f, 1.0f }; // Translation, Rotation, Scale. Defaults: 1 meter, 45 degress, 1 meter

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

		bool m_ShowNewSceneModal = false;
		bool m_ShowNewProjectModal = false;

		bool m_ShowIcons = true;
		bool m_ShowGizmos = true;
		bool m_GizmoAxisFlip = false;

		bool m_ViewportPanelMouseOver = false;
		bool m_ViewportPanelFocused = false;
		bool m_AllowViewportCameraEvents = false;

		bool m_AllowEditorCameraInRuntime = false;

		// UI...
		bool m_ShowImGuiStackToolWindow = false;
		bool m_ShowImGuiMetricsWindow = false;
		bool m_ShowImGuiStyleEditor = false;
		bool m_ShowOnlyViewport = false;
		bool m_ShowRendererInfoOverlay = false;
		bool m_ShowPipelineStatisticsOverlay = false;

		// Shortcuts state
		bool m_SC_SlashKeyState = false; // False => Slash has not been pressed, True => Slash is in the pressed state

		// Just references
		Ref<Texture2D> m_CurrentlySelectedViewIcon = nullptr;
		Ref<Texture2D> m_CurrentlySelectedRenderIcon = nullptr;

		enum class TransformationTarget { MedianPoint, IndividualOrigins };
		TransformationTarget m_MultiTransformTarget = TransformationTarget::MedianPoint;

		enum class TransformationOrigin { Local = 0, World = 1 };
		TransformationOrigin m_TransformationOrigin = TransformationOrigin::World;

		enum class SelectionMode { Entity = 0, SubMesh = 1 };
		SelectionMode m_SelectionMode = SelectionMode::Entity;

		enum class SceneState { Edit = 0, Play = 1, Pause = 2 };
		SceneState m_SceneState = SceneState::Edit;

		float m_TimeSinceLastSave = 0.0f;

		// We can only have on instance of the EditorLayer
		inline static EditorLayer* s_EditorLayerInstance = nullptr;

	};

}