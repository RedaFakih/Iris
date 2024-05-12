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
	 */

	struct SceneRendererCamera
	{
		Camera Camera;
		glm::mat4 ViewMatrix;
		float Near, Far; // Non-reversed (they are reversed originally so that z buffer works)
		float FOV;
	};

	struct SceneRendererOptions
	{
		bool ShowGrid = true;
		bool ShowSelectedInWireFrame = false;
	};

	struct SceneRendererSpecification
	{
		float RendererScale = 1.0f;
		bool JumpFloodPass = true;

		// Means application winow size
		uint32_t ViewportWidth = 0;
		uint32_t ViewportHeight = 0;
	};

	class SceneRenderer : public RefCountedObject
	{
	public:
		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t Meshes = 0;
			uint32_t Instances = 0;
			uint32_t SavedDraw = 0;
		};

		enum class ViewMode
		{
			Lit, Unlit, Wireframe
		};

	public:
		SceneRenderer(Ref<Scene> scene, const SceneRendererSpecification& spec = SceneRendererSpecification());
		~SceneRenderer();

		[[nodiscard]] static Ref<SceneRenderer> Create(Ref<Scene> scene, const SceneRendererSpecification& spec = SceneRendererSpecification());

		void Init();
		void Shutdown();

		void SetScene(Ref<Scene> scene);

		void SetViewportSize(uint32_t width, uint32_t height, float renderScale);

		void BeginScene(const SceneRendererCamera& camera);
		void EndScene();

		void SubmitStaticMesh(Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource, Ref<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), Ref<Material> overrideMaterial = nullptr);
		void SubmitSelectedStaticMesh(Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource, Ref<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), Ref<Material> overrideMaterial = nullptr);

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

		float GetTranslationSnapValue() const { return m_TranslationSnapValue; }
		void SetTranslationSnapValue(float value) { m_TranslationSnapValue = value; }

		const Statistics& GetStatistics() const { return m_Statistics; }
		SceneRendererOptions& GetOptions() { return m_Options; }
		const SceneRendererOptions& GetOptions() const { return m_Options; }
		SceneRendererSpecification& GetSpecification() { return m_Specification; }
		const SceneRendererSpecification& GetSpecification() const { return m_Specification; }

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
		void FlushDrawList();

		// For filling the instance buffer transform data
		void PreRender();
		void ClearPass();

		void PreDepthPass();
		void GeometryPass();
		void JumpFloodPass();
		void CompositePass();

		void UpdateStatistics();

	private:
		Ref<Scene> m_Scene;
		SceneRendererSpecification m_Specification;
		SceneRendererOptions m_Options;

		Ref<RenderCommandBuffer> m_CommandBuffer;

		Ref<Renderer2D> m_Renderer2D;

		struct SceneInfo
		{
			SceneRendererCamera Camera;

			// TODO: Info about lighting and scene environment
		} m_SceneData;

		struct UBCamera // (set = 1, binding = 0)
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

		struct UBScreenData // (set = 1, binding = 1)
		{
			glm::vec2 FullResolution;
			glm::vec2 InverseFullResolution;
			glm::vec2 HalfResolution;
			glm::vec2 InverseHalfResolution;
		} m_ScreenDataUB;

		Ref<UniformBufferSet> m_UBSScreenData;

		// PreDepth
		Ref<Pipeline> m_PreDepthPipeline;
		Ref<RenderPass> m_PreDepthPass;
		Ref<RenderPass> m_WireframeViewPreDepthPass; // For having wireframe view in the future
		Ref<Material> m_PreDepthMaterial;

		// Geometry
		Ref<Pipeline> m_GeometryPipeline;
		Ref<RenderPass> m_GeometryPass;

		// Selected Geometry
		Ref<RenderPass> m_SelectedGeometryPass;
		Ref<Material> m_SelectedGeometryMaterial;

		// TODO: Add wireframe on top pass where there is no depth testing
		// Geometry Wireframe
		Ref<RenderPass> m_GeometryWireFramePass;
		Ref<Material> m_WireFrameMaterial;

		// Grid
		Ref<RenderPass> m_GridPass;
		Ref<Material> m_GridMaterial;
		float m_TranslationSnapValue = 0.5f;

		// Composite
		Ref<Material> m_CompositeMaterial;
		Ref<RenderPass> m_CompositePass;

		// Jump Flood
		Ref<RenderPass> m_JumpFloodInitPass;
		Ref<Material> m_JumpFloodInitMaterial;
		Ref<RenderPass> m_JumpFloodPass[2];
		Ref<Material> m_JumpFloodPassMaterial[2];
		Ref<RenderPass> m_JumpFloodCompositePass;
		Ref<Material> m_JumpFloodCompositeMaterial;
		Ref<Framebuffer> m_JumpFloodFramebuffers[2];

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
		std::map<MeshKey, StaticDrawCommand> m_SelectedStaticMeshDrawList;

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

	};

}