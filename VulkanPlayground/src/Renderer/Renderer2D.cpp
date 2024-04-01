#include "vkPch.h"
#include "Renderer2D.h"

#include "Renderer.h"

namespace vkPlayground {

	// NOTE(TODO): The framebuffer that is created in the init function with all its pipelines and everything is persistant throughout the lifetime of the application
	// since it stays alive by the pipeline being a shader dependency which the Renderer stores in a global map
	// Maybe that is usefull for the runtime? But for the editor it is actually useless and just waste of memory

	Ref<Renderer2D> Renderer2D::Create(const Renderer2DSpecification& specification)
	{
		return CreateRef<Renderer2D>(specification);
	}

	Renderer2D::Renderer2D(const Renderer2DSpecification& specification)
		: m_Specification(specification),
		c_MaxVertices(specification.MaxQuads * 4),
		c_MaxIndices(specification.MaxQuads * 6),
		c_MaxLineVertices(specification.MaxLines * 2),
		c_MaxLineIndices(specification.MaxLines * 6)
	{
		Init();
	}

	Renderer2D::~Renderer2D()
	{
		Shutdown();
	}

	void Renderer2D::Init()
	{
		if (m_Specification.SwapChainTarget)
			m_RenderCommandBuffer = RenderCommandBuffer::CreateFromSwapChain("Renderer2D");
		else
			m_RenderCommandBuffer = RenderCommandBuffer::Create(0, "Renderer2D");

		m_UBSCamera = UniformBufferSet::Create(sizeof(UBCamera));

		m_MemoryStats.TotalAllocated = 0;

		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;

		FramebufferSpecification framebufferSpec = {
			.DebugName = "Renderer2D Framebuffer",
			.ClearColorOnLoad = false,
			.ClearColor = { 0.1f, 0.5f, 0.5f, 1.0f },
			.Attachments = { ImageFormat::RGBA32F, ImageFormat::DEPTH24STENCIL8 },
			.Samples = 1,
			.SwapchainTarget = m_Specification.SwapChainTarget
		};
		Ref<Framebuffer> framebuffer = Framebuffer::Create(framebufferSpec);

		{
			PipelineSpecification pipelineSpecification = {
				.DebugName = "Renderer2D-Quad",
				.Shader = Renderer::GetShadersLibrary()->Get("Renderer2D_Quad"),
				.TargetFramebuffer = framebuffer,
				.VertexLayout = {
					{ ShaderDataType::Float3, "a_Position" },
					{ ShaderDataType::Float4, "a_Color" },
					{ ShaderDataType::Float2, "a_TexCoord" },
					{ ShaderDataType::Float, "a_TexIndex" },
					{ ShaderDataType::Float, "a_TilingFactor" }
				},
				.BackFaceCulling = false
			};

			RenderPassSpecification quadSpec = {
				.DebugName = "Renderer2D-Quad",
				.Pipeline = Pipeline::Create(pipelineSpecification),
				.MarkerColor = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)
			};
			m_QuadPass = RenderPass::Create(quadSpec);
			m_QuadPass->SetInput("Camera", m_UBSCamera);
			m_QuadPass->Bake();

			m_QuadVertexBuffers.resize(1);
			m_QuadVertexBufferBases.resize(1);
			m_QuadVertexBufferPtr.resize(1);

			m_QuadVertexBuffers[0].resize(framesInFlight);
			m_QuadVertexBufferBases[0].resize(framesInFlight);
			for (uint32_t i = 0; i < framesInFlight; i++)
			{
				uint64_t allocationSize = c_MaxVertices * sizeof(QuadVertex);
				m_QuadVertexBuffers[0][i] = VertexBuffer::Create(static_cast<uint32_t>(allocationSize));
				m_MemoryStats.TotalAllocated += allocationSize;
				m_QuadVertexBufferBases[0][i] = new QuadVertex[c_MaxVertices];
			}

			uint32_t* quadIndices = new uint32_t[c_MaxIndices];

			uint32_t offset = 0;
			for (uint32_t i = 0; i < c_MaxIndices; i += 6)
			{
				quadIndices[i + 0] = offset + 0;
				quadIndices[i + 1] = offset + 1;
				quadIndices[i + 2] = offset + 2;

				quadIndices[i + 3] = offset + 2;
				quadIndices[i + 4] = offset + 3;
				quadIndices[i + 5] = offset + 0;

				offset += 4;
			}

			{
				uint64_t allocationSize = c_MaxIndices * sizeof(uint32_t);
				m_QuadIndexBuffer = IndexBuffer::Create(quadIndices, static_cast<uint32_t>(allocationSize));
				m_MemoryStats.TotalAllocated += allocationSize;
			}
			delete[] quadIndices;
		}

