#include "Application.h"

#include "Renderer/Shader.h"
#include "Renderer/Renderer.h"

// TODO: THESE INCLUDES ARE TEMPORARY
#include "Renderer/Core/Vulkan.h" // TODO: TEMP! No vulkan calls should be here
#include "Renderer/Pipeline.h"
#include "Renderer/VertexBuffer.h"
#include "Renderer/IndexBuffer.h"

#include <glfw/glfw3.h>
#include <glm/glm.hpp>

namespace vkPlayground {
	
	// TODO: REMOVE
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Color;
	};

	// TODO: This should NOT BE HERE
	static std::vector<Vertex> s_Vertices;
	static std::vector<uint32_t> s_Indices;

	// TODO: REMOVE
	static void CreateStuff(Ref<Window> window)
	{
		// TODO: REMOVE
		// Interleaved vertex attributes
		s_Vertices = {
#if 0
			{ {-0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
			{ { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
			{ { 0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
			{ {-0.5f,  0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f } }
#else
			{ {-1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
			{ { 1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
			{ { 1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
			{ {-1.0f,  1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } }
#endif
		};

		s_Indices = { 0, 1, 2, 2, 3, 0 };
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
		m_Window->SetEventCallbackFunction([this](Events::Event& e) {Application::OnEvent(e); });

		// Set some more configurations for the window. (e.g. start maximized/centered, is it resizable? and all the stuff...)

		Renderer::SetConfig(spec.RendererConfig);
		// Init Renderer (Should compile all shaders in there and add them to shader library and create all sorts of cool stuff
		Renderer::Init();

		if (spec.StartMaximized)
			m_Window->Maximize();
		else
			m_Window->CentreWindow();

		m_Window->SetResizable(spec.Resizable);

		// TODO: REMOVE
		CreateStuff(m_Window);
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
		Ref<Shader> shader = Shader::Create("Shaders/SimpleShader.glsl"); // TODO: Will be moved to Renderer::Init() with the shaderlibrary

		VertexInputLayout layout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Color"    }
		};
		
		PipelineSpecification spec = {
			.DebugName = "PlaygroundPipeline",
			.Shader = shader,
			.TemporaryRenderPass = m_Window->GetSwapChain().GetRenderPass(), // TODO: TEMPORARY
			.TargetFramebuffer = nullptr,
			.Layout = layout,
			.Topology = PrimitiveTopology::Triangles,
			.DepthOperator = DepthCompareOperator::Greater,
			.BackFaceCulling = true,
			.DepthTest = false,
			.DepthWrite = false,
			.WireFrame = false,
			.LineWidth = 1.0f
		};
		Ref<Pipeline> pipeline = Pipeline::Create(spec);
		Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(s_Vertices.data(), (uint32_t)(sizeof(Vertex) * s_Vertices.size()));
		Ref<IndexBuffer> indexBuffer = IndexBuffer::Create(s_Indices.data(), (uint32_t)(sizeof(uint32_t) * s_Indices.size()));

		// NOTE: We can release the shader modules after the pipeline have been created
		shader->Release();

		while (m_Running)
		{
			ProcessEvents();

			if (!m_Minimized)
			{
				m_Window->GetSwapChain().BeginFrame();

				VkCommandBuffer commandBuffer = m_Window->GetSwapChain().GetCurrentDrawCommandBuffer();

				VkCommandBufferBeginInfo commandBufferBeginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
				VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

				VkRenderPassBeginInfo renderPassBeginInfo = {
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
					.renderPass = m_Window->GetSwapChain().GetRenderPass(),
					.framebuffer = m_Window->GetSwapChain().GetCurrentFramebuffer(),
					.renderArea = {
						.offset = {.x = 0, .y = 0 },
						.extent = {.width = m_Window->GetSwapChain().GetWidth(), .height = m_Window->GetSwapChain().GetHeight() }
					},
					.clearValueCount = 1,
					.pClearValues = [](auto&& temp) { return &temp; }(VkClearValue{.color = {.float32 = { 0.0f, 0.0f, 0.0f, 1.0f } } })
				};

				vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
				{
					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetVulkanPipeline());

					VkViewport viewport = {
						.x = 0.0f,
						.y = 0.0f,
						.width = (float)m_Window->GetSwapChain().GetWidth(),
						.height = (float)m_Window->GetSwapChain().GetHeight(),
						.minDepth = 0.0f,
						.maxDepth = 1.0f
					};
					vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

					VkRect2D scissor = {
						.offset = { 0, 0 },
						.extent = {.width = m_Window->GetSwapChain().GetWidth(), .height = m_Window->GetSwapChain().GetHeight() }
					};
					vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

					VkBuffer vertexVulkanBuffer = vertexBuffer->GetVulkanBuffer();
					vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexVulkanBuffer, [](VkDeviceSize&& s) { return &s; }(0));
					vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetVulkanBuffer(), 0, VK_INDEX_TYPE_UINT32);

					vkCmdDrawIndexed(commandBuffer, (uint32_t)s_Indices.size(), 1, 0, 0, 0);
				}
				vkCmdEndRenderPass(commandBuffer);

				vkEndCommandBuffer(commandBuffer);

				m_Window->SwapBuffers();

				m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % Renderer::GetConfig().FramesInFlight;
			}

			float time = GetTime();
			m_FrameTime = time - m_LastFrameTime;
			m_TimeStep = glm::min<float>(m_FrameTime, 0.0333f);
			m_LastFrameTime = time;
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