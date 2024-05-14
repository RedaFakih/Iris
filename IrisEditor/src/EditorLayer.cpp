#include "EditorLayer.h"

#include "AssetManager/Importers/MeshImporter.h"
#include "Core/Input/Input.h"
#include "Core/Ray.h"
#include "Editor/AssetEditorPanel.h"
#include "Editor/EditorResources.h"
#include "Editor/Panels/ECSDebugPanel.h"
#include "Editor/Panels/SceneHierarchyPanel.h"
#include "Editor/Panels/ShadersPanel.h"
#include "ImGui/ImGuizmo.h"
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

// TODO: REMOVE
// #include <stb/stb_image_writer/stb_image_write.h>

namespace Iris {

	constexpr int c_MAX_PROJECT_NAME_LENGTH = 255;
	constexpr int c_MAX_PROJECT_FILEPATH_LENGTH = 512;
	static char* s_ProjectNameBuffer = new char[c_MAX_PROJECT_NAME_LENGTH];
	static char* s_OpenProjectFilePathBuffer = new char[c_MAX_PROJECT_FILEPATH_LENGTH];
	static char* s_NewProjectFilePathBuffer = new char[c_MAX_PROJECT_FILEPATH_LENGTH];

	EditorLayer::EditorLayer()
		: Layer("EditorLayer"), m_EditorCamera(45.0f, 1280.0f, 720.0f, 0.01f, 10000.0f)
	{
		IR_VERIFY(!s_EditorLayerInstance, "No more than 1 EditorLayer can be created!");
		s_EditorLayerInstance = this;

		m_TitleBarPreviousColor = Colors::Theme::TitlebarRed;
		m_TitleBarTargetColor   = Colors::Theme::TitlebarCyan;
	}

	EditorLayer::~EditorLayer()
	{
	}

