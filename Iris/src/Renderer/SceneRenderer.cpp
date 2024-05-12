#include "IrisPCH.h"
#include "SceneRenderer.h"

#include "AssetManager/AssetManager.h"
#include "Renderer/Renderer.h"
#include "Shaders/Shader.h"
#include "Texture.h"
#include "UniformBufferSet.h"

namespace Iris {

	Ref<SceneRenderer> SceneRenderer::Create(Ref<Scene> scene, const SceneRendererSpecification& spec)
	{
		return CreateRef<SceneRenderer>(scene, spec);
	}

	SceneRenderer::SceneRenderer(Ref<Scene> scene, const SceneRendererSpecification& spec)
		: m_Scene(scene), m_Specification(spec)
	{
		Init();
	}

	SceneRenderer::~SceneRenderer()
	{
		Shutdown();
	}

	// NOTE: We do not want any multisampling...
	constexpr uint32_t c_Samples = 1;

	void SceneRenderer::Init()
	{
		IR_CORE_WARN_TAG("Renderer", "Initializing Scene Renderer for scene: {0}", m_Scene->GetName());

		m_ViewportWidth = m_Specification.ViewportWidth;
		m_ViewportHeight = m_Specification.ViewportHeight;
		if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
			m_NeedsResize = true;

		m_CommandBuffer = RenderCommandBuffer::Create(0, "SceneRenderer");

		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;

		m_UBSCamera = UniformBufferSet::Create(sizeof(UBCamera));
		m_UBSScreenData = UniformBufferSet::Create(sizeof(UBScreenData));

		m_Renderer2D = Renderer2D::Create();

		VertexInputLayout vertexLayout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Normal"   },
			{ ShaderDataType::Float3, "a_Tangent"  },
			{ ShaderDataType::Float3, "a_Binormal" },
			{ ShaderDataType::Float2, "a_TexCoord" }
		};

		// Instance transform buffers layout will stay the same across all shaders... so should be fine using it this way
		VertexInputLayout instanceLayout = {
			{ ShaderDataType::Float4, "a_MatrixRow0" },
			{ ShaderDataType::Float4, "a_MatrixRow1" },
			{ ShaderDataType::Float4, "a_MatrixRow2" }
		};

		// Pre-Depth
		{
			FramebufferSpecification preDepthFBSpec = {
				.DebugName = "OpaquePreDepthFB",
				// Linear depth, reversed depth buffer
				.DepthClearValue = 0.0f,
				.Attachments = { { ImageFormat::DEPTH32F, AttachmentPassThroughUsage::Input } },
				.Samples = c_Samples
			};

			PipelineSpecification preDepthPipelineSpec = {
				.DebugName = "OpaquePreDepthPipeline",
				.Shader = Renderer::GetShadersLibrary()->Get("PreDepth"),
				.TargetFramebuffer = Framebuffer::Create(preDepthFBSpec),
				.VertexLayout = vertexLayout,
				.InstanceLayout = instanceLayout,
				.Topology = PrimitiveTopology::Triangles,
				.DepthOperator = DepthCompareOperator::GreaterOrEqual
			};
			m_PreDepthPipeline = Pipeline::Create(preDepthPipelineSpec);

			RenderPassSpecification preDepthRenderPassSpec = {
				.DebugName = "OpaquePreDepthRenderPass",
				.Pipeline = m_PreDepthPipeline,
				.MarkerColor = { 1.0f, 1.0f, 0.0f, 1.0f }
			};
			m_PreDepthPass = RenderPass::Create(preDepthRenderPassSpec);
			m_PreDepthMaterial = Material::Create(preDepthPipelineSpec.Shader, "PreDepthMaterial");

			m_PreDepthPass->SetInput("Camera", m_UBSCamera);
			m_PreDepthPass->Bake();

			preDepthPipelineSpec.DebugName = "WireframePreDepthPipeline";
			preDepthPipelineSpec.WireFrame = true;
			Ref<Pipeline> pipeline = Pipeline::Create(preDepthPipelineSpec);
			preDepthRenderPassSpec.DebugName = "WireframePreDepthPass";
			preDepthRenderPassSpec.Pipeline = pipeline;
			m_WireframeViewPreDepthPass = RenderPass::Create(preDepthRenderPassSpec);

			m_WireframeViewPreDepthPass->SetInput("Camera", m_UBSCamera);
			m_WireframeViewPreDepthPass->Bake();
		}

