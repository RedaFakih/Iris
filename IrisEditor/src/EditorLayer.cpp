#include "EditorLayer.h"

#include "Asset/MeshImporter.h"
#include "Core/Input/Input.h"
#include "Editor/EditorResources.h"
#include "Editor/Panels/SceneHierarchyPanel.h"
#include "Editor/Panels/ShadersPanel.h"
#include "Editor/Panels/ECSDebugPanel.h"
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
#include "ImGui/ImGuizmo.h"
#include "Utils/FileSystem.h"

#include "ImGui/ImGuiUtils.h"

#include <glfw/include/glfw/glfw3.h>

#include <imgui/imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// TODO: REMOVE
#include <stb/stb_image_writer/stb_image_write.h>

// TODO: REMOVE
bool g_WireFrame = false;
bool g_Render2D = true;

namespace Iris {

	// TODO: REMOVE
	static Ref<Texture2D> s_BillBoardTexture;
	static Ref<MeshSource> s_MeshSource;
	static Ref<StaticMesh> s_Mesh;

	EditorLayer::EditorLayer()
		: Layer("EditorLayer"), m_EditorCamera(45.0f, 1280.0f, 720.0f, 0.1f, 10000.0f)
	{
		m_TitleBarPreviousColor = Colors::Theme::TitlebarRed;
		m_TitleBarTargetColor   = Colors::Theme::TitlebarGreen;
	}

	EditorLayer::~EditorLayer()
	{
		// TODO: REMOVE
		s_BillBoardTexture = nullptr;
		s_MeshSource = nullptr;
		s_Mesh = nullptr;
	}

