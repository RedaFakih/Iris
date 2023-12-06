#include "vkPch.h"
#include "Application.h"

#include "Renderer/Shaders/Shader.h"
#include "Renderer/Renderer.h"

// TODO: THESE INCLUDES ARE TEMPORARY
#include "Renderer/Core/Vulkan.h" // TODO: TEMP! No vulkan calls should be here
#include "Renderer/Pipeline.h"
#include "Renderer/VertexBuffer.h"
#include "Renderer/IndexBuffer.h"
#include "Renderer/UniformBufferSet.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Mesh.h"
#include "Renderer/Material.h"
#include "Editor/EditorCamera.h"
#include "Input/Input.h"

#include <glfw/glfw3.h>

// TODO: REMOVE
//#include <stb_image_writer/stb_image_write.h>
//#include <fastgltf/glm_element_traits.hpp>
//#include <fastgltf/parser.hpp>
//#include <fastgltf/tools.hpp>
//#include <fastgltf/types.hpp>

namespace vkPlayground {
	
	// TODO: REMOVE

	struct QuadVertex
	{
		glm::vec3 Position;
		glm::vec2 TexCoord;
	};

	struct UniformBufferData
	{
		glm::mat4 Model;
		glm::mat4 ViewProjection;
	};

	// TODO: This should NOT BE HERE
	static EditorCamera s_Camera;
	static std::vector<Vertex> s_Vertices;
	static std::vector<uint32_t> s_Indices;
	//static std::vector<Vertex> s_TestVertices;
	//static std::vector<uint32_t> s_TestIndices;

	// TODO: REMOVE
	static void CreateStuff(Window* window)
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

