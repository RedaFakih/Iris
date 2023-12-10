#include "EditorLayer.h"

#include "Core/Input/Input.h"
#include "Editor/EditorResources.h"
#include "Renderer/Core/RenderCommandBuffer.h"
#include "Renderer/Core/Vulkan.h" // TODO: TEMP! No vulkan calls should be here
#include "Renderer/Framebuffer.h"
#include "Renderer/IndexBuffer.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/Pipeline.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderPass.h"
#include "Renderer/UniformBufferSet.h"
#include "Renderer/VertexBuffer.h"

#include "ImGui/ImGuiUtils.h"

#include <glfw/include/glfw/glfw3.h>

#include <imgui/imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// TODO: REMOVE
//////#include <stb/stb_image_writer/stb_image_write.h>
//#include <stb_image_writer/stb_image_write.h>
//#include <fastgltf/glm_element_traits.hpp>
//#include <fastgltf/parser.hpp>
//#include <fastgltf/tools.hpp>
//#include <fastgltf/types.hpp>

namespace vkPlayground {

	// TODO: This should NOT BE HERE
	// TODO: REMOVE
	struct UniformBufferData
	{
		glm::mat4 Model;
		glm::mat4 ViewProjection;
	};

	static std::vector<Vertex> s_Vertices;
	static std::vector<uint32_t> s_Indices;
	//static std::vector<Vertex> s_TestVertices;
	//static std::vector<uint32_t> s_TestIndices;

	//void LoadModelFromFileBase(std::filesystem::path path, glm::mat4 rootTransform, bool binary)
	//{
	//	using fastgltf::Extensions;
	//	auto parser = fastgltf::Parser(Extensions::KHR_texture_basisu | Extensions::KHR_mesh_quantization |
	//		Extensions::EXT_meshopt_compression | Extensions::KHR_lights_punctual);
	//
	//	auto data = fastgltf::GltfDataBuffer();
	//	data.loadFromFile(path);
	//
	//	constexpr auto options = fastgltf::Options::LoadExternalBuffers | //fastgltf::Options::LoadExternalImages |
	//		fastgltf::Options::LoadGLBBuffers;
	//	auto asset = binary ? parser.loadBinaryGLTF(&data, path.parent_path(), options) : parser.loadGLTF(&data, path.parent_path(), options);
	//
	//	if (auto err = asset.error(); err != fastgltf::Error::None)
	//	{
	//		// TODO: Log error
	//	}
	//
	//	fastgltf::Primitive& primitive = asset->meshes[0].primitives[0];
	//	fastgltf::Texture& texture = asset->textures[0];
	//
	//	if (asset->images.size())
	//	{
	//		fastgltf::Image& image = asset->images[texture.imageIndex.value()];
	//		TextureSpecification spec = {
	//			.DebugName = std::string(image.name),
	//		};
	//		auto uri = std::get_if<fastgltf::sources::URI>(&image.data)->uri;
	//		auto uriString = std::string(uri.path());
	//		auto parentPath = path.parent_path();
	//		auto filePath = parentPath / uriString;
	//		// TODO: Load the textures...
	//	}
	//
	//	if (primitive.indicesAccessor.has_value()) 
	//	{
	//		auto& accessor = asset->accessors[primitive.indicesAccessor.value()];
	//		s_TestIndices.resize(accessor.count);
	//
	//		fastgltf::iterateAccessorWithIndex<std::uint32_t>(
	//		asset.get(), accessor, [&](std::uint32_t index, std::size_t idx) {
	//			s_TestIndices[idx] = index;
	//		});
	//	}
	//
	//	if (auto it = primitive.findAttribute("POSITION"); it)
	//	{
	//		auto& accessor = asset->accessors[it->second];
	//		s_TestVertices.resize(accessor.count);
	//
	//		fastgltf::iterateAccessorWithIndex<glm::vec3>(
	//		asset.get(), accessor, [&](glm::vec3 position, std::size_t idx) {
	//			s_TestVertices[idx].Position = position;
	//		});
	//	}
	//
	//	glm::vec3 color = { 1.0f, 0.0f, 1.0f };
	//	if (primitive.materialIndex.has_value())
	//	{
	//		auto& material = asset->materials[primitive.materialIndex.value()];
	//		color.r = material.pbrData.baseColorFactor[0];
	//		color.g = material.pbrData.baseColorFactor[1];
	//		color.b = material.pbrData.baseColorFactor[2];
	//	}
	//
	//	if (auto it = primitive.findAttribute("TEXCOORD_0"); it)
	//	{
	//		auto& accessor = asset->accessors[it->second];
	//		s_TestVertices.resize(accessor.count);
	//
	//		fastgltf::iterateAccessorWithIndex<glm::vec2>(
	//			asset.get(), accessor, [&](glm::vec2 texCoord, std::size_t idx) {
	//				s_TestVertices[idx].TexCoord = texCoord;
	//				s_TestVertices[idx].Color = color;
	//			});
	//	}
	//}

	EditorLayer::EditorLayer()
		: Layer("EditorLayer"), m_EditorCamera(45.0f, 1280.0f, 720.0f, 0.1f, 10000.0f)
	{
	}