	void EditorLayer::OnAttach()
	{
		EditorResources::Init();
		m_CurrentlySelectedViewIcon = EditorResources::PerspectiveIcon;
		m_CurrentlySelectedRenderIcon = EditorResources::LitMaterialIcon;
		m_PanelsManager = PanelsManager::Create();

		Ref<SceneHierarchyPanel> sceneHierarchyPanel = m_PanelsManager->AddPanel<SceneHierarchyPanel>(PanelCategory::View, "SceneHierarchyPanel", "Scene Hierarchy", true, m_EditorScene, SelectionContext::Scene);
		sceneHierarchyPanel->SetEntityDeletedCallback([this](Entity entity) { OnEntityDeleted(entity); });
		sceneHierarchyPanel->AddEntityContextMenuPlugin([this](Entity entity) { SceneHierarchySetEditorCameraTransform(entity); });

		m_PanelsManager->AddPanel<ShadersPanel>(PanelCategory::View, "ShadersPanel", "Shaders", false);

		if (!Project::GetActive())
			EmptyProject();

		// TODO: REMOVE since this should be handled by NewScene and all that stuff
		m_EditorScene = Scene::Create("Editor Scene");
		m_CurrentScene = m_EditorScene;

		// NOTE: For debugging ECS Problems
		// m_PanelsManager->AddPanel<ECSDebugPanel>(PanelCategory::View, "ECSDebugPanel", "ECS", false, m_CurrentScene);

		m_ViewportRenderer = SceneRenderer::Create(m_CurrentScene, { .RendererScale = 1.0f });
		m_ViewportRenderer->SetLineWidth(m_LineWidth);
		m_Renderer2D = Renderer2D::Create();
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
		AssetManager::SyncWithAssetThread();

		m_PanelsManager->SetSceneContext(m_EditorScene);

		// Set jump flood pass on or off based on if we have selected something in the past frame...
		m_ViewportRenderer->GetSpecification().JumpFloodPass = SelectionManager::GetSelectionCount(SelectionContext::Scene) > 0;

		m_EditorCamera.SetActive(m_AllowViewportCameraEvents);
		m_EditorCamera.OnUpdate(ts);

		m_EditorScene->OnUpdateEditor(ts);
		m_EditorScene->OnRenderEditor(m_ViewportRenderer, ts, m_EditorCamera);

		OnRender2D();

		// TODO: Should be moved to OnKeyPressed
		//if (Input::IsKeyDown(KeyCode::R))
		//{
		//	// TODO: This currently does not work since the final image that is returned is a floating point image
		//	Buffer textureBuffer;
		//	Ref<Texture2D> texture = m_ViewportRenderer->GetFinalPassImage();
		//	texture->CopyToHostBuffer(textureBuffer, true);
		//	stbi_flip_vertically_on_write(true);
		//	stbi_write_jpg("Resources/assets/textures/output.jpg", texture->GetWidth(), texture->GetHeight(), 4, textureBuffer.Data, 100);
		//	stbi_flip_vertically_on_write(false);
		//}

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
		m_Renderer2D->SetTargetFramebuffer(m_ViewportRenderer->GetExternalCompositeFramebuffer());

		if (m_ShowBoundingBoxes)
		{
			if (m_ShowBoundingBoxSelectedMeshOnly)
			{
				const auto& selectedEntites = SelectionManager::GetSelections(SelectionContext::Scene);
				for (const auto& entityID : selectedEntites)
				{
					Entity entity = m_CurrentScene->GetEntityWithUUID(entityID);

					const auto& staticMeshComponent = entity.TryGetComponent<StaticMeshComponent>();
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
									const auto& subMeshIndices = staticMesh->GetSubMeshes();
									const auto& subMeshes = meshSource->GetSubMeshes();

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
				auto staticMeshEntities = m_CurrentScene->GetAllEntitiesWith<StaticMeshComponent>();
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

		// `true` indicates that we transition resulting images to presenting layouts...
		m_Renderer2D->EndScene(true);
	}

	void EditorLayer::OnImGuiRender()
	{
		// IR_CORE_FATAL("Main thread still running...");
		
		UI_StartDocking();

		// ImGui::ShowDemoWindow(); // Testing imgui stuff

		UI_ShowViewport();

		m_PanelsManager->OnImGuiRender();

		// TODO: Move into EditorStyle editor panel
		if (m_ShowImGuiStyleEditor)
		{
			ImGui::Begin("Style Editor", &m_ShowImGuiStyleEditor, ImGuiWindowFlags_NoCollapse);
			static int style;
			ImGui::ShowStyleEditor(0, &style);
			ImGui::End();
		}

		if (m_ShowImGuiMetricsWindow)
			ImGui::ShowMetricsWindow(&m_ShowImGuiMetricsWindow);

		if (m_ShowImGuiStackToolWindow)
			ImGui::ShowStackToolWindow(&m_ShowImGuiStackToolWindow);

		// TODO: PanelManager
		UI_ShowFontsPanel();

		AssetEditorPanel::OnImGuiRender();

		if (m_ShowNewSceneModal)
			UI_ShowNewSceneModal();

		UI_EndDocking();
	}

	void EditorLayer::UI_StartDocking()
	{
		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !m_StartedCameraClickInViewport))
		{
			// TODO: Check if scene is not playing
			ImGui::FocusWindow(GImGui->HoveredWindow);
			Input::SetCursorMode(CursorMode::Normal);
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
		ImGui::Begin("Dockspace Demo", nullptr, window_flags);
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

		const float titleBarHeight = UI_DrawTitleBar();
		ImGui::SetCursorPosY(titleBarHeight + ImGui::GetCurrentWindow()->WindowPadding.y);

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

		const float menuBarLeft = ImGui::GetItemRectMin().x - ImGui::GetCurrentWindow()->Pos.x;
		ImGuiFontsLibrary& fontsLib = Application::Get().GetImGuiLayer()->GetFontsLibrary();

		// TODO: Project name
		{
			UI::ImGuiScopedColor textColor(ImGuiCol_Text, Colors::Theme::TextDarker);
			UI::ImGuiScopedColor border(ImGuiCol_Border, IM_COL32(40, 40, 40, 255));

			const std::string title = "TODO: Project Iris";

			const ImVec2 textSize = ImGui::CalcTextSize(title.c_str());
			const float rightOffset = ImGui::GetWindowWidth() / 5.0f;
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - rightOffset - textSize.x);
			UI::ShiftCursorY(1.0f + windowPadding.y);

			{
				UI::ImGuiScopedFont boldFont(fontsLib.GetFont("RobotoBold"));
				ImGui::TextUnformatted(title.c_str());
			}
			UI::SetToolTip("Current Project (ADD PROJECT NAME HERE)");

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
			const std::string sceneName = m_CurrentScene->GetName();

			ImGui::SetCursorPosX(menuBarLeft);
			UI::ShiftCursorX(13.0f);
			{
				UI::ImGuiScopedFont boldFont(fontsLib.GetFont("RobotoBold"));
				ImGui::TextUnformatted(sceneName.c_str());
			}
			UI::SetToolTip("Current Scene (" + sceneName + ")");

			constexpr float underLineThickness = 2.0f;
			constexpr float underLineExpandWidth = 4.0f;
			ImRect itemRect = UI::RectExpanded(UI::GetItemRect(), underLineExpandWidth, 0.0f);

			// Horizontal line
			// itemRect.Min.y = itemRect.Max.y - underLineThickness;
			// itemRect = UI::RectOffset(itemRect, 0.0f, underLineThickness * 2.0f);

			// Vertical line
			itemRect.Max.x = itemRect.Min.x + underLineThickness;
			itemRect = UI::RectOffset(itemRect, -underLineThickness * 2.0f, 0.0f);

			drawList->AddRectFilled(itemRect.Min, itemRect.Max, Colors::Theme::Muted, 2.0f);
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

					if (ImGui::MenuItem("New Scene...", "Ctrl + N"))
					{
						m_ShowNewSceneModal = !m_ShowNewSceneModal;
					}

					if (ImGui::MenuItem("Open Scene...", "Ctrl + O"))
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

					// We only serialize the panels manager for the view panels and not the Edit panels since the Edit ones should always be closed on startup
					for (auto& [id, panelSpec] : m_PanelsManager->GetPanels(PanelCategory::View))
						if (ImGui::MenuItem(panelSpec.Name, nullptr, &panelSpec.IsOpen))
							m_PanelsManager->Serialize();

					ImGui::Separator();

					if (ImGui::MenuItem("Renderer Info"))
					{
						// TODO:
					}

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

					ImGui::Separator();

					ImGui::MenuItem("Whatever");

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
		}

		UI::EndMenuBar();

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
		// TODO: In runtime scenes if we do not want to show gizmos we should not show the gizmo related icons

		UI::PushID();
		
		UI::ImGuiScopedStyle spacing(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });
		UI::ImGuiScopedStyle windowBorder(ImGuiStyleVar_WindowBorderSize, 0.0f);
		UI::ImGuiScopedStyle rounding(ImGuiStyleVar_WindowRounding, 4.0f);
		UI::ImGuiScopedStyle padding(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

		constexpr float buttonSize = 18.0f;
		constexpr float edgeOffset = 4.0f;
		constexpr float windowHeight = 32.0f; // imgui windows can not be smaller than 32 pixels
		constexpr float numberOfButtons = 5.0f + 3.0f; // we add 3 for the dropdown buttons
		constexpr float popupWidth = 310.0f;
		constexpr float textOffset = 4.0f; // Offset between text and label

		const ImColor SelectedGizmoButtonColor = Colors::Theme::Accent;
		const ImColor UnselectedGizmoButtonColor = Colors::Theme::TextBrighter;

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
			UI::SetToolTip(hint);

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

		auto beginSection = [](const char* name, int& sectionIndex, bool columns2 = true)
		{
			ImGuiFontsLibrary& fontsLib = Application::Get().GetImGuiLayer()->GetFontsLibrary();

			if (sectionIndex > 0)
				UI::ShiftCursorY(5.5f);

			fontsLib.PushFont("RobotoBold");

			float halfHeight = ImGui::CalcTextSize(name).y / 2.0f;
			ImVec2 p1 = { 0.0f, halfHeight };
			ImVec2 p2 = { 14.0f, halfHeight };
			UI::UnderLine(Colors::Theme::TextDarker, false, p1, p2, 3.0f);
			UI::ShiftCursorX(17.0f);

			ImGui::TextUnformatted(name);

			fontsLib.PopFont();

			ImVec2 cursorPos = ImGui::GetCursorPos();
			ImGui::SameLine();

			UI::UnderLine(false, 3.0f, halfHeight, 3.0f, Colors::Theme::TextDarker);
			ImGui::SetCursorPos(cursorPos);

			bool result = ImGui::BeginTable("##section_table", columns2 ? 2 : 1, ImGuiTableFlags_SizingStretchSame);
			if (result)
			{
				ImGui::TableSetupColumn("Labels", ImGuiTableColumnFlags_WidthFixed, popupWidth * 0.5f);
				if (columns2)
					ImGui::TableSetupColumn("Widgets", ImGuiTableColumnFlags_WidthFixed, popupWidth * 0.5f);
			}

			sectionIndex++;
			return result;
		};

		auto endSection = []()
		{
			ImGui::EndTable();
		};

		auto selection = [](const char* label, Ref<Texture2D> icon)
		{
			float height = std::min(static_cast<float>(icon->GetHeight()), buttonSize);
			float width = static_cast<float>(icon->GetWidth()) / static_cast<float>(icon->GetHeight()) * height;

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
			bool clicked = ImGui::InvisibleButton(UI::GenerateID(), { columnWidth, 23.0f });
			// TODO: Add color highlighting for the currently selected
			if (ImGui::IsItemHovered())
				ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), Colors::Theme::BackgroundDarkBlend);
			
			return clicked;
		};

