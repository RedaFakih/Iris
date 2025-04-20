#pragma once

#include "Core/Base.h"
#include "SceneRenderer.h"

namespace Iris {

	// TODO: This will also have to do its own PrepareFinalImagesForPresentation() since it wont have 2D rendering

	class SceneRendererLite : public RefCountedObject
	{
	public:
		SceneRendererLite(Ref<Scene> scene, const SceneRendererSpecification& spec = SceneRendererSpecification());
		~SceneRendererLite();

		[[nodiscard]] inline static Ref<SceneRendererLite> Create(Ref<Scene> scene, const SceneRendererSpecification& spec = SceneRendererSpecification())
		{
			return CreateRef<SceneRendererLite>(scene, spec);
		}

		void Init();
		void Shutdown();

		void SetScene(Ref<Scene> scene);

		void SetViewportSize(uint32_t width, uint32_t height);

		void BeginScene(const SceneRendererCamera& camera);
		void EndScene();

		void SubmitStaticMesh(Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource, Ref<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), Ref<Material> overrideMaterial = nullptr);

		Ref<Texture2D> GetFinalPassImage();

		uint32_t GetViewportWidth() const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }

		void SetOpacity(float opacity) { m_Opacity = opacity; }
		float GetOpacity() const { return m_Opacity; }
		float& GetOpacity() { return m_Opacity; }

		SceneRendererOptions& GetOptions() { return m_Options; }
		const SceneRendererOptions& GetOptions() const { return m_Options; }

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

	private:
		void ResetImageLayouts();
		void FlushDrawList();

		// For filling the instance buffer transform data
		void PreRender();
		void ClearPass();

		void PreDepthPass();
		void GeometryPass();
		void SkyboxPass();
		void BloomPass();
		void CompositePass();

		void CreateBloomPassMaterials();

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
		SceneRendererOptions m_Options;

		Ref<RenderCommandBuffer> m_CommandBuffer;

		// No lighting info, only Ambient
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

		// PreDepth
		Ref<RenderPass> m_PreDepthPass;
		Ref<RenderPass> m_DoubleSidedPreDepthPass;
		Ref<Material> m_PreDepthMaterial;

		// Skybox
		Ref<RenderPass> m_SkyboxPass;
		Ref<Material> m_SkyboxMaterial;

		// Geometry
		Ref<RenderPass> m_GeometryPass;
		Ref<RenderPass> m_DoubleSidedGeometryPass;

		// Grid
		Ref<RenderPass> m_GridPass;
		Ref<Material> m_GridMaterial;

		// Composite
		Ref<RenderPass> m_CompositePass;
		Ref<Material> m_CompositeMaterial;

		// For external compositing
		Ref<Framebuffer> m_CompositingFramebuffer;

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

		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;

		float m_InverseViewportWidth = 0.0f;
		float m_InverseViewportHeight = 0.0f;

		bool m_NeedsResize = false;
		bool m_Active = false;
		bool m_ResourcesCreated = false;
		bool m_ResourcesCreatedGPU = false;

		float m_Opacity = 1.0f;

	};

}