#include "IrisPCH.h"
#include "Renderer2D.h"

#include "Renderer.h"
#include "Text/MSDFData.h"

namespace Iris {

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

		// We do not release shader modules for the 2D renderer since we could have multiple instances of it
		constexpr bool releaseShaderModules = false;

		{
			PipelineSpecification pipelineSpecification = {
				.DebugName = "Renderer2D-Quad",
				.Shader = Renderer::GetShadersLibrary()->Get("Renderer2D_Quad"),
				.TargetFramebuffer = m_Specification.TargetFramebuffer,
				.VertexLayout = {
					{ ShaderDataType::Float3, "a_Position" },
					{ ShaderDataType::Float4, "a_Color" },
					{ ShaderDataType::Float2, "a_TexCoord" },
					{ ShaderDataType::Float, "a_TexIndex" },
					{ ShaderDataType::Float, "a_TilingFactor" }
				},
				.BackFaceCulling = false,
				.ReleaseShaderModules = releaseShaderModules
			};

			RenderPassSpecification quadSpec = {
				.DebugName = "Renderer2D-Quad",
				.Pipeline = Pipeline::Create(pipelineSpecification),
				.MarkerColor = { 0.0f, 0.0f, 1.0f, 1.0f }
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
		m_QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[3] = {  0.5f, -0.5f, 0.0f, 1.0f };

		// Lines
		{
			PipelineSpecification pipelineSpecification = {
				.DebugName = "Renderer2D-Line",
				.Shader = Renderer::GetShadersLibrary()->Get("Renderer2D_Line"),
				.TargetFramebuffer = m_Specification.TargetFramebuffer,
				.VertexLayout = {
					{ ShaderDataType::Float3, "a_Position" },
					{ ShaderDataType::Float4, "a_Color" }
				},
				.Topology = PrimitiveTopology::Lines,
				.LineWidth = 2.0f,
				.ReleaseShaderModules = releaseShaderModules
			};

			RenderPassSpecification lineSpec = {
				.DebugName = "Renderer2D-Line",
				.Pipeline = Pipeline::Create(pipelineSpecification),
				.MarkerColor = { 0.0f, 0.0f, 1.0f, 1.0f }
			};
			m_LinePass = RenderPass::Create(lineSpec);
			m_LinePass->SetInput("Camera", m_UBSCamera);
			m_LinePass->Bake();

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

		{
			PipelineSpecification spec = {
				.DebugName = "Renderer2D-Text",
				.Shader = Renderer::GetShadersLibrary()->Get("Renderer2D_Text"),
				.TargetFramebuffer = m_Specification.TargetFramebuffer,
				.VertexLayout = {
					{ ShaderDataType::Float3, "a_Position" },
					{ ShaderDataType::Float4, "a_Color" },
					{ ShaderDataType::Float2, "a_TexCoord" },
					{ ShaderDataType::Float, "a_TexIndex" }
				},
				.BackFaceCulling = false,
				.ReleaseShaderModules = releaseShaderModules
			};

			RenderPassSpecification passSpec = {
				.DebugName = "Renderer2D-Text",
				.Pipeline = Pipeline::Create(spec),
				.MarkerColor = { 0.0f, 0.0f, 1.0f, 1.0f }
			};
			m_TextPass = RenderPass::Create(passSpec);
			m_TextPass->SetInput("Camera", m_UBSCamera);
			m_TextPass->Bake();

			m_TextVertexBuffers.resize(1);
			m_TextVertexBufferBases.resize(1);
			m_TextVertexBufferPtr.resize(1);

			m_TextVertexBuffers[0].resize(framesInFlight);
			m_TextVertexBufferBases[0].resize(framesInFlight);
			for (uint32_t i = 0; i < framesInFlight; i++)
			{
				uint64_t allocationSize = c_MaxVertices * sizeof(TextVertex);
				m_TextVertexBuffers[0][i] = VertexBuffer::Create(static_cast<uint32_t>(allocationSize));
				m_MemoryStats.TotalAllocated += allocationSize;
				m_TextVertexBufferBases[0][i] = new TextVertex[c_MaxVertices];
			}

			uint32_t* textQuadIndices = new uint32_t[c_MaxIndices];

			uint32_t offset = 0;
			for (uint32_t i = 0; i < c_MaxIndices; i += 6)
			{
				textQuadIndices[i + 0] = offset + 0;
				textQuadIndices[i + 1] = offset + 1;
				textQuadIndices[i + 2] = offset + 2;

				textQuadIndices[i + 3] = offset + 2;
				textQuadIndices[i + 4] = offset + 3;
				textQuadIndices[i + 5] = offset + 0;

				offset += 4;
			}

			{
				uint64_t allocationSize = c_MaxIndices * sizeof(uint32_t);
				m_TextIndexBuffer = IndexBuffer::Create(textQuadIndices, static_cast<uint32_t>(allocationSize));
				m_MemoryStats.TotalAllocated += allocationSize;
			}

			delete[] textQuadIndices;
		}

		m_QuadMaterial = Material::Create(Renderer::GetShadersLibrary()->Get("Renderer2D_Quad"), "QuadMaterial");
		m_LineMaterial = Material::Create(Renderer::GetShadersLibrary()->Get("Renderer2D_Line"), "LineMaterial");
		m_TextMaterial = Material::Create(Renderer::GetShadersLibrary()->Get("Renderer2D_Text"), "TextMaterial");
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

		for (auto& buffers : m_TextVertexBufferBases)
		{
			for (auto buffer : buffers)
				delete[] buffer;
		}
	}

	void Renderer2D::BeginScene(const glm::mat4& viewProj, const glm::mat4& view)
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		m_CameraViewProj = viewProj;
		m_CameraView = view;

		Ref<Renderer2D> instance = this;
		Renderer::Submit([instance, viewProj]() mutable
		{
			instance->m_UBSCamera->RT_Get()->RT_SetData(&viewProj, sizeof(UBCamera));
		});

		m_QuadIndexCount = 0;
		for (uint32_t i = 0; i < m_QuadVertexBufferPtr.size(); i++)
			m_QuadVertexBufferPtr[i] = m_QuadVertexBufferBases[i][frameIndex];

		m_LineIndexCount = 0;
		for (uint32_t i = 0; i < m_LineVertexBufferPtr.size(); i++)
			m_LineVertexBufferPtr[i] = m_LineVertexBufferBases[i][frameIndex];

		m_TextIndexCount = 0;
		for (uint32_t i = 0; i < m_TextVertexBufferPtr.size(); i++)
			m_TextVertexBufferPtr[i] = m_TextVertexBufferBases[i][frameIndex];

		m_TextureSlotIndex = 1;
		m_FontTextureSlotIndex = 0;
		m_TextBufferWriteIndex = 0;

		// Set all other texture slots to null and keep only the first slot for the white texture
		for (uint32_t i = 1; i < m_TextureSlots.size(); i++)
			m_TextureSlots[i] = nullptr;

		for (uint32_t i = 0; i < m_TextureSlots.size(); i++)
			m_FontTextureSlots[i] = nullptr;
	}

	void Renderer2D::EndScene(bool prepareForRendering)
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		m_RenderCommandBuffer->Begin();

		// IR_CORE_TRACE_TAG("Renderer", "Renderer2D::EndScene frame {}", frameIndex);
		uint32_t dataSize = 0;

		// Quads
		for (uint32_t i = 0; i <= m_QuadBufferWriteIndex; i++)
		{
			dataSize = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(m_QuadVertexBufferPtr[i]) - reinterpret_cast<uint8_t*>(m_QuadVertexBufferBases[i][frameIndex]));
			if (dataSize)
			{
				uint32_t indexCount = i == m_QuadBufferWriteIndex ? m_QuadIndexCount - (c_MaxIndices * i) : c_MaxIndices;
				m_QuadVertexBuffers[i][frameIndex]->SetData(m_QuadVertexBufferBases[i][frameIndex], dataSize);

				for (uint32_t j = 0; j < m_TextureSlots.size(); j++)
				{
					if (m_TextureSlots[j])
						m_QuadMaterial->Set("u_Textures", m_TextureSlots[j], j);
					else
						m_QuadMaterial->Set("u_Textures", m_WhiteTexture, j);
				}

				Renderer::BeginRenderPass(m_RenderCommandBuffer, m_QuadPass);
				Renderer::RenderGeometry(m_RenderCommandBuffer, m_QuadPass->GetPipeline(), m_QuadMaterial, m_QuadVertexBuffers[i][frameIndex], m_QuadIndexBuffer, glm::mat4(1.0f), indexCount);
				Renderer::EndRenderPass(m_RenderCommandBuffer);

				m_DrawStats.DrawCalls++;
				m_MemoryStats.Used += dataSize;
			}
		}