		auto text = [](const char* label, const char* text)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(label);
			ImGui::TableSetColumnIndex(1);
			ImGuiTable* table = ImGui::GetCurrentTable();
			float columnWidth = ImGui::TableGetMaxColumnWidth(table, 1);
			UI::ShiftCursor(columnWidth - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemInnerSpacing.x, -GImGui->Style.FramePadding.y);
			ImGui::TextUnformatted(text);
		};

		auto checkbox = [](const char* label, bool& value, const char* hint = "")
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImVec2 cursorPos = ImGui::GetCursorPos();
			ImGui::TextUnformatted(label);
			ImVec2 size = ImGui::CalcTextSize(label);
			ImVec2 cursorPosToReset = ImGui::GetCursorPos();
			ImGui::SetCursorPos(cursorPos);
			ImGui::InvisibleButton(UI::GenerateID(), size);
			UI::SetToolTip(hint);
			ImGui::SetCursorPos(cursorPosToReset);
			ImGui::TableSetColumnIndex(1);
			ImGuiTable* table = ImGui::GetCurrentTable();
			float columnWidth = ImGui::TableGetMaxColumnWidth(table, 1);
			UI::ShiftCursor(columnWidth - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemInnerSpacing.x, -GImGui->Style.FramePadding.y);
			return UI::Checkbox(UI::GenerateID(), &value);
		};

		auto slider = [](const char* label, float& value, float min = 0.0f, float max = 0.0f, const char* hint = "")
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImVec2 cursorPos = ImGui::GetCursorPos();
			ImGui::TextUnformatted(label);
			ImVec2 size = ImGui::CalcTextSize(label);
			ImVec2 cursorPosToReset = ImGui::GetCursorPos();
			ImGui::SetCursorPos(cursorPos);
			ImGui::InvisibleButton(UI::GenerateID(), size);
			UI::SetToolTip(hint);
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1);
			UI::ShiftCursor(GImGui->Style.FramePadding.x, -GImGui->Style.FramePadding.y);
			return UI::SliderFloat(UI::GenerateID(), &value, min, max);
		};

		auto drag = [](const char* label, float& value, float delta = 1.0f, float min = 0.0f, float max = 0.0f, const char* hint = "")
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImVec2 cursorPos = ImGui::GetCursorPos();
			ImGui::TextUnformatted(label);
			ImVec2 size = ImGui::CalcTextSize(label);
			ImVec2 cursorPosToReset = ImGui::GetCursorPos();
			ImGui::SetCursorPos(cursorPos);
			ImGui::InvisibleButton(UI::GenerateID(), size);
			UI::SetToolTip(hint);
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1);
			UI::ShiftCursor(GImGui->Style.FramePadding.x, -GImGui->Style.FramePadding.y);
			return UI::DragFloat(UI::GenerateID(), &value, delta, min, max);
		};

		auto dropdown = [](const char* label, const char** options, int32_t optionCount, int32_t* selected, const char* hint = "")
		{
			const char* current = options[*selected];
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImVec2 cursorPos = ImGui::GetCursorPos();
			ImGui::TextUnformatted(label);
			ImVec2 size = ImGui::CalcTextSize(label);
			ImVec2 cursorPosToReset = ImGui::GetCursorPos();
			ImGui::SetCursorPos(cursorPos);
			ImGui::InvisibleButton(UI::GenerateID(), size);
			UI::SetToolTip(hint);
			ImGui::TableSetColumnIndex(1);
			ImGui::PushItemWidth(-1);

			bool result = false;
			UI::ShiftCursor(GImGui->Style.FramePadding.x, -GImGui->Style.FramePadding.y);
			if (UI::BeginCombo(UI::GenerateID(), current))
			{
				for (int i = 0; i < optionCount; i++)
				{
					const bool is_selected = (current == options[i]);
					if (ImGui::Selectable(options[i], is_selected))
					{
						current = options[i];
						*selected = i;
						result = true;
					}

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				UI::EndCombo();
			}
			ImGui::PopItemWidth();

			return result;
		};

		// Top Left corner tools and icons
		{
			static const char* currentlySelectedViewOption = "Perspective";
			static const char* currentlySelectedRenderOption = "Lit";
			ImVec2 viewTextSize = ImGui::CalcTextSize(currentlySelectedViewOption);
			ImVec2 renderTextSize = ImGui::CalcTextSize(currentlySelectedRenderOption);
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
			if (labelIconButton(m_CurrentlySelectedRenderIcon, UnselectedGizmoButtonColor, currentlySelectedRenderOption, renderTextSize, windowRect, "Changes the rendering option"))
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
					if (beginSection("Perspective", sectionIndex, false))
					{
						if (selection("Perspective", EditorResources::PerspectiveIcon))
						{
							m_CurrentlySelectedViewIcon = EditorResources::PerspectiveIcon;
							currentlySelectedViewOption = "Perspective";
							m_EditorCamera.SetPerspectiveProjection();

							ImGui::CloseCurrentPopup();
						}

						endSection();
					}

					if (beginSection("Orthographic", sectionIndex, false))
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

						endSection();
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
					if (beginSection("View Mode", sectionIndex, false))
					{
						if (selection("Lit", EditorResources::LitMaterialIcon))
						{
							m_CurrentlySelectedRenderIcon = EditorResources::LitMaterialIcon;
							currentlySelectedRenderOption = "Lit";
							m_ViewportRenderer->SetViewMode(SceneRenderer::ViewMode::Lit);

							ImGui::CloseCurrentPopup();
						}

						if (selection("Unlit", EditorResources::UnLitMaterialIcon))
						{
							m_CurrentlySelectedRenderIcon = EditorResources::UnLitMaterialIcon;
							currentlySelectedRenderOption = "Unlit";
							m_ViewportRenderer->SetViewMode(SceneRenderer::ViewMode::Unlit);

							ImGui::CloseCurrentPopup();
						}

						if (selection("Wireframe", EditorResources::StaticMeshIcon))
						{
							m_CurrentlySelectedRenderIcon = EditorResources::StaticMeshIcon;
							currentlySelectedRenderOption = "Wireframe";
							m_ViewportRenderer->SetViewMode(SceneRenderer::ViewMode::Wireframe);

							ImGui::CloseCurrentPopup();
						}

						endSection();
					}

					ImGui::EndPopup();
				}
			}

			ImGui::End();
		}

		// Top Right corner tools and icons
		{
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
					int sectionIndex = 0;
					if (beginSection("Snapping", sectionIndex, false))
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);

						// Alternating row colors...
						const ImU32 colRowAlternating = UI::ColorWithMultipliedValue(Colors::Theme::BackgroundDarkBlend, 1.3f);
						UI::ImGuiScopedColor tableBGAlternating(ImGuiCol_TableRowBgAlt, colRowAlternating);

						UI::ImGuiScopedColor tableBg(ImGuiCol_ChildBg, Colors::Theme::BackgroundDarkBlend);

						ImGuiTableFlags tableFlags = ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoPadOuterX;
						if (ImGui::BeginTable("##translate_snap_table", 1, tableFlags))
						{
							ImGui::TableSetupColumn("Values", ImGuiTableColumnFlags_WidthFixed, SnappingTableColumnWidth);

							constexpr const char* translationSnapValues[] = { "  10cm", "  50cm", "  1m", "  2m", "  5m", "  10m" };
							float& currentSnappingValue = GetSnapValues().x;
							for (int i = 0; i < 6; i++)
							{
								constexpr float rowHeight = 21.0f;
								ImGui::TableNextRow(0, rowHeight);
								ImGui::TableNextColumn();

								if (ImGui::Selectable(translationSnapValues[i], false, ImGuiSelectableFlags_SpanAllColumns))
								{
									float value = 0.0f;
									switch (i)
									{
										case 0: value = 0.1f; break;
										case 1: value = 0.5f; break;
										case 2: value = 1.0f; break;
										case 3: value = 2.0f; break;
										case 4: value = 5.0f; break;
										case 5: value = 10.0f; break;
									}
									currentSnappingValue = value;
									m_ViewportRenderer->SetTranslationSnapValue(value);

									ImGui::CloseCurrentPopup();
								}
							}

							ImGui::EndTable();
						}

						endSection();
					}

					ImGui::EndPopup();
				}

				if (openRotateSnapPopup)
					ImGui::OpenPopup("Rotate Snap Value");

				width = edgeOffset * 6.0f + buttonSize * 3.0f + edgeOffset * (numberOfButtons - 1.0f) * 2.0f + SnappingTableColumnWidth * 3 + 21.0f;
				ImGui::SetNextWindowPos({ m_ViewportRect.Max.x - width, m_ViewportRect.Min.y + edgeOffset + 30.0f });
				ImGui::SetNextWindowBgAlpha(0.7f);
				if (ImGui::BeginPopup("Rotate Snap Value", ImGuiWindowFlags_NoMove))
				{
					int sectionIndex = 0;
					if (beginSection("Snapping", sectionIndex, false))
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);

						// Alternating row colors...
						const ImU32 colRowAlternating = UI::ColorWithMultipliedValue(Colors::Theme::BackgroundDarkBlend, 1.3f);
						UI::ImGuiScopedColor tableBGAlternating(ImGuiCol_TableRowBgAlt, colRowAlternating);

						UI::ImGuiScopedColor tableBg(ImGuiCol_ChildBg, Colors::Theme::BackgroundDarkBlend);

						ImGuiTableFlags tableFlags = ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoPadOuterX;
						if (ImGui::BeginTable("##rotate_snap_table", 1, tableFlags))
						{
							ImGui::TableSetupColumn("Values", ImGuiTableColumnFlags_WidthFixed, SnappingTableColumnWidth + 10.0f);

							constexpr const char* translationSnapValues[] = { "  5deg", "  10deg", "  30deg", "  45deg", "  90deg", "  180deg" };
							float& currentSnappingValue = GetSnapValues().y;
							for (int i = 0; i < 6; i++)
							{
								constexpr float rowHeight = 21.0f;
								ImGui::TableNextRow(0, rowHeight);
								ImGui::TableNextColumn();

								static bool currentlySelected = false;
								if (ImGui::Selectable(translationSnapValues[i], &currentlySelected, ImGuiSelectableFlags_SpanAllColumns))
								{
									float value = 0.0f;
									switch (i)
									{
										case 0: value = 5.0f; break;
										case 1: value = 10.0f; break;
										case 2: value = 30.0f; break;
										case 3: value = 45.0f; break;
										case 4: value = 90.0f; break;
										case 5: value = 180.0f; break;
									}
									currentSnappingValue = value;

									ImGui::CloseCurrentPopup();
								}
							}

							ImGui::EndTable();
						}

						endSection();
					}

					ImGui::EndPopup();
				}

				if (openScaleSnapPopup)
					ImGui::OpenPopup("Scale Snap Value");

				width = edgeOffset * 6.0f + buttonSize * 1.0f + edgeOffset * (numberOfButtons - 1.0f) * 2.0f + SnappingTableColumnWidth * 3;
				ImGui::SetNextWindowPos({ m_ViewportRect.Max.x - width, m_ViewportRect.Min.y + edgeOffset + 30.0f });
				ImGui::SetNextWindowBgAlpha(0.7f);
				if (ImGui::BeginPopup("Scale Snap Value", ImGuiWindowFlags_NoMove))
				{
					int sectionIndex = 0;
					if (beginSection("Snapping", sectionIndex, false))
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);

						// Alternating row colors...
						const ImU32 colRowAlternating = UI::ColorWithMultipliedValue(Colors::Theme::BackgroundDarkBlend, 1.3f);
						UI::ImGuiScopedColor tableBGAlternating(ImGuiCol_TableRowBgAlt, colRowAlternating);

						UI::ImGuiScopedColor tableBg(ImGuiCol_ChildBg, Colors::Theme::BackgroundDarkBlend);

						ImGuiTableFlags tableFlags = ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoPadOuterX;
						if (ImGui::BeginTable("##rotate_snap_table", 1, tableFlags))
						{
							ImGui::TableSetupColumn("Values", ImGuiTableColumnFlags_WidthFixed, SnappingTableColumnWidth);

							constexpr const char* translationSnapValues[] = { "  10cm", "  50cm", "  1m", "  2m", "  5m", "  10m" };
							float& currentSnappingValue = GetSnapValues().z;
							for (int i = 0; i < 6; i++)
							{
								constexpr float rowHeight = 21.0f;
								ImGui::TableNextRow(0, rowHeight);
								ImGui::TableNextColumn();

								if (ImGui::Selectable(translationSnapValues[i], false, ImGuiSelectableFlags_SpanAllColumns))
								{
									float value = 0.0f;
									switch (i)
									{
										case 0: value = 0.1f; break;
										case 1: value = 0.5f; break;
										case 2: value = 1.0f; break;
										case 3: value = 2.0f; break;
										case 4: value = 5.0f; break;
										case 5: value = 10.0f; break;
									}
									currentSnappingValue = value;

									ImGui::CloseCurrentPopup();
								}
							}

							ImGui::EndTable();
						}

						endSection();
					}

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

						if (beginSection("General", sectionIndex))
						{
							static const char* s_SelectionModes[] = { "Entity", "Submesh" };
							dropdown("Selection Mode", s_SelectionModes, 2, reinterpret_cast<int32_t*>(&m_SelectionMode), "Mode of how submeshes are selected");

							static const char* s_TransformTargetNames[] = { "Median Point", "Individual Origins" };
							dropdown("Multi-Transform Target", s_TransformTargetNames, 2, reinterpret_cast<int32_t*>(&m_MultiTransformTarget), "Transform around the origin or each entity while in multiselection\nor around a median point with respect to all selected entites");

							static const char* s_TransformAroundOrigin[] = { "Local", "World" };
							dropdown("Transformation Origin", s_TransformAroundOrigin, 2, reinterpret_cast<int32_t*>(&m_TransformationOrigin), "Transform around origin of the entity/entities\nor around the origin of the world");

							endSection();
						}

						if (beginSection("Display", sectionIndex))
						{
							checkbox("Show Gizmos", m_ShowGizmos, "Show transform gizmos");
							if (checkbox("Allow Gizmo Axis Flip", m_GizmoAxisFlip, "Allow transform gizmos to flip their\naxes with respect to the camera"))
								ImGuizmo::AllowAxisFlip(m_GizmoAxisFlip);
							checkbox("Show Bounding Boxes", m_ShowBoundingBoxes, "Show mesh entities bounding boxes, Ctrl + B");
							if (m_ShowBoundingBoxes)
							{
								checkbox("Selected Entity", m_ShowBoundingBoxSelectedMeshOnly, "Show bounding boxes only for the\ncurrently selected entity");

								if (m_ShowBoundingBoxSelectedMeshOnly)
									checkbox("Submeshes", m_ShowBoundingBoxSubMeshes, "Show submesh bounding boxes for the\ncurrently selected entity");
							}
							SceneRendererOptions& rendererOptions = m_ViewportRenderer->GetOptions();
							checkbox("Show Grid", rendererOptions.ShowGrid, "Show Grid, Ctrl + G");
							checkbox("Selected in Wireframe", rendererOptions.ShowSelectedInWireFrame, "Show selected mesh in wireframe mode");

							if (drag("Line Width", m_LineWidth, 0.1f, 0.1f, 10.0f, "Change pipeline line width"))
							{
								m_Renderer2D->SetLineWidth(m_LineWidth);
								m_ViewportRenderer->SetLineWidth(m_LineWidth);
							}

							endSection();
						}

						if (beginSection("Scene Camera", sectionIndex))
						{
							float fov = glm::degrees(m_EditorCamera.GetFOV());
							if (slider("Field of View", fov, 30, 120, "Field of view of the viewport camera"))
								m_EditorCamera.SetFOV(fov);
							slider("Exposure", m_EditorCamera.GetExposure(), 0.0f, 10.0f, "Exposure of viewport camera,\nalso extends into rendered scene");
							drag("Speed", m_EditorCamera.GetNormalSpeed(), 0.001f, 0.0002f, 0.5f, "Speed of viewport camera in fly mode, RightAlt + Scroll");
							float nearClip = m_EditorCamera.GetNearClip();
							if (drag("Near Clip", nearClip, 0.1f, 0.002f, 100.0f, "Viewport camera near clip"))
								m_EditorCamera.SetNearClip(nearClip);
							float farClip = m_EditorCamera.GetFarClip();
							if (drag("Far Clip", farClip, 2.0f, 100.1f, 1'000'000.0f, "Viewport camera far clip"))
								m_EditorCamera.SetFarClip(farClip);

							endSection();
						}

						if (beginSection("Viewport Renderer", sectionIndex))
						{
							std::string string = fmt::format("{}", static_cast<uint32_t>(1.0f / Application::Get().GetFrameTime().GetSeconds()));
							text("Framerate (FPS)", string.c_str());

							bool isVSync = Application::Get().GetWindow().IsVSync();
							if (checkbox("Vertical Sync", isVSync, "Toggle monitor vertical sync"))
								Application::Get().GetWindow().SetVSync(isVSync);

							slider("Opacity", m_ViewportRenderer->GetOpacity(), 0.0f, 1.0f, "Viewport renderer opacity");

							static float renderScale = m_ViewportRenderer->GetSpecification().RendererScale;
							const float prevRenderScale = m_ViewportRenderer->GetSpecification().RendererScale;
							if (slider("Rendering Scale", renderScale, 0.1f, 2.0f, "Viewport renderer rendering scale,\nincrease for better quality result"))
								m_ViewportRenderer->SetViewportSize(
									static_cast<uint32_t>(m_ViewportRenderer->GetViewportWidth() / prevRenderScale),
									static_cast<uint32_t>(m_ViewportRenderer->GetViewportHeight() / prevRenderScale),
									renderScale
								);

							endSection();
						}

						ImGui::EndPopup();
					}
				}
			}

			ImGui::End();
		}

		UI::PopID();
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
		projectionMatrix = m_EditorCamera.GetProjectionMatrix();
		viewMatrix = m_EditorCamera.GetViewMatrix();

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
				return;
			}

			glm::vec3 medianLocation = glm::vec3(0.0f);
			glm::vec3 medianScale = glm::vec3(1.0f);
			glm::vec3 medianRotation = glm::vec3(0.0f);

			// Compuet median point
			for (auto entityID : selections)
			{
				Entity entity = m_CurrentScene->GetEntityWithUUID(entityID);
				medianLocation += entity.Transform().Translation;
				medianScale += entity.Transform().Scale;
				medianRotation += entity.Transform().GetRotationEuler();
			}
			medianLocation /= static_cast<float>(selections.size());
			//medianScale /= static_cast<float>(selections.size());
			medianRotation /= static_cast<float>(selections.size());

			int numOfEntities = static_cast<int>(selections.size());
			float averageScaleX = std::pow(medianScale.x, 1.0f / numOfEntities);
			float averageScaleY = std::pow(medianScale.y, 1.0f / numOfEntities);
			float averageScaleZ = std::pow(medianScale.z, 1.0f / numOfEntities);
			glm::vec3 averageScale = { averageScaleX, averageScaleY, averageScaleZ };

			glm::mat4 medianPointMatrix = glm::translate(glm::mat4(1.0f), medianLocation)
				* glm::toMat4(glm::quat(medianRotation))
				* glm::scale(glm::mat4(1.0f), averageScale);

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
				NewScene(sceneName);
				m_ShowNewSceneModal = false;
				sceneName.clear();
				ImGui::CloseCurrentPopup();
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

	void EditorLayer::DeleteEntity(Entity entity)
	{
		if (!entity)
			return;

		m_EditorScene->DestroyEntity(entity);
	}

	void EditorLayer::UI_ShowViewport()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Viewport", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

		m_ViewportPanelMouseOver = ImGui::IsWindowHovered();
		m_ViewportPanelFocused = ImGui::IsWindowFocused();	

		ImVec2 viewportOffset = ImGui::GetCursorPos(); // Includes tab bar
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		m_ViewportRenderer->SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y), m_ViewportRenderer->GetSpecification().RendererScale);
		m_EditorScene->SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y));
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
		ImGui::Begin("Fonts");

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

	void EditorLayer::OnEvent(Events::Event& e)
	{
		AssetEditorPanel::OnEvent(e);
		m_PanelsManager->OnEvent(e);

		if (m_AllowViewportCameraEvents)
			m_EditorCamera.OnEvent(e);

		Events::EventDispatcher dispatcher(e);
		dispatcher.Dispatch<Events::KeyPressedEvent>([this](Events::KeyPressedEvent& e) { return OnKeyPressed(e); });
		dispatcher.Dispatch<Events::MouseButtonPressedEvent>([this](Events::MouseButtonPressedEvent& e) { return OnMouseButtonPressed(e); });
		dispatcher.Dispatch<Events::WindowTitleBarHitTestEvent>([this](Events::WindowTitleBarHitTestEvent& e)
		{
			e.SetHit(UI_TitleBarHitTest(e.GetX(), e.GetY()));
			return true;
		});
		dispatcher.Dispatch<Events::TitleBarColorChangeEvent>([this](Events::TitleBarColorChangeEvent& e) { return OnTitleBarColorChange(e); });
	}

	bool EditorLayer::OnKeyPressed(Events::KeyPressedEvent& e)
	{
		if (UI::IsWindowFocused("Viewport") || UI::IsWindowFocused("Scene Hierarchy"))
		{
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
						if (SelectionManager::GetSelectionCount(SelectionContext::Scene) == 0)
							break;

						UUID selectedEntityID = SelectionManager::GetSelections(SelectionContext::Scene).front();
						Entity selectedEntity = m_CurrentScene->GetEntityWithUUID(selectedEntityID);
						m_EditorCamera.Focus(m_CurrentScene->GetWorldSpaceTransform(selectedEntity).Translation);
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
					std::vector<UUID> selectedEntities = SelectionManager::GetSelections(SelectionContext::Scene);
					for (auto entity : selectedEntities)
						DeleteEntity(m_CurrentScene->TryGetEntityWithUUID(entity));
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

					break;
				}
				case KeyCode::S:
					SaveScene();
					break;
				case KeyCode::N:
				{
					m_ShowNewSceneModal = true;
					break;
				}
				case KeyCode::O:
				{
					OpenScene();
					break;
				}
			}

			if (Input::IsKeyDown(KeyCode::LeftShift))
			{
				switch (e.GetKeyCode())
				{
					case KeyCode::S:
						SaveSceneAs();
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
			auto [origin, direction] = CastRay(m_EditorCamera, mouseX, mouseY);
			
			auto meshEntities = m_CurrentScene->GetAllEntitiesWith<StaticMeshComponent>();
			for (auto e : meshEntities)
			{
				Entity entity = { e, m_CurrentScene.Raw()};
				auto& mc = entity.GetComponent<StaticMeshComponent>();
			
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

	std::pair<glm::vec3, glm::vec3> EditorLayer::CastRay(const EditorCamera& camera, float x, float y)
	{
		glm::vec4 mouseClipPos = { x, y, -1.0f, 1.0f };

		auto inverseProj = glm::inverse(camera.GetProjectionMatrix());
		auto inverseView = glm::inverse(glm::mat3(camera.GetViewMatrix()));
	
		glm::vec3 rayPos = camera.GetPosition();
		glm::vec4 ray = inverseProj * mouseClipPos;
		if (!camera.IsPerspectiveProjection())
			ray.z = -1.0f;
		glm::vec3 direction = inverseView * ray;

		return { rayPos, camera.IsPerspectiveProjection() ? direction : glm::normalize(glm::vec3(direction) - rayPos) };
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
	}

	void EditorLayer::OpenProject(std::filesystem::path& filePath)
	{
		if (!FileSystem::Exists(filePath))
		{
			IR_CORE_ERROR("Tried to open a project that does not exist. Project Path: {0}", filePath);
			memset(s_OpenProjectFilePathBuffer, 0, c_MAX_PROJECT_FILEPATH_LENGTH);
			return;
		}

		if (Project::GetActive())
			CloseProject();

		Ref<Project> project = Project::Create();
		ProjectSerializer::Deserialize(project, filePath);
		Project::SetActive(project);

		m_PanelsManager->OnProjectChanged(project);

		bool hasScene = !project->GetConfig().StartScene.empty();
		if (hasScene)
			hasScene = OpenScene((Project::GetAssetDirectory() / project->GetConfig().StartScene).string());
		else
			NewScene();

		SelectionManager::DeselectAll();

		m_EditorCamera = EditorCamera(45.0f, 1280.0f, 720.0f, 0.01f, 1000.0f);

		memset(s_ProjectNameBuffer, 0, c_MAX_PROJECT_NAME_LENGTH);
		memset(s_OpenProjectFilePathBuffer, 0, c_MAX_PROJECT_FILEPATH_LENGTH);
		memset(s_NewProjectFilePathBuffer, 0, c_MAX_PROJECT_FILEPATH_LENGTH);
	}

	void EditorLayer::CreateProject(std::filesystem::path projectPath)
	{
	}

	void EditorLayer::EmptyProject()
	{
		if (Project::GetActive())
			CloseProject();

		Ref<Project> project = Project::Create();
		Project::SetActive(project);

		m_PanelsManager->OnProjectChanged(project);
		NewScene();

		SelectionManager::DeselectAll();

		m_EditorCamera = EditorCamera(45.0f, 1280.0f, 720.0f, 0.01f, 1000.0f);
	}

	void EditorLayer::SaveProject()
	{
		if (!Project::GetActive())
			IR_VERIFY(false); 

		auto project = Project::GetActive();
		ProjectSerializer::Serialize(project, project->GetConfig().ProjectDirectory + "/" + project->GetConfig().ProjectFileName);

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

		m_PanelsManager->SetSceneContext(m_EditorScene);
		AssetEditorPanel::SetSceneContext(m_EditorScene);

		m_SceneFilePath = std::string();

		m_EditorCamera = EditorCamera(45.0f, 1280.0f, 720.0f, 0.01f, 1000.0f);
		m_CurrentScene = m_EditorScene;

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

		Ref<Scene> newScene = Scene::Create("New Scene", true);
		SceneSerializer::Deserialize(newScene, filePath);

		m_EditorScene = newScene;
		m_SceneFilePath = filePath.string();

		std::replace(m_SceneFilePath.begin(), m_SceneFilePath.end(), '\\', '/');
		if ((m_SceneFilePath.size() >= 5) && (m_SceneFilePath.substr(m_SceneFilePath.size() - 5) == ".auto"))
			m_SceneFilePath = m_SceneFilePath.substr(0, m_SceneFilePath.size() - 5);

		m_PanelsManager->SetSceneContext(m_EditorScene);
		AssetEditorPanel::SetSceneContext(m_EditorScene);

		SelectionManager::DeselectAll();

		m_CurrentScene = m_EditorScene;

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
			SceneSerializer::Serialize(m_EditorScene, std::filesystem::path(m_SceneFilePath));
		}
		else
		{
			SaveSceneAs();
		}
	}

	void EditorLayer::SaveSceneAs()
	{
		std::filesystem::path filepath = FileSystem::SaveFileDialog({ { "Iris Scene (*.Iscene)", "Iscene" } });

		if (filepath.empty())
			return;

		if (!filepath.has_extension())
			filepath += ".Iscene";

		SceneSerializer::Serialize(m_EditorScene, filepath);

		std::filesystem::path path = filepath;
		m_SceneFilePath = filepath.string();
		std::replace(m_SceneFilePath.begin(), m_SceneFilePath.end(), '\\', '/');
	}

}