		m_WhiteTexture = Renderer::GetWhiteTexture();

		// First Texture slot is always the white texture...
		m_TextureSlots[0] = m_WhiteTexture;

		m_QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[1] = { -0.5f,  0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[2] = { 0.5f,  0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[3] = { 0.5f, -0.5f, 0.0f, 1.0f };

		// Lines
		{
			PipelineSpecification pipelineSpecification = {
				.DebugName = "Renderer2D-Line",
				.Shader = Renderer::GetShadersLibrary()->Get("Renderer2D_Line"),
				.TargetFramebuffer = framebuffer,
				.VertexLayout = {
					{ ShaderDataType::Float3, "a_Position" },
					{ ShaderDataType::Float4, "a_Color" }
				},
				.Topology = PrimitiveTopology::Lines,
				.LineWidth = 2.0f,
			};

			{
				RenderPassSpecification lineSpec = {
					.DebugName = "Renderer2D-Line",
					.Pipeline = Pipeline::Create(pipelineSpecification),
					.MarkerColor = glm::vec4(0.1f, 0.2f, 0.7f, 1.0f)
				};
				m_LinePass = RenderPass::Create(lineSpec);
				m_LinePass->SetInput("Camera", m_UBSCamera);
				m_LinePass->Bake();
			}

			m_LineVertexBuffers.resize(1);
			m_LineVertexBufferBases.resize(1);
			m_LineVertexBufferPtr.resize(1);

			m_LineVertexBuffers[0].resize(framesInFlight);
			m_LineVertexBufferBases[0].resize(framesInFlight);
			for (uint32_t i = 0; i < framesInFlight; i++)
			{
				uint64_t allocationSize = c_MaxLineVertices * sizeof(LineVertex);
				m_LineVertexBuffers[0][i] = VertexBuffer::Create(static_cast<uint32_t>(allocationSize));
				m_MemoryStats.TotalAllocated += allocationSize;
				m_LineVertexBufferBases[0][i] = new LineVertex[c_MaxLineVertices];
			}

			uint32_t* lineIndices = new uint32_t[c_MaxLineIndices];
			for (uint32_t i = 0; i < c_MaxLineIndices; i++)
				lineIndices[i] = i;

			{
				uint64_t allocationSize = c_MaxLineIndices * sizeof(uint32_t);
				m_LineIndexBuffer = IndexBuffer::Create(lineIndices, static_cast<uint32_t>(allocationSize));
				m_MemoryStats.TotalAllocated += allocationSize;
			}
			delete[] lineIndices;
		}

		m_QuadMaterial = Material::Create(m_QuadPass->GetPipeline()->GetShader(), "QuadMaterial");
		m_LineMaterial = Material::Create(m_LinePass->GetPipeline()->GetShader(), "LineMaterial");
	}

	void Renderer2D::Shutdown()
	{
		for (auto& buffers : m_QuadVertexBufferBases)
		{
			for (auto buffer : buffers)
				delete[] buffer;
		}

		for (auto& buffers : m_LineVertexBufferBases)
		{
			for (auto buffer : buffers)
				delete[] buffer;
		}
	}

	void Renderer2D::BeginScene(const glm::mat4& viewProj, const glm::mat4& view, bool depthTest)
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		m_CameraViewProj = viewProj;
		m_CameraView = view;
		m_DepthTest = depthTest;

		m_UBSCamera->Get()->SetData(&viewProj, sizeof(UBCamera));

		// VKPG_CORE_TRACE_TAG("Renderer", "Renderer2D::BeginScene frame {}", frameIndex);

		m_QuadIndexCount = 0;
		for (uint32_t i = 0; i < m_QuadVertexBufferPtr.size(); i++)
			m_QuadVertexBufferPtr[i] = m_QuadVertexBufferBases[i][frameIndex];

		m_LineIndexCount = 0;
		for (uint32_t i = 0; i < m_LineVertexBufferPtr.size(); i++)
			m_LineVertexBufferPtr[i] = m_LineVertexBufferBases[i][frameIndex];

		m_TextureSlotIndex = 1;

		// Set all other texture slots to null and keep only the first slot for the white texture
		for (uint32_t i = 1; i < m_TextureSlots.size(); i++)
			m_TextureSlots[i] = nullptr;
	}