		// Text
		for (uint32_t i = 0; i <= m_TextBufferWriteIndex; i++)
		{
			dataSize = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(m_TextVertexBufferPtr[i]) - reinterpret_cast<uint8_t*>(m_TextVertexBufferBases[i][frameIndex]));
			if (dataSize)
			{
				uint32_t indexCount = i == m_TextBufferWriteIndex ? m_TextIndexCount - (c_MaxIndices * i) : c_MaxIndices;
				m_TextVertexBuffers[i][frameIndex]->SetData(m_TextVertexBufferBases[i][frameIndex], dataSize);

				for (uint32_t j = 0; j < m_FontTextureSlots.size(); j++)
				{
					if (m_FontTextureSlots[j])
						m_TextMaterial->Set("u_FontAtlases", m_FontTextureSlots[j], j);
					else
						m_TextMaterial->Set("u_FontAtlases", m_WhiteTexture, j);
				}

				Renderer::BeginRenderPass(m_RenderCommandBuffer, m_TextPass);
				Renderer::RenderGeometry(m_RenderCommandBuffer, m_TextPass->GetPipeline(), m_TextMaterial, m_TextVertexBuffers[i][frameIndex], m_TextIndexBuffer, glm::mat4(1.0f), indexCount);
				Renderer::EndRenderPass(m_RenderCommandBuffer);

				m_DrawStats.DrawCalls++;
				m_MemoryStats.Used += dataSize;
			}
		}

		// Lines
		for (uint32_t i = 0; i <= m_LineBufferWriteIndex; i++)
		{
			dataSize = static_cast<uint32_t>(reinterpret_cast<uint8_t*>(m_LineVertexBufferPtr[i]) - reinterpret_cast<uint8_t*>(m_LineVertexBufferBases[i][frameIndex]));
			if (dataSize)
			{
				uint32_t indexCount = i == m_LineBufferWriteIndex ? m_LineIndexCount - (c_MaxLineIndices * i) : c_MaxLineIndices;
				m_LineVertexBuffers[i][frameIndex]->SetData(m_LineVertexBufferBases[i][frameIndex], dataSize);

				Renderer::BeginRenderPass(m_RenderCommandBuffer, m_LinePass);
				Renderer::RenderGeometry(m_RenderCommandBuffer, m_LinePass->GetPipeline(), m_LineMaterial, m_LineVertexBuffers[i][frameIndex], m_LineIndexBuffer, glm::mat4(1.0f), indexCount);
				Renderer::EndRenderPass(m_RenderCommandBuffer);

				m_DrawStats.DrawCalls++;
				m_MemoryStats.Used += dataSize;
			}
		}

		// Insert image barriers to prepare the images for rendering
		if (prepareForRendering)
			PrepareImagesForRendering();

		m_RenderCommandBuffer->End();
		m_RenderCommandBuffer->Submit();
	}

	void Renderer2D::PrepareImagesForRendering()
	{
		Ref<Renderer2D> instance = this;
		Renderer::Submit([instance]() mutable
		{
			// For rendering the final SceneRenderer + Renderer2D color image
			Renderer::InsertImageMemoryBarrier(
				instance->m_RenderCommandBuffer->GetActiveCommandBuffer(),
				instance->m_LinePass->GetOutput(0)->GetVulkanImage(),
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_2_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
			);

			// For rendering the final SceneRenderer + Renderer2D depth image
			Renderer::InsertImageMemoryBarrier(
				instance->m_RenderCommandBuffer->GetActiveCommandBuffer(),
				instance->m_LinePass->GetDepthOutput()->GetVulkanImage(),
				VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_2_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
			);
		});
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
				pipelineSpec.ReleaseShaderModules = true;
				RenderPassSpecification& renderpassSpec = m_QuadPass->GetSpecification();
				renderpassSpec.Pipeline = Pipeline::Create(pipelineSpec);
			}

			{
				PipelineSpecification pipelineSpec = m_LinePass->GetSpecification().Pipeline->GetSpecification();
				pipelineSpec.TargetFramebuffer = framebuffer;
				pipelineSpec.ReleaseShaderModules = true;
				RenderPassSpecification& renderpassSpec = m_LinePass->GetSpecification();
				renderpassSpec.Pipeline = Pipeline::Create(pipelineSpec);
			}
			
			{
				PipelineSpecification pipelineSpec = m_TextPass->GetSpecification().Pipeline->GetSpecification();
				pipelineSpec.TargetFramebuffer = framebuffer;
				pipelineSpec.ReleaseShaderModules = true;
				RenderPassSpecification& renderpassSpec = m_TextPass->GetSpecification();
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

	void Renderer2D::AddTextBuffer()
	{
		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;

		VertexBufferPerFrame& newVertexBuffer = m_TextVertexBuffers.emplace_back();
		TextVertexBasePerFrame& newVertexBufferBase = m_TextVertexBufferBases.emplace_back();

		newVertexBuffer.resize(framesInFlight);
		newVertexBufferBase.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			uint64_t allocationSize = c_MaxVertices * sizeof(TextVertex);
			newVertexBuffer[i] = VertexBuffer::Create(static_cast<uint32_t>(allocationSize));
			m_MemoryStats.TotalAllocated += allocationSize;
			newVertexBufferBase[i] = new TextVertex[c_MaxVertices];
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

	Renderer2D::TextVertex*& Renderer2D::GetWriteableTextBuffer()
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		m_TextBufferWriteIndex = m_TextIndexCount / c_MaxIndices;
		if (m_TextBufferWriteIndex >= m_TextVertexBufferBases.size())
		{
			AddTextBuffer();
			m_TextVertexBufferPtr.emplace_back();
			m_TextVertexBufferPtr[m_TextBufferWriteIndex] = m_TextVertexBufferBases[m_TextBufferWriteIndex][frameIndex];
		}

		return m_TextVertexBufferPtr[m_TextBufferWriteIndex];
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color)
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		constexpr size_t quadVertexCount = 4;
		constexpr float textureIndex = 0.0f; // White Texture
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
		constexpr float tilingFactor = 1.0f;

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
				textureIndex = static_cast<float>(i);
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			textureIndex = static_cast<float>(m_TextureSlotIndex);
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
		constexpr float textureIndex = 0.0f; // White Texture
		constexpr float tilingFactor = 1.0f;

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
				textureIndex = static_cast<float>(i);
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			textureIndex = static_cast<float>(m_TextureSlotIndex);
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
		constexpr float textureIndex = 0.0f; // White Texture
		constexpr float tilingFactor = 1.0f;

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
				textureIndex = static_cast<float>(i);
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			textureIndex = static_cast<float>(m_TextureSlotIndex);
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
		constexpr float textureIndex = 0.0f; // White Texture
		constexpr float tilingFactor = 1.0f;

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
				textureIndex = static_cast<float>(i);
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			textureIndex = static_cast<float>(m_TextureSlotIndex);
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

	void Renderer2D::DrawString(const std::string& string, const glm::vec3& position, float maxWidth, const glm::vec4& color)
	{
		DrawString(string, Font::GetDefaultFont(), position, maxWidth, color);
	}

	void Renderer2D::DrawString(const std::string& string, const Ref<Font>& font, const glm::vec3& position, float maxWidth, const glm::vec4& color)
	{
		DrawString(string, font, glm::translate(glm::mat4(1.0f), position), maxWidth, color);
	}

	void Renderer2D::DrawString(const std::string& string, const Ref<Font>& font, const glm::mat4& transform, float maxWidth, const glm::vec4& color, float lineHeightOffset, float kerning)
	{
		if (string.empty())
			return;

		Ref<Texture2D> fontAtlas = font->GetFontAtlas();
		IR_ASSERT(fontAtlas);

		float textureIndex = 0.0f;
		for (uint32_t i = 0; i < m_FontTextureSlotIndex; i++)
		{
			//if (*m_FontTextureSlots[i].Raw() == *fontAtlas.Raw())
			if (m_FontTextureSlots[i]->GetHash() == fontAtlas->GetHash())
			{
				textureIndex = static_cast<float>(i);
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			textureIndex = static_cast<float>(m_FontTextureSlotIndex);
			m_FontTextureSlots[m_FontTextureSlotIndex] = fontAtlas;
			m_FontTextureSlotIndex++;
		}

		const msdf_atlas::FontGeometry& fontGeometry = font->GetMSDFData()->FontGeometry;
		const msdfgen::FontMetrics& metrics = fontGeometry.getMetrics();

		// TODO: Clean up these font metrics <https://freetype.org/freetype2/docs/glyphs/glyphs-3.html>
		std::vector<int> nextLines;
		{
			double x = 0.0;
			double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
			double y = -fsScale * metrics.ascenderY;
			int lastSpace = -1;

			for (int i = 0; i < static_cast<int>(string.size()); i++)
			{
				char character = string[i];

				if (character == '\n')
				{
					x = 0;
					y -= fsScale * metrics.lineHeight + lineHeightOffset;
					continue;
				}

				if (character == '\r')
					continue;

				const msdf_atlas::GlyphGeometry* glyph = fontGeometry.getGlyph(character);
				if (!glyph)
					continue;

				if (character != ' ')
				{
					double pl, pb, pr, pt;
					glyph->getQuadPlaneBounds(pl, pb, pr, pt);
					glm::vec2 quadMin = { static_cast<float>(pl), static_cast<float>(pb) };
					glm::vec2 quadMax = { static_cast<float>(pr), static_cast<float>(pt) };

					quadMin *= fsScale;
					quadMax *= fsScale;
					quadMin += glm::vec2{ x, y };
					quadMax += glm::vec2{ x, y };

					if (quadMax.x > maxWidth && lastSpace != -1)
					{
						i = lastSpace;
						nextLines.emplace_back(lastSpace);
						lastSpace = -1;
						x = 0;
						y -= fsScale * metrics.lineHeight + lineHeightOffset;
					}
				}
				else
				{
					lastSpace = i;
				}

				double advance = glyph->getAdvance();
				fontGeometry.getAdvance(advance, character, string[i + 1]);
				x += fsScale * advance + kerning;
			}
		}

		{
			double x = 0.0;
			double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
			double y = 0.0;

			auto NextLine = [](int index, const std::vector<int>& lines)
			{
				for (int line : lines)
				{
					if (line == index)
						return true;
				}
				
				return false;
			};

			for (int i = 0; i < static_cast<int>(string.size()); i++)
			{
				char character = string[i];

				if (character == '\n' || NextLine(i, nextLines))
				{
					x = 0;
					y -= fsScale * metrics.lineHeight + lineHeightOffset;
					continue;
				}

				const msdf_atlas::GlyphGeometry* glyph = fontGeometry.getGlyph(character);
				if (!glyph)
					continue;

				double l, b, r, t;
				glyph->getQuadAtlasBounds(l, b, r, t);

				double pl, pb, pr, pt;
				glyph->getQuadPlaneBounds(pl, pb, pr, pt);

				pl *= fsScale;
				pb *= fsScale;
				pr *= fsScale;
				pt *= fsScale;

				pl += x;
				pb += y;
				pr += x;
				pt += y;

				double texelWidth = 1.0 / fontAtlas->GetWidth();
				double texelHeight = 1.0 / fontAtlas->GetHeight();

				l *= texelWidth;
				b *= texelHeight;
				r *= texelWidth;
				t *= texelHeight;

				auto& bufferPtr = GetWriteableTextBuffer();
				bufferPtr->Position = transform * glm::vec4{ pl, pb, 0.0f, 1.0f };
				bufferPtr->Color = color;
				bufferPtr->TexCoord = { l, b };
				bufferPtr->TexIndex = textureIndex;
				bufferPtr++;

				bufferPtr->Position = transform * glm::vec4{ pl, pt, 0.0f, 1.0f };
				bufferPtr->Color = color;
				bufferPtr->TexCoord = { l, t };
				bufferPtr->TexIndex = textureIndex;
				bufferPtr++;

				bufferPtr->Position = transform * glm::vec4{ pr, pt, 0.0f, 1.0f };
				bufferPtr->Color = color;
				bufferPtr->TexCoord = { r, t };
				bufferPtr->TexIndex = textureIndex;
				bufferPtr++;

				bufferPtr->Position = transform * glm::vec4{ pr, pb, 0.0f, 1.0f };
				bufferPtr->Color = color;
				bufferPtr->TexCoord = { r, b };
				bufferPtr->TexIndex = textureIndex;
				bufferPtr++;

				double advance = glyph->getAdvance();
				fontGeometry.getAdvance(advance, character, string[i + 1]);
				x += fsScale * advance * kerning;

				m_TextIndexCount += 6;
				m_DrawStats.QuadCount++;
			}
		}
	}

	void Renderer2D::SetLineWidth(float lineWidth)
	{
		m_LineWidth = lineWidth;

		if (m_LinePass)
			m_LinePass->GetPipeline()->GetSpecification().LineWidth = m_LineWidth;
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