#pragma once

#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/RenderPass.h"
#include "Scene/Scene.h"

namespace Iris {

	/*
	 * So what do we want to do here?
	 *	- Build draw lists separated by the type of submitted object (static mesh, dynamic mesh, etc...)
	 *	- Flush draw list at the end with all the different passes that we will undergo
	 *  - Handle uniform buffer updating
	 *  - Handle Pipeline/Framebuffer/Images invalidation in case of screen resize
	 *  - Keep track of viewport size...
	 * 
	 * NOTE: If we are planning to create multiple instances of the SceneRenderer then we CAN NOT release shader modules since the modules will be needed for the other instances' pipelines to be loaded and initialized
	 */

#define IR_BLOOM_COMPUTE_WORKGROUP_SIZE 4u
#define IR_LIGHT_CULLING_WORKGROUP_SIZE 8u

	struct SceneRendererCamera
	{
		Camera Camera;
		glm::mat4 ViewMatrix;
		float Near, Far; // Non-reversed (they are reversed originally so that z buffer works)
		float FOV;
	};

	enum class ShadowResolutionSetting
	{
		None = 0,
		Low,
		Medium,
		High
	};

	struct SceneRendererOptions
	{
		// General
		bool ShowGrid = true;
		bool ShowSelectedInWireFrame = false;

		enum class PhysicsColliderViewOptions { SelectedEntity = 0, All };
		PhysicsColliderViewOptions PhysicsColliderViewMode = PhysicsColliderViewOptions::SelectedEntity;
		bool ShowPhysicsColliders = false;

		// Bloom
		bool BloomEnabled = true;
		float BloomFilterThreshold = 1.0f;
		float BloomIntensity = 1.0f;
		float BloomKnee = 0.1f;
		float BloomUpsampleScale = 1.0f;
		float BloomDirtIntensity = 1.0f;

		// DOF
		bool DOFEnabled = false;
		float DOFFocusDistance = 0.0f;
		float DOFBlurSize = 1.0f;
	};

	struct SceneRendererSpecification
	{
		float RendererScale = 1.0f;
		bool JumpFloodPass = true;

		ShadowResolutionSetting ShadowResolution = ShadowResolutionSetting::High;
		uint32_t NumberOfShadowCascades = 4;

		// Means viewport window size
		uint32_t ViewportWidth = 0;
		uint32_t ViewportHeight = 0;
	};

	class SceneRenderer : public RefCountedObject
	{
	public:
		struct Statistics
		{
			uint32_t TotalDrawCalls = 0;

			uint32_t ColorPassDrawCalls = 0;
			uint32_t Meshes = 0;
			uint32_t Instances = 0;
			uint32_t ColorPassSavedDraws = 0;
		};

		enum class ViewMode
		{
			Lit, Unlit, Wireframe
		};

	public:
		SceneRenderer(Ref<Scene> scene, const SceneRendererSpecification& spec = SceneRendererSpecification());
		~SceneRenderer();

		[[nodiscard]] inline static Ref<SceneRenderer> Create(Ref<Scene> scene, const SceneRendererSpecification& spec = SceneRendererSpecification())
		{
			return CreateRef<SceneRenderer>(scene, spec);
		}

		void Init();
		void Shutdown();

		void SetScene(Ref<Scene> scene);

		void SetViewportSize(uint32_t width, uint32_t height, float renderScale);

		void BeginScene(const SceneRendererCamera& camera);
		void EndScene();

