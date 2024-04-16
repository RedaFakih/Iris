#include "RuntimeLayer.h"

#include "Core/Input/Input.h"
#include "Renderer/Renderer.h"
#include "Asset/MeshImporter.h"

bool g_WireFrame = false;

namespace Iris {

	static Ref<Texture2D> s_BillBoardTexture;
	static Ref<MeshSource> s_MeshSource;
	static Ref<StaticMesh> s_Mesh;

	RuntimeLayer::RuntimeLayer()
		: m_EditorCamera(45.0f, 1280.0f, 720.0f, 0.1f, 1000.0f)
	{
	}

	RuntimeLayer::~RuntimeLayer()
	{
		s_BillBoardTexture = nullptr;
		s_MeshSource = nullptr;
		s_Mesh = nullptr;
	}

	void RuntimeLayer::OnAttach()
	{
		m_RuntimeScene = Scene::Create("Runtime Scene");

		m_ViewportRenderer = SceneRenderer::Create(m_RuntimeScene, { .RendererScale = 1.0f });
		m_ViewportRenderer->SetLineWidth(m_LineWidth);
		m_Renderer2D = Renderer2D::Create();
		m_Renderer2D->SetLineWidth(m_LineWidth);
		
		// Setup swapchain renderpass
		FramebufferSpecification compFramebufferSpec = {
			.DebugName = "SceneComposite",
			.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f },
			.Attachments = { ImageFormat::RGBA },
			.SwapchainTarget = true
		};

		PipelineSpecification pipelineSpec = {
			.DebugName = "CompositePipeline",
			.Shader = Renderer::GetShadersLibrary()->Get("TexturePass"),
			.TargetFramebuffer = Framebuffer::Create(compFramebufferSpec),
			.VertexLayout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			},
			.BackFaceCulling = false,
			.DepthWrite = false
		};

		RenderPassSpecification renderPassSpec = {
			.DebugName = "CompositePass",
			.Pipeline = Pipeline::Create(pipelineSpec),
			.MarkerColor = { 1.0f, 0.5f, 1.0f, 1.0f }
		};
		m_SwapChainRP = RenderPass::Create(renderPassSpec);
		m_SwapChainRP->Bake();

		m_SwapChainMaterial = Material::Create(pipelineSpec.Shader, "SwapChainMaterial");

		m_CommandBuffer = RenderCommandBuffer::CreateFromSwapChain("RuntimeLayer");

		// AssimpMeshImporter importer("Resources/assets/meshes/LowPolySponza/Sponza.gltf");
		AssimpMeshImporter importer("Resources/assets/meshes/stormtrooper/stormtrooper.gltf");
		// AssimpMeshImporter importer("Resources/assets/meshes/Cube.gltf");
		s_MeshSource = importer.ImportToMeshSource();
		s_Mesh = StaticMesh::Create(s_MeshSource);

		TextureSpecification textureSpec = {
			.DebugName = "Qiyana",
			.GenerateMips = true
		};
		s_BillBoardTexture = Texture2D::Create(textureSpec, "Resources/assets/textures/qiyana.png");

		// TODO: REMOVE When we have Editor UI
		Entity meshEntity = m_RuntimeScene->CreateEntity("MeshEntity");
		auto& staticMeshComponent = meshEntity.AddComponent<StaticMeshComponent>();
		staticMeshComponent.StaticMesh = s_Mesh;
		staticMeshComponent.MaterialTable = s_Mesh->GetMaterials();

	}

	void RuntimeLayer::OnDetach()
	{
		m_ViewportRenderer->SetScene(nullptr);

		IR_VERIFY(m_RuntimeScene->GetRefCount() == 1);
		m_RuntimeScene = nullptr;
	}

	void RuntimeLayer::OnUpdate(TimeStep ts)
	{
		auto [width, height] = Application::Get().GetWindow().GetSize();
		m_ViewportRenderer->SetViewportSize(width, height);
		m_RuntimeScene->SetViewportSize(width, height);
		m_EditorCamera.SetViewportSize(width, height);

		if (m_Width != width || m_Height != height)
		{
			m_Width = width;
			m_Height = height;

			m_Renderer2D->OnRecreateSwapchain();

			// Retrieve new main command buffer
			m_CommandBuffer = RenderCommandBuffer::CreateFromSwapChain("RuntimeLayer");
		}

		m_EditorCamera.SetActive(m_AllowViewportCameraEvents);
		m_EditorCamera.OnUpdate(ts);

		m_RuntimeScene->OnUpdateRuntime(ts);
		m_RuntimeScene->OnRenderEditor(m_ViewportRenderer, ts, m_EditorCamera);

		OnRender2D();

		Ref<Texture2D> finalImage = m_ViewportRenderer->GetFinalPassImage();
		if (finalImage)
		{
			m_SwapChainMaterial->Set("u_Texture", finalImage);

			m_CommandBuffer->Begin();
			Renderer::BeginRenderPass(m_CommandBuffer, m_SwapChainRP);
			Renderer::SubmitFullScreenQuad(m_CommandBuffer, m_SwapChainRP->GetPipeline(), m_SwapChainMaterial);
			Renderer::EndRenderPass(m_CommandBuffer);
			m_CommandBuffer->End();
		}
		else
		{
			// Clear render pass if no image is present
			m_CommandBuffer->Begin();
			Renderer::BeginRenderPass(m_CommandBuffer, m_SwapChainRP);
			Renderer::EndRenderPass(m_CommandBuffer);
			m_CommandBuffer->End();
		}
	}

	void RuntimeLayer::OnRender2D()
	{
		if (!m_ViewportRenderer->GetFinalPassImage())
			return;

		m_Renderer2D->ResetStats();
		m_Renderer2D->BeginScene(m_EditorCamera.GetViewProjection(), m_EditorCamera.GetViewMatrix());
		m_Renderer2D->SetTargetFramebuffer(m_ViewportRenderer->GetExternalCompositeFramebuffer());

		// `true` indicates that we transition resulting images to presenting layouts...
		m_Renderer2D->EndScene(true);
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
			case KeyCode::F:
				m_EditorCamera.Focus({ 0.0f, 0.0f, 0.0f });
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

	bool RuntimeLayer::OnMouseButtonPressed(Events::MouseButtonPressedEvent& e)
	{
		// TODO:

		return false;
	}

}