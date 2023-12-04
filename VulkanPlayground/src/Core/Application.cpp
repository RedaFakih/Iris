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

#include <glfw/glfw3.h>

// TODO: REMOVE
#include <stb_image_writer/stb_image_write.h>

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
		glm::mat4 View;
		glm::mat4 Projection;
	};

	// TODO: This should NOT BE HERE
	static std::vector<Vertex> s_Vertices;
	static std::vector<uint32_t> s_Indices;
	static std::vector<QuadVertex> s_QuadVertices;
	static std::vector<uint32_t> s_QuadIndices;

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

		s_QuadVertices = {
			{ {-1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } },
			{ { 1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },
			{ { 1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f } },
			{ {-1.0f,  1.0f, 0.0f }, { 1.0f, 1.0f } }
		};

		s_QuadIndices = {
			0, 1, 2, 2, 3, 0
		};
	}

	// TODO: REMOVE!
	static void DeleteStuff()
	{
		// TODO: REMOVE!
	}

	// TODO: This is here just to create some the hello traingle maybe move all the pipeline initializtion and stuff to somewhere else...
	Application::Application(const ApplicationSpecification& spec)
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
			screenPass->SetInput("u_Texture", renderingPass->GetOutput(0)); // Get the first color output of that pass
			screenPass->Bake();
		}

		Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(s_Vertices.data(), (uint32_t)(sizeof(Vertex) * s_Vertices.size()));
		Ref<IndexBuffer> indexBuffer = IndexBuffer::Create(s_Indices.data(), (uint32_t)(sizeof(uint32_t) * s_Indices.size()));
		Ref<VertexBuffer> quadVertexBuffer = VertexBuffer::Create(s_QuadVertices.data(), (uint32_t)(sizeof(QuadVertex) * s_QuadVertices.size()));
		Ref<IndexBuffer> quadIndexBuffer = IndexBuffer::Create(s_QuadIndices.data(), (uint32_t)(sizeof(uint32_t) * s_QuadIndices.size()));

		while (m_Running)
		{
			static uint64_t frameCounter = 0;

			ProcessEvents();

			if (!m_Minimized)
			{
				m_Window->GetSwapChain().BeginFrame();

				constexpr float speed = 8.0f;
				static glm::vec3 translationVector = { 0.0f, 0.0f, 0.0f };
				if (glfwGetKey(m_Window->GetNativeWindow(), GLFW_KEY_W))
					translationVector.x += 0.1f * m_TimeStep * speed;
				if (glfwGetKey(m_Window->GetNativeWindow(), GLFW_KEY_S))
					translationVector.x -= 0.1f * m_TimeStep * speed;
				if (glfwGetKey(m_Window->GetNativeWindow(), GLFW_KEY_D))
					translationVector.y += 0.1f * m_TimeStep * speed;
				if (glfwGetKey(m_Window->GetNativeWindow(), GLFW_KEY_A))
					translationVector.y -= 0.1f * m_TimeStep * speed;
				if (glfwGetKey(m_Window->GetNativeWindow(), GLFW_KEY_Q))
					translationVector.z -= 0.1f * m_TimeStep * speed;
				if (glfwGetKey(m_Window->GetNativeWindow(), GLFW_KEY_E))
					translationVector.z += 0.1f * m_TimeStep * speed;
				if (glfwGetKey(m_Window->GetNativeWindow(), GLFW_KEY_F))
					translationVector = { 0.0f, 0.0f, 0.0f };

				// Update uniform buffers (Begin Scene stuff)
				UniformBufferData dataUB = {
					.Model = glm::translate(glm::mat4(1.0f), translationVector) * glm::rotate(glm::mat4(1.0f), GetTime() * glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
					.View = glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
					.Projection = glm::perspective(glm::radians(45.0f), m_Window->GetSwapChain().GetWidth() / (float)m_Window->GetSwapChain().GetHeight(), 0.1f, 10.0f)
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
				{
					Renderer::BeginRenderPass(commandBuffer, screenPass);

					VkBuffer vertexVulkanBuffer = quadVertexBuffer->GetVulkanBuffer();
					vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexVulkanBuffer, [](VkDeviceSize&& s) { return &s; }(0));
					vkCmdBindIndexBuffer(commandBuffer, quadIndexBuffer->GetVulkanBuffer(), 0, VK_INDEX_TYPE_UINT32);
					
					vkCmdDrawIndexed(commandBuffer, (uint32_t)s_QuadIndices.size(), 1, 0, 0, 0);

					Renderer::EndRenderPass(commandBuffer);
				}

				vkEndCommandBuffer(commandBuffer);

				m_Window->SwapBuffers();

				m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % Renderer::GetConfig().FramesInFlight;
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