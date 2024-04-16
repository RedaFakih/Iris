#pragma once

#include "Core/AABB.h"
#include "IndexBuffer.h"
#include "Mesh/Material.h"
#include "Renderer/Core/RenderCommandBuffer.h"
#include "RenderPass.h"
#include "UniformBufferSet.h"
#include "VertexBuffer.h"

#include <glm/glm.hpp>

namespace Iris {

	/*
	 * TODO: Ass support to rendering filled circles and also TEXT RENDERING
	 */

	struct Renderer2DSpecification
	{
		bool SwapChainTarget = false;
		uint32_t MaxQuads = 5000; // For both quads and text
		uint32_t MaxLines = 2000;
	};

	class Renderer2D : public RefCountedObject
	{
	public:
		// Stats
		struct DrawStatistics
		{
			uint32_t DrawCalls = 0;
			uint32_t QuadCount = 0;
			uint32_t LineCount = 0;

			uint32_t GetTotalVertexCount() { return QuadCount * 4 + LineCount * 2; }
			uint32_t GetTotalIndexCount() { return QuadCount * 6 + LineCount * 2; }
		};

		struct MemoryStatistics
		{
			uint64_t Used = 0;
			uint64_t TotalAllocated = 0;

			uint64_t GetAllocatedPerFrame() const;
		};

	public:
		Renderer2D(const Renderer2DSpecification& specification = Renderer2DSpecification());
		virtual ~Renderer2D();

		[[nodiscard]] static Ref<Renderer2D> Create(const Renderer2DSpecification& specification = Renderer2DSpecification());

		void Init();
		void Shutdown();

		void BeginScene(const glm::mat4& viewProj, const glm::mat4& view, bool depthTest = true);
		void EndScene(bool prepareForRendering = false);

		void SetTargetFramebuffer(Ref<Framebuffer> framebuffer);

		void OnRecreateSwapchain();

		// Primitives
		void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
		void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f), glm::vec2 uv0 = glm::vec2(0.0f), glm::vec2 uv1 = glm::vec2(1.0f));

		void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
		void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
		void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f), glm::vec2 uv0 = glm::vec2(0.0f), glm::vec2 uv1 = glm::vec2(1.0f));
		void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f), glm::vec2 uv0 = glm::vec2(0.0f), glm::vec2 uv1 = glm::vec2(1.0f));

		void DrawQuadBillboard(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
		void DrawQuadBillboard(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));

		void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color);
		void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color);
		void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));
		void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tintColor = glm::vec4(1.0f));

		void DrawRotatedRect(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color);
		void DrawRotatedRect(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color);

		// Thickness is between 0 and 1
		void DrawCircle(const glm::vec3& p0, const glm::vec3& rotation, float radius, const glm::vec4& color);
		void DrawCircle(const glm::mat4& transform, const glm::vec4& color);

		void DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color = glm::vec4(1.0f));

		void DrawAABB(const AABB& aabb, const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));

		float GetLineWidth() const { return m_LineWidth; }
		void SetLineWidth(float lineWidth);

		void ResetStats();
		DrawStatistics GetDrawStats();
		MemoryStatistics GetMemoryStats();

		const Renderer2DSpecification& GetSpecification() const { return m_Specification; }


	private:
		void PrepareImagesForRendering();

		void AddQuadBuffer();
		void AddLineBuffer();

		struct QuadVertex
		{
			glm::vec3 Position;
			glm::vec4 Color;
			glm::vec2 TexCoord;
			float TexIndex;
			float TilingFactor;
		};

		struct LineVertex
		{
			glm::vec3 Position;
			glm::vec4 Color;
		};

		QuadVertex*& GetWriteableQuadBuffer();
		LineVertex*& GetWriteableLineBuffer();
		
	private:
		Renderer2DSpecification m_Specification;
		Ref<RenderCommandBuffer> m_RenderCommandBuffer;

		static const uint32_t MaxTextureSlots = 32; // TODO: RenderCaps

		const uint32_t c_MaxVertices;
		const uint32_t c_MaxIndices;

		const uint32_t c_MaxLineVertices;
		const uint32_t c_MaxLineIndices;

		Ref<Texture2D> m_WhiteTexture;

		// Per-frame -> and in case we need more we create another one
		using VertexBufferPerFrame = std::vector<Ref<VertexBuffer>>;

		// Quads
		Ref<RenderPass> m_QuadPass;
		std::vector<VertexBufferPerFrame> m_QuadVertexBuffers;
		Ref<IndexBuffer> m_QuadIndexBuffer;
		Ref<Material> m_QuadMaterial;

		uint32_t m_QuadIndexCount = 0;
		using QuadVertexBasePerFrame = std::vector<QuadVertex*>;
		std::vector<QuadVertexBasePerFrame> m_QuadVertexBufferBases;
		std::vector<QuadVertex*> m_QuadVertexBufferPtr;
		uint32_t m_QuadBufferWriteIndex = 0;

		std::array<Ref<Texture2D>, MaxTextureSlots> m_TextureSlots;
		uint32_t m_TextureSlotIndex = 1; // 0 = white texture

		glm::vec4 m_QuadVertexPositions[4];

		// Lines
		Ref<RenderPass> m_LinePass;
		std::vector<VertexBufferPerFrame> m_LineVertexBuffers;
		Ref<IndexBuffer> m_LineIndexBuffer;
		Ref<Material> m_LineMaterial;

		uint32_t m_LineIndexCount = 0;
		using LineVertexBasePerFrame = std::vector<LineVertex*>;
		std::vector<LineVertexBasePerFrame> m_LineVertexBufferBases;
		std::vector<LineVertex*> m_LineVertexBufferPtr;
		uint32_t m_LineBufferWriteIndex = 0;

		glm::mat4 m_CameraViewProj;
		glm::mat4 m_CameraView;
		bool m_DepthTest = true;

		float m_LineWidth = 2.0f;

		DrawStatistics m_DrawStats;
		MemoryStatistics m_MemoryStats;

		struct UBCamera
		{
			glm::mat4 ViewProjection;
		};

		Ref<UniformBufferSet> m_UBSCamera;
	};

}