		s_Camera = EditorCamera(45.0f, 1280.0f, 720.0f, 0.1f, 10000.0f);
	}

	// TODO: REMOVE!
	static void DeleteStuff()
	{
		// TODO: REMOVE!
	}

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

	// TODO: This is here just to create some the hello traingle maybe move all the pipeline initializtion and stuff to somewhere else...
	Application::Application(const ApplicationSpecification& spec)
		: m_Specification(spec)
	{
		PG_ASSERT(!s_Instance, "No more than 1 application can be created!");
		s_Instance = this;

		if (!spec.WorkingDirectory.empty())
			std::filesystem::current_path(spec.WorkingDirectory);

		WindowSpecification windowSpec = {
			.Title = spec.Name,
			.Width = spec.WindowWidth,
			.Height = spec.WindowHeight,
			.VSync = spec.VSync,
			.FullScreen = spec.FullScreen
		};
		m_Window = Window::Create(windowSpec);
		m_Window->Init();
		m_Window->SetEventCallbackFunction([this](Events::Event& e) { Application::OnEvent(e); });

		// Set some more configurations for the window. (e.g. start maximized/centered, is it resizable? and all the stuff...)

		Renderer::SetConfig(spec.RendererConfig);
		Renderer::Init();

		if (spec.StartMaximized)
			m_Window->Maximize();
		else
			m_Window->CentreWindow();

		m_Window->SetResizable(spec.Resizable);

		// TODO: REMOVE
		CreateStuff(m_Window.get());
	}

	Application::~Application()
	{
		// TODO: REMOVE!
		DeleteStuff();

		// TODO: Some other things maybe?
		m_Window->SetEventCallbackFunction([](Events::Event& e) {});

		Renderer::Shutdown();

		// NOTE: We can't set the s_Instance to nullptr here since the application will still be used in other parts of the application to
		// retrieve certain data to destroy other data...
		// s_Instance = nullptr;
	}

	void Application::Run()
	{
		// TODO: REMOVE!
		Ref<Shader> renderingShader = Renderer::GetShadersLibrary()->Get("SimpleShader");
		Ref<Shader> screenShader = Renderer::GetShadersLibrary()->Get("TexturePass");
		Ref<UniformBufferSet> uniformBufferSet = UniformBufferSet::Create(sizeof(UniformBufferData));
		TextureSpecification textureSpec = {
			.DebugName = "Qiyana",
			.GenerateMips = true
		};
		Ref<Texture2D> texture = Texture2D::Create(textureSpec, "assets/textures/qiyana.png");
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

		// Buffer textureBuffer;
		// texture->CopyToHostBuffer(textureBuffer, true);
		// stbi_write_png("assets/textures/output.png", texture->GetWidth(), texture->GetHeight(), 4, textureBuffer.Data, texture->GetWidth() * 4);

		//glm::mat4 transform = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), { 1.0f, 0.0f, 0.0f })
		//	* glm::scale(glm::mat4(1.0f), glm::vec3(0.2f));
		//LoadModelFromFileBase("assets/meshes/stormtrooper/stormtrooper.gltf", transform, false);
		//Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(s_TestVertices.data(), (uint32_t)(sizeof(Vertex) * s_TestVertices.size()));
		//Ref<IndexBuffer> indexBuffer = IndexBuffer::Create(s_TestIndices.data(), (uint32_t)(sizeof(uint32_t) * s_TestIndices.size()));

		// Rendering pass
		Ref<RenderPass> renderingPass;
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
				.BackFaceCulling = true,
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
			renderingPass = RenderPass::Create(renderingPassSpec);
			renderingPass->SetInput("TransformUniformBuffer", uniformBufferSet);
			renderingPass->SetInput("u_Texture", texture);
			renderingPass->Bake();
		}

		// Screen pass
		Ref<RenderPass> screenPass;
		Ref<Material> screenPassMaterial = Material::Create(screenShader, "ScreenPassMaterial");
		{
			FramebufferSpecification screenFBSpec = {
				.DebugName = "Screen FB",
				.Attachments = { ImageFormat::RGBA },
				.SwapchainTarget = true,
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
			screenPass = RenderPass::Create(screenPassSpec);
			screenPass->Bake();
		}

		Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(s_Vertices.data(), (uint32_t)(sizeof(Vertex) * s_Vertices.size()));
		Ref<IndexBuffer> indexBuffer = IndexBuffer::Create(s_Indices.data(), (uint32_t)(sizeof(uint32_t) * s_Indices.size()));

		s_Camera.SetActive(true);

		while (m_Running)
		{
			static uint64_t frameCounter = 0;

			ProcessEvents();

			if (!m_Minimized)
			{
				m_Window->GetSwapChain().BeginFrame();

				if (Input::IsKeyDown(KeyCode::F))
					s_Camera.Focus({ 0.0f, 0.0f, 0.0f });

				// Update uniform buffers (Begin Scene stuff)
				UniformBufferData dataUB = {
					//.Model = glm::rotate(glm::mat4(1.0f), GetTime() * glm::radians(1.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
					.Model = glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, 0.0f }),
					.ViewProjection = std::move(s_Camera.GetProjection() * s_Camera.GetViewMatrix())
				};

				uniformBufferSet->Get()->SetData(&dataUB, sizeof(UniformBufferData));

				VkCommandBuffer commandBuffer = m_Window->GetSwapChain().GetCurrentDrawCommandBuffer();

				VkCommandBufferBeginInfo commandBufferBeginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
				VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

				// First render pass renders to the framebuffer
				{
					Renderer::BeginRenderPass(commandBuffer, renderingPass);

					VkBuffer vertexVulkanBuffer = vertexBuffer->GetVulkanBuffer();
					vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexVulkanBuffer, [](VkDeviceSize&& s) { return &s; }(0));
					vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetVulkanBuffer(), 0, VK_INDEX_TYPE_UINT32);

					vkCmdDrawIndexed(commandBuffer, (uint32_t)indexBuffer->GetCount(), 1, 0, 0, 0);

					Renderer::EndRenderPass(commandBuffer);
				}

				// Second render pass renders to the screen
				Ref<Texture2D> texture = renderingPass->GetOutput(0);
				if (texture)
				{
					screenPassMaterial->Set("u_Texture", texture);
					Renderer::BeginRenderPass(commandBuffer, screenPass);
					Renderer::SubmitFullScreenQuad(commandBuffer, screenPass->GetPipeline(), screenPassMaterial);
					Renderer::EndRenderPass(commandBuffer);
				}
				else
				{
					// Clear pass
					Renderer::BeginRenderPass(commandBuffer, screenPass);
					Renderer::EndRenderPass(commandBuffer);
				}

				vkEndCommandBuffer(commandBuffer);

				m_Window->SwapBuffers();

				m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % Renderer::GetConfig().FramesInFlight;
				s_Camera.OnUpdate(m_TimeStep);
			}

			float time = GetTime();
			m_FrameTime = time - m_LastFrameTime;
			m_TimeStep = glm::min<float>(m_FrameTime, 0.0333f);
			m_LastFrameTime = time;

			++frameCounter;
		}
	}

	void Application::ProcessEvents()
	{
		m_Window->ProcessEvents();

		// NOTE: In case we were to run the events on a different thread
		std::scoped_lock<std::mutex> eventLock(m_EventQueueMutex);

		while (m_EventQueue.size())
		{
			auto& event = m_EventQueue.front();
			event();
			m_EventQueue.pop();
		}
	}

	void Application::OnEvent(Events::Event& e)
	{
		Events::EventDispatcher dispatcher(e);

		s_Camera.OnEvent(e);

		dispatcher.Dispatch<Events::WindowResizeEvent>([this](Events::WindowResizeEvent& event) { return OnWindowResize(event); });
		dispatcher.Dispatch<Events::WindowCloseEvent>([this](Events::WindowCloseEvent& event) { return OnWindowClose(event); });
		dispatcher.Dispatch<Events::WindowMinimizeEvent>([this](Events::WindowMinimizeEvent& event) { return OnWindowMinimize(event); });
	}

	bool Application::OnWindowResize(Events::WindowResizeEvent& e)
	{
		const uint32_t width = e.GetWidth(), height = e.GetHeight();
		if (width == 0 || height == 0)
		{
			// m_Minimized = true; No need to set this here since there is no way the window can be minimized to (0, 0) (otherwise would crash) unless it was `iconified` and that event is handled separately
			return false;
		}

		m_Window->GetSwapChain().OnResize(width, height);

		return false;
	}

	bool Application::OnWindowClose(Events::WindowCloseEvent& e)
	{
		m_Running = false;

		return false;
	}

	bool Application::OnWindowMinimize(Events::WindowMinimizeEvent& e)
	{
		m_Minimized = e.IsMinimized();

		return false;
	}

	float Application::GetTime() const
	{
		return static_cast<float>(glfwGetTime());
	}

}