	EditorLayer::~EditorLayer()
	{
	}

	void EditorLayer::OnAttach()
	{
		// TODO: REMOVE
		// Interleaved vertex attributes
		s_Vertices = {
#if 0
			{ {-0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
			{ { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
			{ { 0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
			{ {-0.5f,  0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
#else
			{ {-0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
			{ { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
			{ { 0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
			{ {-0.5f,  0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } },

			{ {-0.5f, -0.5, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
			{ { 0.5f, -0.5, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
			{ { 0.5f,  0.5, -0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
			{ {-0.5f,  0.5, -0.5f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
#endif
		};

		s_Indices = {
			0, 1, 2, 2, 3, 0,
			4, 5, 6, 6, 7, 4
		};

		EditorResources::Init();

		m_CommandBuffer = RenderCommandBuffer::Create(0, "EditorLayer");

		// TODO: REMOVE!
		Ref<Shader> renderingShader = Renderer::GetShadersLibrary()->Get("SimpleShader");
		Ref<Shader> screenShader = Renderer::GetShadersLibrary()->Get("TexturePass");
		m_UniformBufferSet = UniformBufferSet::Create(sizeof(UniformBufferData));
		TextureSpecification textureSpec = {
			.DebugName = "Qiyana",
			.GenerateMips = true
		};
		Ref<Texture2D> texture = Texture2D::Create(textureSpec, "Resources/assets/textures/qiyana.png");
		TextureSpecification textureSpec2 = {
			.DebugName = "BlackTexture",
			.Width = 1,
			.Height = 1,
			.Format = ImageFormat::RGBA,
			.WrapMode = TextureWrap::Repeat,
			.FilterMode = TextureFilter::Linear,
		};
		// Currently its red but whatever
		constexpr uint32_t blackTextureData = 0xff0000ff;
		Ref<Texture2D> texture2 = Texture2D::Create(textureSpec2, Buffer((uint8_t*)&blackTextureData, sizeof(uint32_t)));

		//glm::mat4 transform = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), { 1.0f, 0.0f, 0.0f })
		//	* glm::scale(glm::mat4(1.0f), glm::vec3(0.2f));
		//LoadModelFromFileBase("assets/meshes/stormtrooper/stormtrooper.gltf", transform, false);
		//Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(s_TestVertices.data(), (uint32_t)(sizeof(Vertex) * s_TestVertices.size()));
		//Ref<IndexBuffer> indexBuffer = IndexBuffer::Create(s_TestIndices.data(), (uint32_t)(sizeof(uint32_t) * s_TestIndices.size()));

		// Rendering pass
		{
			FramebufferSpecification mainFBspec = {
				.DebugName = "Rendering FB",
				.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f },
				.DepthClearValue = 1.0f,
				.Attachments = { ImageFormat::RGBA, ImageFormat::DEPTH32F }
			};

			Ref<Framebuffer> renderingFB = Framebuffer::Create(mainFBspec);

			PipelineSpecification spec = {
				.DebugName = "RenderingPassPipeline",
				.Shader = renderingShader,
				.TargetFramebuffer = renderingFB,
				.VertexLayout = {
					{ ShaderDataType::Float3, "a_Position" },
					{ ShaderDataType::Float3, "a_Color"    },
					{ ShaderDataType::Float2, "a_TexCoord" }
				},
				.Topology = PrimitiveTopology::Triangles,
				.DepthOperator = DepthCompareOperator::Less,
				.BackFaceCulling = false,
				.DepthTest = true,
				.DepthWrite = true,
				.WireFrame = false,
				.LineWidth = 1.0f
			};
			Ref<Pipeline> pipeline = Pipeline::Create(spec);

			RenderPassSpecification renderingPassSpec = {
				.DebugName = "RenderingPass",
				.Pipeline = pipeline,
				.MarkerColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)
			};
			m_RenderingPass = RenderPass::Create(renderingPassSpec);
			m_RenderingPass->SetInput("TransformUniformBuffer", m_UniformBufferSet);
			m_RenderingPass->SetInput("u_Texture", texture);
			m_RenderingPass->Bake();
		}

		// Screen pass
		m_ScreenPassMaterial = Material::Create(screenShader, "ScreenPassMaterial");
		{
			FramebufferSpecification screenFBSpec = {
				.DebugName = "Screen FB",
				.Attachments = { ImageFormat::RGBA },
			};

			Ref<Framebuffer> renderingFB = Framebuffer::Create(screenFBSpec);

			PipelineSpecification spec = {
				.DebugName = "ScreenPassPipeline",
				.Shader = screenShader,
				.TargetFramebuffer = renderingFB,
				.VertexLayout = {
					{ ShaderDataType::Float3, "a_Position" },
					{ ShaderDataType::Float2, "a_TexCoord" }
				},
				.Topology = PrimitiveTopology::Triangles,
				.BackFaceCulling = false,
				.DepthWrite = false,
				.WireFrame = false,
				.LineWidth = 1.0f
			};
			Ref<Pipeline> pipeline = Pipeline::Create(spec);

			RenderPassSpecification screenPassSpec = {
				.DebugName = "ScreenPass",
				.Pipeline = pipeline,
				.MarkerColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)
			};
			m_ScreenPass = RenderPass::Create(screenPassSpec);
			m_ScreenPass->Bake();
		}

		m_VertexBuffer = VertexBuffer::Create(s_Vertices.data(), (uint32_t)(sizeof(Vertex) * s_Vertices.size()));
		m_IndexBuffer = IndexBuffer::Create(s_Indices.data(), (uint32_t)(sizeof(uint32_t) * s_Indices.size()));

		m_EditorCamera.SetActive(true);
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

		// Update uniform buffers (Begin Scene stuff)
		UniformBufferData dataUB = {
			//.Model = glm::rotate(glm::mat4(1.0f), GetTime() * glm::radians(1.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
			.Model = glm::translate(glm::mat4(1.0f), glm::vec3{ 0.0f, 0.0f, 0.0f }),
			.ViewProjection = std::move(m_EditorCamera.GetProjection() * m_EditorCamera.GetViewMatrix())
		};

		m_UniformBufferSet->Get()->SetData(&dataUB, sizeof(UniformBufferData));

		m_CommandBuffer->Begin();
		VkCommandBuffer commandBuffer = m_CommandBuffer->GetActiveCommandBuffer();

		// First render pass renders to the framebuffer
		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_RenderingPass);

			VkBuffer vertexVulkanBuffer = m_VertexBuffer->GetVulkanBuffer();
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexVulkanBuffer, [](VkDeviceSize&& s) { return &s; }(0));
			vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->GetVulkanBuffer(), 0, VK_INDEX_TYPE_UINT32);

			vkCmdDrawIndexed(commandBuffer, (uint32_t)m_IndexBuffer->GetCount(), 1, 0, 0, 0);

			Renderer::EndRenderPass(m_CommandBuffer);
		}

		// Second render pass renders to the screen
		Ref<Texture2D> texture = m_RenderingPass->GetOutput(0);
		if (texture)
		{
			m_ScreenPassMaterial->Set("u_Texture", texture);
			Renderer::BeginRenderPass(m_CommandBuffer, m_ScreenPass);
			Renderer::SubmitFullScreenQuad(m_CommandBuffer, m_ScreenPass->GetPipeline(), m_ScreenPassMaterial);
			Renderer::EndRenderPass(m_CommandBuffer);
		}
		else
		{
			// Clear pass
			Renderer::BeginRenderPass(m_CommandBuffer, m_ScreenPass);
			Renderer::EndRenderPass(m_CommandBuffer);
		}

		m_CommandBuffer->End();
		m_CommandBuffer->Submit();

		//if (Input::IsKeyDown(KeyCode::R))
		//{
		//	Buffer textureBuffer;
		//	texture->CopyToHostBuffer(textureBuffer, true);
		//	stbi_write_png("Resources/assets/textures/output.png", texture->GetWidth(), texture->GetHeight(), 4, textureBuffer.Data, texture->GetWidth() * 4);
		//}

		bool leftAltWithEitherLeftOrMiddleButtonOrJustRight = (Input::IsKeyDown(KeyCode::LeftAlt) && (Input::IsMouseButtonDown(MouseButton::Left) || (Input::IsMouseButtonDown(MouseButton::Middle)))) || Input::IsMouseButtonDown(MouseButton::Right);
		bool notStartCameraViewportAndViewportHoveredFocused = !m_StartedCameraClickInViewport && m_ViewportPanelFocused && m_ViewportPanelMouseOver;
		if (leftAltWithEitherLeftOrMiddleButtonOrJustRight && notStartCameraViewportAndViewportHoveredFocused)
			m_StartedCameraClickInViewport = true;

		bool NotRightAndNOTLeftAltANDLeftOrMiddle = !Input::IsMouseButtonDown(MouseButton::Right) && !(Input::IsKeyDown(KeyCode::LeftAlt) && (Input::IsMouseButtonDown(MouseButton::Left) || Input::IsMouseButtonDown(MouseButton::Middle)));
		if (NotRightAndNOTLeftAltANDLeftOrMiddle)
			m_StartedCameraClickInViewport = true;
	}

	void EditorLayer::OnImGuiRender()
	{
		StartDocking();

		ImGui::ShowDemoWindow(); // Testing imgui stuff

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
		m_EditorCamera.SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y));

		//ImGui::TextColored({ 1.0f, 0.0f, 1.0f, 1.0f }, "Hello my friend, how are you?");
		//static glm::vec3 color = { 0.0f, 0.0f, 0.0f };
		//UI::ColorEdit3Control("Albedo", color);

		Ref<Texture2D> texture = m_RenderingPass->GetOutput(0);
		UI::Image(texture, viewportSize, { 0, 1 }, { 1, 0 });

		m_ViewportRect = UI::GetWindowRect();

		m_AllowViewportCameraEvents = (ImGui::IsMouseHoveringRect(m_ViewportRect.Min, m_ViewportRect.Max) && m_ViewportPanelFocused) || m_StartedCameraClickInViewport;

		ImGui::End();
		ImGui::PopStyleVar();
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