	void Renderer2D::EndScene()
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		m_RenderCommandBuffer->Begin();

		// VKPG_CORE_TRACE_TAG("Renderer", "Renderer2D::EndScene frame {}", frameIndex);
		uint32_t dataSize = 0;

		// Quads
		for (uint32_t i = 0; i <= m_QuadBufferWriteIndex; i++)
		{
			dataSize = (uint32_t)((uint8_t*)m_QuadVertexBufferPtr[i] - (uint8_t*)m_QuadVertexBufferBases[i][frameIndex]);
			if (dataSize)
			{
				uint32_t indexCount = i == m_QuadBufferWriteIndex ? m_QuadIndexCount - (c_MaxIndices * i) : c_MaxIndices;
				m_QuadVertexBuffers[i][frameIndex]->SetData(m_QuadVertexBufferBases[i][frameIndex], dataSize);

				for (uint32_t i = 0; i < m_TextureSlots.size(); i++)
				{
					if (m_TextureSlots[i])
						m_QuadMaterial->Set("u_Textures", m_TextureSlots[i], i);
					else
						m_QuadMaterial->Set("u_Textures", m_WhiteTexture, i);
				}

				Renderer::BeginRenderPass(m_RenderCommandBuffer, m_QuadPass);
				Renderer::RenderGeometry(m_RenderCommandBuffer, m_QuadPass->GetPipeline(), m_QuadMaterial, m_QuadVertexBuffers[i][frameIndex], m_QuadIndexBuffer, glm::mat4(1.0f), indexCount);
				Renderer::EndRenderPass(m_RenderCommandBuffer);

				m_DrawStats.DrawCalls++;
				m_MemoryStats.Used += dataSize;
			}

		}

		// Lines
		VkCommandBuffer commandBuffer = m_RenderCommandBuffer->GetActiveCommandBuffer();
		for (uint32_t i = 0; i <= m_LineBufferWriteIndex; i++)
		{
			dataSize = (uint32_t)((uint8_t*)m_LineVertexBufferPtr[i] - (uint8_t*)m_LineVertexBufferBases[i][frameIndex]);
			if (dataSize)
			{
				uint32_t indexCount = i == m_LineBufferWriteIndex ? m_LineIndexCount - (c_MaxLineIndices * i) : c_MaxLineIndices;
				m_LineVertexBuffers[i][frameIndex]->SetData(m_LineVertexBufferBases[i][frameIndex], dataSize);

				Renderer::BeginRenderPass(m_RenderCommandBuffer, m_LinePass);
				vkCmdSetLineWidth(commandBuffer, m_LineWidth);
				Renderer::RenderGeometry(m_RenderCommandBuffer, m_LinePass->GetPipeline(), m_LineMaterial, m_LineVertexBuffers[i][frameIndex], m_LineIndexBuffer, glm::mat4(1.0f), indexCount);
				Renderer::EndRenderPass(m_RenderCommandBuffer);

				m_DrawStats.DrawCalls++;
				m_MemoryStats.Used += dataSize;
			}

		}

