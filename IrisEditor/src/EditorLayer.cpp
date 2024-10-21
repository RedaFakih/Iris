#include "EditorLayer.h"

#include "AssetManager/Importers/MeshImporter.h"
#include "Core/Input/Input.h"
#include "Core/Ray.h"
#include "Editor/AssetEditorPanel.h"
#include "Editor/EditorResources.h"
#include "Editor/Panels/AssetManagerPanel.h"
#include "Editor/Panels/ECSDebugPanel.h"
#include "Editor/Panels/SceneHierarchyPanel.h"
#include "Editor/Panels/SceneRendererPanel.h"
#include "Editor/Panels/ShadersPanel.h"
#include "ImGui/ImGuizmo.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/ProjectSettingsPanel.h"
#include "Project/ProjectSerializer.h"
#include "Renderer/Core/RenderCommandBuffer.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/IndexBuffer.h"
#include "Renderer/Mesh/Material.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Pipeline.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderPass.h"
#include "Renderer/UniformBufferSet.h"
#include "Renderer/VertexBuffer.h"
#include "Scene/SceneSerializer.h"
#include "Utils/FileSystem.h"

#include "ImGui/ImGuiUtils.h"

#include <glfw/include/glfw/glfw3.h>
#include <imgui/imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Iris {

	namespace Utils {

		static void ReplaceToken(std::string& str, const char* token, const std::string& value)
		{
			size_t pos = 0;
			while ((pos = str.find(token, pos)) != std::string::npos)
			{
				str.replace(pos, strlen(token), value);
				pos += strlen(token);
			}
		}

	}

	constexpr int c_MAX_PROJECT_NAME_LENGTH = 255;
	constexpr int c_MAX_PROJECT_FILEPATH_LENGTH = 512;
	static char* s_ProjectNameBuffer = new char[c_MAX_PROJECT_NAME_LENGTH];
	static char* s_OpenProjectFilePathBuffer = new char[c_MAX_PROJECT_FILEPATH_LENGTH];
	static char* s_NewProjectFilePathBuffer = new char[c_MAX_PROJECT_FILEPATH_LENGTH];
	static char* s_ProjectStartSceneNameBuffer = new char[c_MAX_PROJECT_NAME_LENGTH];

	static const char* s_CurrentlySelectedRenderOption = "Lit";

	EditorLayer::EditorLayer(const Ref<UserPreferences>& userPrefs)
		: Layer("EditorLayer"), m_UserPreferences(userPrefs), m_EditorCamera(45.0f, 1280.0f, 720.0f, 0.01f, 10000.0f)
	{
		IR_VERIFY(!s_EditorLayerInstance, "No more than 1 EditorLayer can be created!");
		s_EditorLayerInstance = this;

		for (auto it = m_UserPreferences->RecentProjects.begin(); it != m_UserPreferences->RecentProjects.end();)
		{
			if (!FileSystem::Exists(it->second.Filepath))
				it = m_UserPreferences->RecentProjects.erase(it);
			else
				it++;
		}

		m_TitleBarPreviousColor = Colors::Theme::TitlebarRed;
		m_TitleBarTargetColor   = Colors::Theme::TitlebarCyan;
	}

	EditorLayer::~EditorLayer()
	{
	}

	void EditorLayer::OnAttach()
	{
		memset(s_ProjectNameBuffer, 0, c_MAX_PROJECT_NAME_LENGTH);
		memset(s_OpenProjectFilePathBuffer, 0, c_MAX_PROJECT_FILEPATH_LENGTH);
		memset(s_NewProjectFilePathBuffer, 0, c_MAX_PROJECT_FILEPATH_LENGTH);
		memset(s_ProjectStartSceneNameBuffer, 0, c_MAX_PROJECT_NAME_LENGTH);

		EditorResources::Init();
		m_CurrentlySelectedViewIcon = EditorResources::PerspectiveIcon;
		m_CurrentlySelectedRenderIcon = EditorResources::LitMaterialIcon;
		m_PanelsManager = PanelsManager::Create();

		m_PanelsManager->AddPanel<ProjectSettingsPanel>(PanelCategory::Edit, "ProjectSettingsPanel", "Project Settings", false);
		Ref<SceneHierarchyPanel> sceneHierarchyPanel = m_PanelsManager->AddPanel<SceneHierarchyPanel>(PanelCategory::View, "SceneHierarchyPanel", "Scene Hierarchy", true, m_CurrentScene, SelectionContext::Scene);
		sceneHierarchyPanel->SetEntityDeletedCallback([this](Entity entity) { OnEntityDeleted(entity); });
		sceneHierarchyPanel->AddEntityContextMenuPlugin([this](Entity entity) { SceneHierarchySetEditorCameraTransform(entity); });

		m_PanelsManager->AddPanel<ShadersPanel>(PanelCategory::View, "ShadersPanel", "Shaders", false);
		m_PanelsManager->AddPanel<AssetManagerPanel>(PanelCategory::View, "AssetManagerPanel", "Asset Manager", false);
		Ref<SceneRendererPanel> sceneRendererPanel = m_PanelsManager->AddPanel<SceneRendererPanel>(PanelCategory::View, "SceneRendererPanel", "Scene Renderer", true);

		Ref<ContentBrowserPanel> contentBrowser = m_PanelsManager->AddPanel<ContentBrowserPanel>(PanelCategory::View, "ContentBrowserPanel", "Content Browser", true);
		//contentBrowser->RegisterItemActivateCallbackForType(AssetType::Scene, [this](const AssetMetadata& metadata)
		//{
		//	OpenScene(metadata);
		//});

#ifdef IR_CONFIG_DEBUG
		m_PanelsManager->AddPanel<ECSDebugPanel>(PanelCategory::View, "ECSDebugPanel", "ECS", false, m_CurrentScene);
#endif

		if (!m_UserPreferences->StartupProject.empty())
			OpenProject(m_UserPreferences->StartupProject);
		else
			IR_VERIFY(false, "No Project provided");

		if (!Project::GetActive())
			EmptyProject();

		// TODO: This is here so that we can set the asset directory after the asset manager has been initialized, should be removed after we rework the content browser panel
		contentBrowser->SetBaseDirectory(Project::GetAssetDirectory());

		m_ViewportRenderer = SceneRenderer::Create(m_CurrentScene, { .RendererScale = 1.0f });
		m_ViewportRenderer->SetLineWidth(m_LineWidth);
		sceneRendererPanel->SetRendererContext(m_ViewportRenderer);

		m_Renderer2D = Renderer2D::Create({ .TargetFramebuffer = m_ViewportRenderer->GetExternalCompositeFramebuffer() });
		m_Renderer2D->SetLineWidth(m_LineWidth);

		AssetEditorPanel::RegisterDefaultEditors();
	}

	void EditorLayer::OnDetach()
	{
		EditorResources::Shutdown();
		CloseProject(true);
		AssetEditorPanel::UnregisterAllEditors();
	}

	void EditorLayer::OnUpdate(TimeStep ts)
	{
		// Sync with the asset thread: Any assets loaded by the asset thread are returned to the asset manager so that they become visible to the engine
		AssetManager::SyncWithAssetThread();

		// Set jump flood pass on or off based on if we have selected something in the past frame...
		m_ViewportRenderer->GetSpecification().JumpFloodPass = SelectionManager::GetSelectionCount(SelectionContext::Scene) > 0;

		switch (m_SceneState)
		{
			case SceneState::Edit:
			{
				m_EditorCamera.SetActive(m_AllowViewportCameraEvents);
				m_EditorCamera.OnUpdate(ts);
							
				m_CurrentScene->OnUpdateEditor(ts);
				m_CurrentScene->OnRenderEditor(m_ViewportRenderer, ts, m_EditorCamera);

				OnRender2D();

				if (const auto& project = Project::GetActive(); project && project->GetConfig().EnableAutoSave)
				{
					m_TimeSinceLastSave += ts;
					if (m_TimeSinceLastSave > project->GetConfig().AutoSaveIntervalSeconds)
					{
						SaveSceneAuto();
					}
				}

				break;
			}
			case SceneState::Play:
			{
				m_RuntimeScene->OnUpdateRuntime(ts);

				if (m_AllowEditorCameraInRuntime)
				{
					m_EditorCamera.SetActive(m_ViewportPanelMouseOver || m_AllowViewportCameraEvents);
					m_EditorCamera.OnUpdate(ts);

					m_RuntimeScene->OnRenderEditor(m_ViewportRenderer, ts, m_EditorCamera);

					OnRender2D();
				}
				else
				{
					m_RuntimeScene->OnRenderRuntime(m_ViewportRenderer, ts);
				}

				break;
			}
			case SceneState::Pause:
			{
				m_EditorCamera.SetActive(m_ViewportPanelMouseOver);
				m_EditorCamera.OnUpdate(ts);

				m_RuntimeScene->OnRenderRuntime(m_ViewportRenderer, ts);

				break;
			}
		}

		bool leftAltWithEitherLeftOrMiddleButtonOrJustRight = (Input::IsKeyDown(KeyCode::LeftAlt) && (Input::IsMouseButtonDown(MouseButton::Left) || (Input::IsMouseButtonDown(MouseButton::Middle)))) || Input::IsMouseButtonDown(MouseButton::Right);
		bool notStartCameraViewportAndViewportHoveredFocused = !m_StartedCameraClickInViewport && m_ViewportPanelFocused && m_ViewportPanelMouseOver;
		if (leftAltWithEitherLeftOrMiddleButtonOrJustRight && notStartCameraViewportAndViewportHoveredFocused)
			m_StartedCameraClickInViewport = true;

		bool NotRightAndNOTLeftAltANDLeftOrMiddle = !Input::IsMouseButtonDown(MouseButton::Right) && !(Input::IsKeyDown(KeyCode::LeftAlt) && (Input::IsMouseButtonDown(MouseButton::Left) || Input::IsMouseButtonDown(MouseButton::Middle)));
		if (NotRightAndNOTLeftAltANDLeftOrMiddle)
			m_StartedCameraClickInViewport = false;

		AssetEditorPanel::OnUpdate(ts);
	}

	void EditorLayer::OnEntityDeleted(Entity e)
	{
		SelectionManager::Deselect(SelectionContext::Scene, e.GetUUID());
	}

	void EditorLayer::OnRender2D()
	{
		if (!m_ViewportRenderer->GetFinalPassImage())
			return;

		m_Renderer2D->ResetStats();
		m_Renderer2D->BeginScene(m_EditorCamera.GetViewProjection(), m_EditorCamera.GetViewMatrix());

		if (m_ShowBoundingBoxes)
		{
			if (m_ShowBoundingBoxSelectedMeshOnly)
			{
				const std::vector<UUID>& selectedEntites = SelectionManager::GetSelections(SelectionContext::Scene);
				for (const UUID entityID : selectedEntites)
				{
					Entity entity = m_CurrentScene->GetEntityWithUUID(entityID);

					if (!entity.HasComponent<VisibleComponent>())
						continue;

					const StaticMeshComponent* staticMeshComponent = entity.TryGetComponent<StaticMeshComponent>();
					if (staticMeshComponent)
					{
						Ref<StaticMesh> staticMesh = AssetManager::GetAssetAsync<StaticMesh>(staticMeshComponent->StaticMesh);
						if (staticMesh)
						{
							Ref<MeshSource> meshSource = AssetManager::GetAssetAsync<MeshSource>(staticMesh->GetMeshSource());
							if (meshSource)
							{
								if (m_ShowBoundingBoxSubMeshes)
								{
									const std::vector<uint32_t>& subMeshIndices = staticMesh->GetSubMeshes();
									const std::vector<MeshUtils::SubMesh>& subMeshes = meshSource->GetSubMeshes();

									for (uint32_t subMeshIndex : subMeshIndices)
									{
										glm::mat4 transform = m_CurrentScene->GetWorldSpaceTransformMatrix(entity);
										const AABB& aabb = subMeshes[subMeshIndex].BoundingBox;
										m_Renderer2D->DrawAABB(aabb, transform * subMeshes[subMeshIndex].Transform, { 1.0f, 1.0f, 1.0f, 1.0f });
									}
								}
								else
								{
									glm::mat4 transform = m_CurrentScene->GetWorldSpaceTransformMatrix(entity);
									const AABB& aabb = meshSource->GetBoundingBox();
									m_Renderer2D->DrawAABB(aabb, transform, { 1.0f, 1.0f, 1.0f, 1.0f });
								}
							}
						}
					}
				}
			}
			else
			{
				auto staticMeshEntities = m_CurrentScene->GetAllEntitiesWith<StaticMeshComponent, VisibleComponent>();
				for (auto e : staticMeshEntities)
				{
					Entity entity = { e, m_CurrentScene.Raw() };

					glm::mat4 transform = m_CurrentScene->GetWorldSpaceTransformMatrix(entity);
					Ref<StaticMesh> staticMesh = AssetManager::GetAssetAsync<StaticMesh>(entity.GetComponent<StaticMeshComponent>().StaticMesh);
					if (staticMesh)
					{
						Ref<MeshSource> meshSource = AssetManager::GetAssetAsync<MeshSource>(staticMesh->GetMeshSource());
						if (meshSource)
						{
							const AABB& aabb = meshSource->GetBoundingBox();
							m_Renderer2D->DrawAABB(aabb, transform, { 1.0f, 1.0f, 1.0f, 1.0f });
						}
					}
				}
			}
		}

		if (m_ShowIcons)
		{
			{
				auto entities = m_CurrentScene->GetAllEntitiesWith<CameraComponent>();
				for (auto e : entities)
				{
					Entity entity = { e, m_CurrentScene.Raw() };
					m_Renderer2D->DrawQuadBillboard(m_CurrentScene->GetWorldSpaceTransform(entity).Translation, { 1.0f, 1.0f }, EditorResources::CameraIcon);
				}
			}
		}

		// `true` indicates that we transition resulting images to presenting layouts...
		m_Renderer2D->EndScene(true);
	}

	void EditorLayer::OnImGuiRender()
	{		
		UI_StartDocking();

		// ImGui::ShowDemoWindow(); // Testing imgui stuff

		UI_ShowViewport();

		if (!m_ShowOnlyViewport)
			m_PanelsManager->OnImGuiRender();

		// TODO: Move into EditorStyle editor panel meaning move into panels manager
		if (m_ShowImGuiStyleEditor && !m_ShowOnlyViewport)
		{
			ImGui::Begin("Style Editor", &m_ShowImGuiStyleEditor, ImGuiWindowFlags_NoCollapse);
			static int style;
			ImGui::ShowStyleEditor(0, &style);
			ImGui::End();
		}

		if (m_ShowImGuiMetricsWindow && !m_ShowOnlyViewport)
			ImGui::ShowMetricsWindow(&m_ShowImGuiMetricsWindow);

		if (m_ShowImGuiStackToolWindow && !m_ShowOnlyViewport)
			ImGui::ShowStackToolWindow(&m_ShowImGuiStackToolWindow);

		if (!m_ShowOnlyViewport)
			// TODO: PanelManager
			UI_ShowFontsPanel();

		if (!m_ShowOnlyViewport)
			AssetEditorPanel::OnImGuiRender();

		if (m_ShowNewSceneModal && !m_ShowOnlyViewport)
			UI_ShowNewSceneModal();

		if (m_ShowNewProjectModal && !m_ShowOnlyViewport)
			UI_ShowNewProjectModal();

		UI_EndDocking();

		if (strlen(s_OpenProjectFilePathBuffer) > 0)
			OpenProject(s_OpenProjectFilePathBuffer);
	}

	void EditorLayer::OnScenePlay()
	{
		m_SceneState = SceneState::Play;

		SelectionManager::DeselectAll();

		m_RuntimeScene = Scene::Create(fmt::format("{} - {}", m_EditorScene->GetName(), "Runtime"));
		m_EditorScene->CopyTo(m_RuntimeScene);
		m_RuntimeScene->OnRuntimeStart();
		m_CurrentScene = m_RuntimeScene;

		AssetEditorPanel::SetSceneContext(m_CurrentScene);
		m_ViewportRenderer->GetOptions().ShowGrid = false;
		m_PanelsManager->SetSceneContext(m_CurrentScene);
	}

	void EditorLayer::OnSceneStop()
	{
		m_SceneState = SceneState::Edit;

		SelectionManager::DeselectAll();

		m_ViewportRenderer->GetOptions().ShowGrid = true;

		m_RuntimeScene->OnRuntimeStop();

		// Unload runtime scene
		m_RuntimeScene = nullptr;

		m_CurrentScene = m_EditorScene;
		AssetEditorPanel::SetSceneContext(m_CurrentScene);
		m_PanelsManager->SetSceneContext(m_CurrentScene);
	}

	void EditorLayer::UI_StartDocking()
	{
		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !m_StartedCameraClickInViewport))
		{
			if (m_SceneState != SceneState::Play)
			{
				ImGui::FocusWindow(GImGui->HoveredWindow);
				Input::SetCursorMode(CursorMode::Normal);
			}
		}

		io.ConfigWindowsResizeFromEdges = io.BackendFlags & ImGuiBackendFlags_HasMouseCursors;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		GLFWwindow* window = Application::Get().GetWindow().GetNativeWindow();
		bool isMaximized = (bool)glfwGetWindowAttrib(window, GLFW_MAXIMIZED);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, isMaximized ? ImVec2(6.0f, 6.0f) : ImVec2(1.0f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.0f);
	
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
		ImGui::Begin("MainEditorLayerDockSapce", nullptr, window_flags);
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(2);

		ImGui::PopStyleVar(2);

		{
			UI::ImGuiScopedColor windowBorder(ImGuiCol_Border, IM_COL32(50, 50, 50, 255));
			// Draw window border if window is not maximized
			if (!isMaximized)
				UI::RenderWindowOuterBorders(ImGui::GetCurrentWindow());
		}

		UI_HandleManualWindowResize();
		
		if (!m_ShowOnlyViewport)
		{
			const float titleBarHeight = UI_DrawTitleBar();
			ImGui::SetCursorPosY(titleBarHeight + ImGui::GetCurrentWindow()->WindowPadding.y);
		}

		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 370.0f;
		ImGui::DockSpace(ImGui::GetID("MyDockspace"));
		style.WindowMinSize.x = minWinSizeX;
	}

	void EditorLayer::UI_EndDocking()
	{
		ImGui::End();
	}

	float EditorLayer::UI_DrawTitleBar()
	{
		constexpr float titlebarHeight = 57.0f;
		const ImVec2 windowPadding = ImGui::GetCurrentWindow()->WindowPadding;

		ImGui::SetCursorPos(windowPadding);
		const ImVec2 titlebarMin = ImGui::GetCursorScreenPos();
		const ImVec2 titlebarMax = { ImGui::GetCursorScreenPos().x + ImGui::GetWindowWidth() - windowPadding.y * 2.0f, ImGui::GetCursorScreenPos().y + titlebarHeight };
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(titlebarMin, titlebarMax, Colors::Theme::Titlebar);

		constexpr float c_AnimationTime = 0.4f;
		static float s_CurrentAnimationTimer = c_AnimationTime;

		if (m_AnimateTitleBarColor)
		{
			float animationPercentage = 1.0f - (s_CurrentAnimationTimer / c_AnimationTime);
			s_CurrentAnimationTimer -= Application::Get().GetTimeStep();

			ImVec4 activeColor = ImColor(m_TitleBarTargetColor).Value;
			ImVec4 prevColor = ImColor(m_TitleBarPreviousColor).Value;

			float r = std::lerp(prevColor.x, activeColor.x, animationPercentage);
			float g = std::lerp(prevColor.y, activeColor.y, animationPercentage);
			float b = std::lerp(prevColor.z, activeColor.z, animationPercentage);

			m_TitleBarActiveColor = IM_COL32(r * 255.0f, g * 255.0f, b * 255.0f, 255.0f);

			if (s_CurrentAnimationTimer <= 0.0f)
			{
				s_CurrentAnimationTimer = c_AnimationTime;
				m_TitleBarActiveColor = m_TitleBarTargetColor;
				m_AnimateTitleBarColor = false;
			}
		}

		drawList->AddRectFilledMultiColor(titlebarMin, { titlebarMin.x + 380.0f, titlebarMax.y }, m_TitleBarActiveColor, Colors::Theme::Titlebar, Colors::Theme::Titlebar, m_TitleBarActiveColor);

		// Logo
		{
			int logoWidth = EditorResources::IrisLogo->GetWidth();
			int logoHeight = EditorResources::IrisLogo->GetHeight();

			// Overrides
			logoWidth = 50;
			logoHeight = 50;

			const ImVec2 logoOffset{ 14.0f + windowPadding.x, 6.0f + windowPadding.y - 1.3f };
			const ImVec2 logoRectStart = { ImGui::GetItemRectMin().x + logoOffset.x, ImGui::GetItemRectMin().y + logoOffset.y };
			const ImVec2 logoRectMax = { logoRectStart.x + logoWidth, logoRectStart.y + logoHeight };
			drawList->AddImage(UI::GetTextureID(EditorResources::IrisLogo), logoRectStart, logoRectMax);
		}

		ImGui::BeginHorizontal("TitleBar", { ImGui::GetWindowWidth() - windowPadding.y * 2.0f, ImGui::GetFrameHeightWithSpacing() });

		static float moveOffsetX;
		static float moveOffsetY;
		const float w = ImGui::GetContentRegionAvail().x;
		const float buttonsAreaWidth = 94.0f;

		// TitleBar drag area.
		
		// GLFW has been modified to handle hit tests internally
		ImGui::InvisibleButton("##titlebarDragZone", ImVec2(w - buttonsAreaWidth, titlebarHeight));
		m_TitleBarHovered = ImGui::IsItemHovered() && (Input::GetCursorMode() != CursorMode::Locked);

		ImGui::SuspendLayout();

		// Draw Menubar
		{			
			ImGui::SetItemAllowOverlap();
			const float logoOffset = 16.0f * 2.0f + 41.0f + windowPadding.x;
			ImGui::SetCursorPos({ logoOffset, 4.0f });
			UI_DrawMainMenuBar();

			if (ImGui::IsItemHovered())
				m_TitleBarHovered = false;
		}

		const float menuBarRight = ImGui::GetItemRectMin().x - ImGui::GetCurrentWindow()->Pos.x;
		ImGuiFontsLibrary& fontsLib = Application::Get().GetImGuiLayer()->GetFontsLibrary();

		{
			UI::ImGuiScopedColor textColor(ImGuiCol_Text, Colors::Theme::TextDarker);
			UI::ImGuiScopedColor border(ImGuiCol_Border, IM_COL32(40, 40, 40, 255));

			const std::string title = Project::GetActive()->GetConfig().Name;

			const ImVec2 textSize = ImGui::CalcTextSize(title.c_str());
			const float rightOffset = ImGui::GetWindowWidth() / 5.0f;
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - rightOffset - textSize.x);
			UI::ShiftCursorY(1.0f + windowPadding.y);

			{
				UI::ImGuiScopedFont boldFont(fontsLib.GetFont("RobotoBold"));
				ImGui::TextUnformatted(title.c_str());
			}
			UI::SetToolTip(fmt::format("Current Project ({})", Project::GetActive()->GetConfig().ProjectFileName).c_str());

			UI::DrawBorder(UI::RectExpanded(UI::GetItemRect(), 24.0f, 68.0f), 1.0f, 3.0f, 0.0f, -60.0f);
		}

		// Centered Window title
		{
			ImVec2 currCursorPos = ImGui::GetCursorPos();
			const char* title = "Iris - Version 0.1";
			ImVec2 textSize = ImGui::CalcTextSize(title);
			ImGui::SetCursorPos({ ImGui::GetWindowWidth() * 0.5f - textSize.x * 0.5f, 2.0f + windowPadding.y + 6.0f });
			UI::ImGuiScopedFont titleBoldFont(fontsLib.GetFont("RobotoBold"));
			ImGui::Text("%s [%s]", title, Application::GetConfigurationName());
			ImGui::SetCursorPos(currCursorPos);
		}

		// Current Scene name
		{
			UI::ImGuiScopedColor textColor(ImGuiCol_Text, Colors::Theme::Text);

			std::string sceneString = Utils::RemoveExtension(std::filesystem::path(m_SceneFilePath).filename().string());
			const std::string sceneName = sceneString.size() ? sceneString : m_CurrentScene->GetName();

			ImGui::SetCursorPosX(menuBarRight);
			UI::ShiftCursorX(13.0f);

			fontsLib.PushFont("RobotoBold");
			ImGui::TextUnformatted(sceneName.c_str());
			fontsLib.PopFont();

			UI::SetToolTip(fmt::format("Current Scene ({})", m_SceneFilePath).c_str());

			const float underLineThickness = 2.0f;
			const float underLineExpandWidth = 4.0f;
			ImRect rect = UI::RectExpanded(UI::GetItemRect(), underLineExpandWidth, 0.0f);

			// Vertical Line
			rect.Max.x = rect.Min.x + underLineThickness;
			rect = UI::RectOffset(rect, -underLineThickness * 2.0f, 0.0f);

			drawList->AddRectFilled(rect.Min, rect.Max, Colors::Theme::Muted, 2.0f);

		}
		ImGui::ResumeLayout();

		// Window buttons
		const ImU32 buttonColN = UI::ColorWithMultipliedValue(Colors::Theme::Text, 0.9f);
		const ImU32 buttonColH = UI::ColorWithMultipliedValue(Colors::Theme::Text, 1.2f);
		const ImU32 buttonColP = Colors::Theme::TextDarker;
		constexpr float buttonWidth = 14.0f;
		constexpr float buttonHeight = 14.0f;

		// Minimize Button
		{
			ImGui::Spring();
			UI::ShiftCursorY(8.0f);

			const int iconWidth = EditorResources::MinimizeIcon->GetWidth();
			const int iconHeight = EditorResources::MinimizeIcon->GetHeight();
			const float padY = (buttonHeight - (float)iconHeight) / 2.0f;
			if (ImGui::InvisibleButton("Minimize", { buttonWidth, buttonHeight }))
			{
				if (auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow()))
				{
					Application::Get().QueueEvent([window]() { glfwIconifyWindow(window); });
				}
			}

			UI::DrawButtonImage(EditorResources::MinimizeIcon, buttonColN, buttonColH, buttonColP, UI::RectExpanded(UI::GetItemRect(), 0.0f, -padY));
		}

		// Maximize Button
		{
			ImGui::Spring(-1.0f, 17.0f);
			UI::ShiftCursorY(8.0f);

			const int iconWidth = EditorResources::MaximizeIcon->GetWidth();
			const int iconHeight = EditorResources::MaximizeIcon->GetHeight();
			bool isMaximized = Application::Get().GetWindow().IsMaximized();

			if (ImGui::InvisibleButton("Maximize", ImVec2(buttonWidth, buttonHeight)))
			{
				if (isMaximized)
				{
					Application::Get().QueueEvent([]() { glfwRestoreWindow(Application::Get().GetWindow().GetNativeWindow()); });
				}
				else
				{
					Application::Get().QueueEvent([]() { Application::Get().GetWindow().Maximize(); });
				}
			}

			UI::DrawButtonImage(isMaximized ? EditorResources::RestoreIcon : EditorResources::MaximizeIcon, buttonColN, buttonColH, buttonColP);
		}

		// Close Button
		{
			ImGui::Spring(-1.0f, 15.0f);
			UI::ShiftCursorY(8.0f);

			const int iconWidth = EditorResources::MaximizeIcon->GetWidth();
			const int iconHeight = EditorResources::MaximizeIcon->GetHeight();

			if (ImGui::InvisibleButton("Close", ImVec2(buttonWidth, buttonHeight)))
			{
				Application::Get().DispatchEvent<Events::WindowCloseEvent>();
			}

			UI::DrawButtonImage(EditorResources::CloseIcon, Colors::Theme::Text, UI::ColorWithMultipliedValue(Colors::Theme::Text, 1.4f), buttonColP);
		}

		ImGui::Spring(-1.0f, 18.0f);
		ImGui::EndHorizontal();

		return titlebarHeight;
	}

	void EditorLayer::UI_HandleManualWindowResize()
	{
		bool maximized = Application::Get().GetWindow().IsMaximized();

		ImVec2 newSize, newPosition;
		if (!maximized && UI::UpdateWindowManualResize(ImGui::GetCurrentWindow(), newSize, newPosition))
		{
			// GLFW has been modified to handle setting new size and position of window internally for windows
		}
	}

	bool EditorLayer::UI_TitleBarHitTest(int x, int y) const
	{
		(void)x;
		(void)y;
		return m_TitleBarHovered;
	}

	void EditorLayer::UI_DrawMainMenuBar()
	{
		const ImRect menuBarRect = { ImGui::GetCursorPos(), { ImGui::GetContentRegionAvail().x + ImGui::GetCursorPos().x, ImGui::GetFrameHeightWithSpacing() } };

		ImGui::BeginGroup();

		if (UI::BeginMenuBar(menuBarRect))
		{
			bool menuOpen = ImGui::IsPopupOpen("##menubar", ImGuiPopupFlags_AnyPopupId);

			if (menuOpen)
			{
				const ImU32 colActive = UI::ColorWithSaturation(Colors::Theme::Accent, 0.5f);
				ImGui::PushStyleColor(ImGuiCol_Header, colActive);
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colActive);
			}

			auto popItemHighlight = [&menuOpen]()
			{
				if (menuOpen)
				{
					ImGui::PopStyleColor(3);
					menuOpen = false;
				}
			};

			auto pushDarkTextIfActive = [](const char* menuName) -> bool
			{
				if (ImGui::IsPopupOpen(menuName))
				{
					ImGui::PushStyleColor(ImGuiCol_Text, Colors::Theme::BackgroundDark);
					return true;
				}

				return false;
			};

			const ImU32 colHovered = IM_COL32(0, 0, 0, 80);

			{
				bool colorPushed = pushDarkTextIfActive("File");

				if (ImGui::BeginMenu("File"))
				{
					popItemHighlight();
					colorPushed = false;

					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colHovered);

					if (ImGui::MenuItem("Create Project..."))
					{
						m_ShowNewProjectModal = true;
					}

					if (ImGui::MenuItem("Open Project...", "Ctrl + O"))
					{
						OpenProject();
					}

					if (ImGui::BeginMenu("Open Recent"))
					{
						std::size_t i = 0;
						for (auto it = m_UserPreferences->RecentProjects.begin(); it != m_UserPreferences->RecentProjects.end(); it++)
						{
							if (i > 5)
								break;

							if (ImGui::MenuItem(it->second.Name.c_str()))
							{
								// Stash the filepath away and defer the creation of the filepath until it is safe to do so
								strcpy(s_OpenProjectFilePathBuffer, it->second.Filepath.data());

								RecentProject entry = {
									.Name = it->second.Name,
									.Filepath = it->second.Filepath,
									.LastOpened = time(NULL)
								};

								it = m_UserPreferences->RecentProjects.erase(it);

								m_UserPreferences->RecentProjects[entry.LastOpened] = entry;

								UserPreferencesSerializer::Serialize(m_UserPreferences, m_UserPreferences->Filepath);

								break;
							}

							i++;
						}

						ImGui::EndMenu();
					}

					if (ImGui::MenuItem("Save Project"))
					{
						SaveProject();
					}

					ImGui::Separator();

					if (ImGui::MenuItem("New Scene...", "Ctrl + N"))
					{
						m_ShowNewSceneModal = true;
					}

					if (ImGui::MenuItem("Open Scene...", "Ctrl + Shift + O"))
					{
						OpenScene();
					}

					ImGui::Separator();

					if (ImGui::MenuItem("Save Scene", "Ctrl + S"))
					{
						SaveScene();
					}

					if (ImGui::MenuItem("Save Scene As...", "Ctrl + Shift + S"))
					{
						SaveSceneAs();
					}

					ImGui::Separator();

					if (ImGui::MenuItem("Exit", "Alt + F4"))
						Application::Get().Close();

					ImGui::PopStyleColor();
					ImGui::EndMenu();
				}

				if (colorPushed)
					ImGui::PopStyleColor();
			}

			{
				bool colorPushed = pushDarkTextIfActive("View");;

				if (ImGui::BeginMenu("View"))
				{
					popItemHighlight();
					colorPushed = false;

					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colHovered);

					int n = 1;
					// We only serialize the panels manager for the view panels and not the Edit panels since the Edit ones should always be closed on startup
					for (auto& [id, panelSpec] : m_PanelsManager->GetPanels(PanelCategory::View))
						if (ImGui::MenuItem(panelSpec.Name, fmt::format("Shift + F{}", n++).c_str(), &panelSpec.IsOpen))
							m_PanelsManager->Serialize();

					ImGui::Separator();

					ImGui::MenuItem("Renderer Info", nullptr, &m_ShowRendererInfoOverlay);

					ImGui::MenuItem("Pipeline Statistics", nullptr, &m_ShowPipelineStatisticsOverlay);

					ImGui::PopStyleColor();
					ImGui::EndMenu();
				}

				if (colorPushed)
					ImGui::PopStyleColor();
			}

			{
				bool colorPushed = pushDarkTextIfActive("Edit");;

				if (ImGui::BeginMenu("Edit"))
				{
					popItemHighlight();
					colorPushed = false;

					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colHovered);

					for (auto& [id, panelSpec] : m_PanelsManager->GetPanels(PanelCategory::Edit))
						ImGui::MenuItem(panelSpec.Name, nullptr, &panelSpec.IsOpen);

					ImGui::PopStyleColor();
					ImGui::EndMenu();
				}

				if (colorPushed)
					ImGui::PopStyleColor();
			}

			{
				bool colorPushed = pushDarkTextIfActive("Tools");;

				if (ImGui::BeginMenu("Tools"))
				{
					popItemHighlight();
					colorPushed = false;

					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colHovered);

					ImGui::MenuItem("ImGui Metrics Tool", nullptr, &m_ShowImGuiMetricsWindow);
					ImGui::MenuItem("ImGui Stack Tool", nullptr, &m_ShowImGuiStackToolWindow);
					ImGui::MenuItem("ImGui Style Editor", nullptr, &m_ShowImGuiStyleEditor);

					ImGui::PopStyleColor();
					ImGui::EndMenu();
				}

				if (colorPushed)
					ImGui::PopStyleColor();
			}

			{
				bool colorPushed = pushDarkTextIfActive("Help");

				if (ImGui::BeginMenu("Help"))
				{
					popItemHighlight();
					colorPushed = false;

					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colHovered);

					if (ImGui::MenuItem("About"))
					{
						// TODO: UI_ShowAboutPopup();
					}

					ImGui::PopStyleColor();
					ImGui::EndMenu();
				}

				if (colorPushed)
					ImGui::PopStyleColor();
			}

			if (menuOpen)
				ImGui::PopStyleColor(2);
		
			UI::EndMenuBar();
		}

		ImGui::EndGroup();
	}

	float EditorLayer::GetSnapValue()
	{
		switch (m_GizmoType)
		{
			case ImGuizmo::OPERATION::TRANSLATE: return m_GizmoSnapValues.x;
			case ImGuizmo::OPERATION::ROTATE: return m_GizmoSnapValues.y;
			case ImGuizmo::OPERATION::SCALE: return m_GizmoSnapValues.z;
		}

		return 0.0f;
	}

	void EditorLayer::SetSnapValue(float value)
	{
		switch (m_GizmoType)
		{
			case ImGuizmo::OPERATION::TRANSLATE: m_GizmoSnapValues.x = value; break;
			case ImGuizmo::OPERATION::ROTATE: m_GizmoSnapValues.y = value; break;
			case ImGuizmo::OPERATION::SCALE: m_GizmoSnapValues.z = value; break;
		}
	}

	void EditorLayer::UI_DrawViewportIcons()
	{
		UI::PushID();
		
		UI::ImGuiScopedStyle spacing(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });
		UI::ImGuiScopedStyle windowBorder(ImGuiStyleVar_WindowBorderSize, 0.0f);
		UI::ImGuiScopedStyle rounding(ImGuiStyleVar_WindowRounding, 4.0f);
		UI::ImGuiScopedStyle padding(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

		constexpr float buttonSize = 18.0f;
		constexpr float edgeOffset = 4.0f;
		constexpr float windowHeight = 32.0f; // imgui windows can not be smaller than 32 pixels
		constexpr float textOffset = 4.0f; // Offset between text and label

		const ImColor SelectedGizmoButtonColor = Colors::Theme::Accent;
		const ImColor UnselectedGizmoButtonColor = Colors::Theme::TextBrighter;

		// NOTE: These couple lambdas here will not be moved in with the UI::SectionXXX functions since they are kinda editor layer specific for now...
		auto labelIconButton = [](const Ref<Texture2D>& icon, const ImColor& tint, const char* label, ImVec2 labelSize, const ImRect& windowRect, const char* hint = "")
		{
			ImGuiWindow* window = ImGui::GetCurrentWindow();

			float height = std::min(static_cast<float>(icon->GetHeight()), buttonSize);
			float width = static_cast<float>(icon->GetWidth()) / static_cast<float>(icon->GetHeight()) * height;
			ImVec2 iconSize = ImGui::CalcItemSize({ width, height }, 0.0f, 0.0f);

			const ImRect iconRect({ windowRect.Min.x + 4.0f, windowRect.Min.y + edgeOffset + 2.0f }, { windowRect.Min.x + iconSize.x + 4.0f,  windowRect.Min.y + edgeOffset + iconSize.y + 2.0f });

			ImVec2 cursorPos = ImGui::GetCursorPos();

			ImRect buttonRect;
			buttonRect.Min = { iconRect.Min.x, iconRect.Min.y };
			buttonRect.Max = { iconRect.Min.x + width + labelSize.x + textOffset + 16.0f, iconRect.Max.y };
			const bool clicked = ImGui::ButtonBehavior(buttonRect, ImGui::GetID(UI::GenerateID()), nullptr, nullptr, ImGuiButtonFlags_PressedOnClick);
			if (ImGui::IsMouseHoveringRect(buttonRect.Min, buttonRect.Max))
			{
				UI::ImGuiScopedStyle tooltipPadding(ImGuiStyleVar_WindowPadding, { 5.0f, 5.0f });
				UI::ImGuiScopedColor textCol(ImGuiCol_Text, Colors::Theme::TextBrighter);
				ImGui::SetTooltip(hint);
			}

			ImColor color = IM_COL32(55, 55, 55, 127);
			ImRect expanded = UI::RectExpanded(buttonRect, 4.0f, 4.0f);
			ImGui::GetWindowDrawList()->AddRectFilled(expanded.Min, expanded.Max, color, 16.0f);
			UI::DrawButtonImage(icon,
				tint,
				tint,
				tint,
				iconRect);

			window->DC.CursorPos = { iconRect.Max.x + textOffset, iconRect.Min.y };

			ImGuiFontsLibrary& fontsLib = Application::Get().GetImGuiLayer()->GetFontsLibrary();
			fontsLib.PushFont("RobotoBold");
			ImGui::TextUnformatted(label);
			fontsLib.PopFont();

			return clicked;
		};

		auto iconButton = [&UnselectedGizmoButtonColor, buttonSize](const Ref<Texture2D>& icon, const ImColor& tint, const char* hint1 = "", bool drawDropdown = false, const char* hint2 = "")
		{
			float height = std::min(static_cast<float>(icon->GetHeight()), buttonSize);
			float width = static_cast<float>(icon->GetWidth()) / static_cast<float>(icon->GetHeight()) * height;
			const bool clicked = ImGui::InvisibleButton(UI::GenerateID(), { width, height });
			UI::SetToolTip(hint1);
			bool dropdownClicked = false;

			const ImRect itemRect = UI::GetItemRect();
			ImColor color = IM_COL32(55, 55, 55, 127);
			if (!drawDropdown)
			{
				const ImRect expanded = UI::RectExpanded(itemRect, 4.0f, 4.0f);
				ImGui::GetWindowDrawList()->AddRectFilled(expanded.Min, expanded.Max, color, 12.0f);
				UI::DrawButtonImage(icon,
					tint,
					tint,
					tint,
					itemRect);
			}
			else
			{
				Ref<Texture2D> dropdownIcon = EditorResources::DropdownIcon;
				height = std::min(static_cast<float>(dropdownIcon->GetHeight()), buttonSize);
				width = static_cast<float>(dropdownIcon->GetWidth()) / static_cast<float>(dropdownIcon->GetHeight()) * height;
				dropdownClicked = ImGui::InvisibleButton(UI::GenerateID(), { width, height });

				const ImRect dropdownRect = UI::GetItemRect();
				const ImRect desiredRect = { { itemRect.Min.x, itemRect.Min.y }, { dropdownRect.Max.x, dropdownRect.Max.y } };

				const ImRect expanded = UI::RectExpanded(desiredRect, 4.0f, 4.0f);
				ImGui::GetWindowDrawList()->AddRectFilled(expanded.Min, expanded.Max, color, 12.0f);

				UI::DrawButtonImage(icon,
					tint,
					tint,
					tint,
					itemRect);

				UI::DrawButtonImage(dropdownIcon,
					UnselectedGizmoButtonColor,
					UnselectedGizmoButtonColor,
					UnselectedGizmoButtonColor,
					UI::GetItemRect());
				UI::SetToolTip(hint2);
			}

			return std::pair{ clicked, dropdownClicked };
		};

		auto selection = [](const char* label, Ref<Texture2D> icon, const char* hint = "")
		{
			float height = std::min(static_cast<float>(icon->GetHeight()), buttonSize);
			float width = static_cast<float>(icon->GetWidth()) / static_cast<float>(icon->GetHeight()) * height;

			bool haveHint = strlen(hint) != 0;

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImVec2 cursorPos = ImGui::GetCursorPos();
			UI::ShiftCursor(4.0f, 3.0f);
			UI::Image(icon, { width, height });
			ImGui::SetCursorPos({ cursorPos.x + width + 8.0f, cursorPos.y + 4.0f });
			ImGui::TextUnformatted(label);
			ImGui::SetCursorPos(cursorPos);
			ImGuiTable* table = ImGui::GetCurrentTable();
			float columnWidth = ImGui::TableGetMaxColumnWidth(table, 0);
			if (haveHint)
				columnWidth += ImGui::TableGetMaxColumnWidth(table, 1);
			bool clicked = ImGui::InvisibleButton(UI::GenerateID(), { columnWidth, 23.0f }, ImGuiButtonFlags_AllowItemOverlap);
			if (ImGui::IsItemHovered())
				ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), Colors::Theme::BackgroundDarkBlend);

			if (haveHint)
			{
				ImGui::TableSetColumnIndex(1);
				ImGui::TextDisabled(hint);
			}
			
			return clicked;
		};

		auto snappingTable = [&](const char* tableID, float tableColumnWidth, const char** displaySnapValues, const float* snapValues, ImGuizmo::OPERATION oper, bool setSceneRendererSnapValue = false)
		{
			int sectionIndex = 0;
			if (UI::BeginSection("Snapping", sectionIndex, false))
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				// Alternating row colors...
				const ImU32 colRowAlternating = UI::ColorWithMultipliedValue(Colors::Theme::BackgroundDarkBlend, 1.3f);
				UI::ImGuiScopedColor tableBGAlternating(ImGuiCol_TableRowBgAlt, colRowAlternating);

				UI::ImGuiScopedColor tableBg(ImGuiCol_ChildBg, Colors::Theme::BackgroundDarkBlend);

				ImGuiTableFlags tableFlags = ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoPadOuterX;
				if (ImGui::BeginTable(tableID, 1, tableFlags))
				{
					ImGui::TableSetupColumn("Values", ImGuiTableColumnFlags_WidthFixed, tableColumnWidth);

					
					float* currentSnappingValue = nullptr;

					switch (oper)
					{
						case ImGuizmo::OPERATION::TRANSLATE:	currentSnappingValue = &GetSnapValues().x; break;
						case ImGuizmo::OPERATION::ROTATE:		currentSnappingValue = &GetSnapValues().y; break;
						case ImGuizmo::OPERATION::SCALE:		currentSnappingValue = &GetSnapValues().z; break;
					}

					for (int i = 0; i < 6; i++)
					{
						constexpr float rowHeight = 21.0f;
						ImGui::TableNextRow(0, rowHeight);
						ImGui::TableNextColumn();

						bool selectedRow = *currentSnappingValue == snapValues[i];
						if (ImGui::Selectable(displaySnapValues[i], selectedRow, ImGuiSelectableFlags_SpanAllColumns))
						{
							float value = 0.0f;
							switch (i)
							{
								case 0: value = snapValues[0]; break;
								case 1: value = snapValues[1]; break;
								case 2: value = snapValues[2]; break;
								case 3: value = snapValues[3]; break;
								case 4: value = snapValues[4]; break;
								case 5: value = snapValues[5]; break;
							}
							*currentSnappingValue = value;
							if (setSceneRendererSnapValue)
								m_ViewportRenderer->SetTranslationSnapValue(value);

							ImGui::CloseCurrentPopup();
						}
					}

					ImGui::EndTable();
				}

				UI::EndSection();
			}
		};

		// Top Left corner tools and icons
		{
			static const char* currentlySelectedViewOption = "Perspective";
			ImVec2 viewTextSize = ImGui::CalcTextSize(currentlySelectedViewOption);
			ImVec2 renderTextSize = ImGui::CalcTextSize(s_CurrentlySelectedRenderOption);
			glm::vec2 maxTextSize = { viewTextSize.x + renderTextSize.x, viewTextSize.y };

			float backgroundWidth = edgeOffset * 6.0f + buttonSize * 2.0f + maxTextSize.x * 1.8f;
			ImVec2 position = { m_ViewportRect.Min.x + 14.0f, m_ViewportRect.Min.y + edgeOffset };
			ImGui::SetNextWindowPos(position);
			ImGui::SetNextWindowSize({ backgroundWidth + 4.0f + 2.0f, windowHeight });
			ImGui::SetNextWindowBgAlpha(0.0f);
			ImGui::Begin("##viewport_left_tools", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking);

			constexpr float desiredHeight = 26.0f;
			ImRect background = UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), 0.0f, -(windowHeight - desiredHeight) / 2.0f);
			// ImGui::GetWindowDrawList()->AddRectFilled(background.Min, background.Max, IM_COL32(15, 15, 15, 127), 4.0f);

			ImGui::BeginVertical("##viewportLeftIconsV", { backgroundWidth, ImGui::GetContentRegionAvail().y });
			ImGui::Spring();
			ImGui::BeginHorizontal("##viewportLeftIconsH", { backgroundWidth, ImGui::GetContentRegionAvail().y });
			ImGui::Spring();

			ImRect windowRect = ImGui::GetCurrentWindow()->Rect();
			bool openViewSelectionPopup = false;
			if (labelIconButton(m_CurrentlySelectedViewIcon, UnselectedGizmoButtonColor, currentlySelectedViewOption, viewTextSize, windowRect, "Changes the viewport projection view"))
				openViewSelectionPopup = true;

			windowRect.Min.x += viewTextSize.x + textOffset + 46.0f;
			bool openRenderSelectionPopup = false;
			if (labelIconButton(m_CurrentlySelectedRenderIcon, UnselectedGizmoButtonColor, s_CurrentlySelectedRenderOption, renderTextSize, windowRect, "Changes the rendering option"))
				openRenderSelectionPopup = true;

			ImGui::Spring();
			ImGui::EndHorizontal();
			ImGui::Spring();
			ImGui::EndVertical();

			// Open popups...
			{
				UI::ImGuiScopedStyle enableWindowBorder(ImGuiStyleVar_WindowBorderSize, 4.0f);
				UI::ImGuiScopedStyle enableWindowPadding(ImGuiStyleVar_WindowPadding, { 4.0f, 4.0f });

				if (openViewSelectionPopup)
					ImGui::OpenPopup("View Selection Popup");

				ImGui::SetNextWindowPos({ m_ViewportRect.Min.x + 14.0f, m_ViewportRect.Min.y + edgeOffset + 30.0f });
				ImGui::SetNextWindowBgAlpha(0.7f);
				if (ImGui::BeginPopup("View Selection Popup", ImGuiWindowFlags_NoMove))
				{
					int sectionIndex = 0;
					if (UI::BeginSection("Perspective", sectionIndex, false))
					{
						if (selection("Perspective", EditorResources::PerspectiveIcon))
						{
							m_CurrentlySelectedViewIcon = EditorResources::PerspectiveIcon;
							currentlySelectedViewOption = "Perspective";
							m_EditorCamera.SetPerspectiveProjection();

							ImGui::CloseCurrentPopup();
						}

						UI::EndSection();
					}

					if (UI::BeginSection("Orthographic", sectionIndex, false))
					{
						if (selection("Top", EditorResources::TopSquareIcon))
						{
							m_CurrentlySelectedViewIcon = EditorResources::TopSquareIcon;
							currentlySelectedViewOption = "Top";
							m_EditorCamera.SetTopView();

							ImGui::CloseCurrentPopup();
						}

						if (selection("Bottom", EditorResources::BottomSquareIcon))
						{
							m_CurrentlySelectedViewIcon = EditorResources::BottomSquareIcon;
							currentlySelectedViewOption = "Bottom";
							m_EditorCamera.SetBottomView();

							ImGui::CloseCurrentPopup();
						}

						if (selection("Left", EditorResources::LeftSquareIcon))
						{
							m_CurrentlySelectedViewIcon = EditorResources::LeftSquareIcon;
							currentlySelectedViewOption = "Left";
							m_EditorCamera.SetLeftView();

							ImGui::CloseCurrentPopup();
						}

						if (selection("Right", EditorResources::RightSquareIcon))
						{
							m_CurrentlySelectedViewIcon = EditorResources::RightSquareIcon;
							currentlySelectedViewOption = "Right";
							m_EditorCamera.SetRightView();

							ImGui::CloseCurrentPopup();
						}

						if (selection("Back", EditorResources::BackSquareIcon))
						{
							m_CurrentlySelectedViewIcon = EditorResources::BackSquareIcon;
							currentlySelectedViewOption = "Back";
							m_EditorCamera.SetBackView();

							ImGui::CloseCurrentPopup();
						}

						if (selection("Front", EditorResources::FrontSquareIcon))
						{
							m_CurrentlySelectedViewIcon = EditorResources::FrontSquareIcon;
							currentlySelectedViewOption = "Front";
							m_EditorCamera.SetFrontView();

							ImGui::CloseCurrentPopup();
						}

						UI::EndSection();
					}

					ImGui::EndPopup();
				}

				if (openRenderSelectionPopup)
					ImGui::OpenPopup("Render Selection Popup");

				ImGui::SetNextWindowPos({ m_ViewportRect.Min.x + 14.0f, m_ViewportRect.Min.y + edgeOffset + 30.0f });
				ImGui::SetNextWindowBgAlpha(0.7f);
				if (ImGui::BeginPopup("Render Selection Popup"))
				{
					int sectionIndex = 0;
					if (UI::BeginSection("View Mode", sectionIndex, true, 0.0f, 48.0f)) // 48.0f is ImGui::CalcTextSize("Shift + 1").x
					{
						if (selection("Lit", EditorResources::LitMaterialIcon, "Shift + 1"))
						{
							m_CurrentlySelectedRenderIcon = EditorResources::LitMaterialIcon;
							s_CurrentlySelectedRenderOption = "Lit";
							m_ViewportRenderer->SetViewMode(SceneRenderer::ViewMode::Lit);

							ImGui::CloseCurrentPopup();
						}

						if (selection("Unlit", EditorResources::UnLitMaterialIcon, "Shift + 2"))
						{
							m_CurrentlySelectedRenderIcon = EditorResources::UnLitMaterialIcon;
							s_CurrentlySelectedRenderOption = "Unlit";
							m_ViewportRenderer->SetViewMode(SceneRenderer::ViewMode::Unlit);

							ImGui::CloseCurrentPopup();
						}

						if (selection("Wireframe", EditorResources::WireframeViewIcon, "Shift + 3"))
						{
							m_CurrentlySelectedRenderIcon = EditorResources::WireframeViewIcon;
							s_CurrentlySelectedRenderOption = "Wireframe";
							m_ViewportRenderer->SetViewMode(SceneRenderer::ViewMode::Wireframe);

							ImGui::CloseCurrentPopup();
						}

						UI::EndSection();
					}

					ImGui::EndPopup();
				}
			}

			ImGui::End();
		}

		{
			constexpr float numberOfButtons = 2.0f;
			constexpr float backgroundWidth = edgeOffset * 6.0f + buttonSize * numberOfButtons + edgeOffset * (numberOfButtons - 1.0f) * 2.0f;
			ImVec2 position = { (m_ViewportRect.Min.x + m_ViewportRect.Max.x) / 2.0f - (backgroundWidth / 2.0f), m_ViewportRect.Min.y + edgeOffset };
			ImGui::SetNextWindowPos(position);
			ImGui::SetNextWindowSize({ backgroundWidth, windowHeight });
			ImGui::SetNextWindowBgAlpha(0.0f);
			ImGui::Begin("##viewport_central_tools", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking);

			// To make a smaller window, we could just fill the desired height that we want with color
			constexpr float desiredHeight = 26.0f;
			ImRect background = UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), 0.0f, -(windowHeight - desiredHeight) / 2.0f);
			// ImGui::GetWindowDrawList()->AddRectFilled(background.Min, background.Max, IM_COL32(15, 15, 15, 127), 4.0f);

			ImGui::BeginVertical("##viewportCentralIconsV", { backgroundWidth, ImGui::GetContentRegionAvail().y });
			ImGui::Spring();
			ImGui::BeginHorizontal("##viewportCentralIconsH", { backgroundWidth, ImGui::GetContentRegionAvail().y });
			ImGui::Spring();

			{
				UI::ImGuiScopedStyle enableSpacing(ImGuiStyleVar_ItemSpacing, { edgeOffset * 2.0f, 0.0f });

				Ref<Texture2D> buttonTex = m_SceneState == SceneState::Play ? EditorResources::SceneStopIcon : EditorResources::ScenePlayIcon;
				if (iconButton(buttonTex, UnselectedGizmoButtonColor, m_SceneState == SceneState::Play ? "Stop Scene" : "Play Scene").first)
				{
					m_TitleBarPreviousColor = m_TitleBarActiveColor;
					if (m_SceneState == SceneState::Edit)
					{
						// Go into play mode
						m_TitleBarTargetColor = Colors::Theme::TitlebarYellow;
						OnScenePlay();
					}
					else
					{
						m_TitleBarTargetColor = Colors::Theme::TitlebarCyan;
						OnSceneStop();
					}
					
					m_AnimateTitleBarColor = true;
				}

				ImGui::Spring(1.0f);
				if (iconButton(EditorResources::ScenePauseIcon, UnselectedGizmoButtonColor, m_SceneState == SceneState::Pause ? "Resume Scene" : "Pause Scene").first)
				{
					m_TitleBarPreviousColor = m_TitleBarActiveColor;

					if (m_SceneState == SceneState::Play)
					{
						m_SceneState = SceneState::Pause;

						m_TitleBarTargetColor = Colors::Theme::TitlebarRed;
					}
					else if (m_SceneState == SceneState::Pause)
					{
						m_SceneState = SceneState::Play;

						m_TitleBarTargetColor = Colors::Theme::TitlebarYellow;
					}

					m_AnimateTitleBarColor = true;
				}
			}

			ImGui::Spring();
			ImGui::EndHorizontal();
			ImGui::Spring();
			ImGui::EndVertical();

			ImGui::End();
		}

		// Top Right corner tools and icons
		{
			constexpr float numberOfButtons = 5.0f + 3.0f; // we add 3 for the dropdown buttons
			constexpr float backgroundWidth = edgeOffset * 6.0f + buttonSize * numberOfButtons + edgeOffset * (numberOfButtons - 1.0f) * 2.0f;
			ImVec2 position = { m_ViewportRect.Max.x - backgroundWidth - 14.0f, m_ViewportRect.Min.y + edgeOffset };
			ImGui::SetNextWindowPos(position);
			ImGui::SetNextWindowSize({ backgroundWidth, windowHeight });
			ImGui::SetNextWindowBgAlpha(0.0f);
			ImGui::Begin("##viewport_right_tools", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking);

			// To make a smaller window, we could just fill the desired height that we want with color
			constexpr float desiredHeight = 26.0f;
			ImRect background = UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), 0.0f, -(windowHeight - desiredHeight) / 2.0f);
			// ImGui::GetWindowDrawList()->AddRectFilled(background.Min, background.Max, IM_COL32(15, 15, 15, 127), 4.0f);

			ImGui::BeginVertical("##viewportRightIconsV", { backgroundWidth, ImGui::GetContentRegionAvail().y });
			ImGui::Spring();
			ImGui::BeginHorizontal("##viewportRightIconsH", { backgroundWidth, ImGui::GetContentRegionAvail().y });
			ImGui::Spring();

			bool openTranslateSnapPopup = false;
			bool openRotateSnapPopup = false;
			bool openScaleSnapPopup = false;
			bool openViewportSettingsPopup = false;
			{
				UI::ImGuiScopedStyle enableSpacing(ImGuiStyleVar_ItemSpacing, { edgeOffset * 2.0f, 0.0f });

				ImColor buttonTint = m_GizmoType == -1 ? SelectedGizmoButtonColor : UnselectedGizmoButtonColor;
				if (iconButton(EditorResources::PointerIcon, buttonTint, "Select, Q").first)
					m_GizmoType = -1;

				ImGui::Spring(1.0f);
				buttonTint = m_GizmoType == ImGuizmo::OPERATION::TRANSLATE ? SelectedGizmoButtonColor : UnselectedGizmoButtonColor;
				auto states = iconButton(EditorResources::TranslateIcon, buttonTint, "Translate, W", true, "Translation Snap Values");
				if (states.first)
					m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
				if (states.second)
					openTranslateSnapPopup = true;

				ImGui::Spring(1.0f);
				buttonTint = m_GizmoType == ImGuizmo::OPERATION::ROTATE ? SelectedGizmoButtonColor : UnselectedGizmoButtonColor;
				states = iconButton(EditorResources::RotateIcon, buttonTint, "Rotate, E", true, "Rotation Snap Values");
				if (states.first)
					m_GizmoType = ImGuizmo::OPERATION::ROTATE;
				if (states.second)
					openRotateSnapPopup = true;

				ImGui::Spring(1.0f);
				buttonTint = m_GizmoType == ImGuizmo::OPERATION::SCALE ? SelectedGizmoButtonColor : UnselectedGizmoButtonColor;
				states = iconButton(EditorResources::ScaleIcon, buttonTint, "Scale, R", true, "Scaling Snap Values");
				if (states.first)
					m_GizmoType = ImGuizmo::OPERATION::SCALE;
				if (states.second)
					openScaleSnapPopup = true;

				ImGui::Spring(1.0f);
				if (iconButton(EditorResources::GearIcon, UnselectedGizmoButtonColor, "ViewportSettings").first)
					openViewportSettingsPopup = true;
			}

			ImGui::Spring();
			ImGui::EndHorizontal();
			ImGui::Spring();
			ImGui::EndVertical();

			// Handle popup opening...
			{
				UI::ImGuiScopedStyle enableWindowBorder(ImGuiStyleVar_WindowBorderSize, 4.0f);
				UI::ImGuiScopedStyle enableWindowPadding(ImGuiStyleVar_WindowPadding, { 4.0f, 4.0f });

				if (openTranslateSnapPopup)
					ImGui::OpenPopup("Translate Snap Value");

				constexpr float SnappingTableColumnWidth = 40.0f;

				float width = edgeOffset * 6.0f + buttonSize * 6.0f + edgeOffset * (numberOfButtons - 1.0f) * 2.0f + SnappingTableColumnWidth * 3 + 20.0f;
				ImGui::SetNextWindowPos({ m_ViewportRect.Max.x - width, m_ViewportRect.Min.y + edgeOffset + 30.0f });
				ImGui::SetNextWindowBgAlpha(0.7f);
				if (ImGui::BeginPopup("Translate Snap Value", ImGuiWindowFlags_NoMove))
				{
					static const char* displayTranslationSnapValues[6] = { "  10cm", "  50cm", "  1m", "  2m", "  5m", "  10m" };
					static const float translationSnapValues[6] = { 0.1f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f };
					snappingTable("##translate_snap_table", SnappingTableColumnWidth, displayTranslationSnapValues, translationSnapValues, ImGuizmo::OPERATION::TRANSLATE, true);

					ImGui::EndPopup();
				}

				if (openRotateSnapPopup)
					ImGui::OpenPopup("Rotate Snap Value");

				width = edgeOffset * 6.0f + buttonSize * 3.0f + edgeOffset * (numberOfButtons - 1.0f) * 2.0f + SnappingTableColumnWidth * 3 + 21.0f;
				ImGui::SetNextWindowPos({ m_ViewportRect.Max.x - width, m_ViewportRect.Min.y + edgeOffset + 30.0f });
				ImGui::SetNextWindowBgAlpha(0.7f);
				if (ImGui::BeginPopup("Rotate Snap Value", ImGuiWindowFlags_NoMove))
				{
					static const char* displayRotationSnapValues[6] = { "  5deg", "  10deg", "  30deg", "  45deg", "  90deg", "  180deg" };
					static const float rotationSnapValues[6] = { 5.0f, 10.0f, 30.0f, 45.0f, 90.0f, 180.0f };
					snappingTable("##rotate_snap_table", SnappingTableColumnWidth + 10.0f, displayRotationSnapValues, rotationSnapValues, ImGuizmo::OPERATION::ROTATE);

					ImGui::EndPopup();
				}

				if (openScaleSnapPopup)
					ImGui::OpenPopup("Scale Snap Value");

				width = edgeOffset * 6.0f + buttonSize * 1.0f + edgeOffset * (numberOfButtons - 1.0f) * 2.0f + SnappingTableColumnWidth * 3;
				ImGui::SetNextWindowPos({ m_ViewportRect.Max.x - width, m_ViewportRect.Min.y + edgeOffset + 30.0f });
				ImGui::SetNextWindowBgAlpha(0.7f);
				if (ImGui::BeginPopup("Scale Snap Value", ImGuiWindowFlags_NoMove))
				{
					static const char* displayScalingSnapValues[6] = { "  10cm", "  50cm", "  1m", "  2m", "  5m", "  10m" };
					static const float scaleSnapValues[6] = { 0.1f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f };
					snappingTable("##rotate_snap_table", SnappingTableColumnWidth, displayScalingSnapValues, scaleSnapValues, ImGuizmo::OPERATION::SCALE);

					ImGui::EndPopup();
				}

				if (openViewportSettingsPopup)
					ImGui::OpenPopup("Viewport Settings");

				// Viewport settings
				{
					UI::ImGuiScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0, 5.5f));
					UI::ImGuiScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
					UI::ImGuiScopedStyle windowRounding(ImGuiStyleVar_PopupRounding, 4.0f);
					UI::ImGuiScopedStyle cellPadding(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 5.5f));

					width = edgeOffset * 6.0f + buttonSize + edgeOffset * (numberOfButtons - 1.0f) * 2.0f + 256.0f;
					ImGui::SetNextWindowPos({ m_ViewportRect.Max.x - width, m_ViewportRect.Min.y + edgeOffset + 30.0f });
					ImGui::SetNextWindowBgAlpha(0.7f);
					if (ImGui::BeginPopup("Viewport Settings", ImGuiWindowFlags_NoMove))
					{
						int sectionIndex = 0;

						if (UI::BeginSection("General", sectionIndex))
						{
							static const char* s_SelectionModes[] = { "Entity", "Submesh" };
							UI::SectionDropdown("Selection Mode", s_SelectionModes, 2, reinterpret_cast<int32_t*>(&m_SelectionMode), "Mode of how submeshes are selected");

							static const char* s_TransformTargetNames[] = { "Median Point", "Individual Origins" };
							UI::SectionDropdown("Multi-Transform Target", s_TransformTargetNames, 2, reinterpret_cast<int32_t*>(&m_MultiTransformTarget), "Transform around the origin or each entity while in multiselection\nor around a median point with respect to all selected entites");

							static const char* s_TransformAroundOrigin[] = { "Local", "World" };
							UI::SectionDropdown("Transformation Origin", s_TransformAroundOrigin, 2, reinterpret_cast<int32_t*>(&m_TransformationOrigin), "Transform around origin of the entity/entities\nor around the origin of the world");

							UI::EndSection();
						}

						if (UI::BeginSection("Display", sectionIndex))
						{
							UI::SectionCheckbox("Show Icons", m_ShowIcons, "Show viewport icons");
							UI::SectionCheckbox("Show Gizmos", m_ShowGizmos, "Show transform gizmos");
							if (UI::SectionCheckbox("Allow Gizmo Axis Flip", m_GizmoAxisFlip, "Allow transform gizmos to flip their\naxes with respect to the camera"))
								ImGuizmo::AllowAxisFlip(m_GizmoAxisFlip);
							UI::SectionCheckbox("Show Bounding Boxes", m_ShowBoundingBoxes, "Show mesh entities bounding boxes, Ctrl + B");
							if (m_ShowBoundingBoxes)
							{
								UI::SectionCheckbox("Selected Entity", m_ShowBoundingBoxSelectedMeshOnly, "Show bounding boxes only for the\ncurrently selected entity");

								if (m_ShowBoundingBoxSelectedMeshOnly)
									UI::SectionCheckbox("Submeshes", m_ShowBoundingBoxSubMeshes, "Show submesh bounding boxes for the\ncurrently selected entity");
							}
							SceneRendererOptions& rendererOptions = m_ViewportRenderer->GetOptions();
							UI::SectionCheckbox("Show Grid", rendererOptions.ShowGrid, "Show Grid, Ctrl + G");
							UI::SectionCheckbox("Selected in Wireframe", rendererOptions.ShowSelectedInWireFrame, "Show selected mesh in wireframe mode");

							UI::SectionCheckbox("Show Physics Colliders", rendererOptions.ShowPhysicsColliders, "Show phsyics colliders in the viewport");
							static const char* s_PhysicsColliderDebugViewMode[] = { "Selected Entity", "All" };
							UI::SectionDropdown("Selection Mode", s_PhysicsColliderDebugViewMode, 2, reinterpret_cast<int32_t*>(&rendererOptions.PhysicsColliderViewMode), "Select the debug view mode of physics colliders");

							if (UI::SectionDrag("Line Width", m_LineWidth, 0.1f, 0.1f, 10.0f, "Change pipeline line width"))
							{
								m_Renderer2D->SetLineWidth(m_LineWidth);
								m_ViewportRenderer->SetLineWidth(m_LineWidth);
							}

							UI::EndSection();
						}

						if (UI::BeginSection("Scene Camera", sectionIndex))
						{
							float fov = glm::degrees(m_EditorCamera.GetFOV());
							if (UI::SectionSlider("Field of View", fov, 30, 120, "Field of view of the viewport camera"))
								m_EditorCamera.SetFOV(fov);
							UI::SectionSlider("Exposure", m_EditorCamera.GetExposure(), 0.0f, 10.0f, "Exposure of viewport camera,\nalso extends into rendered scene");
							static float& cameraSpeed = m_EditorCamera.GetNormalSpeed();
							float displayCameraSpeed = cameraSpeed / 0.0002f;
							if (UI::SectionDrag("Speed", displayCameraSpeed, 0.5f, 0.0002f, 100.0f, "Speed of viewport camera in fly mode, RightAlt + Scroll"))
								cameraSpeed = displayCameraSpeed * 0.0002f;
							float nearClip = m_EditorCamera.GetNearClip();
							if (UI::SectionDrag("Near Clip", nearClip, 0.1f, 0.002f, 100.0f, "Viewport camera near clip"))
								m_EditorCamera.SetNearClip(nearClip);
							float farClip = m_EditorCamera.GetFarClip();
							if (UI::SectionDrag("Far Clip", farClip, 2.0f, 100.1f, 1'000'000.0f, "Viewport camera far clip"))
								m_EditorCamera.SetFarClip(farClip);

							UI::EndSection();
						}

						if (UI::BeginSection("Viewport Renderer", sectionIndex))
						{
							std::string string = fmt::format("{}", static_cast<uint32_t>(1.0f / Application::Get().GetFrameTime().GetSeconds()));
							UI::SectionText("Framerate (FPS)", string.c_str());

							bool isVSync = Application::Get().GetWindow().IsVSync();
							if (UI::SectionCheckbox("Vertical Sync", isVSync, "Toggle monitor vertical sync"))
								Application::Get().GetWindow().SetVSync(isVSync);

							UI::SectionSlider("Opacity", m_ViewportRenderer->GetOpacity(), 0.0f, 1.0f, "Viewport renderer opacity");

							static float renderScale = m_ViewportRenderer->GetSpecification().RendererScale;
							const float prevRenderScale = m_ViewportRenderer->GetSpecification().RendererScale;
							if (UI::SectionSlider("Rendering Scale", renderScale, 0.1f, 2.0f, "Viewport renderer rendering scale,\nincrease for better quality result"))
								m_ViewportRenderer->SetViewportSize(
									static_cast<uint32_t>(m_ViewportRenderer->GetViewportWidth() / prevRenderScale),
									static_cast<uint32_t>(m_ViewportRenderer->GetViewportHeight() / prevRenderScale),
									renderScale
								);

							UI::EndSection();
						}

						ImGui::EndPopup();
					}
				}
			}

			ImGui::End();
		}

		UI::PopID();

		UI_DrawViewportOverlays();
	}

	void EditorLayer::UI_DrawViewportOverlays()
	{
		if (m_ShowRendererInfoOverlay)
		{
			ImGui::SetNextWindowBgAlpha(0.5f);
			ImGui::SetNextWindowSize({ 298.0f, 108.0f });
			ImGui::SetNextWindowPos({ m_ViewportRect.Min.x + 14.0f, m_ViewportRect.Max.y - 120.0f });
			if (ImGui::Begin("Renderer Info", &m_ShowRendererInfoOverlay, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
			{
				UI::BeginPropertyGrid(2, 95.0f);

				const auto& rendererCaps = Renderer::GetCapabilities();
				UI::PropertyStringReadOnly("Vendor", rendererCaps.Vendor.c_str());
				UI::PropertyStringReadOnly("Device", rendererCaps.Device.c_str());
				UI::PropertyStringReadOnly("Version", rendererCaps.Version.c_str());

				UI::EndPropertyGrid();
			}

			ImGui::End();
		}

		if (m_ShowPipelineStatisticsOverlay)
		{
			ImGui::SetNextWindowPos({ m_ViewportRect.Min.x + 20.0f, m_ViewportRect.Min.y + 40.0f });
			if (ImGui::Begin("Pipeline Statistics", &m_ShowPipelineStatisticsOverlay, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs))
			{
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 8.0f, 8.0f });
				ImGuiFontsLibrary& fontsLib = Application::Get().GetImGuiLayer()->GetFontsLibrary();
				fontsLib.PushFont("RobotoBold");

				ImGui::TextUnformatted(fmt::format("Input Vertices: {}", m_ViewportRenderer->GetPipelineStatistics().InputAssemblyVertices).c_str());
				ImGui::TextUnformatted(fmt::format("Vertex Shader Invocations: {}", m_ViewportRenderer->GetPipelineStatistics().VertexShaderInvocations).c_str());
				ImGui::TextUnformatted(fmt::format("Fragment Shader Invocations: {}", m_ViewportRenderer->GetPipelineStatistics().FragmentShaderInvocations).c_str());
				ImGui::TextUnformatted(fmt::format("Compute Shader Invocations: {}", m_ViewportRenderer->GetPipelineStatistics().ComputeShaderInvocations).c_str());

				fontsLib.PopFont();
				ImGui::PopStyleVar();
			}

			ImGui::End();
		}
	}

	void EditorLayer::UI_DrawGizmos()
	{
		if (m_GizmoType == -1)
			return;

		const auto& selections = SelectionManager::GetSelections(SelectionContext::Scene);

		if (selections.empty())
			return;

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

		bool snap = Input::IsKeyDown(KeyCode::LeftControl);

		float snapValue = GetSnapValue();
		float snapValues[3] = { snapValue, snapValue, snapValue };

		glm::mat4 projectionMatrix, viewMatrix;
		if (m_SceneState == SceneState::Play && !m_AllowEditorCameraInRuntime)
		{
			Entity cameraEntity = m_CurrentScene->GetMainCameraEntity();
			SceneCamera& camera = cameraEntity.GetComponent<CameraComponent>().Camera;
			projectionMatrix = camera.GetProjectionMatrix();
			viewMatrix = glm::inverse(m_CurrentScene->GetWorldSpaceTransformMatrix(cameraEntity));
		}
		else
		{
			// Get from EditorCamera instead
			projectionMatrix = m_EditorCamera.GetProjectionMatrix();
			viewMatrix = m_EditorCamera.GetViewMatrix();
		}

		if (selections.size() == 1)
		{
			Entity entity = m_CurrentScene->GetEntityWithUUID(selections[0]);
			TransformComponent& entityTransform = entity.Transform();
			glm::mat4 transform = m_CurrentScene->GetWorldSpaceTransformMatrix(entity);

			if (ImGuizmo::Manipulate(
				glm::value_ptr(viewMatrix),
				glm::value_ptr(projectionMatrix),
				static_cast<ImGuizmo::OPERATION>(m_GizmoType),
				static_cast<ImGuizmo::MODE>(m_TransformationOrigin),
				glm::value_ptr(transform),
				nullptr,
				snap ? snapValues : nullptr)
				)
			{
				Entity parent = m_CurrentScene->TryGetEntityWithUUID(entity.GetParentUUID());
				if (parent)
				{
					glm::mat4 parentTransform = m_CurrentScene->GetWorldSpaceTransformMatrix(parent);
					transform = glm::inverse(parentTransform) * transform;
				}

				// Manipulated transform is now in local space of parent (= world space if no parent)
				// We can decompose into translation, rotation, and scale and compare with original
				// to figure out how to best update entity transform
				//
				// Why do we do this instead of just setting the entire entity transform?
				// Because it's more robust to set only those components of transform
				// that we are meant to be changing (dictated by m_GizmoType). That way we avoid
				// small drift (particularly in rotation and scale) due to numerical precision issues
				// from all those matrix operations.
				glm::vec3 translation;
				glm::quat rotation;
				glm::vec3 scale;
				Math::DecomposeTransform(transform, translation, rotation, scale);

				switch (m_GizmoType)
				{
					case ImGuizmo::TRANSLATE:
					{
						entityTransform.Translation = translation;
						break;
					}
					case ImGuizmo::ROTATE:
					{
						// Do this in Euler in an attempt to preserve any full revolutions (> 360)
						glm::vec3 originalRotationEuler = entityTransform.GetRotationEuler();

						// Map original rotation to range [-180, 180] which is what ImGuizmo gives us
						originalRotationEuler.x = fmodf(originalRotationEuler.x + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();
						originalRotationEuler.y = fmodf(originalRotationEuler.y + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();
						originalRotationEuler.z = fmodf(originalRotationEuler.z + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();

						glm::vec3 deltaRotationEuler = glm::eulerAngles(rotation) - originalRotationEuler;

						// Try to avoid drift due numeric precision
						if (fabs(deltaRotationEuler.x) < 0.001f) deltaRotationEuler.x = 0.0f;
						if (fabs(deltaRotationEuler.y) < 0.001f) deltaRotationEuler.y = 0.0f;
						if (fabs(deltaRotationEuler.z) < 0.001f) deltaRotationEuler.z = 0.0f;

						entityTransform.SetRotationEuler(entityTransform.GetRotationEuler() += deltaRotationEuler);
						break;
					}
					case ImGuizmo::SCALE:
					{
						entityTransform.Scale = scale;
						break;
					}
				}
			}
		}
		else
		{
			if (m_MultiTransformTarget == TransformationTarget::MedianPoint && m_GizmoType == ImGuizmo::SCALE)
			{
				// NOTE: Disabling multi-entity scaling for median point mode for now since it causes strange scaling behavior
			    // NOTE: https://math.stackexchange.com/questions/3245481/rotate-and-scale-a-point-around-different-origins
				return;
			}

			glm::vec3 medianLocation = glm::vec3(0.0f);
			glm::vec3 medianScale = glm::vec3(1.0f);
			glm::quat medianQuat = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);

			// Compuet median point
			for (auto entityID : selections)
			{
				Entity entity = m_CurrentScene->GetEntityWithUUID(entityID);
				const TransformComponent& tc = entity.Transform();
				medianLocation += tc.Translation;
				medianScale += tc.Scale;
				medianQuat += glm::quat(tc.GetRotationEuler());
			}
			medianLocation /= static_cast<float>(selections.size());
			medianScale /= static_cast<float>(selections.size());
			medianQuat = glm::normalize(medianQuat / static_cast<float>(selections.size()));

			glm::mat4 medianPointMatrix = glm::translate(glm::mat4(1.0f), medianLocation)
				* glm::toMat4(medianQuat)
				* glm::scale(glm::mat4(1.0f), medianScale);

			glm::mat4 deltaMatrix = glm::mat4(1.0f);

			if (ImGuizmo::Manipulate(
				glm::value_ptr(viewMatrix),
				glm::value_ptr(projectionMatrix),
				static_cast<ImGuizmo::OPERATION>(m_GizmoType),
				static_cast<ImGuizmo::MODE>(m_TransformationOrigin),
				glm::value_ptr(medianPointMatrix),
				glm::value_ptr(deltaMatrix),
				snap ? snapValues : nullptr)
				)
			{
				switch (m_MultiTransformTarget)
				{
					case TransformationTarget::MedianPoint:
					{
						for (auto entityID : selections)
						{
							Entity entity = m_CurrentScene->GetEntityWithUUID(entityID);
							TransformComponent& transform = entity.Transform();
							transform.SetTransform(deltaMatrix * transform.GetTransform());
						}

						break;
					}
					case TransformationTarget::IndividualOrigins:
					{
						glm::vec3 deltaTranslation, deltaScale;
						glm::quat deltaRotation;
						Math::DecomposeTransform(deltaMatrix, deltaTranslation, deltaRotation, deltaScale);

						for (auto entityID : selections)
						{
							Entity entity = m_CurrentScene->GetEntityWithUUID(entityID);
							TransformComponent& transform = entity.Transform();

							switch (m_GizmoType)
							{
								case ImGuizmo::TRANSLATE:
								{
									transform.Translation += deltaTranslation;
									break;
								}
								case ImGuizmo::ROTATE:
								{
									transform.SetRotationEuler(transform.GetRotationEuler() + glm::eulerAngles(deltaRotation));
									break;
								}
								case ImGuizmo::SCALE:
								{
									if (deltaScale != glm::vec3(1.0f, 1.0f, 1.0f))
										transform.Scale *= deltaScale;
									break;
								}
							}
						}
						break;
					}
				}
			}
		}
	}

	void EditorLayer::UI_ShowNewSceneModal()
	{
		ImGui::OpenPopup("Set Scene Name");

		// Always center this window when appearing
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Set Scene Name", 0, ImGuiWindowFlags_AlwaysAutoResize))
		{
			static std::string sceneName;
			ImGui::SetKeyboardFocusHere();
			ImGui::InputTextWithHint("##scene_name_label", "Name...", &sceneName);

			ImGui::Separator();

			if (ImGui::Button("Create") || Input::IsKeyDown(KeyCode::Enter))
			{
				if (!sceneName.empty())
				{
					NewScene(sceneName);
					m_ShowNewSceneModal = false;
					sceneName.clear();
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel") || Input::IsKeyDown(KeyCode::Escape))
			{
				m_ShowNewSceneModal = false;
				sceneName.clear();
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void EditorLayer::UI_ShowNewProjectModal()
	{
		ImGui::OpenPopup("Create New Project");

		// Always center this window when appearing
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Create New Project", 0, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 10.0f, 7.0f });

			ImGuiFontsLibrary& fontsLib = Application::Get().GetImGuiLayer()->GetFontsLibrary();

			fontsLib.PushFont("RobotoBold");
			std::string fullProjectPath = strlen(s_NewProjectFilePathBuffer) > 0 ? std::string(s_NewProjectFilePathBuffer) + "/" + std::string(s_ProjectNameBuffer) : "";
			ImGui::Text("Full Project Path: %s", fullProjectPath.c_str());
			fontsLib.PopFont();

			ImGui::SetNextItemWidth(-1);
			ImGui::InputTextWithHint("##new_project_name", "Project Name", s_ProjectNameBuffer, c_MAX_PROJECT_NAME_LENGTH);

			ImGui::SetNextItemWidth(-1);
			ImGui::InputTextWithHint("##new_project_start_scene", "Start Scene Name", s_ProjectStartSceneNameBuffer, c_MAX_PROJECT_NAME_LENGTH);

			ImVec2 label_size = ImGui::CalcTextSize("...", NULL, true);
			auto& style = ImGui::GetStyle();
			ImVec2 button_size = ImGui::CalcItemSize(ImVec2(0, 0), label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

			ImGui::SetNextItemWidth(590 - button_size.x - style.FramePadding.x * 2.0f - style.ItemInnerSpacing.x - 1);
			ImGui::InputTextWithHint("##new_project_location", "Project Location", s_NewProjectFilePathBuffer, c_MAX_PROJECT_FILEPATH_LENGTH);

			ImGui::SameLine();

			if (ImGui::Button("..."))
			{
				std::string result = FileSystem::OpenFolderDialog().string();
				memcpy(s_NewProjectFilePathBuffer, result.data(), result.length());
			}

			ImGui::Separator();

			fontsLib.PushFont("RobotoBold");
			if (ImGui::Button("Create") || Input::IsKeyDown(KeyCode::Enter))
			{
				CreateProject(fullProjectPath);
				m_ShowNewProjectModal = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel") || Input::IsKeyDown(KeyCode::Escape))
			{
				memset(s_NewProjectFilePathBuffer, 0, c_MAX_PROJECT_FILEPATH_LENGTH);
				m_ShowNewProjectModal = false;
				ImGui::CloseCurrentPopup();
			}

			fontsLib.PopFont();

			ImGui::PopStyleVar();

			ImGui::EndPopup();
		}
	}

	void EditorLayer::DeleteEntity(Entity entity)
	{
		if (!entity)
			return;

		m_CurrentScene->DestroyEntity(entity);
	}

	void EditorLayer::UI_ShowViewport()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Viewport", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		m_ViewportPanelMouseOver = ImGui::IsWindowHovered();
		m_ViewportPanelFocused = ImGui::IsWindowFocused();	

		// Set to true so that we we activate the EditorCamera in the SceneState::Pause
		if (m_AllowEditorCameraInRuntime)
			m_ViewportPanelMouseOver = true;

		ImVec2 viewportOffset = ImGui::GetCursorPos(); // Includes tab bar
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		m_ViewportRenderer->SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y), m_ViewportRenderer->GetSpecification().RendererScale);
		m_CurrentScene->SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y));
		m_EditorCamera.SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y));

		UI::Image(m_ViewportRenderer->GetFinalPassImage(), viewportSize, { 0, 1 }, { 1, 0 });

		m_ViewportRect = UI::GetWindowRect();

		m_AllowViewportCameraEvents = (ImGui::IsMouseHoveringRect(m_ViewportRect.Min, m_ViewportRect.Max) && m_ViewportPanelFocused) || m_StartedCameraClickInViewport;

		UI_DrawViewportIcons();

		if (m_ShowGizmos)
			UI_DrawGizmos();

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void EditorLayer::UI_ShowFontsPanel()
	{
		ImGui::Begin("Fonts", nullptr, ImGuiWindowFlags_NoCollapse);

		ImGuiFontsLibrary& fontsLib = Application::Get().GetImGuiLayer()->GetFontsLibrary();

		for (auto& [name, font] : fontsLib.GetFonts())
		{
			ImGui::Columns(2);

			ImGui::TextUnformatted(name.c_str());

			ImGui::NextColumn();

			if (ImGui::Button(fmt::format("Set Default##{0}", name).c_str()))
				fontsLib.SetDefaultFont(name);

			ImGui::Columns(1);
		}

		ImGui::End();
	}

	void EditorLayer::SC_Shift1()
	{
		m_CurrentlySelectedRenderIcon = EditorResources::LitMaterialIcon;
		s_CurrentlySelectedRenderOption = "Lit";
		m_ViewportRenderer->SetViewMode(SceneRenderer::ViewMode::Lit);
	}

	void EditorLayer::SC_Shift2()
	{
		m_CurrentlySelectedRenderIcon = EditorResources::UnLitMaterialIcon;
		s_CurrentlySelectedRenderOption = "Unlit";
		m_ViewportRenderer->SetViewMode(SceneRenderer::ViewMode::Unlit);
	}

	void EditorLayer::SC_Shift3()
	{
		m_CurrentlySelectedRenderIcon = EditorResources::WireframeViewIcon;
		s_CurrentlySelectedRenderOption = "Wireframe";
		m_ViewportRenderer->SetViewMode(SceneRenderer::ViewMode::Wireframe);
	}

	void EditorLayer::SC_F()
	{						
		// Since Ctrl + F is a different shortcut
		if (Input::IsKeyDown(KeyCode::LeftControl))
			return;

		if (SelectionManager::GetSelectionCount(SelectionContext::Scene) == 0)
			return;

		UUID selectedEntityID = SelectionManager::GetSelections(SelectionContext::Scene).front();
		Entity selectedEntity = m_CurrentScene->GetEntityWithUUID(selectedEntityID);
		m_EditorCamera.Focus(m_CurrentScene->GetWorldSpaceTransform(selectedEntity).Translation);
	}

	void EditorLayer::SC_H()
	{
		// Since Ctrl + H is a different shortcut
		if (Input::IsKeyDown(KeyCode::LeftControl))
			return;

		if (SelectionManager::GetSelectionCount(SelectionContext::Scene) == 0)
			return;

		const std::vector<UUID>& selectedEntityIDs = SelectionManager::GetSelections(SelectionContext::Scene);
		for (const UUID entityID : selectedEntityIDs)
		{
			Entity selectedEntity = m_CurrentScene->GetEntityWithUUID(entityID);
			if (selectedEntity.HasComponent<VisibleComponent>())
				selectedEntity.RemoveComponent<VisibleComponent>();
			else
				selectedEntity.AddComponent<VisibleComponent>();
		}
	}

	void EditorLayer::SC_Delete()
	{
		std::vector<UUID> selectedEntities = SelectionManager::GetSelections(SelectionContext::Scene);
		for (auto entity : selectedEntities)
			DeleteEntity(m_CurrentScene->TryGetEntityWithUUID(entity));
	}

	void EditorLayer::SC_CtrlD()
	{
		std::vector<UUID> selectedEntities = SelectionManager::GetSelections(SelectionContext::Scene);
		for (const auto& entityID : selectedEntities)
		{
			Entity entity = m_CurrentScene->TryGetEntityWithUUID(entityID);

			if (entity.GetParent())
				continue;

			Entity duplicate = m_CurrentScene->DuplicateEntity(entity);
			SelectionManager::Deselect(SelectionContext::Scene, entity.GetUUID());
			SelectionManager::Select(SelectionContext::Scene, duplicate.GetUUID());
		}
	}

	void EditorLayer::SC_CtrlF()
	{
		Window& window = Application::Get().GetWindow();
		GLFWwindow* nativeWindow = window.GetNativeWindow();
		GLFWmonitor* monitor = window.GetNativePrimaryMonitor();

		static glm::uvec2 previousPos = {};
		static glm::uvec2 previousSize = {};

		if (Application::Get().GetWindow().IsFullScreen())
		{
			glfwSetWindowMonitor(nativeWindow, nullptr, previousPos.x, previousPos.y, previousSize.x, previousSize.y, 0);

			m_ShowOnlyViewport = false;
		}
		else
		{
			glfwGetWindowPos(nativeWindow, reinterpret_cast<int*>(&previousPos.x), reinterpret_cast<int*>(&previousPos.y));
			glfwGetWindowSize(nativeWindow, reinterpret_cast<int*>(&previousSize.x), reinterpret_cast<int*>(&previousSize.y));
			const GLFWvidmode* vidMode = glfwGetVideoMode(monitor);
			glfwSetWindowMonitor(nativeWindow, monitor, 0, 0, vidMode->width, vidMode->height, 0);

			m_ShowOnlyViewport = true;
		}

		Application::Get().DispatchEvent<Events::RenderViewportOnlyEvent>(m_ShowOnlyViewport);
	}

	void EditorLayer::SC_CtrlH(bool useSlashState)
	{
		auto entities = m_CurrentScene->GetAllEntitiesWith<IDComponent>();
		for (auto entity : entities)
		{
			Entity curr = { entity, m_CurrentScene.Raw() };
			if (useSlashState)
			{
				if (m_SC_SlashKeyState && !curr.HasComponent<VisibleComponent>())
					curr.AddComponent<VisibleComponent>();
				else if (curr.HasComponent<VisibleComponent>())
					curr.RemoveComponent<VisibleComponent>();
			}
			else
			{
				if (curr.HasComponent<VisibleComponent>())
					curr.RemoveComponent<VisibleComponent>();
				else
					curr.AddComponent<VisibleComponent>();
			}
		}
	}

	void EditorLayer::SC_Slash()
	{
		// Focus on a median point if multiple objects are selected or on the object itself if only one object is selected
		if (SelectionManager::GetSelectionCount(SelectionContext::Scene) == 0)
			return;

		// Hide all the hidable objects in the scene
		SC_CtrlH(true);

		glm::vec3 medianPoint = { 0.0f, 0.0f, 0.0f };
		const std::vector<UUID>& selectedEntityIDs = SelectionManager::GetSelections(SelectionContext::Scene);
		for (uint32_t i = 0; i < selectedEntityIDs.size(); i++)
		{
			Entity selectedEntity = m_CurrentScene->GetEntityWithUUID(selectedEntityIDs[i]);
			medianPoint += selectedEntity.Transform().Translation;

			if (!selectedEntity.HasComponent<VisibleComponent>())
				selectedEntity.AddComponent<VisibleComponent>();
		}

		medianPoint /= selectedEntityIDs.size();

		m_EditorCamera.Focus(medianPoint);

		// Alternate to next state
		m_SC_SlashKeyState = !m_SC_SlashKeyState;
	}

	void EditorLayer::SC_AltC()
	{
		m_AllowEditorCameraInRuntime = !m_AllowEditorCameraInRuntime;
	}

	void EditorLayer::OnEvent(Events::Event& e)
	{
		AssetEditorPanel::OnEvent(e);
		m_PanelsManager->OnEvent(e);

		if (m_SceneState == SceneState::Edit)
		{
			if (m_AllowViewportCameraEvents)
				m_EditorCamera.OnEvent(e);
		}

		Events::EventDispatcher dispatcher(e);
		dispatcher.Dispatch<Events::KeyPressedEvent>([this](Events::KeyPressedEvent& e) { return OnKeyPressed(e); });
		dispatcher.Dispatch<Events::MouseButtonPressedEvent>([this](Events::MouseButtonPressedEvent& e) { return OnMouseButtonPressed(e); });
		dispatcher.Dispatch<Events::WindowTitleBarHitTestEvent>([this](Events::WindowTitleBarHitTestEvent& e)
		{
			e.SetHit(UI_TitleBarHitTest(e.GetX(), e.GetY()));
			return true;
		});
		dispatcher.Dispatch<Events::TitleBarColorChangeEvent>([this](Events::TitleBarColorChangeEvent& e) { return OnTitleBarColorChange(e); });
		dispatcher.Dispatch<Events::WindowCloseEvent>([this](Events::WindowCloseEvent& e)
		{
			if ((m_SceneState == SceneState::Play) || (m_SceneState == SceneState::Pause))
				OnSceneStop();

			return false;
		});
	}

	bool EditorLayer::OnKeyPressed(Events::KeyPressedEvent& e)
	{
		if (Input::IsKeyDown(KeyCode::LeftShift))
		{
			int n = 0;
			// We only serialize the panels manager for the view panels and not the Edit panels since the Edit ones should always be closed on startup
			for (auto& [id, panelSpec] : m_PanelsManager->GetPanels(PanelCategory::View))
			{
				KeyCode key = static_cast<KeyCode>(static_cast<int>(KeyCode::F1) + n++);
				if (key == e.GetKeyCode())
				{
					panelSpec.IsOpen = !panelSpec.IsOpen;
					m_PanelsManager->Serialize();

					break;
				}
			}
		}

		if (UI::IsWindowFocused("Viewport") || UI::IsWindowFocused("Scene Hierarchy"))
		{
			if (Input::IsKeyDown(KeyCode::LeftShift))
			{
				switch (e.GetKeyCode())
				{
					case KeyCode::Key1:
					{
						SC_Shift1();

						break;
					}
					case KeyCode::Key2:
					{
						SC_Shift2();

						break;
					}
					case KeyCode::Key3:
					{
						SC_Shift3();

						break;
					}
				}
			}

			if (m_ViewportPanelMouseOver && !Input::IsMouseButtonDown(MouseButton::Right))
			{
				switch (e.GetKeyCode())
				{
					case KeyCode::Q:
						m_GizmoType = -1;
						break;
					case KeyCode::W:
						m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
						break;
					case KeyCode::E:
						m_GizmoType = ImGuizmo::OPERATION::ROTATE;
						break;
					case KeyCode::R:
						m_GizmoType = ImGuizmo::OPERATION::SCALE;
						break;
					case KeyCode::F:
					{
						SC_F();

						break;
					}
					case KeyCode::H:
					{
						SC_H();

						break;
					}
					case KeyCode::Slash:
					{
						SC_Slash();

						break;
					}
				}
			}

			switch (e.GetKeyCode())
			{
				case KeyCode::Escape:
				{
					SelectionManager::DeselectAll();
					break;
				}
				case KeyCode::Delete:
				{
					SC_Delete();
					break;
				}
			}
		}

		if (Input::IsKeyDown(KeyCode::LeftControl) && !Input::IsMouseButtonDown(MouseButton::Right))
		{
			switch (e.GetKeyCode())
			{
				// Toggle Grid
				case KeyCode::G:
					m_ViewportRenderer->GetOptions().ShowGrid = !m_ViewportRenderer->GetOptions().ShowGrid;
					break;
				// Toggle Bounding Boxes
				case KeyCode::B:
					m_ShowBoundingBoxes = !m_ShowBoundingBoxes;
					break;
				// Duplicate Selected Entities
				case KeyCode::D:
				{
					SC_CtrlD();
					
					break;
				}
				case KeyCode::S:
				{
					if (Input::IsKeyDown(KeyCode::LeftShift))
						SaveSceneAs();
					else
						SaveScene();
					
					break;
				}
				case KeyCode::N:
				{
					m_ShowNewSceneModal = true;
					break;
				}
				case KeyCode::O:
				{
					if (Input::IsKeyDown(KeyCode::LeftShift))
						OpenScene();
					else
						OpenProject();

					break;
				}
				case KeyCode::F:
				{
					SC_CtrlF();

					break;
				}
				case KeyCode::H:
				{
					SC_CtrlH();

					break;
				}
			}
		}

		if (m_SceneState == SceneState::Play && e.GetKeyCode() == KeyCode::Escape)
			Input::SetCursorMode(CursorMode::Normal);

		if (m_SceneState == SceneState::Play && Input::IsKeyDown(KeyCode::LeftAlt))
		{
			switch (e.GetKeyCode())
			{
				case KeyCode::C:
				{
					SC_AltC();

					break;
				}
			}
		}

		return false;
	}

	bool EditorLayer::OnMouseButtonPressed(Events::MouseButtonPressedEvent& e)
	{
		if (e.GetMouseButton() != MouseButton::Left)
			return false;

		// EditorLayer handles viewport events only... other panels handle the events separatly
		if (!m_ViewportPanelMouseOver)
			return false;

		if (Input::IsKeyDown(KeyCode::LeftAlt) || Input::IsMouseButtonDown(MouseButton::Right))
			return false;

		if (ImGuizmo::IsOver())
			return false;

		ImGui::ClearActiveID();

		std::vector<SelectionData> selectionData;
		auto [mouseX, mouseY] = GetMouseInViewportSpace();
		if (mouseX > -1.0f && mouseX < 1.0f && mouseY > -1.0f && mouseY < 1.0f)
		{
			auto [origin, direction] = CastRay(mouseX, mouseY);
			
			auto meshEntities = m_CurrentScene->GetAllEntitiesWith<StaticMeshComponent>();
			for (auto e : meshEntities)
			{
				Entity entity = { e, m_CurrentScene.Raw() };
				if (!entity.HasComponent<VisibleComponent>())
					continue;
				
				StaticMeshComponent& mc = entity.GetComponent<StaticMeshComponent>();

				// We get it async so that if we are loading asset we do not block the main thread
				Ref<StaticMesh> staticMesh = AssetManager::GetAssetAsync<StaticMesh>(mc.StaticMesh);
				if (staticMesh)
				{
					// We get it async so that if we are loading asset we do not block the main thread
					Ref<MeshSource> meshSource = AssetManager::GetAssetAsync<MeshSource>(staticMesh->GetMeshSource());
					if (meshSource)
					{
						auto& subMeshes = meshSource->GetSubMeshes();
						for (uint32_t i = 0; i < subMeshes.size(); i++)
						{
							MeshUtils::SubMesh& subMesh = subMeshes[i];

							glm::mat4 transform = m_CurrentScene->GetWorldSpaceTransformMatrix(entity);
							Ray ray = {
								glm::inverse(transform * subMesh.Transform) * glm::vec4(origin, 1.0f),
								glm::inverse(glm::mat3(transform * subMesh.Transform)) * direction
							};

							float t;
							bool intersects = ray.IntersectsAABB(subMesh.BoundingBox, t);
							if (intersects)
							{
								const auto& triangleCache = meshSource->GetTriangleCache(i);
								for (const auto& triangle : triangleCache)
								{
									if (ray.IntersectsTriangle(triangle.V0.Position, triangle.V1.Position, triangle.V2.Position, t))
									{
										selectionData.push_back({ entity, &subMesh, t });
										break;
									}
								}
							}
						}
					}
				}
			}

			std::sort(selectionData.begin(), selectionData.end(), [](auto& a, auto& b) { return a.Distance < b.Distance; });

			bool ctrlDown = Input::IsKeyDown(KeyCode::LeftControl) || Input::IsKeyDown(KeyCode::RightControl);
			bool shiftDown = Input::IsKeyDown(KeyCode::LeftShift) || Input::IsKeyDown(KeyCode::RightShift);

			if (!ctrlDown)
				SelectionManager::DeselectAll();

			if (!selectionData.empty())
			{
				Entity entity = selectionData.front().Entity;
				if (shiftDown)
				{
					while (entity.GetParent())
					{
						entity = entity.GetParent();
					}
				}

				if (SelectionManager::IsSelected(SelectionContext::Scene, entity.GetUUID()) && ctrlDown)
					SelectionManager::Deselect(SelectionContext::Scene, entity.GetUUID());
				else
					SelectionManager::Select(SelectionContext::Scene, entity.GetUUID());
			}
		}

		return false;
	}

	std::pair<float, float> EditorLayer::GetMouseInViewportSpace() const
	{
		auto [mx, my] = ImGui::GetMousePos();

		mx -= m_ViewportRect.Min.x;
		my -= m_ViewportRect.Min.y;
		ImVec2 viewportSize = { m_ViewportRect.Max.x - m_ViewportRect.Min.x, m_ViewportRect.Max.y - m_ViewportRect.Min.y };
		my = viewportSize.y - my; // Invert my

		return { (mx / viewportSize.x) * 2.0f - 1.0f, (my / viewportSize.y) * 2.0f - 1.0f };
	}

	std::pair<glm::vec3, glm::vec3> EditorLayer::CastRay(float x, float y)
	{
		glm::vec4 mouseClipPos = { x, y, -1.0f, 1.0f };

		auto inverseProj = glm::inverse(m_EditorCamera.GetProjectionMatrix());
		auto inverseView = glm::inverse(glm::mat3(m_EditorCamera.GetViewMatrix()));

		glm::vec3 rayPos = m_EditorCamera.GetPosition();
		glm::vec4 ray = inverseProj * mouseClipPos;
		if (!m_EditorCamera.IsPerspectiveProjection())
			ray.z = -1.0f;
		glm::vec3 direction = inverseView * ray;

		return { rayPos, m_EditorCamera.IsPerspectiveProjection() ? direction : glm::normalize(direction) };
	}

	void EditorLayer::SceneHierarchySetEditorCameraTransform(Entity entity)
	{
		TransformComponent& tc = entity.Transform();
		tc.SetTransform(glm::inverse(m_EditorCamera.GetViewMatrix()));
	}

	bool EditorLayer::OnTitleBarColorChange(Events::TitleBarColorChangeEvent& e)
	{
		m_AnimateTitleBarColor = true;
		m_TitleBarPreviousColor = m_TitleBarActiveColor;
		m_TitleBarTargetColor = e.GetTargetColor();

		return true;
	}

	void EditorLayer::OpenProject()
	{
		std::filesystem::path filePath = FileSystem::OpenFileDialog({ { "Iris Project (*.Iproj)", "Iproj" } });
		if (filePath.empty())
			return;

		// stash the filepath away.  Actual opening of project is deferred until it is "safe" to do so.
		strcpy(s_OpenProjectFilePathBuffer, filePath.string().data());

		RecentProject projectEntry;
		projectEntry.Name = Utils::RemoveExtension(filePath.filename().string());
		projectEntry.Filepath = filePath.string();
		projectEntry.LastOpened = time(NULL);

		for (auto it = m_UserPreferences->RecentProjects.begin(); it != m_UserPreferences->RecentProjects.end(); it++)
		{
			if (it->second.Name == projectEntry.Name)
			{
				m_UserPreferences->RecentProjects.erase(it);
				break;
			}
		}

		m_UserPreferences->RecentProjects[projectEntry.LastOpened] = projectEntry;
		UserPreferencesSerializer::Serialize(m_UserPreferences, m_UserPreferences->Filepath);
	}

	void EditorLayer::OpenProject(const std::filesystem::path& filePath)
	{
		if (!FileSystem::Exists(filePath))
		{
			IR_CORE_ERROR("Tried to open a project that does not exist. Project Path: {0}", filePath);
			memset(s_OpenProjectFilePathBuffer, 0, c_MAX_PROJECT_FILEPATH_LENGTH);
			return;
		}

		RendererContext::WaitDeviceIdle();

		if (Project::GetActive())
			CloseProject();

		Ref<Project> project = Project::Create();
		ProjectSerializer::Deserialize(project, filePath);
		Project::SetActive(project);

		m_PanelsManager->OnProjectChanged(project);

		bool startSceneFileExists = FileSystem::Exists(Project::GetAssetDirectory() / project->GetConfig().StartScene);
		bool hasScene = !project->GetConfig().StartScene.empty() && startSceneFileExists;
		if (hasScene)
			hasScene = OpenScene((Project::GetAssetDirectory() / project->GetConfig().StartScene).string());
		else
		{
			if (startSceneFileExists)
				m_SceneFilePath = (Project::GetAssetDirectory() / project->GetConfig().StartScene).string();
			NewScene();
		}

		SelectionManager::DeselectAll();

		m_EditorCamera = EditorCamera(45.0f, 1280.0f, 720.0f, 0.01f, 1000.0f);

		memset(s_ProjectNameBuffer, 0, c_MAX_PROJECT_NAME_LENGTH);
		memset(s_OpenProjectFilePathBuffer, 0, c_MAX_PROJECT_FILEPATH_LENGTH);
		memset(s_NewProjectFilePathBuffer, 0, c_MAX_PROJECT_FILEPATH_LENGTH);
		memset(s_ProjectStartSceneNameBuffer, 0, c_MAX_PROJECT_NAME_LENGTH);
	}

	void EditorLayer::CreateProject(std::filesystem::path projectPath)
	{
		if (!FileSystem::Exists(projectPath))
			FileSystem::CreateDirectory(projectPath);

		std::filesystem::copy("Resources/NewProjectTemplate", projectPath, std::filesystem::copy_options::recursive);
		std::filesystem::path irisRootDirectory = std::filesystem::absolute("./Resources").parent_path().string();
		std::string irisRootDirectoryString = irisRootDirectory.string();

		if (irisRootDirectory.stem().string() == "IrisEditor")
			irisRootDirectory = irisRootDirectory.parent_path().string();

		std::replace(irisRootDirectoryString.begin(), irisRootDirectoryString.end(), '\\', '/');

		{
			// Project File
			std::ifstream stream(projectPath / "Project.Iproj");
			IR_VERIFY(stream.is_open());
			std::stringstream ss;
			ss << stream.rdbuf();
			stream.close();

			std::string str = ss.str();
			Utils::ReplaceToken(str, "$PROJECT_NAME$", s_ProjectNameBuffer);

			Utils::ReplaceToken(str, "$START_SCENE_NAME$", fmt::format("{}.Iscene", s_ProjectStartSceneNameBuffer));

			std::ofstream ostream(projectPath / "Project.Iproj");
			ostream << str;
			ostream.close();

			std::string newProjectFileName = std::string(s_ProjectNameBuffer) + ".Iproj";
			std::filesystem::rename(projectPath / "Project.Iproj", projectPath / newProjectFileName);
		}

		std::filesystem::create_directories(projectPath / "Assets" / "Materials");
		std::filesystem::create_directories(projectPath / "Assets" / "Meshes" / "Default");
		std::filesystem::create_directories(projectPath / "Assets" / "Meshes" / "Default" / "Source");
		std::filesystem::create_directories(projectPath / "Assets" / "Scenes");
		std::filesystem::create_directories(projectPath / "Assets" / "Textures");
		std::filesystem::create_directories(projectPath / "Assets" / "EnvironmentMaps");

		// NOTE: Copying meshes from resources, change this in the future to just a vertex buffer thats built into 
		// the engine since we wouldn't really want to modify these meshes and hence there's no point to have gltf files...
		{
			std::filesystem::path originalFilePath = irisRootDirectoryString + "/Resources/Meshes/Default";
			std::filesystem::path targetPath = projectPath / "Assets" / "Meshes" / "Default" / "Source";
			IR_VERIFY(std::filesystem::exists(originalFilePath));

			for (const auto& dirEntry : std::filesystem::directory_iterator(originalFilePath))
				std::filesystem::copy(dirEntry.path(), targetPath);
		}

		{
			RecentProject projectEntry;
			projectEntry.Name = s_ProjectNameBuffer;
			projectEntry.Filepath = (projectPath / (std::string(s_ProjectNameBuffer) + ".Iproj")).string();
			projectEntry.LastOpened = time(NULL);
			m_UserPreferences->RecentProjects[projectEntry.LastOpened] = projectEntry;

			UserPreferencesSerializer::Serialize(m_UserPreferences, m_UserPreferences->Filepath);
		}

		OpenProject(projectPath.string() + "/" + std::string(s_ProjectNameBuffer) + ".Iproj");
	}

	void EditorLayer::EmptyProject()
	{
		if (Project::GetActive())
			CloseProject();

		Ref<Project> project = Project::Create();
		Project::SetActive(project);

		m_PanelsManager->OnProjectChanged(project);
		NewScene("Editor Scene");

		SelectionManager::DeselectAll();

		m_EditorCamera = EditorCamera(45.0f, 1280.0f, 720.0f, 0.01f, 1000.0f);

		memset(s_ProjectNameBuffer, 0, c_MAX_PROJECT_NAME_LENGTH);
		memset(s_OpenProjectFilePathBuffer, 0, c_MAX_PROJECT_FILEPATH_LENGTH);
		memset(s_NewProjectFilePathBuffer, 0, c_MAX_PROJECT_FILEPATH_LENGTH);
		memset(s_ProjectStartSceneNameBuffer, 0, c_MAX_PROJECT_NAME_LENGTH);
	}

	void EditorLayer::SaveProject()
	{
		if (!Project::GetActive())
			IR_VERIFY(false); 

		auto project = Project::GetActive();
		ProjectSerializer::Serialize(project, fmt::format("{}/{}", project->GetConfig().ProjectDirectory, project->GetConfig().ProjectFileName));

		m_PanelsManager->Serialize();
	}

	void EditorLayer::CloseProject(bool unloadProject)
	{
		SaveProject();

		// There are several things holding references to the scene.
		// These all need to be set back to nullptr so that you end up with a zero reference count and the scene is destroyed.
		// If you do not do this, then after opening the new project (and possibly loading script assemblies that are un-related to the old scene)
		// you still have the old scene kicking around => Not ideal.
		// Of course you could just wait until all of these things (eventually) get pointed to the new scene, and then when the old scene is
		// finally decref'd to zero it will be destroyed.  However, that will be after the project has been closed, and the new project has
		// been opened, which is out of order.  Seems safter to make sure the old scene is cleaned up before closing project.
		m_PanelsManager->SetSceneContext(nullptr);
		AssetEditorPanel::SetSceneContext(nullptr);
		m_ViewportRenderer->SetScene(nullptr);
		m_CurrentScene = nullptr;

		// Check that m_EditorScene is the last one (so setting it null here will destroy the scene)
		IR_ASSERT(m_EditorScene->GetRefCount() == 1, "Scene will not be destroyed after project is closed - something is still holding scene refs!");
		m_EditorScene = nullptr;

		if (unloadProject)
			Project::SetActive(nullptr);
	}

	void EditorLayer::NewScene(const std::string& name)
	{
		SelectionManager::DeselectAll();

		m_EditorScene = Scene::Create(name, true);
		m_CurrentScene = m_EditorScene;

		m_PanelsManager->SetSceneContext(m_CurrentScene);
		AssetEditorPanel::SetSceneContext(m_CurrentScene);

		m_SceneFilePath = std::string();

		m_EditorCamera = EditorCamera(45.0f, 1280.0f, 720.0f, 0.01f, 1000.0f);

		if (m_ViewportRenderer)
			m_ViewportRenderer->SetScene(m_CurrentScene);
	}

	bool EditorLayer::OpenScene()
	{
		std::filesystem::path filepath = FileSystem::OpenFileDialog({ { "Iris Scene (*.Iscene)", "Iscene" } });
		if (!filepath.empty())
			return OpenScene(filepath);

		return false;
	}

	bool EditorLayer::OpenScene(const std::filesystem::path filePath)
	{
		if (!FileSystem::Exists(filePath))
		{
			IR_CORE_ERROR("Tried loading a non-existing scene: {0}", filePath);
			return false;
		}

		if (m_SceneState == SceneState::Play)
			OnSceneStop();

		Ref<Scene> newScene = Scene::Create("New Scene", true);
		SceneSerializer::Deserialize(newScene, filePath);

		m_EditorScene = newScene;
		m_CurrentScene = m_EditorScene;
		m_SceneFilePath = filePath.string();

		std::replace(m_SceneFilePath.begin(), m_SceneFilePath.end(), '\\', '/');
		if ((m_SceneFilePath.size() >= 5) && (m_SceneFilePath.substr(m_SceneFilePath.size() - 5) == ".auto"))
			m_SceneFilePath = m_SceneFilePath.substr(0, m_SceneFilePath.size() - 5);

		m_PanelsManager->SetSceneContext(m_CurrentScene);
		AssetEditorPanel::SetSceneContext(m_CurrentScene);

		SelectionManager::DeselectAll();


		if (m_ViewportRenderer)
			m_ViewportRenderer->SetScene(m_CurrentScene);

		return true;
	}

	bool EditorLayer::OpenScene(const AssetMetaData& metaData)
	{
		std::filesystem::path workingDirPath = Project::GetAssetDirectory() / metaData.FilePath;
		return OpenScene(workingDirPath.string());
	}

	void EditorLayer::SaveScene()
	{
		if (!m_SceneFilePath.empty())
		{
			SceneSerializer::Serialize(m_CurrentScene, std::filesystem::path(m_SceneFilePath));
		}
		else
		{
			SaveSceneAs();
		}
	}

	// Auto save the scene.
	// We save to different files so as not to overwrite the manually saved scene.
	// (the manually saved scene is the authoritative source, auto saved scene is just a backup)
	// If scene has never been saved, then do nothing (there is no nice option for where to save the scene to in this situation)
	void EditorLayer::SaveSceneAuto()
	{
		if (!m_SceneFilePath.empty())
		{
			SceneSerializer::Serialize(m_EditorScene, m_SceneFilePath + ".auto"); // this isn't perfect as there is a non-zero chance (admittedly small) of overwriting some existing .auto file that the user actually wanted)

			m_TimeSinceLastSave = 0.0f;
		}
	}

	void EditorLayer::SaveSceneAs()
	{
		std::filesystem::path filepath = FileSystem::SaveFileDialog({ { "Iris Scene (*.Iscene)", "Iscene" } });

		if (filepath.empty())
			return;

		if (!filepath.has_extension())
			filepath += ".Iscene";

		SceneSerializer::Serialize(m_CurrentScene, filepath);

		std::filesystem::path path = filepath;
		m_SceneFilePath = filepath.string();
		std::replace(m_SceneFilePath.begin(), m_SceneFilePath.end(), '\\', '/');
	}

}