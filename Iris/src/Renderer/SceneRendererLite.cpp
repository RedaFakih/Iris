#include "IrisPCH.h"
#include "SceneRendererLite.h"

#include "AssetManager/AssetManager.h"
#include "ComputePass.h"
#include "Editor/EditorSettings.h"

namespace Iris {

	SceneRendererLite::SceneRendererLite(Ref<Scene> scene, const SceneRendererSpecification& spec)
		: m_Scene(scene), m_ViewportWidth(spec.ViewportWidth), m_ViewportHeight(spec.ViewportHeight)
	{
		Init();
	}

	SceneRendererLite::~SceneRendererLite()
	{
		Shutdown();
	}

	void SceneRendererLite::Init()
	{
		if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
			m_NeedsResize = true;

		m_CommandBuffer = RenderCommandBuffer::Create(0, "SceneRenderer");

		const uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;

		// Unifrom Buffer
		{
			m_UBSCamera = UniformBufferSet::Create(sizeof(UBCamera));
			m_UBSScreenData = UniformBufferSet::Create(sizeof(UBScreenData));
			m_UBSSceneData = UniformBufferSet::Create(sizeof(UBScene));
		}

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

		// Bloom
		{
			{
				TextureSpecification spec;
				spec.Format = ImageFormat::RGBA32F;
				spec.Width = 1;
				spec.Height = 1;
				spec.WrapMode = TextureWrap::Clamp;
				spec.GenerateMips = true;
				spec.Usage = ImageUsage::Storage;

				spec.DebugName = "BloomTexture-0";
				m_BloomTextures[0].Texture = Texture2D::Create(spec);
				spec.DebugName = "BloomTexture-1";
				m_BloomTextures[1].Texture = Texture2D::Create(spec);
				spec.DebugName = "BloomTexture-2";
				m_BloomTextures[2].Texture = Texture2D::Create(spec);
			}

			//// Image Views (per-mip)
			ImageViewSpecification imageViewSpec;
			for (uint32_t i = 0; i < 3; i++)
			{
				uint32_t mipCount = m_BloomTextures[i].Texture->GetMipLevelCount();
				m_BloomTextures[i].ImageViews.resize(mipCount);
				for (uint32_t mip = 0; mip < mipCount; mip++)
				{
					imageViewSpec.DebugName = fmt::format("BloomImageView-({} - {})", i, mip);
					imageViewSpec.Image = m_BloomTextures[i].Texture;
					imageViewSpec.Mip = mip;
					m_BloomTextures[i].ImageViews[mip] = ImageView::Create(imageViewSpec);
				}
			}

			// Pre-Filter
			Ref<Shader> preFilterShader = Renderer::GetShadersLibrary()->Get("Bloom-Prefilter");
			ComputePassSpecification preFilterPassSpec = {
				.DebugName = "BloomPreFilterPass",
				.Pipeline = ComputePipeline::Create(preFilterShader, "BloomPreFilterPipeline"),
				.MarkerColor = { 0.3f, 0.85f, 0.5f, 1.0f }
			};
			m_BloomPreFilterPass = ComputePass::Create(preFilterPassSpec);

			// No resources to add since everything will be set by the materials
			m_BloomPreFilterPass->Bake();

			// Downsample
			Ref<Shader> downSampleShader = Renderer::GetShadersLibrary()->Get("Bloom-Downsample");
			ComputePassSpecification downSamplePassSpec = {
				.DebugName = "BloomDownsamplePass",
				.Pipeline = ComputePipeline::Create(downSampleShader, "BloomDownSamplePipeline"),
				.MarkerColor = { 0.3f, 0.85f, 0.5f, 1.0f }
			};
			m_BloomDownSamplePass = ComputePass::Create(downSamplePassSpec);

			// No resources to add since everything will be set by the materials
			m_BloomDownSamplePass->Bake();

			// First-Upsample
			Ref<Shader> firstUpsampleShader = Renderer::GetShadersLibrary()->Get("Bloom-FirstUpsample");
			ComputePassSpecification firstUpsampleSpec = {
				.DebugName = "BloomFirstUpsamplePass",
				.Pipeline = ComputePipeline::Create(firstUpsampleShader, "BloomFirstUpsamplePipeline"),
				.MarkerColor = { 0.3f, 0.85f, 0.5f, 1.0f }
			};
			m_BloomFirstUpSamplePass = ComputePass::Create(firstUpsampleSpec);

			// No resources to add since everything will be set by the materials
			m_BloomFirstUpSamplePass->Bake();

			// Upsample
			Ref<Shader> upsampleShader = Renderer::GetShadersLibrary()->Get("Bloom-Upsample");
			ComputePassSpecification upsampleSpec = {
				.DebugName = "BloomUpsamplePass",
				.Pipeline = ComputePipeline::Create(upsampleShader, "BloomUpsamplePipeline"),
				.MarkerColor = { 0.3f, 0.85f, 0.5f, 1.0f }
			};
			m_BloomUpSamplePass = ComputePass::Create(upsampleSpec);

			// No resources to add since everything will be set by the materials
			m_BloomUpSamplePass->Bake();

			m_BloomDirtTexture = Renderer::GetBlackTexture();
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

			geometryFBSpec.ExistingImages[3] = m_PreDepthPass->GetDepthOutput();

			PipelineSpecification staticGeometryPipelineSpec = {
				.DebugName = "PBRStaticPipeline",
				.Shader = Renderer::GetShadersLibrary()->Get("IrisPBRStaticLite"),
				.TargetFramebuffer = Framebuffer::Create(geometryFBSpec),
				.VertexLayout = vertexLayout,
				.InstanceLayout = instanceLayout,
				.BackFaceCulling = true,
				.DepthOperator = DepthCompareOperator::Equal,
				.DepthWrite = false,
				.WireFrame = false
			};

			RenderPassSpecification staticGeometryPassSpec = {
				.DebugName = "StaticGeometryRenderPass",
				.Pipeline = Pipeline::Create(staticGeometryPipelineSpec),
				.MarkerColor = { 1.0f, 0.0f, 0.0f, 1.0f }
			};
			m_GeometryPass = RenderPass::Create(staticGeometryPassSpec);

			m_GeometryPass->SetInput("Camera", m_UBSCamera);
			m_GeometryPass->SetInput("SceneData", m_UBSSceneData);
			m_GeometryPass->SetInput("u_RadianceMap", Renderer::GetBlackCubeTexture());
			m_GeometryPass->SetInput("u_IrradianceMap", Renderer::GetBlackCubeTexture());
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
				m_DoubleSidedGeometryPass->SetInput("u_RadianceMap", Renderer::GetBlackCubeTexture());
				m_DoubleSidedGeometryPass->SetInput("u_IrradianceMap", Renderer::GetBlackCubeTexture());
				m_DoubleSidedGeometryPass->SetInput("u_BRDFLutTexture", Renderer::GetBRDFLutTexture());

				m_DoubleSidedGeometryPass->Bake();
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
				.ReleaseShaderModules = false
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
			m_CompositePass->SetInput("u_BloomTexture", m_BloomTextures[2].Texture);
			m_CompositePass->SetInput("u_BloomDirtTexture", m_BloomDirtTexture);
			// TODO: To use this we need to also transition its layout in the composite pass
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
				.ReleaseShaderModules = false
			};
		
			RenderPassSpecification gridPassSpec = {
				.DebugName = "GridPass",
				.Pipeline = Pipeline::Create(gridPipelineSpec),
				.MarkerColor = { 1.0f, 0.5f, 0.0f, 1.0f }
			};
			m_GridPass = RenderPass::Create(gridPassSpec);
			m_GridMaterial = Material::Create(gridPipelineSpec.Shader, "GridMaterial");
			m_GridMaterial->Set("u_Uniforms.Scale", EditorSettings::Get().TranslationSnapValue);
		
			m_GridPass->SetInput("Camera", m_UBSCamera);
			m_GridPass->Bake();
		}