		// Geometry
		{
			FramebufferSpecification geometryFBSpec = {
				.DebugName = "StaticGeometryPassFB",
				.ClearColorOnLoad = true,
				.ClearColor = { 0.04f, 0.04f, 0.04f, 1.0f },
				.ClearDepthOnLoad = false,
				// TODO: The first attachment has to load... Since the skybox (when we have it...) pass writes to it before
				// NOTE: The second attachment does not blend since we do not want to blend with luminance in the alpha channel
				.Attachments = { { ImageFormat::RGBA32F, AttachmentLoadOp::Clear } , { ImageFormat::RGBA16F, false }, ImageFormat::RGBA, { ImageFormat::DEPTH32F, AttachmentPassThroughUsage::Input } },
				.Samples = c_Samples
			};
			geometryFBSpec.ExistingImages[3] = m_PreDepthPass->GetDepthOutput(true); // Ignore the resolve image if the buffer is multisampeld

			PipelineSpecification staticGeometryPipelineSpec = {
				.DebugName = "PBRStaticPipeline",
				.Shader = Renderer::GetShadersLibrary()->Get("IrisPBRStatic"),
				.TargetFramebuffer = Framebuffer::Create(geometryFBSpec),
				.VertexLayout = vertexLayout,
				.InstanceLayout = instanceLayout,
				.BackFaceCulling = true,
				.DepthOperator = DepthCompareOperator::Equal,
				.DepthWrite = false,
				.LineWidth = m_LineWidth
			};
			m_GeometryPipeline = Pipeline::Create(staticGeometryPipelineSpec);

			RenderPassSpecification staticGeometryPassSpec = {
				.DebugName = "StaticGeometryRenderPass",
				.Pipeline = m_GeometryPipeline,
				.MarkerColor = { 1.0f, 0.0f, 0.0f, 1.0f }
			};
			m_GeometryPass = RenderPass::Create(staticGeometryPassSpec);

			m_GeometryPass->SetInput("Camera", m_UBSCamera);
			m_GeometryPass->Bake();
		}

		// Selected Geometry isolation
		{
			FramebufferSpecification selectedGeoFB = {
				.DebugName = "SelectedGeoIsolationFB",
				.ClearColorOnLoad = true,
				.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f },
				.ClearDepthOnLoad = true,
				.DepthClearValue = 1.0f,
				.Attachments = { ImageFormat::RGBA32F, ImageFormat::DEPTH32F },
				.Samples = 1
			};

			PipelineSpecification selectedGeoPipeline = {
				.DebugName = "SelectedGeoIsolationPipeline",
				.Shader = Renderer::GetShadersLibrary()->Get("SelectedGeometry"),
				.TargetFramebuffer = Framebuffer::Create(selectedGeoFB),
				.VertexLayout = vertexLayout,
				.InstanceLayout = instanceLayout,
				.DepthOperator = DepthCompareOperator::LessOrEqual
			};

			RenderPassSpecification selectedGeoPass = {
				.DebugName = "SelectedGeoPass",
				.Pipeline = Pipeline::Create(selectedGeoPipeline),
				.MarkerColor = { 0.8f, 0.3f, 0.2f, 1.0f }
			};
			m_SelectedGeometryPass = RenderPass::Create(selectedGeoPass);

			m_SelectedGeometryPass->SetInput("Camera", m_UBSCamera);
			m_SelectedGeometryPass->Bake();

