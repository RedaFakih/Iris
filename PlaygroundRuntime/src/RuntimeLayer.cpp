#include "RuntimeLayer.h"

#include "Core/Input/Input.h"
#include "Renderer/Renderer.h"

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

	RuntimeLayer::RuntimeLayer()
	{
	}

	RuntimeLayer::~RuntimeLayer()
	{
	}

	void RuntimeLayer::OnAttach()
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
		m_Renderer2D = Renderer2D::Create({ .CommandBuffer = m_CommandBuffer }); // TODO: Temp also for all the stuff inside Renderer2D!
		m_Renderer2D->SetLineWidth(2.0f);

		m_CommandBuffer = RenderCommandBuffer::CreateFromSwapChain("RuntimeLayer");

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
				// .ClearColorOnLoad = false,
				.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f },
				//.ClearDepthOnLoad = false,
				.DepthClearValue = 1.0f,
				.Attachments = { ImageFormat::RGBA, ImageFormat::DEPTH32F },
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
				.SwapchainTarget = true
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
				.MarkerColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)
			};
			m_ScreenPass = RenderPass::Create(screenPassSpec);
			m_ScreenPass->Bake();
		}

		// Intermediate Renderer2D framebuffer
		{
			FramebufferSpecification intermediateBufferSpec = {
				.DebugName = "IntermediateFB",
				.ClearColorOnLoad = false,
				.ClearDepthOnLoad = false,
				.Attachments = { ImageFormat::RGBA, ImageFormat::DEPTH32F }
			};
			intermediateBufferSpec.ExistingImages[0] = m_ScreenPass->GetOutput(0);
			intermediateBufferSpec.ExistingImages[1] = m_RenderingPass->GetDepthOutput();
			//m_IntermediateBuffer = Framebuffer::Create(intermediateBufferSpec);
		}

		m_VertexBuffer = VertexBuffer::Create(s_Vertices.data(), (uint32_t)(sizeof(Vertex) * s_Vertices.size()));
		m_IndexBuffer = IndexBuffer::Create(s_Indices.data(), (uint32_t)(sizeof(uint32_t) * s_Indices.size()));
	}

	void RuntimeLayer::OnDetach()
	{
	}

	void RuntimeLayer::OnUpdate(TimeStep ts)
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

		// Second render pass renders to the screen (TODO: This is currently used so that we could render to the screen without imgui)
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

		// TODO: Should insert an image memory barrier here for the compositing images (color and depth) that will be used for the renderer2D
		//Renderer::InsertImageMemoryBarrier(m_CommandBuffer->GetActiveCommandBuffer(),
		//	m_ScreenPass->GetOutput(0)->GetVulkanImage(),
		//	VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		//	VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
		//	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		//	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		//	VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		//	VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		//	{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1});

		m_CommandBuffer->End();
		m_CommandBuffer->Submit();

		// TODO: For now we can not render 2D stuff unless we render them to the screen pass which works for now.
		// TODO: However we want to create an intermediate framebuffer that is just a placeholder of existing images that
		//		 allows the renderer2D to render to those images without clearing them and render on top of existing data.
		//if (m_ScreenPass->GetOutput(0))
		//{
		//	m_Renderer2D->ResetStats();
		//	m_Renderer2D->BeginScene(m_EditorCamera.GetViewProjection(), m_EditorCamera.GetViewMatrix());
		//	m_Renderer2D->SetTargetFramebuffer(m_IntermediateBuffer);

		//	for (int x = -15; x < 15; x++)
		//	{
		//		for (int y = -3; y < 3; y++)
		//		{
		//			m_Renderer2D->DrawQuad({ x, y }, { 1, 1 }, { glm::sin(x), glm::cos(y), glm::sin(x + y), 1.0f });
		//		}
		//	}

		//	m_Renderer2D->DrawAABB({ {5.0f, 5.0f, -2.0f}, {7.0f, 7.0f, -4.0f} }, glm::mat4(1.0f));
		//	m_Renderer2D->DrawCircle({ 5.0f, 5.0f, -2.0f }, glm::vec3(1.0f), 2.0f, { 1.0f, 0.0f, 1.0f, 1.0f });
		//	m_Renderer2D->DrawLine(glm::vec3(0.0f), glm::vec3(6.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
		//	m_Renderer2D->DrawQuadBillboard({ -2.0f, -2.0f, 2.0f }, { 2.0f, 2.0f }, { 0.0f, 0.0f, 1.0f, 1.0f });

		//	m_Renderer2D->EndScene();
		//}

		//if (Input::IsKeyDown(KeyCode::R))
		//{
		//	Buffer textureBuffer;
		//	texture->CopyToHostBuffer(textureBuffer, true);
		//	stbi_write_png("Resources/assets/textures/output.png", texture->GetWidth(), texture->GetHeight(), 4, textureBuffer.Data, texture->GetWidth() * 4);
		//}
	}

	void RuntimeLayer::OnEvent(Events::Event& e)
	{
		if (m_AllowViewportCameraEvents)
			m_EditorCamera.OnEvent(e);

		Events::EventDispatcher dispatcher(e);
		dispatcher.Dispatch<Events::KeyPressedEvent>([this](Events::KeyPressedEvent& e) { return OnKeyPressed(e); });
		dispatcher.Dispatch<Events::MouseButtonPressedEvent>([this](Events::MouseButtonPressedEvent& e) { return OnMouseButtonPressed(e); });
	}

	bool RuntimeLayer::OnKeyPressed(Events::KeyPressedEvent& e)
	{
		switch (e.GetKeyCode())
		{
			// TODO:
			case KeyCode::F:
				m_EditorCamera.Focus(glm::vec3(0.0f));
		}

		return false;
	}

	bool RuntimeLayer::OnMouseButtonPressed(Events::MouseButtonPressedEvent& e)
	{
		// TODO:

		return false;
	}

}