		void SubmitStaticMesh(Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource, Ref<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), Ref<Material> overrideMaterial = nullptr);
		void SubmitSelectedStaticMesh(Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource, Ref<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), Ref<Material> overrideMaterial = nullptr);

		void SubmitPhysicsStaticDebugMesh(Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource, const glm::mat4& transform = glm::mat4(1.0f), const bool isSimpleCollider = true);

		Ref<Texture2D> GetFinalPassImage();

		Ref<Framebuffer> GetExternalCompositeFramebuffer() { return m_CompositingFramebuffer; }

		Ref<Renderer2D> GetRenderer2D() { return m_Renderer2D; }

		void SetViewMode(ViewMode mode) { m_ViewMode = mode; }

		void SetLineWidth(float lineWidth);
		float GetLineWidth() const { return m_LineWidth; }

		uint32_t GetViewportWidth() const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }

		void SetOpacity(float opacity) { m_Opacity = opacity; }
		float GetOpacity() const { return m_Opacity; }
		float& GetOpacity() { return m_Opacity; }

		const Statistics& GetStatistics() const { return m_Statistics; }
		SceneRendererOptions& GetOptions() { return m_Options; }
		const SceneRendererOptions& GetOptions() const { return m_Options; }
		SceneRendererSpecification& GetSpecification() { return m_Specification; }
		const SceneRendererSpecification& GetSpecification() const { return m_Specification; }
		const PipelineStatistics& GetPipelineStatistics() const;

		bool IsReady() const { return m_ResourcesCreatedGPU; }

	private:
		struct MeshKey
		{
			AssetHandle MeshHandle;
			AssetHandle MaterialHandle;
			uint32_t SubMeshIndex;
			bool IsSelected;

			MeshKey(AssetHandle meshHandle, AssetHandle materialAsset, uint32_t subMeshIndex, bool isSelected)
				: MeshHandle(meshHandle), MaterialHandle(materialAsset), SubMeshIndex(subMeshIndex), IsSelected(isSelected)
			{
			}

			bool operator<(const MeshKey& other) const
			{
				if (MeshHandle < other.MeshHandle)
					return true;

				if (MeshHandle > other.MeshHandle)
					return false;

				if (SubMeshIndex < other.SubMeshIndex)
					return true;

				if (SubMeshIndex > other.SubMeshIndex)
					return false;

				if (MaterialHandle < other.MaterialHandle)
					return true;

				if (MaterialHandle > other.MaterialHandle)
					return false;

				return IsSelected < other.IsSelected;
			}
		};

		struct CascadeData
		{
			glm::mat4 ViewProjMatrix;
			glm::mat4 ViewMatrix;
			float SplitDepth;
		};

	private:
		void ResetImageLayouts();
		void FlushDrawList();

		// For filling the instance buffer transform data
		void PreRender();
		void ClearPass();

		void DirectionalShadowPass();
		void PreDepthPass();
		void LightCullingPass();
		void GeometryPass();
		void SkyboxPass();
		void JumpFloodPass();
		void BloomPass();
		void CompositePass();

		void CreateBloomPassMaterials();
		void CalculateCascades(const SceneRendererCamera& sceneCamera, const glm::vec3& lightDirection, CascadeData* cascades) const;
		void CopyFromDOFImage();

		void UpdateStatistics();

	private:
		// Shader structs
		struct DirLight
		{
			glm::vec4 Direction; // Alpha channel is the ShadowOpacity
			glm::vec4 Radiance; // Alpha channel is the Intensity
			float LightSize;
			glm::vec3 Padding0{};
		};

	private:
		Ref<Scene> m_Scene;
		SceneRendererSpecification m_Specification;
		SceneRendererOptions m_Options;

		Ref<RenderCommandBuffer> m_CommandBuffer;

		Ref<Renderer2D> m_Renderer2D;

		struct SceneInfo
		{
			SceneRendererCamera Camera;

			Ref<Environment> SceneEnvironment;
			float SceneEnvironmentIntensity = 1.0f;
			float SkyboxLod = 0.0f;

			LightEnvironment SceneLightEnvironment;
		} m_SceneInfo;

		// (set = 1, binding = 0)
		struct UBCamera
		{
			glm::mat4 ViewProjectionMatrix;
			glm::mat4 InverseViewProjectionMatrix;
			glm::mat4 ProjectionMatrix; // With Near and Far inverted
			glm::mat4 InverseProjectionMatrix;
			glm::mat4 ViewMatrix;
			glm::mat4 InverseViewMatrix;
			glm::vec2 DepthUnpackConsts;
		} m_CameraDataUB;
		Ref<UniformBufferSet> m_UBSCamera;

		// (set = 1, binding = 1)
		struct UBScreenData
		{
			glm::vec2 FullResolution;
			glm::vec2 InverseFullResolution;
			glm::vec2 HalfResolution;
			glm::vec2 InverseHalfResolution;
		} m_ScreenDataUB;
		Ref<UniformBufferSet> m_UBSScreenData;

		// (set = 1, binding = 2)
		struct UBScene
		{
			DirLight Light;
			glm::vec3 CameraPosition;
			float EnvironmentMapIntensity = 1.0f;
		} m_SceneDataUB;
		Ref<UniformBufferSet> m_UBSSceneData;

		// (set = 1, binding = 3)
		struct UBDirectionalShadowData
		{
			glm::mat4 DirectionalLightMatrices[4]; // View Projection Matrices
		} m_DirectionalShadowDataUB;
		Ref<UniformBufferSet> m_UBSDirectionalShadowData;

		// (set = 1, binding = 4)
		struct UBPointLights
		{
			uint32_t Count = 0;
			glm::vec3 padding{};
			ScenePointLight PointLights[100]{};
		} m_PointLightsUB;
		Ref<UniformBufferSet> m_UBSPointLights;

		// (set = 1, binding = 5)
		Ref<StorageBufferSet> m_SBSVisiblePointLightIndicesBuffer;
		
		// (set = 1, binding = 6)
		struct UBSpotLights
		{
			uint32_t Count = 0;
			glm::vec3 Padding{};
			SceneSpotLight SpotLights[100]{};
		} m_SpotLightsUB;
		Ref<UniformBufferSet> m_UBSSpotLights;

		// (set = 1, binding = 7)
		Ref<StorageBufferSet> m_SBSVisibleSpotLightIndicesBuffer;

		// (set = 1, binding = 8)
		struct UBRendererData
		{
			glm::vec4 CascadeSplits;			// size 16, 0ffset 0
			char Padding0[4] = { 0, 0, 0, 0 };  
			float MaxShadowDistance = 300.0f;	// size 4, offset 20
			float ShadowFade = 1.0f;			// size 4, offset 24
			float CascadeTransitionFade = 1.0f; // size 4, offset 28
			uint32_t TilesCountX = 0;			// size 4, offset 32
			bool CascadeFading = true;			// size 4, offset 36
			char Padding1[3] = { 0, 0, 0 };
			bool SoftShadows = true;			// size 4, offset 40
			char Padding2[3] = { 0, 0, 0 };
			bool ShowCascades = false;			// size 4, offset 44
			char Padding3[3] = { 0, 0, 0 };
			bool ShowLightComplexity = false;	// size 4, offset 48
			char Padding4[3] = { 0, 0, 0 };
			bool Unlit = false;					// size 4, offset 52
			char Padding5[3] = { 0, 0, 0 };
		} m_RendererDataUB;						// Total Size: 56 bytes
		Ref<UniformBufferSet> m_UBSRendererData;

		// Directional Shadow (No material required since it only runs a vertex shader)
		std::vector<Ref<RenderPass>> m_DirectionalShadowPasses;
		Ref<Material> m_DirectionalShadowMaterial;
		std::vector<Ref<ImageView>> m_DirShadowImagePerLayerViews; // For cascaded shadow mapping

		// Cascade settings
		glm::vec4 m_CascadeSplits = {};
		float m_CascadeSplitLambda = 0.92f; // Variable
		float m_ScaleShadowCascadesToOrigin = 0.0f; // Variable
		float m_CascadeNearPlaneOffset = -50.0f; // Variable
		float m_CascadeFarPlaneOffset = 50.0f; // Variable

		// PreDepth
		Ref<RenderPass> m_PreDepthPass;
		Ref<RenderPass> m_DoubleSidedPreDepthPass;
		Ref<RenderPass> m_WireframeViewPreDepthPass; // For having wireframe view in the future
		Ref<Material> m_PreDepthMaterial;

		// Skybox
		Ref<RenderPass> m_SkyboxPass;
		Ref<Material> m_SkyboxMaterial;

		// Geometry
		Ref<RenderPass> m_GeometryPass;
		Ref<RenderPass> m_DoubleSidedGeometryPass;
		Ref<RenderPass> m_WireframeViewGeometryPass;

		// Selected Geometry
		Ref<RenderPass> m_SelectedGeometryPass;
		Ref<RenderPass> m_DoubleSidedSelectedGeometryPass;
		Ref<Material> m_SelectedGeometryMaterial;

		// TODO: Add wireframe on top pass where there is no depth testing
		// Geometry Wireframe
		Ref<RenderPass> m_GeometryWireFramePass;
		Ref<Material> m_WireFrameMaterial;

		// Grid
		Ref<RenderPass> m_GridPass;
		Ref<Material> m_GridMaterial;

		// Composite
		Ref<RenderPass> m_CompositePass;
		Ref<Material> m_CompositeMaterial;

		// Jump Flood
		Ref<RenderPass> m_JumpFloodInitPass;
		Ref<Material> m_JumpFloodInitMaterial;
		std::array<Ref<RenderPass>, 2> m_JumpFloodPass;
		std::array<Ref<Material>, 2> m_JumpFloodPassMaterial;
		Ref<RenderPass> m_JumpFloodCompositePass;
		Ref<Material> m_JumpFloodCompositeMaterial;
		std::array<Ref<Framebuffer>, 2> m_JumpFloodFramebuffers;

		// Bloom
		Ref<ComputePass> m_BloomPreFilterPass;
		Ref<ComputePass> m_BloomDownSamplePass;
		Ref<ComputePass> m_BloomFirstUpSamplePass;
		Ref<ComputePass> m_BloomUpSamplePass;
		Ref<Texture2D> m_BloomDirtTexture;
		
		struct BloomTextures
		{
			Ref<Texture2D> Texture;
			std::vector<Ref<ImageView>> ImageViews; // Per-mip
		};
		std::array<BloomTextures, 3> m_BloomTextures;

		struct BloomMaterials
		{
			Ref<Material> PreFilterMaterial;
			std::vector<Ref<Material>> DownSampleAMaterials;
			std::vector<Ref<Material>> DownSampleBMaterials;
			Ref<Material> FirstUpSampleMaterial;
			std::vector<Ref<Material>> UpSampleMaterials;
		} m_BloomMaterials;

		// Depth Of Field
		Ref<RenderPass> m_DOFPass;
		Ref<Material> m_DOFMaterial;

		// Light Culling
		Ref<ComputePass> m_LightCullingPass;
		Ref<Material> m_LightCullingMaterial;
		glm::uvec3 m_LightCullingWorkGroups;

		// For external compositing
		Ref<Framebuffer> m_CompositingFramebuffer;

		// For instanced rendering support...
		struct TransformVertexData
		{
			glm::vec4 MatrixRow[3];
		};

		struct TransformBuffer
		{
			Ref<VertexBuffer> VertexBuffer;
			TransformVertexData* Data = nullptr;
		};

		// Per frame-in-flight
		std::vector<TransformBuffer> m_MeshTransformBuffers;

		// Basically whenever there is a `Submit` call we create the transform for that mesh in this map and then at the end, in PreRender we iterate through this map
		// and fill up the m_MeshTransformBuffers for that frame with the transform information and then update the vertex buffer
		struct TransformMapData
		{
			std::vector<TransformVertexData> Transforms;
			uint32_t TransformOffset = 0;
		};
		std::map<MeshKey, TransformMapData> m_MeshTransformMap;

		struct StaticDrawCommand
		{
			Ref<StaticMesh> StaticMesh;
			Ref<MeshSource> MeshSource;
			uint32_t SubMeshIndex;
			Ref<MaterialTable> MaterialTable;
			Ref<Material> OverrideMaterial;

			uint32_t InstanceCount = 0;
			uint32_t InstanceOffset = 0;
		};

		std::map<MeshKey, StaticDrawCommand> m_StaticMeshDrawList;
		std::map<MeshKey, StaticDrawCommand> m_DoubleSidedStaticMeshDrawList;
		std::map<MeshKey, StaticDrawCommand> m_SelectedStaticMeshDrawList;
		std::map<MeshKey, StaticDrawCommand> m_DoubleSidedSelectedStaticMeshDrawList;

		// Shadow Draw Lists
		std::map<MeshKey, StaticDrawCommand> m_StaticMeshShadowDrawList;

		// Debug Physics Draw List
		std::map<MeshKey, StaticDrawCommand> m_StaticColliderDrawList;

		Ref<Material> m_SimpleColliderMaterial;
		Ref<Material> m_ComplexColliderMaterial;

		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;

		float m_InverseViewportWidth = 0.0f;
		float m_InverseViewportHeight = 0.0f;

		bool m_NeedsResize = false;
		bool m_Active = false;
		bool m_ResourcesCreated = false;
		bool m_ResourcesCreatedGPU = false;

		float m_LineWidth = 2.0f;
		float m_Opacity = 1.0f;

		Statistics m_Statistics;

		ViewMode m_ViewMode = ViewMode::Lit;

		friend class SceneRendererPanel;		
	};

}