		m_RenderCommandBuffer->End();
		m_RenderCommandBuffer->Submit();
	}

	void Renderer2D::SetTargetFramebuffer(Ref<Framebuffer> framebuffer)
	{
		// All passes should have the same framebuffer (set in SetTargetFramebuffer) so we can only check for one of the passes if they have the same framebuffer
		// because if we do not check this we will leak memory in Renderer::ShaderDependencies
		if (framebuffer != m_QuadPass->GetTargetFramebuffer())
		{
			{
				PipelineSpecification pipelineSpec = m_QuadPass->GetSpecification().Pipeline->GetSpecification();
				pipelineSpec.TargetFramebuffer = framebuffer;
				RenderPassSpecification& renderpassSpec = m_QuadPass->GetSpecification();
				renderpassSpec.Pipeline = Pipeline::Create(pipelineSpec);
			}

			{
				PipelineSpecification pipelineSpec = m_LinePass->GetSpecification().Pipeline->GetSpecification();
				pipelineSpec.TargetFramebuffer = framebuffer;
				RenderPassSpecification& renderpassSpec = m_LinePass->GetSpecification();
				renderpassSpec.Pipeline = Pipeline::Create(pipelineSpec);
			}
		}
	}

	void Renderer2D::OnRecreateSwapchain()
	{
		if (m_Specification.SwapChainTarget)
			m_RenderCommandBuffer = RenderCommandBuffer::CreateFromSwapChain("Renderer2D");
	}

	void Renderer2D::AddQuadBuffer()
	{
		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;

		VertexBufferPerFrame& newVertexBuffer = m_QuadVertexBuffers.emplace_back();
		QuadVertexBasePerFrame& newVertexBufferBase = m_QuadVertexBufferBases.emplace_back();

		newVertexBuffer.resize(framesInFlight);
		newVertexBufferBase.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			uint64_t allocationSize = c_MaxVertices * sizeof(QuadVertex);
			newVertexBuffer[i] = VertexBuffer::Create(static_cast<uint32_t>(allocationSize));
			m_MemoryStats.TotalAllocated += allocationSize;
			newVertexBufferBase[i] = new QuadVertex[c_MaxVertices];
		}
	}

	void Renderer2D::AddLineBuffer()
	{
		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;

		VertexBufferPerFrame& newVertexBuffer = m_LineVertexBuffers.emplace_back();
		LineVertexBasePerFrame& newVertexBufferBase = m_LineVertexBufferBases.emplace_back();

		newVertexBuffer.resize(framesInFlight);
		newVertexBufferBase.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			uint64_t allocationSize = c_MaxLineVertices * sizeof(LineVertex);
			newVertexBuffer[i] = VertexBuffer::Create(static_cast<uint32_t>(allocationSize));
			m_MemoryStats.TotalAllocated += allocationSize;
			newVertexBufferBase[i] = new LineVertex[c_MaxVertices];
		}
	}

	Renderer2D::QuadVertex*& Renderer2D::GetWriteableQuadBuffer()
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		m_QuadBufferWriteIndex = m_QuadIndexCount / c_MaxIndices;
		if (m_QuadBufferWriteIndex >= m_QuadVertexBufferBases.size())
		{
			AddQuadBuffer();
			m_QuadVertexBufferPtr.emplace_back();
			m_QuadVertexBufferPtr[m_QuadBufferWriteIndex] = m_QuadVertexBufferBases[m_QuadBufferWriteIndex][frameIndex];
		}

		return m_QuadVertexBufferPtr[m_QuadBufferWriteIndex];
	}

	Renderer2D::LineVertex*& Renderer2D::GetWriteableLineBuffer()
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		m_LineBufferWriteIndex = m_LineIndexCount / c_MaxIndices;
		if (m_LineBufferWriteIndex >= m_LineVertexBufferBases.size())
		{
			AddLineBuffer();
			m_LineVertexBufferPtr.emplace_back();
			m_LineVertexBufferPtr[m_LineBufferWriteIndex] = m_LineVertexBufferBases[m_LineBufferWriteIndex][frameIndex];
		}

		return m_LineVertexBufferPtr[m_LineBufferWriteIndex];
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color)
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		constexpr size_t quadVertexCount = 4;
		const float textureIndex = 0.0f; // White Texture
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
		const float tilingFactor = 1.0f;

		m_QuadBufferWriteIndex = m_QuadIndexCount / c_MaxIndices;
		if (m_QuadBufferWriteIndex >= m_QuadVertexBufferBases.size())
		{
			AddQuadBuffer();
			m_QuadVertexBufferPtr.emplace_back();
			m_QuadVertexBufferPtr[m_QuadBufferWriteIndex] = m_QuadVertexBufferBases[m_QuadBufferWriteIndex][frameIndex];
		}

		auto& bufferPtr = m_QuadVertexBufferPtr[m_QuadBufferWriteIndex];
		for (size_t i = 0; i < quadVertexCount; i++)
		{
			bufferPtr->Position = transform * m_QuadVertexPositions[i];
			bufferPtr->Color = color;
			bufferPtr->TexCoord = textureCoords[i];
			bufferPtr->TexIndex = textureIndex;
			bufferPtr->TilingFactor = tilingFactor;
			bufferPtr++;
		}

		m_QuadIndexCount += 6;

		m_DrawStats.QuadCount++;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor, glm::vec2 uv0, glm::vec2 uv1)
	{
		constexpr size_t quadVertexCount = 4;
		glm::vec2 textureCoords[] = { uv0, { uv1.x, uv0.y }, uv1, { uv0.x, uv1.y } };

		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < m_TextureSlotIndex; i++)
		{
			if (m_TextureSlots[i]->GetHash() == texture->GetHash())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			textureIndex = (float)m_TextureSlotIndex;
			m_TextureSlots[m_TextureSlotIndex] = texture;
			m_TextureSlotIndex++;
		}

		auto& bufferPtr = m_QuadVertexBufferPtr[m_QuadBufferWriteIndex];
		for (size_t i = 0; i < quadVertexCount; i++)
		{
			bufferPtr->Position = transform * m_QuadVertexPositions[i];
			bufferPtr->Color = tintColor;
			bufferPtr->TexCoord = textureCoords[i];
			bufferPtr->TexIndex = textureIndex;
			bufferPtr->TilingFactor = tilingFactor;
			bufferPtr++;
		}

		m_QuadIndexCount += 6;

		m_DrawStats.QuadCount++;
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		const float textureIndex = 0.0f; // White Texture
		const float tilingFactor = 1.0f;

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		auto& bufferPtr = GetWriteableQuadBuffer();
		bufferPtr->Position = transform * m_QuadVertexPositions[0];
		bufferPtr->Color = color;
		bufferPtr->TexCoord = { 0.0f, 0.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = transform * m_QuadVertexPositions[1];
		bufferPtr->Color = color;
		bufferPtr->TexCoord = { 1.0f, 0.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = transform * m_QuadVertexPositions[2];
		bufferPtr->Color = color;
		bufferPtr->TexCoord = { 1.0f, 1.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = transform * m_QuadVertexPositions[3];
		bufferPtr->Color = color;
		bufferPtr->TexCoord = { 0.0f, 1.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		m_QuadIndexCount += 6;

		m_DrawStats.QuadCount++;
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor, glm::vec2 uv0, glm::vec2 uv1)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, texture, tilingFactor, tintColor, uv0, uv1);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor, glm::vec2 uv0, glm::vec2 uv1)
	{
		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < m_TextureSlotIndex; i++)
		{
			if (m_TextureSlots[i]->GetHash() == texture->GetHash())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			textureIndex = (float)m_TextureSlotIndex;
			m_TextureSlots[m_TextureSlotIndex] = texture;
			m_TextureSlotIndex++;
		}

		glm::vec2 textureCoords[] = { uv0, { uv1.x, uv0.y }, uv1, { uv0.x, uv1.y } };

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		auto& bufferPtr = GetWriteableQuadBuffer();
		bufferPtr->Position = transform * m_QuadVertexPositions[0];
		bufferPtr->Color = tintColor;
		bufferPtr->TexCoord = textureCoords[0];
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = transform * m_QuadVertexPositions[1];
		bufferPtr->Color = tintColor;
		bufferPtr->TexCoord = textureCoords[1];
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = transform * m_QuadVertexPositions[2];
		bufferPtr->Color = tintColor;
		bufferPtr->TexCoord = textureCoords[2];
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = transform * m_QuadVertexPositions[3];
		bufferPtr->Color = tintColor;
		bufferPtr->TexCoord = textureCoords[3];
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		m_QuadIndexCount += 6;

		m_DrawStats.QuadCount++;
	}

	void Renderer2D::DrawQuadBillboard(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		const float textureIndex = 0.0f; // White Texture
		const float tilingFactor = 1.0f;

		glm::vec3 camRightWS = { m_CameraView[0][0], m_CameraView[1][0], m_CameraView[2][0] };
		glm::vec3 camUpWS = { m_CameraView[0][1], m_CameraView[1][1], m_CameraView[2][1] };

		auto& bufferPtr = GetWriteableQuadBuffer();
		bufferPtr->Position = position + camRightWS * (m_QuadVertexPositions[0].x) * size.x + camUpWS * m_QuadVertexPositions[0].y * size.y;
		bufferPtr->Color = color;
		bufferPtr->TexCoord = { 0.0f, 0.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = position + camRightWS * m_QuadVertexPositions[1].x * size.x + camUpWS * m_QuadVertexPositions[1].y * size.y;
		bufferPtr->Color = color;
		bufferPtr->TexCoord = { 1.0f, 0.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = position + camRightWS * m_QuadVertexPositions[2].x * size.x + camUpWS * m_QuadVertexPositions[2].y * size.y;
		bufferPtr->Color = color;
		bufferPtr->TexCoord = { 1.0f, 1.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = position + camRightWS * m_QuadVertexPositions[3].x * size.x + camUpWS * m_QuadVertexPositions[3].y * size.y;
		bufferPtr->Color = color;
		bufferPtr->TexCoord = { 0.0f, 1.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		m_QuadIndexCount += 6;

		m_DrawStats.QuadCount++;
	}

	void Renderer2D::DrawQuadBillboard(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < m_TextureSlotIndex; i++)
		{
			if (m_TextureSlots[i]->GetHash() == texture->GetHash())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			textureIndex = (float)m_TextureSlotIndex;
			m_TextureSlots[m_TextureSlotIndex] = texture;
			m_TextureSlotIndex++;
		}

		glm::vec3 camRightWS = { m_CameraView[0][0], m_CameraView[1][0], m_CameraView[2][0] };
		glm::vec3 camUpWS = { m_CameraView[0][1], m_CameraView[1][1], m_CameraView[2][1] };

		auto& bufferPtr = GetWriteableQuadBuffer();
		bufferPtr->Position = position + camRightWS * (m_QuadVertexPositions[0].x) * size.x + camUpWS * m_QuadVertexPositions[0].y * size.y;
		bufferPtr->Color = tintColor;
		bufferPtr->TexCoord = { 0.0f, 1.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = position + camRightWS * m_QuadVertexPositions[1].x * size.x + camUpWS * m_QuadVertexPositions[1].y * size.y;
		bufferPtr->Color = tintColor;
		bufferPtr->TexCoord = { 0.0f, 0.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = position + camRightWS * m_QuadVertexPositions[2].x * size.x + camUpWS * m_QuadVertexPositions[2].y * size.y;
		bufferPtr->Color = tintColor;
		bufferPtr->TexCoord = { 1.0f, 0.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = position + camRightWS * m_QuadVertexPositions[3].x * size.x + camUpWS * m_QuadVertexPositions[3].y * size.y;
		bufferPtr->Color = tintColor;
		bufferPtr->TexCoord = { 1.0f, 1.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		m_QuadIndexCount += 6;

		m_DrawStats.QuadCount++;
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, color);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		const float textureIndex = 0.0f; // White Texture
		const float tilingFactor = 1.0f;

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		auto& bufferPtr = GetWriteableQuadBuffer();
		bufferPtr->Position = transform * m_QuadVertexPositions[0];
		bufferPtr->Color = color;
		bufferPtr->TexCoord = { 0.0f, 0.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = transform * m_QuadVertexPositions[1];
		bufferPtr->Color = color;
		bufferPtr->TexCoord = { 1.0f, 0.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = transform * m_QuadVertexPositions[2];
		bufferPtr->Color = color;
		bufferPtr->TexCoord = { 1.0f, 1.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = transform * m_QuadVertexPositions[3];
		bufferPtr->Color = color;
		bufferPtr->TexCoord = { 0.0f, 1.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		m_QuadIndexCount += 6;

		m_DrawStats.QuadCount++;
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, texture, tilingFactor, tintColor);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < m_TextureSlotIndex; i++)
		{
			if (m_TextureSlots[i]->GetHash() == texture->GetHash())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			textureIndex = (float)m_TextureSlotIndex;
			m_TextureSlots[m_TextureSlotIndex] = texture;
			m_TextureSlotIndex++;
		}

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		auto& bufferPtr = GetWriteableQuadBuffer();
		bufferPtr->Position = transform * m_QuadVertexPositions[0];
		bufferPtr->Color = tintColor;
		bufferPtr->TexCoord = { 0.0f, 0.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = transform * m_QuadVertexPositions[1];
		bufferPtr->Color = tintColor;
		bufferPtr->TexCoord = { 1.0f, 0.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = transform * m_QuadVertexPositions[2];
		bufferPtr->Color = tintColor;
		bufferPtr->TexCoord = { 1.0f, 1.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		bufferPtr->Position = transform * m_QuadVertexPositions[3];
		bufferPtr->Color = tintColor;
		bufferPtr->TexCoord = { 0.0f, 1.0f };
		bufferPtr->TexIndex = textureIndex;
		bufferPtr->TilingFactor = tilingFactor;
		bufferPtr++;

		m_QuadIndexCount += 6;

		m_DrawStats.QuadCount++;
	}

	void Renderer2D::DrawRotatedRect(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		DrawRotatedRect({ position.x, position.y, 0.0f }, size, rotation, color);
	}

	void Renderer2D::DrawRotatedRect(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		glm::vec3 positions[4] =
		{
			transform * m_QuadVertexPositions[0],
			transform * m_QuadVertexPositions[1],
			transform * m_QuadVertexPositions[2],
			transform * m_QuadVertexPositions[3]
		};

		for (int i = 0; i < 4; i++)
		{
			auto& v0 = positions[i];
			auto& v1 = positions[(i + 1) % 4];

			auto& bufferPtr = GetWriteableLineBuffer();
			bufferPtr->Position = v0;
			bufferPtr->Color = color;
			bufferPtr++;

			bufferPtr->Position = v1;
			bufferPtr->Color = color;
			bufferPtr++;

			m_LineIndexCount += 2;
			m_DrawStats.LineCount++;
		}
	}

	void Renderer2D::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color)
	{
		auto& bufferPtr = GetWriteableLineBuffer();
		bufferPtr->Position = p0;
		bufferPtr->Color = color;
		bufferPtr++;

		bufferPtr->Position = p1;
		bufferPtr->Color = color;
		bufferPtr++;

		m_LineIndexCount += 2;

		m_DrawStats.LineCount++;
	}

	void Renderer2D::DrawCircle(const glm::vec3& position, const glm::vec3& rotation, float radius, const glm::vec4& color)
	{
		const glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), rotation.x, { 1.0f, 0.0f, 0.0f })
			* glm::rotate(glm::mat4(1.0f), rotation.y, { 0.0f, 1.0f, 0.0f })
			* glm::rotate(glm::mat4(1.0f), rotation.z, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), glm::vec3(radius));

		DrawCircle(transform, color);
	}

	void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color)
	{
		int segments = 32;
		for (int i = 0; i < segments; i++)
		{
			float angle = 2.0f * glm::pi<float>() * (float)i / segments;
			glm::vec4 startPosition = { glm::cos(angle), glm::sin(angle), 0.0f, 1.0f };
			angle = 2.0f * glm::pi<float>() * (float)((i + 1) % segments) / segments;
			glm::vec4 endPosition = { glm::cos(angle), glm::sin(angle), 0.0f, 1.0f };

			glm::vec3 p0 = transform * startPosition;
			glm::vec3 p1 = transform * endPosition;
			DrawLine(p0, p1, color);
		}
	}

	void Renderer2D::DrawAABB(const AABB& aabb, const glm::mat4& transform, const glm::vec4& color /*= glm::vec4(1.0f)*/)
	{
		glm::vec4 min = { aabb.Min.x, aabb.Min.y, aabb.Min.z, 1.0f };
		glm::vec4 max = { aabb.Max.x, aabb.Max.y, aabb.Max.z, 1.0f };

		glm::vec4 corners[8] =
		{
			transform * glm::vec4 { aabb.Min.x, aabb.Min.y, aabb.Max.z, 1.0f },
			transform * glm::vec4 { aabb.Min.x, aabb.Max.y, aabb.Max.z, 1.0f },
			transform * glm::vec4 { aabb.Max.x, aabb.Max.y, aabb.Max.z, 1.0f },
			transform * glm::vec4 { aabb.Max.x, aabb.Min.y, aabb.Max.z, 1.0f },

			transform * glm::vec4 { aabb.Min.x, aabb.Min.y, aabb.Min.z, 1.0f },
			transform * glm::vec4 { aabb.Min.x, aabb.Max.y, aabb.Min.z, 1.0f },
			transform * glm::vec4 { aabb.Max.x, aabb.Max.y, aabb.Min.z, 1.0f },
			transform * glm::vec4 { aabb.Max.x, aabb.Min.y, aabb.Min.z, 1.0f }
		};

		for (uint32_t i = 0; i < 4; i++)
			DrawLine(corners[i], corners[(i + 1) % 4], color);

		for (uint32_t i = 0; i < 4; i++)
			DrawLine(corners[i + 4], corners[((i + 1) % 4) + 4], color);

		for (uint32_t i = 0; i < 4; i++)
			DrawLine(corners[i], corners[i + 4], color);
	}

	float Renderer2D::GetLineWidth()
	{
		return m_LineWidth;
	}

	void Renderer2D::SetLineWidth(float lineWidth)
	{
		m_LineWidth = lineWidth;
	}

	void Renderer2D::ResetStats()
	{
		std::memset(&m_DrawStats, 0, sizeof(DrawStatistics));
		m_MemoryStats.Used = 0;
	}

	Renderer2D::DrawStatistics Renderer2D::GetDrawStats()
	{
		return m_DrawStats;
	}

	Renderer2D::MemoryStatistics Renderer2D::GetMemoryStats()
	{
		return m_MemoryStats;
	}

	uint64_t Renderer2D::MemoryStatistics::GetAllocatedPerFrame() const
	{
		return TotalAllocated / Renderer::GetConfig().FramesInFlight;
	}

}