	void EditorLayer::OnAttach()
	{
		EditorResources::Init();
		m_PanelsManager = PanelsManager::Create();

		Ref<SceneHierarchyPanel> sceneHierarchyPanel = m_PanelsManager->AddPanel<SceneHierarchyPanel>(PanelCategory::View, "SceneHierarchyPanel", "Scene Hierarchy", true, m_EditorScene, SelectionContext::Scene);
		sceneHierarchyPanel->SetEntityDeletedCallback([this](Entity entity) { OnEntityDeleted(entity); });
		sceneHierarchyPanel->AddEntityContextMenuPlugin([this](Entity entity) { SceneHierarchySetEditorCameraTransform(entity); });

		m_PanelsManager->AddPanel<ShadersPanel>(PanelCategory::View, "ShadersPanel", "Shaders", false);

		// TODO: REMOVE since this should be handled by NewScene and all that stuff
		m_EditorScene = Scene::Create("Editor Scene");
		m_CurrentScene = m_EditorScene;

		// NOTE: For debugging ECS Problems
		// m_PanelsManager->AddPanel<ECSDebugPanel>(PanelCategory::View, "ECSDebugPanel", "ECS", false, m_CurrentScene);

		m_ViewportRenderer = SceneRenderer::Create(m_EditorScene, { .RendererScale = 1.0f });
		m_ViewportRenderer->SetLineWidth(m_LineWidth);
		m_Renderer2D = Renderer2D::Create();
		m_Renderer2D->SetLineWidth(m_LineWidth);

		// AssimpMeshImporter importer("Resources/assets/meshes/LowPolySponza/Sponza.gltf");
		AssimpMeshImporter importer("Resources/assets/meshes/stormtrooper/stormtrooper.gltf");
		s_MeshSource = importer.ImportToMeshSource();
		s_Mesh = StaticMesh::Create(s_MeshSource);

		TextureSpecification textureSpec = {
			.DebugName = "Qiyana",
			.GenerateMips = true
		};
		s_BillBoardTexture = Texture2D::Create(textureSpec, "Resources/assets/textures/qiyana.png");

		// TODO: REMOVE When we have Editor UI
		Entity meshEntity = m_CurrentScene->CreateEntity("MeshEntity");
		auto& staticMeshComponent = meshEntity.AddComponent<StaticMeshComponent>();
		staticMeshComponent.StaticMesh = s_Mesh;
		staticMeshComponent.MaterialTable = s_Mesh->GetMaterials();

		Entity spriteEntity = m_CurrentScene->CreateEntity("Sprite");
		spriteEntity.SetParent(meshEntity);
		auto& spriteRC = spriteEntity.AddComponent<SpriteRendererComponent>();
		spriteRC.Color = { 1.0f, 0.5f, 0.0f, 1.0f };
		auto& transform = spriteEntity.GetComponent<TransformComponent>();
		transform.SetTransform(transform.GetTransform() * glm::translate(glm::mat4(1.0f), { 1.0f, 0.0f, 0.0f }));
		
		Entity spriteEntity2 = m_CurrentScene->CreateEntity("Sprite2");
		spriteEntity2.SetParent(spriteEntity);
		auto& spriteRC2 = spriteEntity2.AddComponent<SpriteRendererComponent>();
		spriteRC2.Color = { 1.0f, 0.5f, 1.0f, 1.0f };
		spriteRC2.TilingFactor = 3.0f;
		spriteRC2.Texture = s_BillBoardTexture;
		auto& transform2 = spriteEntity2.GetComponent<TransformComponent>();
		transform2.SetTransform(transform2.GetTransform() * glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, 1.0f }));
	}

	void EditorLayer::OnDetach()
	{
		EditorResources::Shutdown();

		m_ViewportRenderer->SetScene(nullptr);
		m_CurrentScene = nullptr;

		m_PanelsManager->SetSceneContext(nullptr);

		// Check that m_EditorScene is the last one (so setting it null here will destroy the scene)
		IR_ASSERT(m_EditorScene->GetRefCount() == 1, "Scene will not be destroyed after project is closed - something is still holding scene refs!");
		m_EditorScene = nullptr;
	}

	void EditorLayer::OnUpdate(TimeStep ts)
	{
		m_PanelsManager->SetSceneContext(m_EditorScene);

		m_EditorCamera.SetActive(m_AllowViewportCameraEvents);
		m_EditorCamera.OnUpdate(ts);

		m_EditorScene->OnUpdateEditor(ts);
		m_EditorScene->OnRenderEditor(m_ViewportRenderer, ts, m_EditorCamera);

		// TODO: Here there is a bug that if we want to render to the 2D renderer twice we have to actually handle the layout transition of the images
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

		if (g_Render2D)
		{
			m_Renderer2D->DrawQuadBillboard({ -3.053855f, 4.328760f, 1.0f }, { 2.0f, 2.0f }, { 1.0f, 0.0f, 1.0f, 1.0f });

			for (int x = -15; x < 15; x++)
			{
				for (int y = -3; y < 3; y++)
				{
					m_Renderer2D->DrawQuad({ x, y, 2.0f }, { 1, 1 }, { glm::sin(x), glm::cos(y), glm::sin(x + y), 1.0f });
				}
			}

			m_Renderer2D->DrawAABB({ {5.0f, 5.0f, 1.0f}, {7.0f, 7.0f, -4.0f} }, glm::mat4(1.0f));
			m_Renderer2D->DrawAABB({ {4.0f, 4.0f, -1.0f}, {5.0f, 5.0f, -4.0f} }, glm::translate(glm::mat4(1.0f), { 2.0f, -1.0f, -0.5f }), { 0.0f, 1.0f, 1.0f, 1.0f });
			m_Renderer2D->DrawCircle({ 5.0f, 5.0f, -2.0f }, glm::vec3(1.0f), 2.0f, { 1.0f, 0.0f, 1.0f, 1.0f });
			// m_Renderer2D->DrawLine(glm::vec3(0.0f), glm::vec3(6.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
			// m_Renderer2D->DrawLine(glm::vec3(0.0f), glm::vec3(-20.2f, 3.0f, -5.0f), glm::vec4(0.2f, 0.3f, 0.8f, 1.0f));
			// m_Renderer2D->DrawLine(glm::vec3(0.0f, -2.0f, 0.0f), glm::vec3(0.0f, -5.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
			m_Renderer2D->DrawQuadBillboard({ -2.0f, -2.0f, -0.5f }, { 2.0f, 2.0f }, s_BillBoardTexture, 2.0f, { 1.0f, 0.7f, 1.0f, 0.5f });
			// m_Renderer2D->DrawAABB(s_MeshSource->GetBoundingBox(), glm::mat4(1.0f));

			//m_Renderer2D->DrawLine(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
			//m_Renderer2D->DrawLine(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
			//m_Renderer2D->DrawLine(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
		}

		// `true` indicates that we transition resulting images to presenting layouts...
		m_Renderer2D->EndScene(true);
	}

	void EditorLayer::OnImGuiRender()
	{
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
		UI_ShowShadersPanel();
		UI_ShowFontsPanel();

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
				ImGui::Text(title.c_str());
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
				ImGui::Text(sceneName.c_str());
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
						// TODO:
					}

					if (ImGui::MenuItem("Open Scene...", "Ctrl + O"))
					{
						// TODO:
					}

					ImGui::Separator();

					if (ImGui::MenuItem("Save Scene", "Ctrl + S"))
					{
						// TODO:
					}

					if (ImGui::MenuItem("Save Scene As...", "Ctrl + Shift + S"))
					{
						// TODO:
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
			case ImGuizmo::OPERATION::TRANSLATE: return 0.1f;
			case ImGuizmo::OPERATION::ROTATE: return 45.0f;
			case ImGuizmo::OPERATION::SCALE: return 0.1f;
		}

		return 0.0f;
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
				(ImGuizmo::OPERATION)m_GizmoType,
				ImGuizmo::LOCAL,
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
				// that we are meant to be changing (dictated by m_GizmoType).  That way we avoid
				// small drift (particularly in rotation and scale) due numerical precision issues
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
					if (fabs(deltaRotationEuler.x) < 0.001) deltaRotationEuler.x = 0.0f;
					if (fabs(deltaRotationEuler.y) < 0.001) deltaRotationEuler.y = 0.0f;
					if (fabs(deltaRotationEuler.z) < 0.001) deltaRotationEuler.z = 0.0f;

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
		else // Multi select transforms not supported
		{
			glm::vec3 medianLocation = glm::vec3(0.0f);
			glm::vec3 medianScale = glm::vec3(1.0f);
			glm::vec3 medianRotation = glm::vec3(0.0f);
			for (auto entityID : selections)
			{
				Entity entity = m_CurrentScene->GetEntityWithUUID(entityID);
				medianLocation += entity.Transform().Translation;
				medianScale += entity.Transform().Scale;
				medianRotation += entity.Transform().GetRotationEuler();
			}
			medianLocation /= selections.size();
			medianScale /= selections.size();
			medianRotation /= selections.size();

			glm::mat4 medianPointMatrix = glm::translate(glm::mat4(1.0f), medianLocation)
				* glm::toMat4(glm::quat(medianRotation))
				* glm::scale(glm::mat4(1.0f), medianScale);

			glm::mat4 deltaMatrix = glm::mat4(1.0f);

			if (ImGuizmo::Manipulate(
				glm::value_ptr(viewMatrix),
				glm::value_ptr(projectionMatrix),
				(ImGuizmo::OPERATION)m_GizmoType,
				ImGuizmo::LOCAL,
				glm::value_ptr(medianPointMatrix),
				glm::value_ptr(deltaMatrix),
				snap ? snapValues : nullptr)
				)
			{
				//switch (m_MultiTransformTarget)
				//{
				//	case TransformationTarget::MedianPoint:
				//	{
				//		for (auto entityID : selections)
				//		{
				//			Entity entity = m_CurrentScene->GetEntityWithUUID(entityID);
				//			TransformComponent& transform = entity.Transform();
				//			transform.SetTransform(deltaMatrix * transform.GetTransform());
				//		}

				//		break;
				//	}
				//	case TransformationTarget::IndividualOrigins:
				//	{
				//		glm::vec3 deltaTranslation, deltaScale;
				//		glm::quat deltaRotation;
				//		Math::DecomposeTransform(deltaMatrix, deltaTranslation, deltaRotation, deltaScale);

				//		for (auto entityID : selections)
				//		{
				//			Entity entity = m_CurrentScene->GetEntityWithUUID(entityID);
				//			TransformComponent& transform = entity.Transform();

				//			switch (m_GizmoType)
				//			{
				//			case ImGuizmo::TRANSLATE:
				//			{
				//				transform.Translation += deltaTranslation;
				//				break;
				//			}
				//			case ImGuizmo::ROTATE:
				//			{
				//				transform.SetRotationEuler(transform.GetRotationEuler() + glm::eulerAngles(deltaRotation));
				//				break;
				//			}
				//			case ImGuizmo::SCALE:
				//			{
				//				if (deltaScale != glm::vec3(1.0f, 1.0f, 1.0f))
				//					transform.Scale *= deltaScale;
				//				break;
				//			}
				//			}
				//		}
				//		break;
				//	}
				//}
			}
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
		m_ViewportRenderer->SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y));
		m_EditorScene->SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y));
		m_EditorCamera.SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y));

		// Here we get the output from the rendering pass since the screen pass is there in case there was no imgui in the
		// application and we have to render directly to the screen...
		// Ref<Texture2D> texture = m_RenderingPass->GetOutput(0);
		Ref<Texture2D> texture = m_ViewportRenderer->GetFinalPassImage();
		UI::Image(texture, viewportSize, { 0, 1 }, { 1, 0 });

		m_ViewportRect = UI::GetWindowRect();

		m_AllowViewportCameraEvents = (ImGui::IsMouseHoveringRect(m_ViewportRect.Min, m_ViewportRect.Max) && m_ViewportPanelFocused) || m_StartedCameraClickInViewport;

		// TODO: Toolbars.. settings...

		if (m_ShowGizmos)
		{
			UI_DrawGizmos();
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void EditorLayer::UI_ShowShadersPanel()
	{
		ImGui::Begin("Renderer Stuff");

		float fov = glm::degrees(m_EditorCamera.GetFOV());
		if (ImGui::SliderFloat("FOV", &fov, 30, 120))
			m_EditorCamera.SetFOV(fov);

		float& exposure = m_EditorCamera.GetExposure();
		ImGui::SliderFloat("Exposure", &exposure, 0.0f, 5.0f);

		float opacity = m_ViewportRenderer->GetOpacity();
		if (ImGui::SliderFloat("Opacity", &opacity, 0.0f, 5.0f))
			m_ViewportRenderer->SetOpacity(opacity);

		float lineWidth = m_ViewportRenderer->GetLineWidth();
		if (ImGui::SliderFloat("LineWidth", &lineWidth, 0.1f, 6.0f))
		{
			m_ViewportRenderer->SetLineWidth(lineWidth);
			// TODO: And m_Renderer2D line width too?
		}

		ImGui::Text("Average Framerate: %i", static_cast<uint32_t>(1.0f / Application::Get().GetFrameTime().GetSeconds()));

		bool isVSync = Application::Get().GetWindow().IsVSync();
		if (ImGui::Checkbox("VSync", &isVSync))
			Application::Get().GetWindow().SetVSync(isVSync);

		ImGui::Checkbox("WireFrame", &g_WireFrame);

		ImGui::Checkbox("Grid", &m_ViewportRenderer->GetOptions().ShowGrid);

		ImGui::Checkbox("Render 2D", &g_Render2D);

		ImGui::End();
	}

	void EditorLayer::UI_ShowFontsPanel()
	{
		ImGui::Begin("Fonts");

		ImGuiFontsLibrary& fontsLib = Application::Get().GetImGuiLayer()->GetFontsLibrary();

		for (auto& [name, font] : fontsLib.GetFonts())
		{
			ImGui::Columns(2);

			ImGui::Text(name.c_str());

			ImGui::NextColumn();

			if (ImGui::Button(fmt::format("Set Default##{0}", name).c_str()))
				fontsLib.SetDefaultFont(name);

			ImGui::Columns(1);
		}

		ImGui::End();
	}

	void EditorLayer::OnEvent(Events::Event& e)
	{
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
					auto selectedEntities = SelectionManager::GetSelections(SelectionContext::Scene);
					for (auto entity : selectedEntities)
						DeleteEntity(m_CurrentScene->TryGetEntityWithUUID(entity));
					break;
				}
			}
		}

		if (Input::IsKeyDown(KeyCode::LeftControl) && !Input::IsMouseButtonDown(MouseButton::Left))
		{
			switch (e.GetKeyCode())
			{
				// Toggle Grid
				case KeyCode::G:
					m_ViewportRenderer->GetOptions().ShowGrid = !m_ViewportRenderer->GetOptions().ShowGrid;
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

		// TODO: ImGuizmo

		ImGui::ClearActiveID();

		// TODO: Select entity...

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

	void EditorLayer::SceneHierarchySetEditorCameraTransform(Entity entity)
	{
		TransformComponent& tc = entity.Transform();
		tc.SetTransform(glm::inverse(m_EditorCamera.GetViewMatrix()));
	}

}