		// Skybox
		{
			FramebufferSpecification skyboxFBSpec = {
				.DebugName = "SkyboxFB",
				.ClearColorOnLoad = false,
				.ClearColor = { 1.0f, 0.5f, 1.0f, 1.0f },
				.ClearDepthOnLoad = false,
				.Attachments = { ImageFormat::RGBA32F, ImageFormat::Depth32F },
			};
			skyboxFBSpec.ExistingImages[0] = m_GeometryPass->GetOutput(0);
			skyboxFBSpec.ExistingImages[1] = m_PreDepthPass->GetDepthOutput();

			PipelineSpecification skyboxPipelineSpec = {
				.DebugName = "SkyboxPipeline",
				.Shader = Renderer::GetShadersLibrary()->Get("Skybox"),
				.TargetFramebuffer = Framebuffer::Create(skyboxFBSpec),
				.VertexLayout = {
					{ ShaderDataType::Float3, "a_Position" },
					{ ShaderDataType::Float2, "a_TexCoord" }
				},
				.DepthOperator = DepthCompareOperator::GreaterOrEqual,
				.DepthTest = true,
				.DepthWrite = false,
				.ReleaseShaderModules = false
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

		// NOTE: Maybe 64 is alot as well?
		constexpr std::size_t TransformBufferCount = 64; // 64 transforms
		m_MeshTransformBuffers.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			m_MeshTransformBuffers[i].VertexBuffer = VertexBuffer::Create(sizeof(TransformVertexData) * TransformBufferCount);
			m_MeshTransformBuffers[i].Data = new TransformVertexData[TransformBufferCount];
		}

		Ref<SceneRendererLite> instance = this;
		Renderer::Submit([instance]() mutable { instance->m_ResourcesCreatedGPU = true; });
	}

