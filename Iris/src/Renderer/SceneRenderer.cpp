#include "IrisPCH.h"
#include "SceneRenderer.h"

#include "AssetManager/AssetManager.h"
#include "ComputePass.h"
#include "Renderer/Renderer.h"
#include "Scene/SceneEnvironment.h"
#include "Shaders/Shader.h"
#include "StorageBufferSet.h"
#include "Texture.h"
#include "UniformBufferSet.h"

#include <glm/gtx/compatibility.hpp>

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
		m_UBSSceneData= UniformBufferSet::Create(sizeof(UBScene));
		m_UBSDirectionalShadowData = UniformBufferSet::Create(sizeof(UBDirectionalShadowData));
		m_UBSRendererData = UniformBufferSet::Create(sizeof(UBRendererData));

		m_RendererDataUB.SoftShadows = m_Specification.ShadowResolution == ShadowResolutionSetting::High;

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

		// Directional Shadow
		{
			uint32_t shadowMapResolution = 4096;
			switch (m_Specification.ShadowResolution)
			{
				case ShadowResolutionSetting::Low:
					shadowMapResolution = 1024; // Results in a 16 MB image
					break;
				case ShadowResolutionSetting::Medium:
					shadowMapResolution = 2048; // Results in a 64 MB image
					break;
				case ShadowResolutionSetting::High:
					shadowMapResolution = 4096; // Results in a 256 MB image
					break;
			}

			TextureSpecification cascadedImageSpec = {
				.DebugName = "DirectionalShadowCascadedImage",
				.Width = shadowMapResolution,
				.Height = shadowMapResolution,
				.Format = ImageFormat::Depth32F,
				.Usage = ImageUsage::Attachment,
				.WrapMode = TextureWrap::Clamp,
				.FilterMode = TextureFilter::Linear,
				.GenerateMips = false,
				.Layers = m_Specification.NumberOfShadowCascades
			};
			Ref<Texture2D> cascadedShadowImage = Texture2D::Create(cascadedImageSpec);
			m_DirShadowImagePerLayerViews = cascadedShadowImage->CreatePerLayerImageViews();

			m_DirectionalShadowPasses.reserve(m_Specification.NumberOfShadowCascades);
			for (uint32_t i = 0; i < m_Specification.NumberOfShadowCascades; i++)
			{
				FramebufferSpecification directionalShadowFBSpec = {
					.DebugName = fmt::format("DirectionalShadowFB-{}", i),
					.Width = shadowMapResolution,
					.Height = shadowMapResolution,
					.ClearDepthOnLoad = true,
					.DepthClearValue = 1.0f,
					.Attachments = { ImageFormat::Depth32F }
				};
				directionalShadowFBSpec.ExistingImages[0] = cascadedShadowImage;
				directionalShadowFBSpec.ExistingImageLayerViews[0] = m_DirShadowImagePerLayerViews[i];

				PipelineSpecification directionalShadowPiplineSpec = {
					.DebugName = fmt::format("DirectionalShadowPipeline-{}", i),
					.Shader = Renderer::GetShadersLibrary()->Get("DirectionalShadow"),
					.TargetFramebuffer = Framebuffer::Create(directionalShadowFBSpec),
					.VertexLayout = vertexLayout,
					.InstanceLayout = instanceLayout,
					.DepthOperator = DepthCompareOperator::LessOrEqual
				};

				RenderPassSpecification directionalShadowRPSpec = {
					.DebugName = fmt::format("DirectionalShadowPass-{}", i),
					.Pipeline = Pipeline::Create(directionalShadowPiplineSpec),
					.MarkerColor = { 0.06f, 0.22f, 0.06f - i * 0.005f, 1.0f}
				};
				m_DirectionalShadowPasses.push_back(RenderPass::Create(directionalShadowRPSpec));

				m_DirectionalShadowPasses[i]->SetInput("DirectionalShadowData", m_UBSDirectionalShadowData);
				m_DirectionalShadowPasses[i]->Bake();
			}

			m_DirectionalShadowMaterial = Material::Create(Renderer::GetShadersLibrary()->Get("DirectionalShadow"), "DirectionalShadowMaterial");
		}

		// Pre-Depth
		{
			FramebufferSpecification preDepthFBSpec = {
				.DebugName = "OpaquePreDepthFB",
				// Linear depth, reversed depth buffer
				.DepthClearValue = 0.0f,
				.Attachments = { ImageFormat::Depth32F }
			};

			PipelineSpecification preDepthPipelineSpec = {
				.DebugName = "OpaquePreDepthPipeline",
				.Shader = Renderer::GetShadersLibrary()->Get("PreDepth"),
				.TargetFramebuffer = Framebuffer::Create(preDepthFBSpec),
				.VertexLayout = vertexLayout,
				.InstanceLayout = instanceLayout,
				.Topology = PrimitiveTopology::Triangles,
				.BackFaceCulling = true,
				.DepthOperator = DepthCompareOperator::GreaterOrEqual
			};

			RenderPassSpecification preDepthRenderPassSpec = {
				.DebugName = "OpaquePreDepthRenderPass",
				.Pipeline = Pipeline::Create(preDepthPipelineSpec),
				.MarkerColor = { 1.0f, 1.0f, 0.0f, 1.0f }
			};
			m_PreDepthPass = RenderPass::Create(preDepthRenderPassSpec);
			m_PreDepthMaterial = Material::Create(preDepthPipelineSpec.Shader, "PreDepthMaterial");

			m_PreDepthPass->SetInput("Camera", m_UBSCamera);
			m_PreDepthPass->Bake();

			// DoubleSided
			{
				FramebufferSpecification doubleSidedFBSpec = {
					.DebugName = "DoubleSidedPreDepthFB",
					.ClearDepthOnLoad = false,
					.Attachments = { ImageFormat::Depth32F }
				};
				doubleSidedFBSpec.ExistingImages[0] = m_PreDepthPass->GetDepthOutput();
				
				preDepthPipelineSpec.DebugName = "DoubleSidedPreDepthPipeline";
				preDepthPipelineSpec.TargetFramebuffer = Framebuffer::Create(doubleSidedFBSpec);
				preDepthPipelineSpec.BackFaceCulling = false;
				preDepthRenderPassSpec.DebugName = "DoubleSidedPreDepthPass";
				preDepthRenderPassSpec.Pipeline = Pipeline::Create(preDepthPipelineSpec);
				m_DoubleSidedPreDepthPass = RenderPass::Create(preDepthRenderPassSpec);
				
				m_DoubleSidedPreDepthPass->SetInput("Camera", m_UBSCamera);
				m_DoubleSidedPreDepthPass->Bake();
			}
			
			// Wireframe view
			{
				preDepthPipelineSpec.DebugName = "WireframePreDepthPipeline";
				preDepthPipelineSpec.TargetFramebuffer = m_PreDepthPass->GetTargetFramebuffer();
				preDepthPipelineSpec.BackFaceCulling = false;
				preDepthPipelineSpec.WireFrame = true;
				preDepthPipelineSpec.ReleaseShaderModules = true; // Since this is the last predepth related pipeline we can release the shader modules
				preDepthRenderPassSpec.DebugName = "WireframePreDepthPass";
				preDepthRenderPassSpec.Pipeline = Pipeline::Create(preDepthPipelineSpec);
				m_WireframeViewPreDepthPass = RenderPass::Create(preDepthRenderPassSpec);
			
				m_WireframeViewPreDepthPass->SetInput("Camera", m_UBSCamera);
				m_WireframeViewPreDepthPass->Bake();
			}
		}

		// Geometry
		{
			FramebufferSpecification geometryFBSpec = {
				.DebugName = "StaticGeometryPassFB",
				.ClearColorOnLoad = true,
				.ClearColor = { 0.04f, 0.04f, 0.04f, 1.0f },
				.ClearDepthOnLoad = false,
				// NOTE: The second attachment does not blend since we do not want to blend with luminance in the alpha channel
				.Attachments = { ImageFormat::RGBA32F, { ImageFormat::RGBA16F, false }, ImageFormat::RGBA, ImageFormat::Depth32F }
			};
			geometryFBSpec.Attachments.Attachments[0].LoadOp = AttachmentLoadOp::Load; // Load since skybox pass writes to it

			geometryFBSpec.ExistingImages[3] = m_PreDepthPass->GetDepthOutput();

			PipelineSpecification staticGeometryPipelineSpec = {
				.DebugName = "PBRStaticPipeline",
				.Shader = Renderer::GetShadersLibrary()->Get("IrisPBRStatic"),
				.TargetFramebuffer = Framebuffer::Create(geometryFBSpec),
				.VertexLayout = vertexLayout,
				.InstanceLayout = instanceLayout,
				.BackFaceCulling = true,
				.DepthOperator = DepthCompareOperator::Equal,
				.DepthWrite = false,
				.WireFrame = false,
				.LineWidth = m_LineWidth
			};

			RenderPassSpecification staticGeometryPassSpec = {
				.DebugName = "StaticGeometryRenderPass",
				.Pipeline = Pipeline::Create(staticGeometryPipelineSpec),
				.MarkerColor = { 1.0f, 0.0f, 0.0f, 1.0f }
			};
			m_GeometryPass = RenderPass::Create(staticGeometryPassSpec);

			m_GeometryPass->SetInput("Camera", m_UBSCamera);
			m_GeometryPass->SetInput("SceneData", m_UBSSceneData);
			m_GeometryPass->SetInput("DirectionalShadowData", m_UBSDirectionalShadowData);
			m_GeometryPass->SetInput("RendererData", m_UBSRendererData);
			m_GeometryPass->SetInput("u_RadianceMap", Renderer::GetBlackCubeTexture());
			m_GeometryPass->SetInput("u_IrradianceMap", Renderer::GetBlackCubeTexture());
			m_GeometryPass->SetInput("u_ShadowMap", m_DirectionalShadowPasses[0]->GetDepthOutput());
			m_GeometryPass->SetInput("u_BRDFLutTexture", Renderer::GetBRDFLutTexture());

			m_GeometryPass->Bake();

			// DoubleSided
			{
				FramebufferSpecification doubleSidedGeometryFB = {
					.DebugName = "DoubleSidedGeometryFB",
					.ClearColorOnLoad = false,
					.ClearColor = { 0.04f, 0.04f, 0.04f, 1.0f },
					.ClearDepthOnLoad = false,
					.Attachments = { ImageFormat::RGBA32F, { ImageFormat::RGBA16F, false }, ImageFormat::RGBA, ImageFormat::Depth32F }
				};
				doubleSidedGeometryFB.ExistingImages[0] = m_GeometryPass->GetOutput(0);
				doubleSidedGeometryFB.ExistingImages[1] = m_GeometryPass->GetOutput(1);
				doubleSidedGeometryFB.ExistingImages[2] = m_GeometryPass->GetOutput(2);
				doubleSidedGeometryFB.ExistingImages[3] = m_PreDepthPass->GetDepthOutput();
			
				staticGeometryPipelineSpec.DebugName = "DoubleSidedGeometryPipeline";
				staticGeometryPipelineSpec.TargetFramebuffer = Framebuffer::Create(doubleSidedGeometryFB);
				staticGeometryPipelineSpec.BackFaceCulling = false;
				staticGeometryPassSpec.DebugName = "DoubleSidedGeometryPass";
				staticGeometryPassSpec.Pipeline = Pipeline::Create(staticGeometryPipelineSpec);
				m_DoubleSidedGeometryPass = RenderPass::Create(staticGeometryPassSpec);
				
				m_DoubleSidedGeometryPass->SetInput("Camera", m_UBSCamera);
				m_DoubleSidedGeometryPass->SetInput("SceneData", m_UBSSceneData);
				m_DoubleSidedGeometryPass->SetInput("DirectionalShadowData", m_UBSDirectionalShadowData);
				m_DoubleSidedGeometryPass->SetInput("RendererData", m_UBSRendererData);
				m_DoubleSidedGeometryPass->SetInput("u_RadianceMap", Renderer::GetBlackCubeTexture());
				m_DoubleSidedGeometryPass->SetInput("u_IrradianceMap", Renderer::GetBlackCubeTexture());
				m_DoubleSidedGeometryPass->SetInput("u_ShadowMap", m_DirectionalShadowPasses[0]->GetDepthOutput());
				m_DoubleSidedGeometryPass->SetInput("u_BRDFLutTexture", Renderer::GetBRDFLutTexture());

				m_DoubleSidedGeometryPass->Bake();
			}
			
			// Wireframe view
			{
				staticGeometryPipelineSpec.DebugName = "WireframeViewPipeline";
				staticGeometryPipelineSpec.TargetFramebuffer = m_GeometryPass->GetTargetFramebuffer();
				staticGeometryPipelineSpec.BackFaceCulling = false;
				staticGeometryPipelineSpec.DepthOperator = DepthCompareOperator::GreaterOrEqual;
				staticGeometryPipelineSpec.WireFrame = true;
				staticGeometryPipelineSpec.ReleaseShaderModules = true; // Since this is the last geometry related pipeline we can release the shader modules
				staticGeometryPassSpec.DebugName = "WireframeViewPass";
				staticGeometryPassSpec.Pipeline = Pipeline::Create(staticGeometryPipelineSpec);
				m_WireframeViewGeometryPass = RenderPass::Create(staticGeometryPassSpec);
			
				m_WireframeViewGeometryPass->SetInput("Camera", m_UBSCamera);
				m_WireframeViewGeometryPass->SetInput("SceneData", m_UBSSceneData);
				m_WireframeViewGeometryPass->SetInput("DirectionalShadowData", m_UBSDirectionalShadowData);
				m_WireframeViewGeometryPass->SetInput("RendererData", m_UBSRendererData);
				m_WireframeViewGeometryPass->SetInput("u_RadianceMap", Renderer::GetBlackCubeTexture());
				m_WireframeViewGeometryPass->SetInput("u_IrradianceMap", Renderer::GetBlackCubeTexture());
				m_WireframeViewGeometryPass->SetInput("u_ShadowMap", m_DirectionalShadowPasses[0]->GetDepthOutput());
				m_WireframeViewGeometryPass->SetInput("u_BRDFLutTexture", Renderer::GetBRDFLutTexture());

				m_WireframeViewGeometryPass->Bake();
			}
		}

		// Selected Geometry isolation
		{
			FramebufferSpecification selectedGeoFB = {
				.DebugName = "SelectedGeoIsolationFB",
				.ClearColorOnLoad = true,
				.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f },
				.Attachments = { ImageFormat::RGBA32F }
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
			m_SelectedGeometryMaterial = Material::Create(selectedGeoPipeline.Shader, "SelectedGeoIsolationMaterial");
		
			m_SelectedGeometryPass->SetInput("Camera", m_UBSCamera);
			m_SelectedGeometryPass->Bake();
		
			// DoubleSided
			{
				FramebufferSpecification doubleSidedFBSpec = {
					.DebugName = "DoubleSidedSelectedGeoFB",
					.ClearColorOnLoad = false,
					.ClearDepthOnLoad = false,
					.Attachments = { ImageFormat::RGBA32F }
				};
				doubleSidedFBSpec.ExistingImages[0] = m_SelectedGeometryPass->GetOutput(0);
		
				selectedGeoPipeline.DebugName = "DoubleSidedSelectedGeoPipeline";
				selectedGeoPipeline.TargetFramebuffer = Framebuffer::Create(doubleSidedFBSpec);
				selectedGeoPipeline.BackFaceCulling = false;
				selectedGeoPipeline.ReleaseShaderModules = true;
				selectedGeoPass.DebugName = "DoubleSidedSelectedGeoPass";
				selectedGeoPass.Pipeline = Pipeline::Create(selectedGeoPipeline);
				m_DoubleSidedSelectedGeometryPass = RenderPass::Create(selectedGeoPass);
		
				m_DoubleSidedSelectedGeometryPass->SetInput("Camera", m_UBSCamera);
				m_DoubleSidedSelectedGeometryPass->Bake();
			}
		}

		// Composite
		{
			FramebufferSpecification compFBSpec = {
				.DebugName = "CompositePassFB",
				.ClearColor = { 0.2f, 0.3f, 0.8f, 1.0f },
				.Attachments = { ImageFormat::RGBA32F }
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
				.DepthWrite = false,
				.ReleaseShaderModules = true
			};

			RenderPassSpecification compositePassSpec = {
				.DebugName = "CompositePass",
				.Pipeline = Pipeline::Create(compPipelineSpec),
				.MarkerColor = { 0.0f, 1.0f, 0.0f, 1.0f }
			};
			m_CompositePass = RenderPass::Create(compositePassSpec);
			m_CompositeMaterial = Material::Create(compPipelineSpec.Shader, "CompositingMaterial");

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
				.Attachments = { ImageFormat::RGBA32F, ImageFormat::Depth32F }
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
				.DepthWrite = true,
				.ReleaseShaderModules = true
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
				.LineWidth = 2.0f,
				.ReleaseShaderModules = true
			};
		
			RenderPassSpecification wireFramePassSpec = {
				.DebugName = "WireFramePass",
				.Pipeline = Pipeline::Create(wireframePipelineSpec),
				.MarkerColor = { 0.2f, 0.8f, 0.3f, 1.0f }
			};
			m_GeometryWireFramePass = RenderPass::Create(wireFramePassSpec);
			m_WireFrameMaterial = Material::Create(wireframePipelineSpec.Shader, "WireFrameMaterial");
			// m_WireFrameMaterial->Set("u_Uniforms.Color", glm::vec4{ 1.0f, 0.5f, 0.0f, 1.0f });
			m_WireFrameMaterial->Set("u_Uniforms.Color", glm::vec4{ glm::vec3{ 0.14f, 0.8f, 0.52f } * 0.9f, 1.0f }); // Use this color for wireframe of ONLY current selection
		
			m_GeometryWireFramePass->SetInput("Camera", m_UBSCamera);
			m_GeometryWireFramePass->Bake();
		}

		// Skybox
		{
			FramebufferSpecification skyboxFBSpec = {
				.DebugName = "SkyboxFB",
				.Attachments = { ImageFormat::RGBA32F }
			};
			skyboxFBSpec.ExistingImages[0] = m_GeometryPass->GetOutput(0);
			
			PipelineSpecification skyboxPipelineSpec = {
				.DebugName = "SkyboxPipeline",
				.Shader = Renderer::GetShadersLibrary()->Get("Skybox"),
				.TargetFramebuffer = Framebuffer::Create(skyboxFBSpec),
				.VertexLayout = {
					{ ShaderDataType::Float3, "a_Position" },
					{ ShaderDataType::Float2, "a_TexCoord" }
				},
				.DepthTest = false,
				.DepthWrite = false,
				.ReleaseShaderModules = true
			};
			
			RenderPassSpecification skyboxPassSpec = {
				.DebugName = "SkyboxPass",
				.Pipeline = Pipeline::Create(skyboxPipelineSpec),
				.MarkerColor = { 1.0f, 1.0f, 1.0f, 1.0f }
			};
			m_SkyboxPass = RenderPass::Create(skyboxPassSpec);
			m_SkyboxMaterial = Material::Create(skyboxPipelineSpec.Shader, "SkyboxMaterial");
			m_SkyboxMaterial->SetFlag(MaterialFlag::DepthTest, false);
			
			m_SkyboxPass->SetInput("Camera", m_UBSCamera);
			m_SkyboxPass->Bake();
		}

		// References: <https://bgolus.medium.com/the-quest-for-very-wide-outlines-ba82ed442cd9>
		// JFA (Outline)
		/*
		 * Init pass renders into JumpFloodFB[0]
		 * Even pass into JumpFloodFB[1]; // Reads from JumpFlood[0] => Transfer image layout of JumpFlood[0] to SHADER_READ_ONLY then back to COLOR_ATTACHMENT_OPTIMAL
		 * Odd pass into JumpFloodFB[0]; // Reads from JumpFlood[1] => Transfer image layout of JumpFlood[1] to SHADER_READ_ONLY then we could do nothing since it is not used to composite
		 * Composite Reads from JumpFlood[0];
		 */
		{
			// Jump Flood framebuffers
			FramebufferSpecification jFFramebuffersSpec = {
				.DebugName = "JumpFloodFramebuffer1",
				.ClearColorOnLoad = true,
				.ClearColor = { 0.1f, 0.1f, 0.5f, 1.0f },
				.Attachments = { ImageFormat::RGBA32F },
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
				.DepthWrite = false,
				.ReleaseShaderModules = true
			};
		
			RenderPassSpecification jumpFloodInitPass = {
				.DebugName = "JumpFloodInitPass",
				.Pipeline = Pipeline::Create(jumpFloodInitPipeline),
				.MarkerColor = { 0.8f, 0.3f, 0.8f, 1.0f }
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
					.DepthWrite = false,
					.ReleaseShaderModules = i == 1 // On the second iteration we release shader modules
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
					.Attachments = { ImageFormat::RGBA32F }
				};
				jumpFloodCompositeFB.ExistingImages[0] = m_CompositePass->GetOutput(0);
		
				PipelineSpecification jumpFloodCompositePipeline = {
					.DebugName = "JumpFloodCompsitePipeline",
					.Shader = Renderer::GetShadersLibrary()->Get("JumpFloodComposite"),
					.TargetFramebuffer = Framebuffer::Create(jumpFloodCompositeFB),
					.VertexLayout = {
						{ ShaderDataType::Float3, "a_Position" },
						{ ShaderDataType::Float2, "a_TexCoord" }
					},
					.DepthTest = false,
					.ReleaseShaderModules = true
				};
		
				RenderPassSpecification jumpFloodCompositePass = {
					.DebugName = "JumpFloodCompositePass",
					.Pipeline = Pipeline::Create(jumpFloodCompositePipeline),
					.MarkerColor = { 0.8f, 0.3f, 0.8f, 1.0f }
				};
				m_JumpFloodCompositePass = RenderPass::Create(jumpFloodCompositePass);
				m_JumpFloodCompositeMaterial = Material::Create(jumpFloodCompositePipeline.Shader, "JumpFloodCompositeMaterial");
				m_JumpFloodCompositeMaterial->Set("u_Uniforms.Color", glm::vec4{ 0.14f, 0.8f, 0.52f, 1.0f });
		
				// Take the output of the first framebuffer since we only do two jump flood passes (ping pong only once)
				m_JumpFloodCompositePass->SetInput("u_Texture", m_JumpFloodFramebuffers[0]->GetImage(0));
				m_JumpFloodCompositePass->Bake();
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

		m_Renderer2D = Renderer2D::Create({ .TargetFramebuffer = m_CompositingFramebuffer });

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

		m_SceneInfo.Camera = camera;
		m_SceneInfo.SceneEnvironment = m_Scene->m_Environment;
		m_SceneInfo.SkyboxLod = m_Scene->m_SkyboxLod;
		m_SceneInfo.SceneEnvironmentIntensity = m_Scene->m_EnvironmentIntensity;
		m_SceneInfo.SceneLightEnvironment = m_Scene->m_LightEnvironment;

		m_GeometryPass->SetInput("u_RadianceMap", m_SceneInfo.SceneEnvironment->RadianceMap);
		m_GeometryPass->SetInput("u_IrradianceMap", m_SceneInfo.SceneEnvironment->IrradianceMap);
		m_GeometryPass->SetInput("u_ShadowMap", m_DirectionalShadowPasses[0]->GetDepthOutput());

		m_DoubleSidedGeometryPass->SetInput("u_RadianceMap", m_SceneInfo.SceneEnvironment->RadianceMap);
		m_DoubleSidedGeometryPass->SetInput("u_IrradianceMap", m_SceneInfo.SceneEnvironment->IrradianceMap);

		m_WireframeViewGeometryPass->SetInput("u_RadianceMap", m_SceneInfo.SceneEnvironment->RadianceMap);
		m_WireframeViewGeometryPass->SetInput("u_IrradianceMap", m_SceneInfo.SceneEnvironment->IrradianceMap);
		m_WireframeViewGeometryPass->SetInput("u_ShadowMap", m_DirectionalShadowPasses[0]->GetDepthOutput());

		// Resize resources if needed
		if (m_NeedsResize)
		{
			m_NeedsResize = false;
			
		 	// PreDepth and Geometry framebuffers need to be resized first since other framebuffers reference images in them
			m_PreDepthPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
		 	m_GeometryPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
		 	m_SelectedGeometryPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
		 	m_SkyboxPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
		 	m_CompositePass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);

		 	m_CompositingFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);
			
		 	m_DoubleSidedPreDepthPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
		 	m_DoubleSidedGeometryPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
		 	m_DoubleSidedSelectedGeometryPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);

		 	for (auto& jumpFloodFB : m_JumpFloodFramebuffers)
		 		jumpFloodFB->Resize(m_ViewportWidth, m_ViewportHeight);
		 
		 	if (m_JumpFloodCompositePass)
		 		m_JumpFloodCompositePass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
		}

		// Update uniform buffers data for the starting frame
		// Camera uniform buffer (set = 1, binding = 0)
		{
			UBCamera& cameraData = m_CameraDataUB;

			SceneRendererCamera& sceneCamera = m_SceneInfo.Camera;
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
				instance->m_UBSScreenData->RT_Get()->RT_SetData(&screenData, sizeof(UBScreenData));
			});
		}

		// Scene Data uniform buffer (set = 1, binding = 2)
		{
			UBScene& sceneData = m_SceneDataUB;

			const auto& directionalLight = m_SceneInfo.SceneLightEnvironment.DirectionalLight;
			sceneData.Light.Direction = { directionalLight.Direction.x, directionalLight.Direction.y, directionalLight.Direction.z, directionalLight.ShadowOpacity };
			sceneData.Light.Radiance = { directionalLight.Radiance.r, directionalLight.Radiance.g, directionalLight.Radiance.b, directionalLight.Intensity };
			
			sceneData.CameraPosition = m_CameraDataUB.InverseViewMatrix[3];
			sceneData.EnvironmentMapIntensity = m_SceneInfo.SceneEnvironmentIntensity; // Mirrors the value that is sent to the Skybox shader

			Ref<SceneRenderer> instance = this;
			Renderer::Submit([instance, sceneData]() mutable
			{
				instance->m_UBSSceneData->RT_Get()->RT_SetData(&sceneData, sizeof(UBScene));
			});
		}

		// Directional Shadow Data uniform buffer (set = 2, binding = 3)
		{
			UBDirectionalShadowData& dirShadowData = m_DirectionalShadowDataUB;

			CascadeData cascades[4];
			CalculateCascades(m_SceneInfo.Camera, glm::vec3(m_SceneDataUB.Light.Direction), cascades);

			m_CascadeSplits.x = cascades[0].SplitDepth;
			m_CascadeSplits.y = cascades[1].SplitDepth;
			m_CascadeSplits.z = cascades[2].SplitDepth;
			m_CascadeSplits.w = cascades[3].SplitDepth;

			dirShadowData.DirectionalLightMatrices[0] = cascades[0].ViewProjMatrix;
			dirShadowData.DirectionalLightMatrices[1] = cascades[1].ViewProjMatrix;
			dirShadowData.DirectionalLightMatrices[2] = cascades[2].ViewProjMatrix;
			dirShadowData.DirectionalLightMatrices[3] = cascades[3].ViewProjMatrix;

			Ref<SceneRenderer> instance = this;
			Renderer::Submit([instance, dirShadowData]() mutable
			{
				instance->m_UBSDirectionalShadowData->RT_Get()->RT_SetData(&dirShadowData, sizeof(UBDirectionalShadowData));
			});
		}

		// Renderer Data uniform buffer (set = 2, binding = 4)
		{
			UBRendererData& rendererData = m_RendererDataUB;

			// NOTE: Other data is set from scene renderer panel or manually through provided methods
			rendererData.CascadeSplits = m_CascadeSplits;

			if (m_ViewMode == ViewMode::Unlit || m_ViewMode == ViewMode::Wireframe)
				rendererData.Unlit = true;
			else
				rendererData.Unlit = false;

			Ref<SceneRenderer> instance = this;
			Renderer::Submit([instance, rendererData]() mutable
			{
				instance->m_UBSRendererData->RT_Get()->RT_SetData(&rendererData, sizeof(UBRendererData));
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
				bool isDoubleSided = materialAsset->IsDoubleSided();
				auto& destDrawList = isDoubleSided == true ? m_DoubleSidedStaticMeshDrawList : m_StaticMeshDrawList;
				auto& dc = destDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.MeshSource = meshSource;
				dc.SubMeshIndex = subMeshIndex;
				dc.MaterialTable = materialTable;
				dc.OverrideMaterial = overrideMaterial;
				dc.InstanceCount++;
			}

			// For Shadow Pass
			if (materialAsset->IsShadowCasting())
			{
				auto& dc = m_StaticMeshShadowDrawList[meshKey];
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
				bool isDoubleSided = materialAsset->IsDoubleSided();
				auto& destDrawList =  isDoubleSided == true ? m_DoubleSidedStaticMeshDrawList : m_StaticMeshDrawList;
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
				bool isDoubleSided = materialAsset->IsDoubleSided();
				auto& dc = isDoubleSided == true ? m_DoubleSidedSelectedStaticMeshDrawList[meshKey] : m_SelectedStaticMeshDrawList[meshKey];
				dc.StaticMesh = staticMesh;
				dc.MeshSource = meshSource;
				dc.SubMeshIndex = subMeshIndex;
				dc.MaterialTable = materialTable;
				dc.OverrideMaterial = overrideMaterial;
				dc.InstanceCount++;
			}

			// For Shadow Pass
			if (materialAsset->IsShadowCasting())
			{
				auto& dc = m_StaticMeshShadowDrawList[meshKey];
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

	const PipelineStatistics& SceneRenderer::GetPipelineStatistics() const
	{
		return m_CommandBuffer->GetPipelineStatistics(Renderer::GetCurrentFrameIndex());
	}

	void SceneRenderer::ResetImageLayouts()
	{
		Renderer::Submit([cmdBuffer = m_CommandBuffer, preDepthPass = m_PreDepthPass, geoPass = m_GeometryPass, selectedGeo = m_SelectedGeometryPass, jfaFBs = m_JumpFloodFramebuffers, compPass = m_CompositePass, dirShadowPasses = m_DirectionalShadowPasses, spec = m_Specification]()
		{
			// Directional Shadow map image (NOTE: Do only for one of them since all the passes reference the same image)
			Renderer::InsertImageMemoryBarrier(
				cmdBuffer->GetActiveCommandBuffer(),
				dirShadowPasses[0]->GetDepthOutput()->GetVulkanImage(),
				0,
				VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = spec.NumberOfShadowCascades }
			);

			// PreDepth image
			Renderer::InsertImageMemoryBarrier(
				cmdBuffer->GetActiveCommandBuffer(),
				preDepthPass->GetDepthOutput()->GetVulkanImage(),
				0,
				VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
			);

			// Geometry Color images
			Renderer::InsertImageMemoryBarrier(
				cmdBuffer->GetActiveCommandBuffer(),
				geoPass->GetOutput(0)->GetVulkanImage(),
				0,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
			);

			// JFA
			if (spec.JumpFloodPass)
			{
				// Selected geo isolation
				Renderer::InsertImageMemoryBarrier(
					cmdBuffer->GetActiveCommandBuffer(),
					selectedGeo->GetOutput(0)->GetVulkanImage(),
					0,
					VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
					{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
				);

				// Transition jumpflood framebuffers images into writing-to layouts
				for (int i = 0; i < static_cast<int>(jfaFBs.size()); i++)
				{
					Renderer::InsertImageMemoryBarrier(
						cmdBuffer->GetActiveCommandBuffer(),
						jfaFBs[i]->GetImage(0)->GetVulkanImage(),
						0,
						VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
						VK_IMAGE_LAYOUT_UNDEFINED,
						VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
						VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
						{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
					);
				}
			}

			// Composite color image
			Renderer::InsertImageMemoryBarrier(
				cmdBuffer->GetActiveCommandBuffer(),
				compPass->GetOutput(0)->GetVulkanImage(),
				0,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
			);
		});
	}

	void SceneRenderer::FlushDrawList()
	{
		if (m_ResourcesCreated && m_ViewportWidth > 0 && m_ViewportHeight > 0)
		{
			PreRender();

			m_CommandBuffer->Begin();

			ResetImageLayouts();
			DirectionalShadowPass();
			PreDepthPass();
			SkyboxPass();
			GeometryPass();

			if (m_Specification.JumpFloodPass)
				JumpFloodPass();

			// NOTE: If we want to sample the depth texture in the composite pass we need to transition to shader read only then transition back
			// to attachment since the renderer2D uses it.
			CompositePass();

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
		m_DoubleSidedStaticMeshDrawList.clear();
		m_SelectedStaticMeshDrawList.clear();
		m_DoubleSidedSelectedStaticMeshDrawList.clear();
		m_StaticMeshShadowDrawList.clear();

		m_SceneInfo = {};

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

	void SceneRenderer::ClearPass()
	{
		Renderer::Submit([cmdBuffer = m_CommandBuffer, preDepthPass = m_PreDepthPass, compPass = m_CompositePass]()
		{
			// PreDepth image
			Renderer::InsertImageMemoryBarrier(
				cmdBuffer->GetActiveCommandBuffer(),
				preDepthPass->GetDepthOutput()->GetVulkanImage(),
				0,
				VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
			);

			// Geometry Color images
			Renderer::InsertImageMemoryBarrier(
				cmdBuffer->GetActiveCommandBuffer(),
				compPass->GetOutput(0)->GetVulkanImage(),
				0,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
			);
		});

		// Here we do not need to explicit clear since the attachment is LOAD_OP_CLEAR
		Renderer::BeginRenderPass(m_CommandBuffer, m_PreDepthPass);
		Renderer::EndRenderPass(m_CommandBuffer);

		// Here we do not need to explicit clear since the attachment is LOAD_OP_CLEAR
		Renderer::BeginRenderPass(m_CommandBuffer, m_CompositePass);
		Renderer::EndRenderPass(m_CommandBuffer);
	}

	void SceneRenderer::DirectionalShadowPass()
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		const SceneDirectionalLight& directionalLight = m_SceneInfo.SceneLightEnvironment.DirectionalLight;
		if (!directionalLight.CastShadows || directionalLight.Intensity == 0.0f)
		{
			// Clear Shadow Maps
			for (uint32_t i = 0; i < m_Specification.NumberOfShadowCascades; i++)
			{
				Renderer::BeginRenderPass(m_CommandBuffer, m_DirectionalShadowPasses[i]);
				Renderer::EndRenderPass(m_CommandBuffer);
			}

			// Make image ready to be read from
			Renderer::Submit([cmdBuffer = m_CommandBuffer, dirShadowPasses = m_DirectionalShadowPasses, cascades = m_Specification.NumberOfShadowCascades]()
			{
				Renderer::InsertImageMemoryBarrier(
					cmdBuffer->GetActiveCommandBuffer(),
					dirShadowPasses[0]->GetDepthOutput()->GetVulkanImage(),
					VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					VK_ACCESS_2_SHADER_READ_BIT,
					VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = cascades }
				);
			});

			return;
		}

		for (uint32_t i = 0; i < m_Specification.NumberOfShadowCascades; i++)
		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_DirectionalShadowPasses[i]);

			const Buffer cascade(reinterpret_cast<const uint8_t*>(&i), sizeof(uint32_t));
			for (const auto& [mk, dc] : m_StaticMeshShadowDrawList)
			{
				IR_VERIFY(m_MeshTransformMap.contains(mk));
				const auto& transformData = m_MeshTransformMap.at(mk);
				Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_DirectionalShadowPasses[i]->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, m_DirectionalShadowMaterial, m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset, dc.InstanceCount, cascade);
			}

			Renderer::EndRenderPass(m_CommandBuffer);
		}

		// Make image ready to be read from
		Renderer::Submit([cmdBuffer = m_CommandBuffer, dirShadowPasses = m_DirectionalShadowPasses, cascades = m_Specification.NumberOfShadowCascades]()
		{
				Renderer::InsertImageMemoryBarrier(
				cmdBuffer->GetActiveCommandBuffer(),
				dirShadowPasses[0]->GetDepthOutput()->GetVulkanImage(),
				VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_2_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = cascades }
			);
		});
	}

	void SceneRenderer::PreDepthPass()
	{
		// Render all objects into a depth texture only and use that in all other passes that need depth

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		if (m_ViewMode == ViewMode::Lit || m_ViewMode == ViewMode::Unlit)
		{
			Ref<RenderPass> renderPassToUse = nullptr;
			switch (m_ViewMode)
			{
				case ViewMode::Lit: renderPassToUse = m_PreDepthPass; break;
				case ViewMode::Unlit: renderPassToUse = m_PreDepthPass; break;
			}

			Renderer::BeginRenderPass(m_CommandBuffer, renderPassToUse);

			for (const auto& [mk, dc] : m_StaticMeshDrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				glm::mat4 transform = dc.MeshSource->GetSubMeshes()[dc.SubMeshIndex].Transform;
				Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, renderPassToUse->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, m_PreDepthMaterial, m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset, dc.InstanceCount);
			}

			Renderer::EndRenderPass(m_CommandBuffer);

			switch (m_ViewMode)
			{
				case ViewMode::Lit: renderPassToUse = m_DoubleSidedPreDepthPass; break;
				case ViewMode::Unlit: renderPassToUse = m_DoubleSidedPreDepthPass; break;
			}
			
			if (m_DoubleSidedStaticMeshDrawList.size())
			{
				Renderer::BeginRenderPass(m_CommandBuffer, renderPassToUse);
			
				for (const auto& [mk, dc] : m_DoubleSidedStaticMeshDrawList)
				{
					const auto& transformData = m_MeshTransformMap.at(mk);
					glm::mat4 transform = dc.MeshSource->GetSubMeshes()[dc.SubMeshIndex].Transform;
					Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, renderPassToUse->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, m_PreDepthMaterial, m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset, dc.InstanceCount);
				}
			
				Renderer::EndRenderPass(m_CommandBuffer);
			}
		}
		else
		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_WireframeViewPreDepthPass);
			
			for (const auto& [mk, dc] : m_StaticMeshDrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				glm::mat4 transform = dc.MeshSource->GetSubMeshes()[dc.SubMeshIndex].Transform;
				Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_WireframeViewPreDepthPass->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, m_PreDepthMaterial, m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset, dc.InstanceCount);
			}
			
			for (const auto& [mk, dc] : m_DoubleSidedStaticMeshDrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				glm::mat4 transform = dc.MeshSource->GetSubMeshes()[dc.SubMeshIndex].Transform;
				Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_WireframeViewPreDepthPass->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, m_PreDepthMaterial, m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset, dc.InstanceCount);
			}
			
			Renderer::EndRenderPass(m_CommandBuffer);
		}
	}

	void SceneRenderer::SkyboxPass()
	{
		m_SkyboxMaterial->Set("u_Uniforms.TextureLod", m_SceneInfo.SkyboxLod);
		m_SkyboxMaterial->Set("u_Uniforms.Intensity", m_SceneInfo.SceneEnvironmentIntensity);
		
		const Ref<TextureCube> radianceMap = m_SceneInfo.SceneEnvironment ? m_SceneInfo.SceneEnvironment->RadianceMap : Renderer::GetBlackCubeTexture();
		m_SkyboxMaterial->Set("u_Texture", radianceMap);
		
		Renderer::BeginRenderPass(m_CommandBuffer, m_SkyboxPass);
		Renderer::SubmitFullScreenQuad(m_CommandBuffer, m_SkyboxPass->GetPipeline(), m_SkyboxMaterial);
		Renderer::EndRenderPass(m_CommandBuffer);
	}

	void SceneRenderer::GeometryPass()
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		// Selected Geometry Isolation (Only happens when we have something selected, so as long we do not have anything selected we do not want to start these passes)
		if (m_Specification.JumpFloodPass)
		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_SelectedGeometryPass);
		
			for (auto& [mk, dc] : m_SelectedStaticMeshDrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_SelectedGeometryPass->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, m_SelectedGeometryMaterial, m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset + dc.InstanceOffset * sizeof(TransformVertexData), dc.InstanceCount);
			}
		
			Renderer::EndRenderPass(m_CommandBuffer);
		
			Renderer::BeginRenderPass(m_CommandBuffer, m_DoubleSidedSelectedGeometryPass);
		
			for (auto& [mk, dc] : m_DoubleSidedSelectedStaticMeshDrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_DoubleSidedSelectedGeometryPass->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, m_SelectedGeometryMaterial, m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset + dc.InstanceOffset * sizeof(TransformVertexData), dc.InstanceCount);
			}
		
			Renderer::EndRenderPass(m_CommandBuffer);
		}

		// Lit and Unlit
		if (m_ViewMode == ViewMode::Lit || m_ViewMode == ViewMode::Unlit)
		{
			Ref<RenderPass> renderPassToUse = nullptr;
			switch (m_ViewMode)
			{
				case ViewMode::Lit: renderPassToUse = m_GeometryPass; break;
				case ViewMode::Unlit: renderPassToUse = m_GeometryPass; break;
			}

			Renderer::BeginRenderPass(m_CommandBuffer, renderPassToUse);

			for (const auto& [mk, dc] : m_StaticMeshDrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				Renderer::RenderStaticMesh(m_CommandBuffer, renderPassToUse->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.StaticMesh->GetMaterials(), m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset, dc.InstanceCount);
			}

			Renderer::EndRenderPass(m_CommandBuffer);

			switch (m_ViewMode)
			{
				case ViewMode::Lit: renderPassToUse = m_DoubleSidedGeometryPass; break;
				case ViewMode::Unlit: renderPassToUse = m_DoubleSidedGeometryPass; break;
			}
			
			if (m_DoubleSidedStaticMeshDrawList.size())
			{
				Renderer::BeginRenderPass(m_CommandBuffer, renderPassToUse);
			
				for (const auto& [mk, dc] : m_DoubleSidedStaticMeshDrawList)
				{
					const auto& transformData = m_MeshTransformMap.at(mk);
					Renderer::RenderStaticMesh(m_CommandBuffer, renderPassToUse->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.StaticMesh->GetMaterials(), m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset, dc.InstanceCount);
				}
			
				Renderer::EndRenderPass(m_CommandBuffer);
			}
		}
		// Wireframe view
		else
		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_WireframeViewGeometryPass);
			
			for (const auto& [mk, dc] : m_StaticMeshDrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				Renderer::RenderStaticMesh(m_CommandBuffer, m_WireframeViewGeometryPass->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.StaticMesh->GetMaterials(), m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset, dc.InstanceCount);
			}
			
			for (const auto& [mk, dc] : m_DoubleSidedStaticMeshDrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				Renderer::RenderStaticMesh(m_CommandBuffer, m_WireframeViewGeometryPass->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.StaticMesh->GetMaterials(), m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset, dc.InstanceCount);
			}
			
			Renderer::EndRenderPass(m_CommandBuffer);
		}
	}

	void SceneRenderer::JumpFloodPass()
	{
		// Init pass
		Renderer::Submit([cmdBuffer = m_CommandBuffer, selectedGeo = m_SelectedGeometryPass]()
		{
			Renderer::InsertImageMemoryBarrier(
				cmdBuffer->GetActiveCommandBuffer(),
				selectedGeo->GetOutput(0)->GetVulkanImage(),
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_2_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
			);
		});

		Renderer::BeginRenderPass(m_CommandBuffer, m_JumpFloodInitPass);
		Renderer::SubmitFullScreenQuad(m_CommandBuffer, m_JumpFloodInitPass->GetPipeline(), m_JumpFloodInitMaterial);
		Renderer::EndRenderPass(m_CommandBuffer);
	
		Ref<Framebuffer> passFb = m_JumpFloodPass[0]->GetTargetFramebuffer();
		glm::vec2 texelSize = { 1.0f / static_cast<float>(passFb->GetWidth()), 1.0f / static_cast<float>(passFb->GetHeight()) };
		
		Buffer vertexOverrides;
		vertexOverrides.Allocate(sizeof(glm::vec2) + sizeof(int));
		vertexOverrides.Write(reinterpret_cast<const uint8_t*>(glm::value_ptr(texelSize)), sizeof(glm::vec2));
		
		// Even Pass		
		Renderer::Submit([cmdBuffer = m_CommandBuffer, jfaFBs = m_JumpFloodFramebuffers]()
		{
			Renderer::InsertImageMemoryBarrier(
				cmdBuffer->GetActiveCommandBuffer(),
				jfaFBs[0]->GetImage(0)->GetVulkanImage(),
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_2_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
			);
		});

		int step = 1;
		vertexOverrides.Write(reinterpret_cast<const uint8_t*>(&step), sizeof(int), sizeof(glm::vec2));

		Renderer::BeginRenderPass(m_CommandBuffer, m_JumpFloodPass[0]);
		Renderer::SubmitFullScreenQuadWithOverrides(m_CommandBuffer, m_JumpFloodPass[0]->GetPipeline(), m_JumpFloodPassMaterial[0], vertexOverrides, Buffer());
		Renderer::EndRenderPass(m_CommandBuffer);

		Renderer::Submit([cmdBuffer = m_CommandBuffer, jfaFBs = m_JumpFloodFramebuffers]()
		{
			Renderer::InsertImageMemoryBarrier(
				cmdBuffer->GetActiveCommandBuffer(),
				jfaFBs[0]->GetImage(0)->GetVulkanImage(),
				VK_ACCESS_2_SHADER_READ_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
			);

			// Odd Pass
			Renderer::InsertImageMemoryBarrier(
				cmdBuffer->GetActiveCommandBuffer(),
				jfaFBs[1]->GetImage(0)->GetVulkanImage(),
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_2_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
			);
		});

		// Odd Pass
		step = 1;
		vertexOverrides.Write(reinterpret_cast<const uint8_t*>(&step), sizeof(int), sizeof(glm::vec2));

		Renderer::BeginRenderPass(m_CommandBuffer, m_JumpFloodPass[1]);
		Renderer::SubmitFullScreenQuadWithOverrides(m_CommandBuffer, m_JumpFloodPass[1]->GetPipeline(), m_JumpFloodPassMaterial[1], vertexOverrides, Buffer());
		Renderer::EndRenderPass(m_CommandBuffer);

		vertexOverrides.Release();
	}

	void SceneRenderer::CompositePass()
	{
		// Composite the final scene image and apply gamma correction and tone mapping

		// Transition Geometry Pass image to shader read only format
		Renderer::Submit([cmd = m_CommandBuffer, geoPass = m_GeometryPass]()
		{
			Renderer::InsertImageMemoryBarrier(
				cmd->GetActiveCommandBuffer(),
				geoPass->GetOutput(0)->GetVulkanImage(),
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_2_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
			);
		});

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		float exposure = m_SceneInfo.Camera.Camera.GetExposure();

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
			// Prepare image of m_JumpFloodFB[0] for coposite pass since it is the one that we read from
			Renderer::Submit([cmdBuff = m_CommandBuffer, jfaFBs = m_JumpFloodFramebuffers]()
			{
				Renderer::InsertImageMemoryBarrier(
					cmdBuff->GetActiveCommandBuffer(),
					jfaFBs[0]->GetImage(0)->GetVulkanImage(),
					VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
					VK_ACCESS_2_SHADER_READ_BIT,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
				);
			});

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
		
			for (const auto& [mk, dc] : m_DoubleSidedSelectedStaticMeshDrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_GeometryWireFramePass->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, m_WireFrameMaterial, m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset + dc.InstanceOffset * sizeof(TransformVertexData), dc.InstanceCount);
			}
		
			Renderer::EndRenderPass(m_CommandBuffer);
		}
	}

	void SceneRenderer::CalculateCascades(const SceneRendererCamera& sceneCamera, const glm::vec3& lightDirection, CascadeData* cascades) const
	{
		// This basically sets whether we want to increase shadow quality around the origin of the scene so we increase the value closer to 1.0, and if we want more evenly spread out
		// shadows then we should set this value closer to 0.0
		float scaleToOrigin = m_ScaleShadowCascadesToOrigin;

		glm::mat4 viewMatrix = sceneCamera.ViewMatrix;
		constexpr glm::vec4 origin = { 0.0f, 0.0f, 0.0f, 1.0f };
		viewMatrix[3] = glm::lerp(viewMatrix[3], origin, scaleToOrigin);

		glm::mat4 viewProjection = sceneCamera.Camera.GetUnReversedProjectionMatrix() * viewMatrix;

		constexpr int SHADOW_MAP_CASCADES = 4;
		float cascadeSplits[SHADOW_MAP_CASCADES];

		float nearClip = sceneCamera.Near;
		float farClip = sceneCamera.Far;
		float clipRange = farClip - nearClip;

		float minZ = nearClip;
		float maxZ = nearClip + clipRange; // = farClip

		float range = maxZ - minZ;
		float ratio = maxZ / minZ;

		// Calculate split depths based on view camera frustum
		// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
		for (int i = 0; i < SHADOW_MAP_CASCADES; i++)
		{
			float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADES);
			float log = minZ * std::pow(ratio, p);
			float uniform = minZ + range * p;
			float d = m_CascadeSplitLambda * (log - uniform) + uniform;
			cascadeSplits[i] = (d - nearClip) / clipRange;
		}

		cascadeSplits[3] = 0.3f;

		// Calculate orthographic projection matrix for each cascade
		float lastSplitDist = 0.0f;
		for (int i = 0; i < SHADOW_MAP_CASCADES; i++)
		{
			float splitDist = cascadeSplits[i];

			glm::vec3 frustumCorners[8] =
			{
				glm::vec3(-1.0f,  1.0f, -1.0f),
				glm::vec3(1.0f,  1.0f, -1.0f),
				glm::vec3(1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f, -1.0f,  1.0f),
				glm::vec3(-1.0f, -1.0f,  1.0f),
			};

			// Project frustum corners into world space
			glm::mat4 invCam = glm::inverse(viewProjection);
			for (int i = 0; i < 8; i++)
			{
				glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
				frustumCorners[i] = invCorner / invCorner.w;
			}

			for (int i = 0; i < 4; i++)
			{
				glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
				frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
				frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
			}

			// Get frustum center
			glm::vec3 frustumCenter = glm::vec3(0.0f);
			for (int i = 0; i < 8; i++)
				frustumCenter += frustumCorners[i];

			frustumCenter /= 8.0f;

			float radius = 0.0f;
			for (int i = 0; i < 8; i++)
			{
				float distance = glm::length(frustumCorners[i] - frustumCenter);
				radius = glm::max(radius, distance);
			}

			radius = std::ceil(radius * 16.0f) / 16.0f;

			glm::vec3 maxExtents = glm::vec3(radius);
			glm::vec3 minExtents = -maxExtents;

			glm::vec3 lightDir = -lightDirection;
			glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, { 0.0f, 0.0f, 1.0f });
			glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f + m_CascadeNearPlaneOffset, maxExtents.z - minExtents.z + m_CascadeFarPlaneOffset);

			// Offset to texel space to avoid shimmering (from https://stackoverflow.com/questions/33499053/cascaded-shadow-map-shimmering)
			glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
			float shadowMapResolution = static_cast<float>(m_DirectionalShadowPasses[0]->GetTargetFramebuffer()->GetWidth());

			glm::vec4 shadowOrigin = (shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) * shadowMapResolution / 2.0f;
			glm::vec4 roundedOrigin = glm::round(shadowOrigin);
			glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
			roundOffset = roundOffset * 2.0f / shadowMapResolution;
			roundOffset.z = 0.0f;
			roundOffset.w = 0.0f;

			lightOrthoMatrix[3] += roundOffset;

			cascades[i].ViewProjMatrix = shadowMatrix;
			cascades[i].ViewMatrix = lightViewMatrix;
			cascades[i].SplitDepth = (nearClip + splitDist * clipRange) * -1.0f;

			lastSplitDist = cascadeSplits[i];
		}
	}

	void SceneRenderer::UpdateStatistics()
	{
		m_Statistics.TotalDrawCalls = 0;
		m_Statistics.ColorPassDrawCalls = 0;
		m_Statistics.Instances = 0;
		m_Statistics.Meshes = 0;

		for (const auto& [mk, dc] : m_SelectedStaticMeshDrawList)
		{
			m_Statistics.Instances += dc.InstanceCount;
			m_Statistics.ColorPassDrawCalls += 1;
			m_Statistics.Meshes += 1;
		}

		for (const auto& [mk, dc] : m_DoubleSidedSelectedStaticMeshDrawList)
		{
			m_Statistics.Instances += dc.InstanceCount;
			m_Statistics.ColorPassDrawCalls += 1;
			m_Statistics.Meshes += 1;
		}

		for (const auto& [mk, dc] : m_StaticMeshDrawList)
		{
			m_Statistics.Instances += dc.InstanceCount;
			m_Statistics.ColorPassDrawCalls += 1;
			m_Statistics.Meshes += 1;
		}

		for (const auto& [mk, dc] : m_DoubleSidedStaticMeshDrawList)
		{
			m_Statistics.Instances += dc.InstanceCount;
			m_Statistics.ColorPassDrawCalls += 1;
			m_Statistics.Meshes += 1;
		}

		m_Statistics.TotalDrawCalls = Renderer::GetTotalDrawCallCount();
		m_Statistics.ColorPassSavedDraws = m_Statistics.Instances - m_Statistics.ColorPassDrawCalls;
	}

}