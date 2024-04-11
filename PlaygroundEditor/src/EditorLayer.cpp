#include "EditorLayer.h"

#include "Core/Input/Input.h"
#include "Editor/EditorResources.h"
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
#include "Asset/MeshImporter.h"

#include "ImGui/ImGuiUtils.h"

#include <glfw/include/glfw/glfw3.h>

#include <imgui/imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

// TODO: REMOVE
#include <stb/stb_image_writer/stb_image_write.h>

bool g_WireFrame = false;

namespace vkPlayground {

	// TODO: REMOVE
	static Ref<Texture2D> s_BillBoardTexture;
	static Ref<MeshSource> s_MeshSource;
	static Ref<StaticMesh> s_Mesh;

	EditorLayer::EditorLayer()
		: Layer("EditorLayer"), m_EditorCamera(45.0f, 1280.0f, 720.0f, 0.1f, 10000.0f)
	{
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
		// TODO: REMOVE
		m_EditorScene = Scene::Create("Editor Scene");

		m_ViewportRenderer = SceneRenderer::Create(m_EditorScene, { .RendererScale = 1.0f });
		m_ViewportRenderer->SetScene(m_EditorScene);
		m_ViewportRenderer->SetLineWidth(m_LineWidth);

		EditorResources::Init();

		AssimpMeshImporter importer("Resources/assets/meshes/Sponza/Sponza.gltf");
		// AssimpMeshImporter importer("Resources/assets/meshes/stormtrooper/stormtrooper.gltf");
		s_MeshSource = importer.ImportToMeshSource();
		s_Mesh = StaticMesh::Create(s_MeshSource);

		TextureSpecification textureSpec = {
			.DebugName = "Qiyana",
			.GenerateMips = true
		};
		s_BillBoardTexture = Texture2D::Create(textureSpec, "Resources/assets/textures/qiyana.png");
	}

	void EditorLayer::OnDetach()
	{
		EditorResources::Shutdown();
	}

	void EditorLayer::OnUpdate(TimeStep ts)
	{
		m_EditorCamera.SetActive(m_AllowViewportCameraEvents);
		m_EditorCamera.OnUpdate(ts);

		if (Input::IsKeyDown(KeyCode::F))
			m_EditorCamera.Focus({ 0.0f, 0.0f, 0.0f });

		float depthLinearizeMul = (-m_EditorCamera.GetProjectionMatrix()[3][2]);
		float depthLinearizeAdd = (m_EditorCamera.GetProjectionMatrix()[2][2]);
		if (depthLinearizeMul * depthLinearizeAdd < 0)
			depthLinearizeAdd = -depthLinearizeAdd;

		m_ViewportRenderer->SetScene(m_EditorScene);
		m_ViewportRenderer->BeginScene({ m_EditorCamera, m_EditorCamera.GetViewMatrix(), m_EditorCamera.GetNearClip(), m_EditorCamera.GetFarClip(), m_EditorCamera.GetFOV() });
		
		m_ViewportRenderer->SubmitStaticMesh(s_Mesh, s_MeshSource, s_Mesh->GetMaterials(), glm::mat4(1.0f));
		
		m_ViewportRenderer->EndScene();

		if (m_ViewportRenderer->GetFinalPassImage())
		{
			Ref<Renderer2D> renderer2D = m_ViewportRenderer->GetRenderer2D();

			renderer2D->ResetStats();
			renderer2D->BeginScene(m_EditorCamera.GetViewProjection(), m_EditorCamera.GetViewMatrix());
			renderer2D->SetTargetFramebuffer(m_ViewportRenderer->GetExternalCompositeFramebuffer());
		
			renderer2D->DrawQuadBillboard({ -3.053855f, 4.328760f, 1.0f }, { 2.0f, 2.0f }, { 1.0f, 0.0f, 1.0f, 1.0f });
			
			for (int x = -15; x < 15; x++)
			{
				for (int y = -3; y < 3; y++)
				{
					renderer2D->DrawQuad({ x, y, 2.0f }, { 1, 1 }, { glm::sin(x), glm::cos(y), glm::sin(x + y), 1.0f });
				}
			}
			
			renderer2D->DrawAABB({ {5.0f, 5.0f, 1.0f}, {7.0f, 7.0f, -4.0f} }, glm::mat4(1.0f));
			renderer2D->DrawAABB({ {4.0f, 4.0f, -1.0f}, {5.0f, 5.0f, -4.0f} }, glm::translate(glm::mat4(1.0f), {2.0f, -1.0f, -0.5f}), { 0.0f, 1.0f, 1.0f, 1.0f });
			renderer2D->DrawCircle({ 5.0f, 5.0f, -2.0f }, glm::vec3(1.0f), 2.0f, { 1.0f, 0.0f, 1.0f, 1.0f });
			// renderer2D->DrawLine(glm::vec3(0.0f), glm::vec3(6.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
			// renderer2D->DrawLine(glm::vec3(0.0f), glm::vec3(-20.2f, 3.0f, -5.0f), glm::vec4(0.2f, 0.3f, 0.8f, 1.0f));
			// renderer2D->DrawLine(glm::vec3(0.0f, -2.0f, 0.0f), glm::vec3(0.0f, -5.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
			renderer2D->DrawQuadBillboard({ -2.0f, -2.0f, -0.5f }, { 2.0f, 2.0f }, s_BillBoardTexture, 2.0f, { 1.0f, 0.7f, 1.0f, 0.5f });
			// renderer2D->DrawAABB(s_MeshSource->GetBoundingBox(), glm::mat4(1.0f));
		
			// Draw the Axes of my system
			renderer2D->DrawLine(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f) * 10.0f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
			renderer2D->DrawLine(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f) * 10.0f, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
			renderer2D->DrawLine(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f) * 10.0f, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

			renderer2D->EndScene();
		}

		if (Input::IsKeyDown(KeyCode::R))
		{
			// TODO: This currently does not work since the final image that is returned is a floating point image
			Buffer textureBuffer;
			// Ref<Texture2D> texture = m_RenderingPass->GetOutput(0);
			Ref<Texture2D> texture = m_ViewportRenderer->GetFinalPassImage();
			texture->CopyToHostBuffer(textureBuffer, true);
			stbi_flip_vertically_on_write(true);
			stbi_write_jpg("Resources/assets/textures/output.jpg", texture->GetWidth(), texture->GetHeight(), 4, textureBuffer.Data, 100);
			stbi_flip_vertically_on_write(false);
		}

		bool leftAltWithEitherLeftOrMiddleButtonOrJustRight = (Input::IsKeyDown(KeyCode::LeftAlt) && (Input::IsMouseButtonDown(MouseButton::Left) || (Input::IsMouseButtonDown(MouseButton::Middle)))) || Input::IsMouseButtonDown(MouseButton::Right);
		bool notStartCameraViewportAndViewportHoveredFocused = !m_StartedCameraClickInViewport && m_ViewportPanelFocused && m_ViewportPanelMouseOver;
		if (leftAltWithEitherLeftOrMiddleButtonOrJustRight && notStartCameraViewportAndViewportHoveredFocused)
			m_StartedCameraClickInViewport = true;

		bool NotRightAndNOTLeftAltANDLeftOrMiddle = !Input::IsMouseButtonDown(MouseButton::Right) && !(Input::IsKeyDown(KeyCode::LeftAlt) && (Input::IsMouseButtonDown(MouseButton::Left) || Input::IsMouseButtonDown(MouseButton::Middle)));
		if (NotRightAndNOTLeftAltANDLeftOrMiddle)
			m_StartedCameraClickInViewport = false;
	}