	void SceneRendererLite::Shutdown()
	{
		for (auto& transformBuffer : m_MeshTransformBuffers)
			delete[] transformBuffer.Data;
	}

	void SceneRendererLite::SetScene(Ref<Scene> scene)
	{
		IR_VERIFY(!m_Active, "Not able to change scenes while active renderer");
		m_Scene = scene;
	}

	void SceneRendererLite::SetViewportSize(uint32_t width, uint32_t height)
	{
		// Set the render scale here instead of individually for all framebuffers...
		width = static_cast<uint32_t>(width);
		height = static_cast<uint32_t>(height);

		if (m_ViewportWidth == width && m_ViewportHeight == height)
			return;

		m_ViewportWidth = width;
		m_ViewportHeight = height;
		m_InverseViewportWidth = 1.0f / m_ViewportWidth;
		m_InverseViewportHeight = 1.0f / m_ViewportHeight;
		m_NeedsResize = true; // Resize all framebuffers...
	}

	void SceneRendererLite::BeginScene(const SceneRendererCamera& camera)
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

		m_GeometryPass->SetInput("u_RadianceMap", m_SceneInfo.SceneEnvironment->RadianceMap);
		m_GeometryPass->SetInput("u_IrradianceMap", m_SceneInfo.SceneEnvironment->IrradianceMap);

		m_DoubleSidedGeometryPass->SetInput("u_RadianceMap", m_SceneInfo.SceneEnvironment->RadianceMap);
		m_DoubleSidedGeometryPass->SetInput("u_IrradianceMap", m_SceneInfo.SceneEnvironment->IrradianceMap);

		// Resize resources if needed
		if (m_NeedsResize)
		{
			m_NeedsResize = false;

			// PreDepth and Geometry framebuffers need to be resized first since other framebuffers reference images in them
			m_PreDepthPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
			m_GeometryPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
			m_SkyboxPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
			m_CompositePass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);

			m_CompositingFramebuffer->Resize(m_ViewportWidth, m_ViewportHeight);

			m_DoubleSidedPreDepthPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
			m_DoubleSidedGeometryPass->GetTargetFramebuffer()->Resize(m_ViewportWidth, m_ViewportHeight);

			const glm::uvec2 viewportSize = { m_ViewportWidth, m_ViewportHeight };

			// Bloom
			{
				glm::uvec2 bloomSize = (viewportSize + 1u) / 2u;
				bloomSize += IR_BLOOM_COMPUTE_WORKGROUP_SIZE - bloomSize % IR_BLOOM_COMPUTE_WORKGROUP_SIZE;

				m_BloomTextures[0].Texture->Resize(bloomSize.x, bloomSize.y);
				m_BloomTextures[1].Texture->Resize(bloomSize.x, bloomSize.y);
				m_BloomTextures[2].Texture->Resize(bloomSize.x, bloomSize.y);

				// Image Views (per-mip)
				ImageViewSpecification imageViewSpec;
				for (uint32_t i = 0; i < 3; i++)
				{
					uint32_t mipCount = m_BloomTextures[i].Texture->GetMipLevelCount();
					m_BloomTextures[i].ImageViews.resize(mipCount);
					for (uint32_t mip = 0; mip < mipCount; mip++)
					{
						imageViewSpec.DebugName = fmt::format("BloomImageView-({} - {})", i, mip);
						imageViewSpec.Image = m_BloomTextures[i].Texture;
						imageViewSpec.Mip = mip;
						m_BloomTextures[i].ImageViews[mip] = ImageView::Create(imageViewSpec);
					}
				}

				// Re-setup materials with new image views
				CreateBloomPassMaterials();
			}
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