			m_SelectedGeometryMaterial = Material::Create(selectedGeoPipeline.Shader, "SelectedGeoIsolationMaterial");
		}

		// Composite
		{
			FramebufferSpecification compFBSpec = {
				.DebugName = "CompositePassFB",
				.ClearColor = { 0.2f, 0.3f, 0.8f, 1.0f },
				.Attachments = { { ImageFormat::RGBA32F, AttachmentPassThroughUsage::Input } },
				.Samples = 1 // This will not have anything but resolve images at the end
			};
		
			PipelineSpecification compPipelineSpec = {
				.DebugName = "CompositePipeline",
				.Shader = Renderer::GetShadersLibrary()->Get("Compositing"),
				.TargetFramebuffer = Framebuffer::Create(compFBSpec),
				.VertexLayout = {
					{ ShaderDataType::Float3, "a_Position" },
					{ ShaderDataType::Float2, "a_TexCoord" }
				},
				.BackFaceCulling = false,
				.DepthTest = false,
				.DepthWrite = false
			};

			RenderPassSpecification compositePassSpec = {
				.DebugName = "CompositePass",
				.Pipeline = Pipeline::Create(compPipelineSpec),
				.MarkerColor = { 0.0f, 1.0f, 0.0f, 1.0f }
			};
			m_CompositePass = RenderPass::Create(compositePassSpec);
			m_CompositeMaterial = Material::Create(Renderer::GetShadersLibrary()->Get("Compositing"));

			// For if we decide we want to view the depth image
			// m_CompositePass->SetInput("Camera", m_UBSCamera);
			m_CompositePass->SetInput("u_Texture", m_GeometryPass->GetOutput(0));
			// m_CompositePass->SetInput("u_DepthTexture", m_PreDepthPass->GetDepthOutput());
			m_CompositePass->Bake();
		}

		// Compositing Framebuffer
		{
			FramebufferSpecification compositingFBSpec = {
				.DebugName = "CompositingFB",
				.ClearColorOnLoad = false,
				.ClearDepthOnLoad = false,
				.Attachments = { { ImageFormat::RGBA32F, AttachmentPassThroughUsage::Input }, { ImageFormat::DEPTH32F, AttachmentPassThroughUsage::Input } },
				.Samples = 1 // Will have only resolve images
			};
			compositingFBSpec.ExistingImages[0] = m_CompositePass->GetOutput(0);
			compositingFBSpec.ExistingImages[1] = m_PreDepthPass->GetDepthOutput();
			m_CompositingFramebuffer = Framebuffer::Create(compositingFBSpec);
		}

		// Grid
		{
			PipelineSpecification gridPipelineSpec = {
				.DebugName = "GridPipeline",
				.Shader = Renderer::GetShadersLibrary()->Get("Grid"),
				.TargetFramebuffer = m_CompositingFramebuffer,
				.VertexLayout = {
					{ ShaderDataType::Float3, "a_Position" },
					{ ShaderDataType::Float2, "a_TexCoord" }
				},
				.BackFaceCulling = false,
				.DepthTest = true,
				.DepthWrite = true
			};

			RenderPassSpecification gridPassSpec = {
				.DebugName = "GridPass",
				.Pipeline = Pipeline::Create(gridPipelineSpec),
				.MarkerColor = { 1.0f, 0.5f, 0.0f, 1.0f }
			};
			m_GridPass = RenderPass::Create(gridPassSpec);
			m_GridMaterial = Material::Create(gridPipelineSpec.Shader, "GridMaterial");
			m_GridMaterial->Set("u_Uniforms.Scale", m_TranslationSnapValue);

			m_GridPass->SetInput("Camera", m_UBSCamera);
			m_GridPass->Bake();
		}

		// Wireframe pass
		{
			PipelineSpecification wireframePipelineSpec = {
				.DebugName = "WireFramePipeline",
				.Shader = Renderer::GetShadersLibrary()->Get("WireFrame"),
				.TargetFramebuffer = m_CompositingFramebuffer,
				.VertexLayout = vertexLayout,
				.InstanceLayout = instanceLayout,
				.BackFaceCulling = false,
				.DepthTest = true,
				.DepthWrite = true,
				.WireFrame = true,
				.LineWidth = 2.0f
			};

			RenderPassSpecification wireFramePassSpec = {
				.DebugName = "WireFramePass",
				.Pipeline = Pipeline::Create(wireframePipelineSpec),
				.MarkerColor = { 0.2f, 0.8f, 0.3f, 1.0f }
			};
			m_GeometryWireFramePass = RenderPass::Create(wireFramePassSpec);
			m_WireFrameMaterial = Material::Create(wireframePipelineSpec.Shader, "WireFrameMaterial");
			m_WireFrameMaterial->Set("u_Uniforms.Color", glm::vec4{ 1.0f, 0.5f, 0.0f, 1.0f });

			m_GeometryWireFramePass->SetInput("Camera", m_UBSCamera);
			m_GeometryWireFramePass->Bake();
		}

		// Skybox
		{
			// TODO:
		}

		// References: <https://bgolus.medium.com/the-quest-for-very-wide-outlines-ba82ed442cd9>
		// JFA (Outline)
		{
			// Jump Flood framebuffers
			FramebufferSpecification jFFramebuffersSpec = {
				.DebugName = "JumpFloodFramebuffer1",
				.ClearColorOnLoad = true,
				.ClearColor = { 0.1f, 0.1f, 0.5f, 1.0f },
				.Attachments = { ImageFormat::RGBA32F },
				.Samples = c_Samples,
				.BlendMode = FramebufferBlendMode::OneZero
			};
			m_JumpFloodFramebuffers[0] = Framebuffer::Create(jFFramebuffersSpec);
			jFFramebuffersSpec.DebugName = "JumpFloodFramebuffer2";
			m_JumpFloodFramebuffers[1] = Framebuffer::Create(jFFramebuffersSpec);

			// Init...
			PipelineSpecification jumpFloodInitPipeline = {
				.DebugName = "JumpFloodInitPipeline",
				.Shader = Renderer::GetShadersLibrary()->Get("JumpFloodInit"),
				.TargetFramebuffer = m_JumpFloodFramebuffers[0],
				.VertexLayout = {
					{ ShaderDataType::Float3, "a_Position" },
				    { ShaderDataType::Float2, "a_TexCoord" }
				},
				.DepthWrite = false
			};

			RenderPassSpecification jumpFloodInitPass = {
				.DebugName = "JumpFloodInitPass",
				.Pipeline = Pipeline::Create(jumpFloodInitPipeline),
				.MarkerColor = { 0.9f, 0.4f, 0.9f, 1.0f }
			};
			m_JumpFloodInitPass = RenderPass::Create(jumpFloodInitPass);

			m_JumpFloodInitPass->SetInput("u_Texture", m_SelectedGeometryPass->GetOutput(0));
			m_JumpFloodInitPass->Bake();

			m_JumpFloodInitMaterial = Material::Create(jumpFloodInitPipeline.Shader, "JumpFloodInitMaterial");

			// Pass...
			const char* passName[2] = { "EvenPass", "OddPass" };
			for (uint32_t i = 0; i < 2; i++)
			{
				PipelineSpecification jumpFloodPassPipeline = {
					.DebugName = fmt::format("JumpFlood{0}Pipeline", passName[i]),
					.Shader = Renderer::GetShadersLibrary()->Get("JumpFloodPass"),
					.TargetFramebuffer = m_JumpFloodFramebuffers[(i + 1) % 2],
					.VertexLayout = {
						{ ShaderDataType::Float3, "a_Position" },
						{ ShaderDataType::Float2, "a_TexCoord" }
					},
					.DepthWrite = false
				};

				RenderPassSpecification jumpFloodPassPass = {
					.DebugName = fmt::format("JumpFlood{0}", passName[i]),
					.Pipeline = Pipeline::Create(jumpFloodPassPipeline),
					.MarkerColor = { 0.8f, 0.3f, 0.8f, 1.0f }
				};
				m_JumpFloodPass[i] = RenderPass::Create(jumpFloodPassPass);

				m_JumpFloodPass[i]->SetInput("u_Texture", m_JumpFloodFramebuffers[i]->GetImage(0));
				m_JumpFloodPass[i]->Bake();

				m_JumpFloodPassMaterial[i] = Material::Create(jumpFloodPassPipeline.Shader, fmt::format("JumpFlood{0}Material", passName[i]));
			}

			// Composite... We check this bool since we do not usualy want to do this in runtime... Unless otherwise specified
			if (m_Specification.JumpFloodPass)
			{
				FramebufferSpecification jumpFloodCompositeFB = {
					.DebugName = "JumpFloodCompositeFB",
					.ClearColorOnLoad = false,
					.Attachments = { { ImageFormat::RGBA32F, AttachmentPassThroughUsage::Input } },
					.Samples = 1
				};
				jumpFloodCompositeFB.ExistingImages[0] = m_CompositePass->GetOutput(0);

				PipelineSpecification jumpFloodCompositePipeline = {
					.DebugName = "JumpFloodCompsitePipeline",
					.Shader = Renderer::GetShadersLibrary()->Get("JumpFloodComposite"),
					// TODO: Is it possible to move this pipeline and the skybox to use samae framebuffer?
					.TargetFramebuffer = Framebuffer::Create(jumpFloodCompositeFB),
					//.TargetFramebuffer = m_CompositingFramebuffer,
					.VertexLayout = {
						{ ShaderDataType::Float3, "a_Position" },
						{ ShaderDataType::Float2, "a_TexCoord" }
					},
					.DepthTest = false
				};

				RenderPassSpecification jumpFloodCompositePass = {
					.DebugName = "JumpFloodCompositePass",
					.Pipeline = Pipeline::Create(jumpFloodCompositePipeline),
					.MarkerColor = { 0.7f, 0.2f, 0.7f, 1.0f }
				};
				m_JumpFloodCompositePass = RenderPass::Create(jumpFloodCompositePass);

				// Take the output of the first framebuffer since we only do two jump flood passes (ping pong only once)
				m_JumpFloodCompositePass->SetInput("u_Texture", m_JumpFloodFramebuffers[0]->GetImage(0));
				m_JumpFloodCompositePass->Bake();

				m_JumpFloodCompositeMaterial = Material::Create(jumpFloodCompositePipeline.Shader, "JumpFloodCompositeMaterial");
			}
		}

		// TODO: Resizable
		constexpr std::size_t TransformBufferCount = 10 * 1024; // 10240 transforms
		m_MeshTransformBuffers.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			m_MeshTransformBuffers[i].VertexBuffer = VertexBuffer::Create(sizeof(TransformVertexData) * TransformBufferCount);
			m_MeshTransformBuffers[i].Data = new TransformVertexData[TransformBufferCount];
		}

		Ref<SceneRenderer> instance = this;
		Renderer::Submit([instance]() mutable { instance->m_ResourcesCreatedGPU = true; });
	}

	void SceneRenderer::Shutdown()
	{
		for (auto& transformBuffer : m_MeshTransformBuffers)
			delete[] transformBuffer.Data;
	}

	void SceneRenderer::SetScene(Ref<Scene> scene)
	{
		IR_VERIFY(!m_Active, "Not able to change scenes while active renderer");
		m_Scene = scene;
	}

	void SceneRenderer::SetViewportSize(uint32_t width, uint32_t height, float renderScale)
	{
		if (m_Specification.RendererScale != renderScale)
			m_Specification.RendererScale = renderScale;

		// Set the render scale here instead of individually for all framebuffers...
		width = static_cast<uint32_t>(width * m_Specification.RendererScale);
		height = static_cast<uint32_t>(height * m_Specification.RendererScale);

		if (m_ViewportWidth == width && m_ViewportHeight == height)
			return;

		m_ViewportWidth = width;
		m_ViewportHeight = height;
		m_InverseViewportWidth = 1.0f / m_ViewportWidth;
		m_InverseViewportHeight = 1.0f / m_ViewportHeight;
		m_NeedsResize = true; // Resize all framebuffers...
	}

	void SceneRenderer::BeginScene(const SceneRendererCamera& camera)
	{
		IR_ASSERT(!m_Active);
		IR_ASSERT(m_Scene);

		m_Active = true;

		if (m_ResourcesCreatedGPU)
			m_ResourcesCreated = true;

		if (!m_ResourcesCreated)
			return;

		m_SceneData.Camera = camera;

		// Resize resources if needed
		if (m_NeedsResize)
		{
			m_NeedsResize = false;
						
			// PreDepth and Geometry framebuffers need to be resized first since other framebuffers reference images in them
			m_PreDepthPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
			m_GeometryPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
			m_SelectedGeometryPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
			m_CompositePass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);

			// NOTE: We are able to not resize the grid pass since it references the Compositing framebuffer whic is resized manually
			// m_GridPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);

			for (auto& jumpFloodFB : m_JumpFloodFramebuffers)
				jumpFloodFB->Resize(m_ViewportWidth, m_ViewportHeight);

			if (m_JumpFloodCompositePass)
				m_JumpFloodCompositePass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);

			m_CompositingFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);
		}

		// Update uniform buffers data for the starting frame
		// Camera uniform buffer (set = 1, binding = 0)
		{
			UBCamera& cameraData = m_CameraDataUB;

			SceneRendererCamera& sceneCamera = m_SceneData.Camera;
			const glm::mat4 viewProjection = sceneCamera.Camera.GetProjectionMatrix() * sceneCamera.ViewMatrix;
			const glm::mat4 viewInverse = glm::inverse(sceneCamera.ViewMatrix);
			const glm::mat4 projectionInverse = glm::inverse(sceneCamera.Camera.GetProjectionMatrix());
			const glm::vec3 cameraPosition = viewInverse[3];

			cameraData.ViewProjectionMatrix = viewProjection;
			cameraData.ProjectionMatrix = sceneCamera.Camera.GetProjectionMatrix();
			cameraData.InverseProjectionMatrix = projectionInverse;
			cameraData.ViewMatrix = sceneCamera.ViewMatrix;
			cameraData.InverseViewMatrix = viewInverse;
			cameraData.InverseViewProjectionMatrix = viewInverse * projectionInverse;

			// (clipFar * clipNear) / (clipFar - clipNear)
			float depthLinearMul = (-cameraData.ProjectionMatrix[3][2]);
			// clipFar / (clipFar - clipNear)
			float depthLinearizeAdd = (cameraData.ProjectionMatrix[2][2]);
			// Correct handedness issue
			if (depthLinearMul * depthLinearizeAdd < 0)
				depthLinearizeAdd = -depthLinearizeAdd;

			cameraData.DepthUnpackConsts = { depthLinearMul, depthLinearizeAdd };

			Ref<SceneRenderer> instance = this;
			Renderer::Submit([instance, cameraData]() mutable
			{
				instance->m_UBSCamera->RT_Get()->RT_SetData(&cameraData, sizeof(UBCamera));
			});
		}

		// Screen Data uniform buffer (set = 1, binding = 1)
		{
			UBScreenData& screenData = m_ScreenDataUB;

			screenData.FullResolution = { m_ViewportWidth, m_ViewportHeight };
			screenData.InverseFullResolution = { m_InverseViewportWidth, m_InverseViewportHeight };
			screenData.HalfResolution = { m_ViewportWidth / 2, m_ViewportHeight / 2 };
			screenData.InverseHalfResolution = { m_InverseViewportWidth / 2, m_InverseViewportHeight / 2 };

			Ref<SceneRenderer> instance = this;
			Renderer::Submit([instance, screenData]() mutable
			{
				instance->m_UBSScreenData->Get()->SetData(&screenData, sizeof(UBScreenData));
			});
		}
	}

	void SceneRenderer::EndScene()
	{
		IR_VERIFY(m_Active);

		FlushDrawList();

		m_Active = false;
	}

	void SceneRenderer::SubmitStaticMesh(Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource, Ref<MaterialTable> materialTable, const glm::mat4& transform, Ref<Material> overrideMaterial)
	{
		const std::vector<MeshUtils::SubMesh>& subMeshData = meshSource->GetSubMeshes();
		for (uint32_t subMeshIndex : staticMesh->GetSubMeshes())
		{
			const MeshUtils::SubMesh& subMesh = subMeshData[subMeshIndex];

			glm::mat4 subMeshTransform = transform * subMesh.Transform;

			uint32_t materialIndex = subMesh.MaterialIndex;

			AssetHandle materialAssetHandle = materialTable->HasMaterial(materialIndex) ? materialTable->GetMaterial(materialIndex) : staticMesh->GetMaterials()->GetMaterial(materialIndex);
			IR_VERIFY(materialAssetHandle);
			Ref<MaterialAsset> materialAsset = AssetManager::GetAsset<MaterialAsset>(materialAssetHandle);

			MeshKey meshKey = { staticMesh->Handle, materialAssetHandle, subMeshIndex, false };
			TransformVertexData& transformStorage = m_MeshTransformMap[meshKey].Transforms.emplace_back();

			// glm::mat4 [column][row]
			transformStorage.MatrixRow[0] = { subMeshTransform[0][0],  subMeshTransform[1][0], subMeshTransform[2][0] , subMeshTransform[3][0] };
			transformStorage.MatrixRow[1] = { subMeshTransform[0][1],  subMeshTransform[1][1], subMeshTransform[2][1] , subMeshTransform[3][1] };
			transformStorage.MatrixRow[2] = { subMeshTransform[0][2],  subMeshTransform[1][2], subMeshTransform[2][2] , subMeshTransform[3][2] };

			// For main geometry drawlist
			// TODO: Check if transparent for transparent materials
			{
				auto& destDrawList = m_StaticMeshDrawList;
				auto& dc = destDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.MeshSource = meshSource;
				dc.SubMeshIndex = subMeshIndex;
				dc.MaterialTable = materialTable;
				dc.OverrideMaterial = overrideMaterial;
				dc.InstanceCount++;
			}
		}
	}

	void SceneRenderer::SubmitSelectedStaticMesh(Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource, Ref<MaterialTable> materialTable, const glm::mat4& transform, Ref<Material> overrideMaterial)
	{
		const std::vector<MeshUtils::SubMesh>& subMeshData = meshSource->GetSubMeshes();
		for (uint32_t subMeshIndex : staticMesh->GetSubMeshes())
		{
			const MeshUtils::SubMesh& subMesh = subMeshData[subMeshIndex];

			glm::mat4 subMeshTransform = transform * subMesh.Transform;

			uint32_t materialIndex = subMesh.MaterialIndex;

			AssetHandle materialAssetHandle = materialTable->HasMaterial(materialIndex) ? materialTable->GetMaterial(materialIndex) : staticMesh->GetMaterials()->GetMaterial(materialIndex);
			IR_VERIFY(materialAssetHandle);
			Ref<MaterialAsset> materialAsset = AssetManager::GetAsset<MaterialAsset>(materialAssetHandle);

			MeshKey meshKey = { staticMesh->Handle, materialAssetHandle, subMeshIndex, true };
			TransformVertexData& transformStorage = m_MeshTransformMap[meshKey].Transforms.emplace_back();

			// glm::mat4 [column][row]
			transformStorage.MatrixRow[0] = { subMeshTransform[0][0],  subMeshTransform[1][0], subMeshTransform[2][0] , subMeshTransform[3][0] };
			transformStorage.MatrixRow[1] = { subMeshTransform[0][1],  subMeshTransform[1][1], subMeshTransform[2][1] , subMeshTransform[3][1] };
			transformStorage.MatrixRow[2] = { subMeshTransform[0][2],  subMeshTransform[1][2], subMeshTransform[2][2] , subMeshTransform[3][2] };

			// For main geometry drawlist
			// TODO: Check if transparent for transparent materials
			{
				auto& destDrawList = m_StaticMeshDrawList;
				auto& dc = destDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.MeshSource = meshSource;
				dc.SubMeshIndex = subMeshIndex;
				dc.MaterialTable = materialTable;
				dc.OverrideMaterial = overrideMaterial;
				dc.InstanceCount++;
			}

			// For selected mesh drawlist
			{
				auto& dc = m_SelectedStaticMeshDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.MeshSource = meshSource;
				dc.SubMeshIndex = subMeshIndex;
				dc.MaterialTable = materialTable;
				dc.OverrideMaterial = overrideMaterial;
				dc.InstanceCount++;
			}
		}
	}

	Ref<Texture2D> SceneRenderer::GetFinalPassImage()
	{
		if (!m_ResourcesCreated)
			return nullptr;

		return m_CompositePass->GetOutput(0);
	}

	void SceneRenderer::SetLineWidth(float lineWidth)
	{
		m_LineWidth = lineWidth;

		m_Renderer2D->SetLineWidth(lineWidth);

		if (m_GeometryWireFramePass)
			m_GeometryWireFramePass->GetPipeline()->GetSpecification().LineWidth = lineWidth;
	}

	void SceneRenderer::FlushDrawList()
	{
		if (m_ResourcesCreated && m_ViewportWidth > 0 && m_ViewportHeight > 0)
		{
			PreRender();

			m_CommandBuffer->Begin();

			PreDepthPass();
			GeometryPass();

			if (m_Specification.JumpFloodPass)
				JumpFloodPass();

			// NOTE: If we want to sample the depth texture in the composite pass we need to transition to shader read only then transition back
			// to attachment since the renderer2D uses it.
			CompositePass();

			/*
			 * Here PreDepthImage is in VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			 */

			m_CommandBuffer->End();
			m_CommandBuffer->Submit();
		}
		else
		{
			m_CommandBuffer->Begin();

			ClearPass();

			m_CommandBuffer->End();
			m_CommandBuffer->Submit();
		}

		UpdateStatistics();

		m_StaticMeshDrawList.clear();
		m_SelectedStaticMeshDrawList.clear();

		m_SceneData = {};

		// Clear transform map so that we fill it again for the next frame and we dont have duplicated transforms...
		m_MeshTransformMap.clear();
	}

	void SceneRenderer::PreRender()
	{
		// Fill instance vertex buffer with instance transform data...

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		uint32_t offset = 0;
		for (auto& [meshKey, transformData] : m_MeshTransformMap)
		{
			transformData.TransformOffset = offset * sizeof(TransformVertexData);
			for (const auto& transform : transformData.Transforms)
			{
				*(m_MeshTransformBuffers[frameIndex].Data + offset) = transform;

				offset++;
			}
		}

		m_MeshTransformBuffers[frameIndex].VertexBuffer->SetData(m_MeshTransformBuffers[frameIndex].Data, offset * sizeof(TransformVertexData));
	}

	void SceneRenderer::PreDepthPass()
	{
		// Render all objects into a depth texture only and use that in all other passes that need depth

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		Ref<RenderPass> renderPassToUse = nullptr;
		switch (m_ViewMode)
		{
			case ViewMode::Lit: renderPassToUse = m_PreDepthPass; break;
			case ViewMode::Unlit: renderPassToUse = m_PreDepthPass; break;
			case ViewMode::Wireframe: renderPassToUse = m_WireframeViewPreDepthPass; break;
		}

		Renderer::BeginRenderPass(m_CommandBuffer, renderPassToUse);

		for (const auto& [mk, dc] : m_StaticMeshDrawList)
		{
			const auto& transformData = m_MeshTransformMap.at(mk);
			glm::mat4 transform = dc.MeshSource->GetSubMeshes()[dc.SubMeshIndex].Transform;
			Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, renderPassToUse->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, m_PreDepthMaterial, m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset, dc.InstanceCount);
		}

		Renderer::EndRenderPass(m_CommandBuffer);
	}

	void SceneRenderer::GeometryPass()
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		Renderer::BeginRenderPass(m_CommandBuffer, m_SelectedGeometryPass);

		for (auto& [mk, dc] : m_SelectedStaticMeshDrawList)
		{
			const auto& transformData = m_MeshTransformMap.at(mk);
			Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_SelectedGeometryPass->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, m_SelectedGeometryMaterial, m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset + dc.InstanceOffset * sizeof(TransformVertexData), dc.InstanceCount);
		}

		Renderer::EndRenderPass(m_CommandBuffer);

		Renderer::BeginRenderPass(m_CommandBuffer, m_GeometryPass);

		for (const auto& [mk, dc] : m_StaticMeshDrawList)
		{
			const auto& transformData = m_MeshTransformMap.at(mk);
			Renderer::RenderStaticMesh(m_CommandBuffer, m_GeometryPipeline, dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.StaticMesh->GetMaterials(), m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset, dc.InstanceCount);
		}

		Renderer::EndRenderPass(m_CommandBuffer);
	}

	void SceneRenderer::JumpFloodPass()
	{
		Renderer::BeginRenderPass(m_CommandBuffer, m_JumpFloodInitPass);
		Renderer::SubmitFullScreenQuad(m_CommandBuffer, m_JumpFloodInitPass->GetPipeline(), m_JumpFloodInitMaterial);
		Renderer::EndRenderPass(m_CommandBuffer);

		constexpr float steps = 2.5;
		int step = static_cast<int>(glm::round(glm::pow(steps - 1.0f, 2.0f)));
		int index = 0;

		Ref<Framebuffer> passFb = m_JumpFloodPass[0]->GetTargetFramebuffer();
		glm::vec2 texelSize = { 1.0f / static_cast<float>(passFb->GetWidth()), 1.0f / static_cast<float>(passFb->GetHeight()) };

		Buffer vertexOverrides;
		vertexOverrides.Allocate(sizeof(glm::vec2) + sizeof(int));
		vertexOverrides.Write(reinterpret_cast<const uint8_t*>(glm::value_ptr(texelSize)), sizeof(glm::vec2));

		int counter = 0;
		while (step != 0)
		{
			counter++;
			vertexOverrides.Write(reinterpret_cast<const uint8_t*>(&step), sizeof(int), sizeof(glm::vec2));

			Renderer::BeginRenderPass(m_CommandBuffer, m_JumpFloodPass[index]);
			Renderer::SubmitFullScreenQuadWithOverrides(m_CommandBuffer, m_JumpFloodPass[index]->GetPipeline(), m_JumpFloodPassMaterial[index], vertexOverrides, Buffer());
			Renderer::EndRenderPass(m_CommandBuffer);

			index = (index + 1) % 2;
			step /= 2;
		}

		vertexOverrides.Release();
	}

	void SceneRenderer::CompositePass()
	{
		// Composite the final scene image and apply gamma correction and tone mapping

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		float exposure = m_SceneData.Camera.Camera.GetExposure();

		m_CompositeMaterial->Set("u_Uniforms.Exposure", exposure);
		m_CompositeMaterial->Set("u_Uniforms.Opacity", m_Opacity);
		m_CompositeMaterial->Set("u_Uniforms.Time", Application::Get().GetTime());

		Renderer::BeginRenderPass(m_CommandBuffer, m_CompositePass);
		Renderer::SubmitFullScreenQuad(m_CommandBuffer, m_CompositePass->GetPipeline(), m_CompositeMaterial);
		Renderer::EndRenderPass(m_CommandBuffer);

		if (m_Options.ShowGrid)
		{
			m_GridMaterial->Set("u_Uniforms.Scale", m_TranslationSnapValue);
			Renderer::BeginRenderPass(m_CommandBuffer, m_GridPass);
			Renderer::SubmitFullScreenQuad(m_CommandBuffer, m_GridPass->GetPipeline(), m_GridMaterial);
			Renderer::EndRenderPass(m_CommandBuffer);
		}

		// We usualy do not want to do this in runtime
		if (m_Specification.JumpFloodPass)
		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_JumpFloodCompositePass);
			Renderer::SubmitFullScreenQuad(m_CommandBuffer, m_JumpFloodCompositePass->GetPipeline(), m_JumpFloodCompositeMaterial);
			Renderer::EndRenderPass(m_CommandBuffer);
		}
		
		if (m_Options.ShowSelectedInWireFrame)
		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_GeometryWireFramePass);
		
			for (const auto& [mk, dc] : m_SelectedStaticMeshDrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_GeometryWireFramePass->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, m_WireFrameMaterial, m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset + dc.InstanceOffset * sizeof(TransformVertexData), dc.InstanceCount);
			}
		
			Renderer::EndRenderPass(m_CommandBuffer);
		}
	}

	void SceneRenderer::ClearPass()
	{
		// Here we do not need to explicit clear since the attachment is LOAD_OP_CLEAR
		Renderer::BeginRenderPass(m_CommandBuffer, m_PreDepthPass);
		Renderer::EndRenderPass(m_CommandBuffer);

		// Here we do not need to explicit clear since the attachment is LOAD_OP_CLEAR
		Renderer::BeginRenderPass(m_CommandBuffer, m_CompositePass);
		Renderer::EndRenderPass(m_CommandBuffer);
	}

	void SceneRenderer::UpdateStatistics()
	{
		m_Statistics.DrawCalls = 0;
		m_Statistics.Instances = 0;
		m_Statistics.Meshes = 0;

		for (const auto& [mk, dc] : m_SelectedStaticMeshDrawList)
		{
			m_Statistics.Instances += dc.InstanceCount;
			m_Statistics.DrawCalls += 1;
			m_Statistics.Meshes += 1;
		}

		for (const auto& [mk, dc] : m_StaticMeshDrawList)
		{
			m_Statistics.Instances += dc.InstanceCount;
			m_Statistics.DrawCalls += 1;
			m_Statistics.Meshes += 1;
		}

		m_Statistics.SavedDraw = m_Statistics.Instances - m_Statistics.DrawCalls;
	}

}