	void EditorLayer::OnImGuiRender()
	{
		StartDocking();

		ImGui::ShowDemoWindow(); // Testing imgui stuff

		ShowShadersPanel();

		ShowFontsPanel();

		ShowViewport();

		EndDocking();
	}

	void EditorLayer::StartDocking()
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

		// TODO: Render window outer border?
		{
			UI::ImGuiScopedColor windowBorder(ImGuiCol_Border, IM_COL32(50, 50, 50, 255));
			// Draw window border if window is not maximized
			if (!isMaximized)
				UI::RenderWindowOuterBorders(ImGui::GetCurrentWindow());
		}

		float minWinSizeX = style.WindowMinSize.x;
		ImGui::DockSpace(ImGui::GetID("MyDockspace"));
		style.WindowMinSize.x = minWinSizeX;
	}

	void EditorLayer::EndDocking()
	{
		ImGui::End();
	}

	void EditorLayer::ShowViewport()
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

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void EditorLayer::ShowShadersPanel()
	{
		ImGui::Begin("Shaders");

		Ref<ShadersLibrary> shadersLib = Renderer::GetShadersLibrary();

		for (auto& [name, shader] : shadersLib->GetShaders())
		{
			ImGui::Columns(2);

			ImGui::Text(name.c_str());

			ImGui::NextColumn();

			if (ImGui::Button(fmt::format("Reload##{0}", name).c_str()))
				shader->Reload();

			ImGui::Columns(1);
		}

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
			m_ViewportRenderer->SetLineWidth(lineWidth);

		ImGui::Text("Average Framerate: %i", static_cast<uint32_t>(1.0f / Application::Get().GetFrameTime().GetSeconds()));

		bool isVSync = Application::Get().GetWindow().IsVSync();
		if (ImGui::Checkbox("VSync", &isVSync))
			Application::Get().GetWindow().SetVSync(isVSync);

		ImGui::Checkbox("WireFrame", &g_WireFrame);

		ImGui::End();
	}

	void EditorLayer::ShowFontsPanel()
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
		if (m_AllowViewportCameraEvents)
			m_EditorCamera.OnEvent(e);

		Events::EventDispatcher dispatcher(e);
		dispatcher.Dispatch<Events::KeyPressedEvent>([this](Events::KeyPressedEvent& e) { return OnKeyPressed(e); });
		dispatcher.Dispatch<Events::MouseButtonPressedEvent>([this](Events::MouseButtonPressedEvent& e) { return OnMouseButtonPressed(e); });
	}

	bool EditorLayer::OnKeyPressed(Events::KeyPressedEvent& e)
	{
		if (UI::IsWindowFocused("Viewport"))
		{
			if (m_ViewportPanelMouseOver && !Input::IsMouseButtonDown(MouseButton::Right))
			{
				switch (e.GetKeyCode())
				{
					// TODO:
				case KeyCode::F:
					m_EditorCamera.Focus(glm::vec3(0.0f));
				}
			}
		}

		return false;
	}

	bool EditorLayer::OnMouseButtonPressed(Events::MouseButtonPressedEvent& e)
	{
		// TODO:

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

}