			Ref<SceneRendererLite> instance = this;
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
			screenData.InverseHalfResolution = { m_InverseViewportWidth * 2.0f, m_InverseViewportHeight * 2.0f };

			Ref<SceneRendererLite> instance = this;
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
			sceneData.Light.LightSize = directionalLight.LightSize;

			sceneData.CameraPosition = m_CameraDataUB.InverseViewMatrix[3];
			sceneData.EnvironmentMapIntensity = m_SceneInfo.SceneEnvironmentIntensity; // Mirrors the value that is sent to the Skybox shader

			Ref<SceneRendererLite> instance = this;
			Renderer::Submit([instance, sceneData]() mutable
			{
				instance->m_UBSSceneData->RT_Get()->RT_SetData(&sceneData, sizeof(UBScene));
			});
		}
	}

	void SceneRendererLite::EndScene()
	{
		IR_VERIFY(m_Active);

		FlushDrawList();

		m_Active = false;
	}

	void SceneRendererLite::SubmitStaticMesh(Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource, Ref<MaterialTable> materialTable, const glm::mat4& transform, Ref<Material> overrideMaterial)
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
		}
	}

	Ref<Texture2D> SceneRendererLite::GetFinalPassImage()
	{
		if (!m_ResourcesCreated)
			return nullptr;

		return m_CompositePass->GetOutput(0);
	}

	void SceneRendererLite::ResetImageLayouts()
	{
		Ref<SceneRendererLite> instance = this;
		Renderer::Submit([instance]()
		{
			// PreDepth image
			Renderer::InsertImageMemoryBarrier(
				instance->m_CommandBuffer->GetActiveCommandBuffer(),
				instance->m_PreDepthPass->GetDepthOutput()->GetVulkanImage(),
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
				instance->m_CommandBuffer->GetActiveCommandBuffer(),
				instance->m_GeometryPass->GetOutput(0)->GetVulkanImage(),
				0,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
			);

			// Composite color image
			Renderer::InsertImageMemoryBarrier(
				instance->m_CommandBuffer->GetActiveCommandBuffer(),
				instance->m_CompositePass->GetOutput(0)->GetVulkanImage(),
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

	void SceneRendererLite::FlushDrawList()
	{
		if (m_ResourcesCreated && m_ViewportWidth > 0 && m_ViewportHeight > 0)
		{
			PreRender();

			m_CommandBuffer->Begin();

			ResetImageLayouts();
			PreDepthPass();
			GeometryPass();
			SkyboxPass();

			// From now on the main geometry attachment is read only since it is used for post processing
			Ref<SceneRendererLite> instance = this;
			Renderer::Submit([instance]()
			{
				Renderer::InsertImageMemoryBarrier(
					instance->m_CommandBuffer->GetActiveCommandBuffer(),
					instance->m_GeometryPass->GetOutput(0)->GetVulkanImage(),
					VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
				);
			});

			if (m_Options.BloomEnabled)
				BloomPass();

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

		m_StaticMeshDrawList.clear();
		m_DoubleSidedStaticMeshDrawList.clear();

		m_SceneInfo = {};

		// Clear transform map so that we fill it again for the next frame and we dont have duplicated transforms...
		m_MeshTransformMap.clear();
	}

	void SceneRendererLite::PreRender()
	{
		// Fill instance vertex buffer with instance transform data...

		const uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

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

	void SceneRendererLite::ClearPass()
	{
		Ref<SceneRendererLite> instance = this;
		Renderer::Submit([instance]()
		{
			// PreDepth image
			Renderer::InsertImageMemoryBarrier(
				instance->m_CommandBuffer->GetActiveCommandBuffer(),
				instance->m_PreDepthPass->GetDepthOutput()->GetVulkanImage(),
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
				instance->m_CommandBuffer->GetActiveCommandBuffer(),
				instance->m_CompositePass->GetOutput(0)->GetVulkanImage(),
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

	void SceneRendererLite::PreDepthPass()
	{
		// Render all objects into a depth texture only and use that in all other passes that need depth

		const uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		Renderer::BeginRenderPass(m_CommandBuffer, m_PreDepthPass);

		for (const auto& [mk, dc] : m_StaticMeshDrawList)
		{
			const auto& transformData = m_MeshTransformMap.at(mk);
			glm::mat4 transform = dc.MeshSource->GetSubMeshes()[dc.SubMeshIndex].Transform;
			Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_PreDepthPass->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, m_PreDepthMaterial, m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset, dc.InstanceCount);
		}

		Renderer::EndRenderPass(m_CommandBuffer);

		if (m_DoubleSidedStaticMeshDrawList.size())
		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_DoubleSidedPreDepthPass);

			for (const auto& [mk, dc] : m_DoubleSidedStaticMeshDrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				glm::mat4 transform = dc.MeshSource->GetSubMeshes()[dc.SubMeshIndex].Transform;
				Renderer::RenderStaticMeshWithMaterial(m_CommandBuffer, m_DoubleSidedPreDepthPass->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, m_PreDepthMaterial, m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset, dc.InstanceCount);
			}

			Renderer::EndRenderPass(m_CommandBuffer);
		}
	}

	void SceneRendererLite::GeometryPass()
	{
		const uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		Renderer::BeginRenderPass(m_CommandBuffer, m_GeometryPass);

		for (const auto& [mk, dc] : m_StaticMeshDrawList)
		{
			const auto& transformData = m_MeshTransformMap.at(mk);
			Renderer::RenderStaticMesh(m_CommandBuffer, m_GeometryPass->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.StaticMesh->GetMaterials(), m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset, dc.InstanceCount);
		}

		Renderer::EndRenderPass(m_CommandBuffer);

		if (m_DoubleSidedStaticMeshDrawList.size())
		{
			Renderer::BeginRenderPass(m_CommandBuffer, m_DoubleSidedGeometryPass);

			for (const auto& [mk, dc] : m_DoubleSidedStaticMeshDrawList)
			{
				const auto& transformData = m_MeshTransformMap.at(mk);
				Renderer::RenderStaticMesh(m_CommandBuffer, m_DoubleSidedGeometryPass->GetPipeline(), dc.StaticMesh, dc.MeshSource, dc.SubMeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.StaticMesh->GetMaterials(), m_MeshTransformBuffers[frameIndex].VertexBuffer, transformData.TransformOffset, dc.InstanceCount);
			}

			Renderer::EndRenderPass(m_CommandBuffer);
		}
	}

	void SceneRendererLite::SkyboxPass()
	{
		m_SkyboxMaterial->Set("u_Uniforms.TextureLod", m_SceneInfo.SkyboxLod);
		m_SkyboxMaterial->Set("u_Uniforms.Intensity", m_SceneInfo.SceneEnvironmentIntensity);

		const Ref<TextureCube> radianceMap = m_SceneInfo.SceneEnvironment ? m_SceneInfo.SceneEnvironment->RadianceMap : Renderer::GetBlackCubeTexture();
		m_SkyboxMaterial->Set("u_Texture", radianceMap);

		Renderer::BeginRenderPass(m_CommandBuffer, m_SkyboxPass);
		Renderer::SubmitFullScreenQuad(m_CommandBuffer, m_SkyboxPass->GetPipeline(), m_SkyboxMaterial);
		Renderer::EndRenderPass(m_CommandBuffer);
	}

	void SceneRendererLite::BloomPass()
	{
		Ref<SceneRendererLite> instance = this;
		glm::uvec3 workGroups{ 0 };

		struct BloomComputePushConstants
		{
			glm::vec4 Params;
			float UpsampleScale = 1.0f;
			float LOD = 0.0f;
		} bloomComputePushConstants;
		bloomComputePushConstants.Params = { m_Options.BloomFilterThreshold, m_Options.BloomFilterThreshold - m_Options.BloomKnee, m_Options.BloomKnee * 2.0f, 0.25f / m_Options.BloomKnee };
		bloomComputePushConstants.UpsampleScale = m_Options.BloomUpsampleScale;

		const uint32_t mips = m_BloomTextures[0].Texture->GetMipLevelCount() - 2;

		// PreFilter
		{
			Renderer::BeginComputePass(m_CommandBuffer, m_BloomPreFilterPass);

			workGroups = { m_BloomTextures[0].Texture->GetWidth() / IR_BLOOM_COMPUTE_WORKGROUP_SIZE, m_BloomTextures[0].Texture->GetHeight() / IR_BLOOM_COMPUTE_WORKGROUP_SIZE, 1 };
			Renderer::DispatchComputePass(m_CommandBuffer, m_BloomPreFilterPass, m_BloomMaterials.PreFilterMaterial, workGroups, Buffer(reinterpret_cast<const uint8_t*>(&bloomComputePushConstants), sizeof(bloomComputePushConstants)));

			Renderer::Submit([instance]()
			{
				Renderer::InsertImageMemoryBarrier(
					instance->m_CommandBuffer->GetActiveCommandBuffer(),
					instance->m_BloomTextures[0].Texture->GetVulkanImage(),
					VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
					VK_IMAGE_LAYOUT_GENERAL,
					VK_IMAGE_LAYOUT_GENERAL,
					VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = instance->m_BloomTextures[0].Texture->GetMipLevelCount(), .baseArrayLayer = 0, .layerCount = 1 }
				);
			});

			Renderer::EndComputePass(m_CommandBuffer, m_BloomPreFilterPass);
		}

		// DownSample
		{
			Renderer::BeginComputePass(m_CommandBuffer, m_BloomDownSamplePass);

			for (uint32_t i = 1; i < mips; i++)
			{
				const glm::ivec2 mipSize = m_BloomTextures[0].Texture->GetMipSize(i);
				workGroups = { static_cast<uint32_t>(glm::ceil(static_cast<float>(mipSize.x) / static_cast<float>(IR_BLOOM_COMPUTE_WORKGROUP_SIZE))) ,static_cast<uint32_t>(glm::ceil(static_cast<float>(mipSize.y) / static_cast<float>(IR_BLOOM_COMPUTE_WORKGROUP_SIZE))), 1 };

				// DownsampleA reads from m_BloomTextures[0] and Writes into m_BloomTextures[1];
				bloomComputePushConstants.LOD = i - 1.0f;
				Renderer::DispatchComputePass(m_CommandBuffer, m_BloomDownSamplePass, m_BloomMaterials.DownSampleAMaterials[i], workGroups, Buffer(reinterpret_cast<const uint8_t*>(&bloomComputePushConstants), sizeof(bloomComputePushConstants)));

				Renderer::Submit([instance]()
				{
					// To Write
					Renderer::InsertImageMemoryBarrier(
						instance->m_CommandBuffer->GetActiveCommandBuffer(),
						instance->m_BloomTextures[0].Texture->GetVulkanImage(),
						VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
						VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
						VK_IMAGE_LAYOUT_GENERAL,
						VK_IMAGE_LAYOUT_GENERAL,
						VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = instance->m_BloomTextures[0].Texture->GetMipLevelCount(), .baseArrayLayer = 0, .layerCount = 1 }
					);

					// To Read
					Renderer::InsertImageMemoryBarrier(
						instance->m_CommandBuffer->GetActiveCommandBuffer(),
						instance->m_BloomTextures[1].Texture->GetVulkanImage(),
						VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
						VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
						VK_IMAGE_LAYOUT_GENERAL,
						VK_IMAGE_LAYOUT_GENERAL,
						VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = instance->m_BloomTextures[1].Texture->GetMipLevelCount(), .baseArrayLayer = 0, .layerCount = 1 }
					);
				});

				// DownsampleB reads from m_BloomTextures[1] and Writes into m_BloomTextures[0];
				bloomComputePushConstants.LOD = static_cast<float>(i);
				Renderer::DispatchComputePass(m_CommandBuffer, m_BloomDownSamplePass, m_BloomMaterials.DownSampleBMaterials[i], workGroups, Buffer(reinterpret_cast<const uint8_t*>(&bloomComputePushConstants), sizeof(bloomComputePushConstants)));

				Renderer::Submit([instance]()
				{
					Renderer::InsertImageMemoryBarrier(
						instance->m_CommandBuffer->GetActiveCommandBuffer(),
						instance->m_BloomTextures[0].Texture->GetVulkanImage(),
						VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
						VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
						VK_IMAGE_LAYOUT_GENERAL,
						VK_IMAGE_LAYOUT_GENERAL,
						VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = instance->m_BloomTextures[0].Texture->GetMipLevelCount(), .baseArrayLayer = 0, .layerCount = 1 }
					);

					Renderer::InsertImageMemoryBarrier(
						instance->m_CommandBuffer->GetActiveCommandBuffer(),
						instance->m_BloomTextures[1].Texture->GetVulkanImage(),
						VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
						VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
						VK_IMAGE_LAYOUT_GENERAL,
						VK_IMAGE_LAYOUT_GENERAL,
						VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = instance->m_BloomTextures[1].Texture->GetMipLevelCount(), .baseArrayLayer = 0, .layerCount = 1 }
					);
				});
			}

			Renderer::EndComputePass(m_CommandBuffer, m_BloomDownSamplePass);
		}

		// FirstUpSample
		{
			Renderer::BeginComputePass(m_CommandBuffer, m_BloomFirstUpSamplePass);

			bloomComputePushConstants.LOD--;
			const glm::ivec2 mipSize = m_BloomTextures[2].Texture->GetMipSize(mips - 2);
			workGroups.x = static_cast<uint32_t>(glm::ceil(static_cast<float>(mipSize.x) / static_cast<float>(IR_BLOOM_COMPUTE_WORKGROUP_SIZE)));
			workGroups.y = static_cast<uint32_t>(glm::ceil(static_cast<float>(mipSize.y) / static_cast<float>(IR_BLOOM_COMPUTE_WORKGROUP_SIZE)));

			Renderer::DispatchComputePass(m_CommandBuffer, m_BloomFirstUpSamplePass, m_BloomMaterials.FirstUpSampleMaterial, workGroups, Buffer(reinterpret_cast<const uint8_t*>(&bloomComputePushConstants), sizeof(bloomComputePushConstants)));

			Renderer::Submit([instance]()
			{
				Renderer::InsertImageMemoryBarrier(
					instance->m_CommandBuffer->GetActiveCommandBuffer(),
					instance->m_BloomTextures[2].Texture->GetVulkanImage(),
					VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
					VK_IMAGE_LAYOUT_GENERAL,
					VK_IMAGE_LAYOUT_GENERAL,
					VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = instance->m_BloomTextures[2].Texture->GetMipLevelCount(), .baseArrayLayer = 0, .layerCount = 1 }
				);
			});

			Renderer::EndComputePass(m_CommandBuffer, m_BloomFirstUpSamplePass);
		}

		// UpSample
		{
			Renderer::BeginComputePass(m_CommandBuffer, m_BloomUpSamplePass);

			for (int32_t mip = mips - 3; mip >= 0; mip--)
			{
				const glm::ivec2 mipSize = m_BloomTextures[2].Texture->GetMipSize(mip);
				workGroups.x = static_cast<uint32_t>(glm::ceil(static_cast<float>(mipSize.x) / static_cast<float>(IR_BLOOM_COMPUTE_WORKGROUP_SIZE)));
				workGroups.y = static_cast<uint32_t>(glm::ceil(static_cast<float>(mipSize.y) / static_cast<float>(IR_BLOOM_COMPUTE_WORKGROUP_SIZE)));

				bloomComputePushConstants.LOD = static_cast<float>(mip);
				Renderer::DispatchComputePass(m_CommandBuffer, m_BloomUpSamplePass, m_BloomMaterials.UpSampleMaterials[mip], workGroups, Buffer(reinterpret_cast<const uint8_t*>(&bloomComputePushConstants), sizeof(bloomComputePushConstants)));

				Renderer::Submit([instance, mip]()
				{
					Renderer::InsertImageMemoryBarrier(
						instance->m_CommandBuffer->GetActiveCommandBuffer(),
						instance->m_BloomTextures[2].Texture->GetVulkanImage(),
						VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
						VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
						VK_IMAGE_LAYOUT_GENERAL,
						VK_IMAGE_LAYOUT_GENERAL,
						VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
						{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = instance->m_BloomTextures[2].Texture->GetMipLevelCount(), .baseArrayLayer = 0, .layerCount = 1 }
					);
				});
			}

			Renderer::EndComputePass(m_CommandBuffer, m_BloomUpSamplePass);
		}
	}

	void SceneRendererLite::CompositePass()
	{
		// Composite the final scene image and apply gamma correction and tone mapping

		Ref<SceneRendererLite> instance = this;
		const uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		const float exposure = m_SceneInfo.Camera.Camera.GetExposure();

		// We set this every frame so that we can always set the proper dirt texture in case it was changed
		m_CompositePass->SetInput("u_BloomDirtTexture", m_BloomDirtTexture);

		m_CompositeMaterial->Set("u_Uniforms.Exposure", exposure);
		if (m_Options.BloomEnabled)
		{
			m_CompositeMaterial->Set("u_Uniforms.BloomIntensity", m_Options.BloomIntensity);
			m_CompositeMaterial->Set("u_Uniforms.BloomDirtIntensity", m_Options.BloomDirtIntensity);
			m_CompositeMaterial->Set("u_Uniforms.BloomUpsampleScale", m_Options.BloomUpsampleScale);
		}
		else
		{
			m_CompositeMaterial->Set("u_Uniforms.BloomIntensity", 0.0f);
			m_CompositeMaterial->Set("u_Uniforms.BloomDirtIntensity", 0.0f);
			m_CompositeMaterial->Set("u_Uniforms.BloomUpsampleScale", 0.5f);
		}
		m_CompositeMaterial->Set("u_Uniforms.Opacity", m_Opacity);
		m_CompositeMaterial->Set("u_Uniforms.Time", Application::Get().GetTime());

		Renderer::BeginRenderPass(m_CommandBuffer, m_CompositePass);
		Renderer::SubmitFullScreenQuad(m_CommandBuffer, m_CompositePass->GetPipeline(), m_CompositeMaterial);
		Renderer::EndRenderPass(m_CommandBuffer);

		if (m_Options.ShowGrid)
		{
			m_GridMaterial->Set("u_Uniforms.Scale", EditorSettings::Get().TranslationSnapValue);
			Renderer::BeginRenderPass(m_CommandBuffer, m_GridPass);
			Renderer::SubmitFullScreenQuad(m_CommandBuffer, m_GridPass->GetPipeline(), m_GridMaterial);
			Renderer::EndRenderPass(m_CommandBuffer);
		}
	}

	void SceneRendererLite::CreateBloomPassMaterials()
	{
		Ref<Texture2D> inputImage = m_GeometryPass->GetOutput(0);

		// Prefilter
		m_BloomMaterials.PreFilterMaterial = Material::Create(Renderer::GetShadersLibrary()->Get("Bloom-Prefilter"), "Bloom-Prefilter");
		m_BloomMaterials.PreFilterMaterial->Set("o_Image", m_BloomTextures[0].ImageViews[0]);
		m_BloomMaterials.PreFilterMaterial->Set("u_Texture", inputImage);

		// Downsample
		const uint32_t mips = m_BloomTextures[0].Texture->GetMipLevelCount() - 2;
		m_BloomMaterials.DownSampleAMaterials.resize(mips);
		m_BloomMaterials.DownSampleBMaterials.resize(mips);
		for (uint32_t i = 0; i < mips; i++)
		{
			m_BloomMaterials.DownSampleAMaterials[i] = Material::Create(Renderer::GetShadersLibrary()->Get("Bloom-Downsample"), "Bloom-DownsampleA");
			m_BloomMaterials.DownSampleAMaterials[i]->Set("o_Image", m_BloomTextures[1].ImageViews[i]);
			m_BloomMaterials.DownSampleAMaterials[i]->Set("u_Texture", m_BloomTextures[0].Texture);

			m_BloomMaterials.DownSampleBMaterials[i] = Material::Create(Renderer::GetShadersLibrary()->Get("Bloom-Downsample"), "Bloom-DownsampleB");
			m_BloomMaterials.DownSampleBMaterials[i]->Set("o_Image", m_BloomTextures[0].ImageViews[i]);
			m_BloomMaterials.DownSampleBMaterials[i]->Set("u_Texture", m_BloomTextures[1].Texture);
		}

		// First Upsample
		m_BloomMaterials.FirstUpSampleMaterial = Material::Create(Renderer::GetShadersLibrary()->Get("Bloom-FirstUpsample"), "Bloom-FirstUpsample");
		m_BloomMaterials.FirstUpSampleMaterial->Set("o_Image", m_BloomTextures[2].ImageViews[mips - 2]);
		m_BloomMaterials.FirstUpSampleMaterial->Set("u_Texture", m_BloomTextures[0].Texture);

		// Upsample
		m_BloomMaterials.UpSampleMaterials.resize(mips - 3 + 1);
		for (int32_t mip = mips - 3; mip >= 0; mip--)
		{
			m_BloomMaterials.UpSampleMaterials[mip] = Material::Create(Renderer::GetShadersLibrary()->Get("Bloom-Upsample"), "Bloom-Upsample");
			m_BloomMaterials.UpSampleMaterials[mip]->Set("o_Image", m_BloomTextures[2].ImageViews[mip]);
			m_BloomMaterials.UpSampleMaterials[mip]->Set("u_Texture", m_BloomTextures[0].Texture);
			m_BloomMaterials.UpSampleMaterials[mip]->Set("u_BloomTexture", m_BloomTextures[2].Texture);
		}
	}

}