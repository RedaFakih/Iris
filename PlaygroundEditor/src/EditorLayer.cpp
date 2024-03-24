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
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

// TODO: REMOVE
#include <stb/stb_image_writer/stb_image_write.h>
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
		glm::vec2 DepthUnpackConsts; // TODO: TEMPORARY ALSO IN SHADERS
	};

	static std::vector<Vertex> s_Vertices;
	static std::vector<uint32_t> s_Indices;
	static Ref<Texture2D> s_BillBoardTexture;
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
		// TODO: REMOVE
		s_BillBoardTexture = nullptr;
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

		m_EditorScene = Scene::Create("Editor Scene");
		m_ViewportRenderer = SceneRenderer::Create(m_EditorScene);
		// TODO: 
		//m_ViewportRenderer->SetScene(m_EditorScene);
		//m_ViewportRenderer->SetLineWidth(m_LineWidth);
		m_Renderer2D = Renderer2D::Create(); // TODO: Temp also for all the stuff inside Renderer2D!
		m_Renderer2D->SetLineWidth(2.0f);

		EditorResources::Init();

		m_CommandBuffer = RenderCommandBuffer::Create(0, "EditorLayer");
		m_ScreenCommandBuffer = RenderCommandBuffer::Create(0, "ScreenPass"); // TODO: TEMP

		// TODO: REMOVE!
		Ref<Shader> renderingShader = Renderer::GetShadersLibrary()->Get("SimpleShader");
		Ref<Shader> screenShader = Renderer::GetShadersLibrary()->Get("TexturePass");
		Ref<Shader> compositingShader = Renderer::GetShadersLibrary()->Get("Compositing");

		m_UniformBufferSet = UniformBufferSet::Create(sizeof(UniformBufferData));

		TextureSpecification textureSpec = {
			.DebugName = "Qiyana",
			.GenerateMips = true
		};
		s_BillBoardTexture = Texture2D::Create(textureSpec, "Resources/assets/textures/qiyana.png");

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
				.DepthClearValue = 0.0f,
				.Attachments = { { ImageFormat::RGBA, AttachmentPassThroughUsage::Input }, { ImageFormat::DEPTH32F, AttachmentPassThroughUsage::Input } },
			};

			PipelineSpecification spec = {
				.DebugName = "RenderingPassPipeline",
				.Shader = renderingShader,
				.TargetFramebuffer = Framebuffer::Create(mainFBspec),
				.VertexLayout = {
					{ ShaderDataType::Float3, "a_Position" },
					{ ShaderDataType::Float3, "a_Color"    },
					{ ShaderDataType::Float2, "a_TexCoord" }
				},
				.Topology = PrimitiveTopology::Triangles,
				.DepthOperator = DepthCompareOperator::GreaterOrEqual,
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
			m_RenderingPass->SetInput("u_Texture", s_BillBoardTexture);
			m_RenderingPass->Bake();
		}

		// Screen pass (Currently used to render the depth image)
		{
			FramebufferSpecification screenFBSpec = {
				.DebugName = "Screen FB",
				.Attachments = { ImageFormat::RGBA },
			};

			PipelineSpecification spec = {
				.DebugName = "ScreenPassPipeline",
				.Shader = screenShader,
				.TargetFramebuffer = Framebuffer::Create(screenFBSpec),
				.VertexLayout = {
					{ ShaderDataType::Float3, "a_Position" },
					{ ShaderDataType::Float2, "a_TexCoord" }
				},
				.Topology = PrimitiveTopology::Triangles,
				.BackFaceCulling = false,
				.DepthTest = false,
				.DepthWrite = false,
				.WireFrame = false,
				.LineWidth = 1.0f
			};
			Ref<Pipeline> pipeline = Pipeline::Create(spec);

			RenderPassSpecification screenPassSpec = {
				.DebugName = "ScreenPass",
				.Pipeline = pipeline,
				.MarkerColor = { 0.0f, 1.0f, 0.0f, 1.0f }
			};
			m_ScreenPass = RenderPass::Create(screenPassSpec);
			m_ScreenPass->SetInput("TransformUniformBuffer", m_UniformBufferSet);
			m_ScreenPass->Bake();

			m_ScreenPassMaterial = Material::Create(screenShader, "ScreenPassMaterial");
		}

		// Intermediate Renderer2D framebuffer
		{
			FramebufferSpecification intermediateBufferSpec = {
				.DebugName = "IntermediateFB",
				.ClearColorOnLoad = false,
				.ClearDepthOnLoad = false,
				.Attachments = { { ImageFormat::RGBA, AttachmentPassThroughUsage::Input }, { ImageFormat::DEPTH32F, AttachmentPassThroughUsage::Input } }
			};
			intermediateBufferSpec.ExistingImages[0] = m_RenderingPass->GetOutput(0);
			intermediateBufferSpec.ExistingImages[1] = m_RenderingPass->GetDepthOutput();
			m_IntermediateBuffer = Framebuffer::Create(intermediateBufferSpec);
		}

		m_VertexBuffer = VertexBuffer::Create(s_Vertices.data(), (uint32_t)(sizeof(Vertex) * s_Vertices.size()));
		m_IndexBuffer = IndexBuffer::Create(s_Indices.data(), (uint32_t)(sizeof(uint32_t) * s_Indices.size()));
	}

	void EditorLayer::OnDetach()
	{
		EditorResources::Shutdown();
	}

	static glm::vec3 s_Position;

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

		// Update uniform buffers (Begin Scene stuff)
		UniformBufferData dataUB = {
			.Model = glm::rotate(glm::mat4(1.0f), Application::Get().GetTime() * glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::scale(glm::mat4(1.0f), {5.0f, 5.0f, 1.0f}),
			// .Model = glm::translate(glm::mat4(1.0f), glm::vec3{ 0.0f, 0.0f, 0.0f }),
			.ViewProjection = std::move(m_EditorCamera.GetProjectionMatrix() * m_EditorCamera.GetViewMatrix()),
			.DepthUnpackConsts = { depthLinearizeMul, depthLinearizeAdd }
		};

		m_UniformBufferSet->Get()->SetData(&dataUB, sizeof(UniformBufferData));

		m_CommandBuffer->Begin();

		// First render pass renders to the framebuffer
		{
			VkCommandBuffer commandBuffer = m_CommandBuffer->GetActiveCommandBuffer();
			Renderer::BeginRenderPass(m_CommandBuffer, m_RenderingPass);

			VkBuffer vertexVulkanBuffer = m_VertexBuffer->GetVulkanBuffer();
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexVulkanBuffer, [](VkDeviceSize&& s) { return &s; }(0));
			vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->GetVulkanBuffer(), 0, VK_INDEX_TYPE_UINT32);

			vkCmdDrawIndexed(commandBuffer, (uint32_t)m_IndexBuffer->GetCount(), 1, 0, 0, 0);

			Renderer::EndRenderPass(m_CommandBuffer);
		}

		// Second render pass renders to the screen (TODO: This is currently used so that we could render to the screen without imgui)
		// NOTE: For now disabled since it is useless for the editor layer
		//Ref<Texture2D> texture = m_RenderingPass->GetOutput(0);
		//if (texture)
		//{
		//	m_ScreenPassMaterial->Set("u_Texture", texture);
		//	Renderer::BeginRenderPass(m_CommandBuffer, m_ScreenPass);
		//	Renderer::SubmitFullScreenQuad(m_CommandBuffer, m_ScreenPass->GetPipeline(), m_ScreenPassMaterial);
		//	Renderer::EndRenderPass(m_CommandBuffer);
		//}
		//else
		//{
		//	// Clear pass
		//	Renderer::BeginRenderPass(m_CommandBuffer, m_ScreenPass);
		//	Renderer::EndRenderPass(m_CommandBuffer);
		//}

		m_CommandBuffer->End();
		m_CommandBuffer->Submit();

		if (m_RenderingPass->GetOutput(0))
		{
			m_Renderer2D->ResetStats();
			m_Renderer2D->BeginScene(m_EditorCamera.GetViewProjection(), m_EditorCamera.GetViewMatrix());
			m_Renderer2D->SetTargetFramebuffer(m_IntermediateBuffer);
			m_Renderer2D->SetLineWidth(5.0f);

			m_Renderer2D->DrawQuadBillboard({ -3.053855f, 4.328760f, 1.0f }, { 2.0f, 2.0f }, { 1.0f, 0.0f, 1.0f, 1.0f });

			for (int x = -15; x < 15; x++)
			{
				for (int y = -3; y < 3; y++)
				{
					m_Renderer2D->DrawQuad({ x, y, 2.0f }, { 1, 1 }, { glm::sin(x), glm::cos(y), glm::sin(x + y), 1.0f });
				}
			}

			m_Renderer2D->DrawAABB({ {5.0f, 5.0f, 1.0f}, {7.0f, 7.0f, -4.0f} }, glm::mat4(1.0f));
			m_Renderer2D->DrawAABB({ {4.0f, 4.0f, -1.0f}, {5.0f, 5.0f, -4.0f} }, glm::translate(glm::mat4(1.0f), {2.0f, -1.0f, -0.5f}), { 0.0f, 1.0f, 1.0f, 1.0f });
			m_Renderer2D->DrawCircle({ 5.0f, 5.0f, -2.0f }, glm::vec3(1.0f), 2.0f, { 1.0f, 0.0f, 1.0f, 1.0f });
			m_Renderer2D->DrawLine(glm::vec3(0.0f), glm::vec3(6.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
			m_Renderer2D->DrawQuadBillboard({ -2.0f, -2.0f, -0.5f }, { 2.0f, 2.0f }, s_BillBoardTexture, 2.0f, {1.0f, 0.7f, 1.0f, 1.0f});

			m_Renderer2D->EndScene();
			// Here the color attachment is now in VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			// Depth attachment is now in VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			// so we need to pipeline barrier the images to sample from them in the following passes
		}

		// TODO: This here is kind of temporary since rendering the depth image is not really a main thing to do in the engine LOL
		// NOTE: This is to visualize the depth image with all the 2D depth just for debugging purposes
		{
			m_ScreenCommandBuffer->Begin();

			Renderer::InsertImageMemoryBarrier(
				m_ScreenCommandBuffer->GetActiveCommandBuffer(),
				m_IntermediateBuffer->GetDepthImage()->GetVulkanImage(),
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
			);

			Renderer::InsertImageMemoryBarrier(
				m_ScreenCommandBuffer->GetActiveCommandBuffer(),
				m_IntermediateBuffer->GetImage(0)->GetVulkanImage(),
				0,
				0,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
			);

			m_ScreenPassMaterial->Set("u_Texture", m_IntermediateBuffer->GetDepthImage());
			Renderer::BeginRenderPass(m_ScreenCommandBuffer, m_ScreenPass);
			Renderer::SubmitFullScreenQuad(m_ScreenCommandBuffer, m_ScreenPass->GetPipeline(), m_ScreenPassMaterial);
			Renderer::EndRenderPass(m_ScreenCommandBuffer);

			m_ScreenCommandBuffer->End();
			m_ScreenCommandBuffer->Submit();
		}

		// NOTE: This gives the image upside down since that is the output of the SceneRenderer and then we flip it in imgui so...
		// if (Input::IsKeyDown(KeyCode::R))
		// {
		// 	Buffer textureBuffer;
		// 	Ref<Texture2D> texture = m_RenderingPass->GetOutput(0);
		// 	texture->CopyToHostBuffer(textureBuffer, true);
		// 	stbi_write_png("Resources/assets/textures/output.png", texture->GetWidth(), texture->GetHeight(), 4, textureBuffer.Data, texture->GetWidth() * 4);
		// }

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

		ImGui::Begin("Depth Image");

		UI::Image(m_ScreenPass->GetOutput(0), ImGui::GetContentRegionAvail(), { 0, 1 }, { 1, 0 });

		ImGui::End();

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
		// TODO: 
		//m_ViewportRenderer->SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y));
		m_EditorScene->SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y));
		m_EditorCamera.SetViewportSize(static_cast<uint32_t>(viewportSize.x), static_cast<uint32_t>(viewportSize.y));

		// Here we get the output from the rendering pass since the screen pass is there in case there was no imgui in the
		// application and we have to render directly to the screen...
		Ref<Texture2D> texture = m_RenderingPass->GetOutput(0);
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