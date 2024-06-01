#include "IrisPCH.h"
#include "Renderer.h"

#include "AssetManager/AssetManager.h"
#include "ComputePass.h"
#include "IndexBuffer.h"
#include "IndexBuffer.h"
#include "Mesh/Material.h"
#include "Mesh/MaterialAsset.h"
#include "Mesh/Mesh.h"
#include "Renderer/Core/RenderCommandBuffer.h"
#include "RenderPass.h"
#include "Scene/SceneEnvironment.h"
#include "Shaders/Compiler/ShaderCompiler.h"
#include "Shaders/Shader.h"
#include "StorageBufferSet.h"
#include "Texture.h"
#include "UniformBufferSet.h"
#include "VertexBuffer.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Iris {

	namespace Utils {

		constexpr static const char* VulkanVendorIDToString(uint32_t vendorID)
		{
			switch (vendorID)
			{
				case 0x10DE: return "NVIDIA";
				case 0x1002: return "AMD";
				case 0x8086: return "INTEL";
				case 0x13B5: return "ARM";
			}

			return "Unknown";
		}

	}

	struct ShaderDependencies
	{
		std::vector<Ref<Pipeline>> Pipelines;
		std::vector<Ref<ComputePipeline>> ComputePipelines;
		std::vector<Ref<Material>> Meterials;
	};

	static std::unordered_map<std::size_t, ShaderDependencies> s_ShaderDependencies;

	struct RendererData
	{
		Ref<ShadersLibrary> ShaderLibrary;

		Ref<Texture2D> BlackTexutre;
		Ref<Texture2D> WhiteTexutre;
		Ref<Texture2D> ErrorTexture;
		Ref<TextureCube> BlackCubeTexture;
		Ref<Environment> EmptyEnvironment;

		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<IndexBuffer> QuadIndexBuffer;

		// ComputePasses and ComputePipelines
		Ref<Material> EquirectToCubemapMaterial;
		Ref<ComputePass> EquirectToCubemapPass;

		//Ref<Material> PreethamSkyMaterial;
		//Ref<ComputePass> PreethamSkyPass;

		std::vector<VkDescriptorPool> DescriptorPools;
		std::vector<uint32_t> DescriptorPoolAllocationCount;

		// Rendering Command Queue
		constexpr static uint32_t c_RenderCommandQueueCount = 2;
		RenderCommandQueue CommandQueue[c_RenderCommandQueueCount];
		std::atomic<uint32_t> RenderCommandQueueSubmissionIndex = 0;

		// Resource Release Queue
		// We create 3 which is corresponding with the max number of frames in flight we might run... (3)
		constexpr static uint32_t c_ResourceFreeQueueCount = 3;
		RenderCommandQueue RendererResourceFreeQueue[c_ResourceFreeQueueCount];

		uint32_t DrawCallCount = 0;

		RendererCapabilities RendererCaps;
	};

	static RendererConfiguration s_RendererConfig;
	static RendererData* s_Data = nullptr;
	
	void Renderer::Init()
	{
		s_Data = new RendererData();
		
		s_RendererConfig.FramesInFlight = glm::min<uint32_t>(s_RendererConfig.FramesInFlight, Application::Get().GetWindow().GetSwapChain().GetImageCount());

		{
			Ref<VulkanPhysicalDevice> physicalDevice = RendererContext::GetCurrentDevice()->GetPhysicalDevice();
			const VkPhysicalDeviceProperties& properties = physicalDevice->GetPhysicalDeviceProperties();

			RendererCapabilities& caps = s_Data->RendererCaps;
			caps.Vendor = Utils::VulkanVendorIDToString(properties.vendorID);
			caps.Device = properties.deviceName;
			caps.Version = std::to_string(properties.driverVersion);
			caps.MaxAnisotropy = properties.limits.maxSamplerAnisotropy;
			caps.MaxSamples = properties.limits.framebufferColorSampleCounts;
			// NOTE: Not sure if this is true but it makes sense...
			caps.MaxTextureUnits = properties.limits.maxPerStageDescriptorSampledImages + properties.limits.maxPerStageDescriptorStorageImages;
		}

		IR_CORE_INFO_TAG("Renderer", "Vendor: {}", s_Data->RendererCaps.Vendor);
		IR_CORE_INFO_TAG("Renderer", "Device: {}", s_Data->RendererCaps.Device);
		IR_CORE_INFO_TAG("Renderer", "Driver Version: {}", s_Data->RendererCaps.Version);

		s_Data->ShaderLibrary = ShadersLibrary::Create();

		s_Data->DescriptorPools.resize(s_RendererConfig.FramesInFlight);
		s_Data->DescriptorPoolAllocationCount.resize(s_RendererConfig.FramesInFlight);


		Renderer::Submit([]()
		{
			// Create Descriptor Pool
			// TODO: What is the situation here? Do we want to keep this? most probably yes since it allocates discriptors for imgui textures and also will allocate descriptors
			// for stuff like environment maps...
			// If for environment maps it does not really work then we should create two descriptor pools in the renderer... One that is global that allocates
			// sets that are persistant across frames and another one that resets at the beginning of every frame kind of like the one we have right now
			// And in case we switch to the 2 descriptor pool setup mentioned above we might want to just cancel the imgui descriptor pool and use the global renderer pool
			VkDescriptorPoolSize poolSizes[] =
			{
				{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
			};

			VkDescriptorPoolCreateInfo poolInfo = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.maxSets = 100000,
				.poolSizeCount = (uint32_t)std::size(poolSizes),
				.pPoolSizes = poolSizes
			};

			VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
			uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;
			for (uint32_t i = 0; i < framesInFlight; i++)
			{
				VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolInfo, nullptr, &s_Data->DescriptorPools[i]));
				s_Data->DescriptorPoolAllocationCount[i] = 0;
			}
		});

		// Create fullscreen quad
		struct QuadVertex
		{
			glm::vec3 Position;
			glm::vec2 TexCoord;
		};

		QuadVertex data[4];
		data[0].Position = glm::vec3(-1.0f, -1.0f, 0.0f);
		data[0].TexCoord = glm::vec2(0.0f, 0.0f);

		data[1].Position = glm::vec3(1.0f, -1.0f, 0.0f);
		data[1].TexCoord = glm::vec2(1.0f, 0.0f);

		data[2].Position = glm::vec3(1.0f, 1.0f, 0.0f);
		data[2].TexCoord = glm::vec2(1.0f, 1.0f);

		data[3].Position = glm::vec3(-1.0f, 1.0f, 0.0f);
		data[3].TexCoord = glm::vec2(0.0f, 1.0f);

		s_Data->QuadVertexBuffer = VertexBuffer::Create(data, 4 * sizeof(QuadVertex));
		uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };
		s_Data->QuadIndexBuffer = IndexBuffer::Create(indices, 6 * sizeof(uint32_t));

		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/Compositing.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/EquirectangularToCubemap.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/Grid.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/IrisPBRStatic.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/JumpFloodComposite.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/JumpFloodInit.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/JumpFloodPass.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/PreDepth.glsl");
		//Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/PreethamSky.glsl"); // TODO:
		// NOTE: For later to be fixed...
		// Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/RayCastedGrid.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/Renderer2D_Line.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/Renderer2D_Quad.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/SelectedGeometry.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/Skybox.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/TexturePass.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/WireFrame.glsl");

		constexpr uint32_t whiteTextureData = 0xffffffff;
		TextureSpecification spec = {
			.DebugName = "WhiteTexture",
			.Width = 1,
			.Height = 1,
			.Format = ImageFormat::RGBA
		};
		s_Data->WhiteTexutre = Texture2D::Create(spec, Buffer((uint8_t*)&whiteTextureData, sizeof(uint32_t)));

		constexpr uint32_t blackTextureData = 0xff000000;
		spec.DebugName = "BlackTexture";
		s_Data->BlackTexutre = Texture2D::Create(spec, Buffer((uint8_t*)&blackTextureData, sizeof(uint32_t)));
		
		constexpr uint32_t errorTextureData = 0xff0000ff;
		spec.DebugName = "ErrorTexture";
		s_Data->ErrorTexture = Texture2D::Create(spec, Buffer((uint8_t*)&errorTextureData, sizeof(uint32_t)));

		constexpr uint32_t blackCubeTextureData[6] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
		spec.DebugName = "BlackCubeTexture";
		s_Data->BlackCubeTexture = TextureCube::Create(spec, Buffer(reinterpret_cast<const uint8_t*>(&blackCubeTextureData), sizeof(blackTextureData)));

		s_Data->EmptyEnvironment = Environment::Create(s_Data->BlackCubeTexture, s_Data->BlackCubeTexture);

		Renderer::Submit([]()
		{
			Ref<Shader> equirectConversionShader = Renderer::GetShadersLibrary()->Get("EquirectangularToCubemap");
			ComputePassSpecification equirRectPassSpec = {
				.DebugName = "EquirectToCubemapPass",
				.Pipeline = ComputePipeline::Create(equirectConversionShader, "EquirectToCubemapPipeline"),
				.MarkerColor = { 0.2f, 0.8f, 0.2f, 1.0f }
			};
			s_Data->EquirectToCubemapPass = ComputePass::Create(equirRectPassSpec);
			s_Data->EquirectToCubemapMaterial = Material::Create(equirectConversionShader, "EquirectToCubemapMaterial");
		
			s_Data->EquirectToCubemapPass->Bake();

			//Ref<Shader> preethamSkyShader = Renderer::GetShadersLibrary()->Get("PreethamSky");
			//ComputePassSpecification preethamSkyPassSpec = {
			//	.DebugName = "PreethamSkyPass",
			//	.Pipeline = ComputePipeline::Create(preethamSkyShader, "PreethamSkyPipeline"),
			//	.MarkerColor = { 0.3f, 0.15f, 0.8f, 1.0f }
			//};
			//s_Data->PreethamSkyPass = ComputePass::Create(preethamSkyPassSpec);
			//s_Data->PreethamSkyMaterial = Material::Create(preethamSkyShader, "PreethamSkyMaterial");

			//s_Data->PreethamSkyPass->Bake();
		});
	}

	void Renderer::Shutdown()
	{
		s_ShaderDependencies.clear();

		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
		vkDeviceWaitIdle(device);

		// Release renderer owned data
		s_Data->ShaderLibrary.Reset();
		s_Data->WhiteTexutre.Reset();
		s_Data->ErrorTexture.Reset();
		s_Data->BlackTexutre.Reset();
		s_Data->BlackCubeTexture.Reset();
		s_Data->EmptyEnvironment.Reset();
		s_Data->QuadVertexBuffer.Reset();
		s_Data->QuadIndexBuffer.Reset();
		s_Data->EquirectToCubemapMaterial.Reset();
		s_Data->EquirectToCubemapPass.Reset();

		// Execute any remaining commands
		Renderer::ExecuteAllRenderCommandQueues();

		for (VkDescriptorPool descriptorPool : s_Data->DescriptorPools)
			vkDestroyDescriptorPool(device, descriptorPool, nullptr);

		ShaderCompiler::ClearUniformAndStorageBuffers();

		// Execute any remaining resource freeing that could be done
		for (uint32_t i = 0; i < RendererData::c_ResourceFreeQueueCount; i++)
		{
			RenderCommandQueue& resourceReleaseQueue = GetRendererResourceReleaseQueue(i);
			resourceReleaseQueue.Execute();
		}

		delete s_Data;
	}

	Ref<ShadersLibrary> Renderer::GetShadersLibrary()
	{
		return s_Data->ShaderLibrary;
	}

	GPUMemoryStats Renderer::GetGPUMemoryStats()
	{
		return VulkanAllocator::GetStats();
	}

	RendererCapabilities& Renderer::GetCapabilities()
	{
		return s_Data->RendererCaps;
	}

	RendererConfiguration& Renderer::GetConfig()
	{
		return s_RendererConfig;
	}

	void Renderer::SetConfig(const RendererConfiguration& config)
	{
		s_RendererConfig = config;
	}

	void Renderer::ExecuteAllRenderCommandQueues()
	{
		for (uint32_t i = 0; i < RendererData::c_RenderCommandQueueCount; i++)
		{
			RenderCommandQueue& resourceReleaseQueue = GetRenderCommandQueue();
			resourceReleaseQueue.Execute();
			SwapQueues();
		}
	}

	void Renderer::SwapQueues()
	{
		s_Data->RenderCommandQueueSubmissionIndex = (s_Data->RenderCommandQueueSubmissionIndex + 1) % RendererData::c_RenderCommandQueueCount;
	}

	void Renderer::WaitAndRender(RenderThread* renderThread)
	{
		{
			Timer waitTimer;

			renderThread->WaitAndSet(ThreadState::Kick, ThreadState::Busy);

			waitTimer.ElapsedMillis();
		}

		Timer workTimer;

		s_Data->CommandQueue[GetRenderQueueIndex()].Execute();
		// Rendering complete, set state back to idle
		renderThread->Set(ThreadState::Idle);

		workTimer.ElapsedMillis();
	}

	void Renderer::RenderThreadFunc(RenderThread* renderThread)
	{
		while (renderThread->IsRunning())
		{
			Renderer::WaitAndRender(renderThread);
		}
	}

	uint32_t Renderer::GetRenderQueueIndex()
	{
		return (s_Data->RenderCommandQueueSubmissionIndex + 1) % RendererData::c_RenderCommandQueueCount;
	}

	uint32_t Renderer::GetRenderQueueSubmissionIndex()
	{
		return s_Data->RenderCommandQueueSubmissionIndex;
	}

	uint32_t Renderer::GetMainThreadResourceFreeingQueueIndex()
	{
		return (Renderer::GetCurrentFrameIndex() + 1) % RendererData::c_ResourceFreeQueueCount;
	}

	void Renderer::BeginFrame()
	{
		Renderer::Submit([]()
		{
			Ref<VulkanDevice> logicalDevice = RendererContext::GetCurrentDevice();
			VkDevice device = logicalDevice->GetVulkanDevice();

			// Reset the command pools of the temporary and secondary command buffers
			logicalDevice->GetOrCreateThreadLocalCommandPool()->Reset();

			uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
			vkResetDescriptorPool(device, s_Data->DescriptorPools[bufferIndex], 0);
			std::memset(s_Data->DescriptorPoolAllocationCount.data(), 0, s_Data->DescriptorPoolAllocationCount.size() * sizeof(uint32_t));

			s_Data->DrawCallCount = 0;
		});
	}

	void Renderer::EndFrame()
	{
		// IR_CORE_DEBUG("DescriptorPoolAllocationCount: {0}", s_Data->DescriptorPoolAllocationCount[Renderer::GetCurrentFrameIndex()]);
	}

	void Renderer::BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RenderPass> renderPass, bool explicitClear)
	{
		Renderer::Submit([renderCommandBuffer, renderPass, explicitClear]() mutable
		{
			// IR_CORE_TRACE_TAG("Renderer", "BeginRenderPass - {}", renderPass->GetSpecification().DebugName);

			VkCommandBuffer commandBuffer = renderCommandBuffer->GetActiveCommandBuffer();
			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();

			VkDebugUtilsLabelEXT debugLabel = {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
				.pLabelName = renderPass->GetSpecification().DebugName.c_str()
			};
			std::memcpy(&debugLabel.color, glm::value_ptr(renderPass->GetSpecification().MarkerColor), 4 * sizeof(float));
			fpCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);

			Ref<Framebuffer> framebuffer = renderPass->GetTargetFramebuffer();
			const FramebufferSpecification& fbSpec = framebuffer->GetSpecification();

			uint32_t width = framebuffer->GetWidth();
			uint32_t height = framebuffer->GetHeight();

			VkViewport viewport = {
				.minDepth = 0.0f,
				.maxDepth = 1.0f
			};

			VkRenderPassBeginInfo renderPassBeginInfo = {
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.pNext = nullptr
			};
			if (fbSpec.SwapchainTarget == true)
			{
				SwapChain& swapChain = Application::Get().GetWindow().GetSwapChain();
				width = swapChain.GetWidth();
				height = swapChain.GetHeight();
				renderPassBeginInfo.renderPass = framebuffer->GetVulkanRenderPass();
				renderPassBeginInfo.framebuffer = swapChain.GetCurrentFramebuffer();
				renderPassBeginInfo.renderArea = {
					.offset = {.x = 0, .y = 0 },
					.extent = {.width = width, .height = height }
				};

				// Here we flip the viewport so that the image to display is not flipped
				viewport.x = 0.0f;
				viewport.y = static_cast<float>(height);
				// viewport.y = 0.0f;
				viewport.width = static_cast<float>(width);
				viewport.height = -static_cast<float>(height);
				// viewport.height = static_cast<float>(height);
			}
			else
			{
				width = framebuffer->GetWidth();
				height = framebuffer->GetHeight();
				renderPassBeginInfo.renderPass = framebuffer->GetVulkanRenderPass();
				renderPassBeginInfo.framebuffer = framebuffer->GetVulkanFramebuffer();
				renderPassBeginInfo.renderArea = {
					.offset = {.x = 0, .y = 0 },
					.extent = {.width = width, .height = height }
				};

				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = static_cast<float>(width);
				viewport.height = static_cast<float>(height);
			}

			const std::vector<VkClearValue>& clearValues = framebuffer->GetVulkanClearValues();
			renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
			renderPassBeginInfo.pClearValues = clearValues.data();

			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			if (explicitClear)
			{
				const uint32_t colorAttachmentCount = static_cast<uint32_t>(framebuffer->GetColorAttachmentCount());
				const uint32_t totalAttachmentCount = colorAttachmentCount + (framebuffer->HasDepthAttachment() ? 1 : 0);
				IR_ASSERT(clearValues.size() == totalAttachmentCount, "");

				std::vector<VkClearAttachment> attachments(totalAttachmentCount);
				std::vector<VkClearRect> clearRects(totalAttachmentCount);
				for (uint32_t i = 0; i < colorAttachmentCount; i++)
				{
					attachments[i] = VkClearAttachment{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.colorAttachment = i,
						.clearValue = clearValues[i]
					};

					clearRects[i] = VkClearRect{
						.rect = {
							.offset = { 0, 0 },
							.extent = {.width = width, .height = height }
						},
						.baseArrayLayer = 0,
						.layerCount = 1
					};
				}

				if (framebuffer->HasDepthAttachment())
				{
					attachments[colorAttachmentCount] = VkClearAttachment{
						// We do not need the stencil aspect since we are not using stencil buffers in our renderer
						.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT /* | VK_IMAGE_ASPECT_STENCIL_BIT */,
						.clearValue = clearValues[colorAttachmentCount]
					};

					clearRects[colorAttachmentCount] = VkClearRect{
						.rect = {
							.offset = { 0, 0 },
							.extent = {.width = width, .height = height }
						},
						.baseArrayLayer = 0,
						.layerCount = 1
					};
				}

				vkCmdClearAttachments(commandBuffer, totalAttachmentCount, attachments.data(), totalAttachmentCount, clearRects.data());
			}

			// Update dynamic viewport and scissor state
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			VkRect2D scissor = {
				.offset = { 0, 0 },
				.extent = {.width = width, .height = height }
			};
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			Ref<Pipeline> pipeline = renderPass->GetPipeline();
			VkPipeline vulkanPipeline = pipeline->GetVulkanPipeline();
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipeline);

			if (pipeline->IsDynamicLineWidth())
				vkCmdSetLineWidth(commandBuffer, pipeline->GetSpecification().LineWidth);

			renderPass->Prepare();
			if (renderPass->HasDescriptorSets())
			{
				const std::vector<VkDescriptorSet>& descriptorSets = renderPass->GetDescriptorSets(frameIndex);

				vkCmdBindDescriptorSets(
					commandBuffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					pipeline->GetVulkanPipelineLayout(),
					renderPass->GetFirstSetIndex(),
					static_cast<uint32_t>(descriptorSets.size()),
					descriptorSets.data(),
					0, nullptr
				);
			}
		});
	}

	void Renderer::EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		Renderer::Submit([renderCommandBuffer]()
		{
			VkCommandBuffer commandBuffer = renderCommandBuffer->GetActiveCommandBuffer();

			vkCmdEndRenderPass(commandBuffer);
			fpCmdEndDebugUtilsLabelEXT(commandBuffer);
		});
	}

	void Renderer::SubmitFullScreenQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<Material> material)
	{
		Renderer::Submit([renderCommandBuffer, pipeline, material]() mutable
		{
			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer vkCommandBuffer = renderCommandBuffer->GetActiveCommandBuffer();

			VkPipelineLayout layout = pipeline->GetVulkanPipelineLayout();

			VkBuffer vbQuadBuffer = s_Data->QuadVertexBuffer->GetVulkanBuffer();
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(vkCommandBuffer, 0, 1, &vbQuadBuffer, offsets);

			VkBuffer ibQuadBuffer = s_Data->QuadIndexBuffer->GetVulkanBuffer();
			vkCmdBindIndexBuffer(vkCommandBuffer, ibQuadBuffer, 0, VK_INDEX_TYPE_UINT32);

			if (material)
			{
				VkDescriptorSet descSet = material->GetDescriptorSet(frameIndex);
				if (descSet)
					vkCmdBindDescriptorSets(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, material->GetFirstSetIndex(), 1, &descSet, 0, nullptr);

				Buffer uniformStorageBuffer = material->GetUniformStorageBuffer();
				if (uniformStorageBuffer)
					vkCmdPushConstants(
						vkCommandBuffer,
						layout,
						VK_SHADER_STAGE_FRAGMENT_BIT,
						0,
						static_cast<uint32_t>(uniformStorageBuffer.Size),
						uniformStorageBuffer.Data);
			}

			vkCmdDrawIndexed(vkCommandBuffer, s_Data->QuadIndexBuffer->GetCount(), 1, 0, 0, 0);
			s_Data->DrawCallCount++;
		});
	}

	void Renderer::SubmitFullScreenQuadWithOverrides(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<Material> material, Buffer vertexShaderOverrides, Buffer fragmentShaderOverrides)
	{
		Buffer vertexPushConstantBuffer;
		if (vertexShaderOverrides)
		{
			vertexPushConstantBuffer.Allocate(vertexShaderOverrides.Size);
			vertexPushConstantBuffer.Write(vertexShaderOverrides.Data, vertexShaderOverrides.Size);
		}

		Buffer fragmentPushConstantBuffer;
		if (fragmentShaderOverrides)
		{
			fragmentPushConstantBuffer.Allocate(fragmentShaderOverrides.Size);
			fragmentPushConstantBuffer.Write(fragmentShaderOverrides.Data, fragmentShaderOverrides.Size);
		}

		Renderer::Submit([renderCommandBuffer, pipeline, material, vertexPushConstantBuffer, fragmentPushConstantBuffer]() mutable
		{
			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer vkCommandBuffer = renderCommandBuffer->GetActiveCommandBuffer();

			VkPipelineLayout layout = pipeline->GetVulkanPipelineLayout();

			VkBuffer vbQuadBuffer = s_Data->QuadVertexBuffer->GetVulkanBuffer();
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(vkCommandBuffer, 0, 1, &vbQuadBuffer, offsets);

			VkBuffer ibQuadBuffer = s_Data->QuadIndexBuffer->GetVulkanBuffer();
			vkCmdBindIndexBuffer(vkCommandBuffer, ibQuadBuffer, 0, VK_INDEX_TYPE_UINT32);

			if (material)
			{
				VkDescriptorSet descSet = material->GetDescriptorSet(frameIndex);
				if (descSet)
					vkCmdBindDescriptorSets(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, material->GetFirstSetIndex(), 1, &descSet, 0, nullptr);

				if (vertexPushConstantBuffer)
					vkCmdPushConstants(vkCommandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<uint32_t>(vertexPushConstantBuffer.Size), vertexPushConstantBuffer.Data);

				if (fragmentPushConstantBuffer)
					vkCmdPushConstants(vkCommandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, static_cast<uint32_t>(vertexPushConstantBuffer.Size), static_cast<uint32_t>(fragmentPushConstantBuffer.Size), fragmentPushConstantBuffer.Data);
			}

			vkCmdDrawIndexed(vkCommandBuffer, s_Data->QuadIndexBuffer->GetCount(), 1, 0, 0, 0);
			s_Data->DrawCallCount++;

			vertexPushConstantBuffer.Release();
			fragmentPushConstantBuffer.Release();
		});
	}

	void Renderer::RenderStaticMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource, uint32_t subMeshIndex, Ref<MaterialTable> materialTable, Ref<VertexBuffer> transformBuffer, uint32_t transformOffset, uint32_t instanceCount, int viewMode)
	{
		IR_VERIFY(staticMesh);
		IR_VERIFY(meshSource);
		IR_VERIFY(materialTable);

		Renderer::Submit([renderCommandBuffer, pipeline, staticMesh, meshSource, subMeshIndex, materialTable, transformBuffer, transformOffset, instanceCount, viewMode]() mutable
		{
			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer->GetActiveCommandBuffer();

			Ref<VertexBuffer> meshVB = meshSource->GetVertexBuffer();
			VkBuffer vulkanMeshVB = meshVB->GetVulkanBuffer();
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vulkanMeshVB, offsets);

			VkBuffer vulkanTransformVB = transformBuffer->GetVulkanBuffer();
			VkDeviceSize instanceOffset[1] = { transformOffset };
			vkCmdBindVertexBuffers(commandBuffer, 1, 1, &vulkanTransformVB, instanceOffset);

			Ref<IndexBuffer> meshIB = meshSource->GetIndexBuffer();
			VkBuffer vulkanMeshIB = meshIB->GetVulkanBuffer();
			vkCmdBindIndexBuffer(commandBuffer, vulkanMeshIB, 0, VK_INDEX_TYPE_UINT32);

			const auto& subMeshes = meshSource->GetSubMeshes();
			const MeshUtils::SubMesh& subMesh = subMeshes[subMeshIndex];
			Ref<MaterialTable> meshMaterialTable = staticMesh->GetMaterials();
			AssetHandle materialAssetHandle = materialTable->HasMaterial(subMesh.MaterialIndex) ?
				materialTable->GetMaterial(subMesh.MaterialIndex) :
				meshMaterialTable->GetMaterial(subMesh.MaterialIndex);

			Ref<MaterialAsset> materialAsset = AssetManager::GetAsset<MaterialAsset>(materialAssetHandle);

			if (viewMode == 0)
				materialAsset->SetLit();
			else if (viewMode == 1 || viewMode == 2)
				materialAsset->SetUnlit();

			Ref<Material> vulkanMaterial = materialAsset->GetMaterial();

			VkPipelineLayout layout = pipeline->GetVulkanPipelineLayout();

			VkDescriptorSet descriptorSet = vulkanMaterial->GetDescriptorSet(frameIndex);
			if (descriptorSet)
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, vulkanMaterial->GetFirstSetIndex(), 1, &descriptorSet, 0, nullptr);

			Buffer uniformStorageBuffer = vulkanMaterial->GetUniformStorageBuffer();
			vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, static_cast<uint32_t>(uniformStorageBuffer.Size), uniformStorageBuffer.Data);

			vkCmdDrawIndexed(commandBuffer, subMesh.IndexCount, instanceCount, subMesh.BaseIndex, subMesh.BaseVertex, 0);
			s_Data->DrawCallCount++;
		});
	}

	void Renderer::RenderStaticMeshWithMaterial(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource, uint32_t subMeshIndex, Ref<Material> material, Ref<VertexBuffer> transformBuffer, uint32_t transformOffset, uint32_t instanceCount)
	{
		IR_VERIFY(staticMesh);
		IR_VERIFY(meshSource);
		IR_VERIFY(material);

		Renderer::Submit([renderCommandBuffer, pipeline, staticMesh, meshSource, subMeshIndex, material, transformBuffer, transformOffset, instanceCount]() mutable
		{
			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer->GetActiveCommandBuffer();

			Ref<VertexBuffer> meshVB = meshSource->GetVertexBuffer();
			VkBuffer vulkanMeshVB = meshVB->GetVulkanBuffer();
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vulkanMeshVB, offsets);

			VkBuffer vulkanTrasformVB = transformBuffer->GetVulkanBuffer();
			VkDeviceSize instanceOffset[1] = { transformOffset };
			vkCmdBindVertexBuffers(commandBuffer, 1, 1, &vulkanTrasformVB, instanceOffset);

			Ref<IndexBuffer> meshIB = meshSource->GetIndexBuffer();
			VkBuffer vulkanMeshIB = meshIB->GetVulkanBuffer();
			vkCmdBindIndexBuffer(commandBuffer, vulkanMeshIB, 0, VK_INDEX_TYPE_UINT32);

			VkPipelineLayout layout = pipeline->GetVulkanPipelineLayout();

			// We do not do this since the passes that use this method do not have descriptor sets.. only push constants
			// VkDescriptorSet descriptorSet = material->GetDescriptorSet(frameIndex);
			// if (descriptorSet)
			// 	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, material->GetFirstSetIndex(), 1, &descriptorSet, 0, nullptr);

			uint32_t pushConstantOffset = 0;
			Buffer uniformStorageBuffer = material->GetUniformStorageBuffer();
			if (uniformStorageBuffer)
			{
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, pushConstantOffset, static_cast<uint32_t>(uniformStorageBuffer.Size), uniformStorageBuffer.Data);
			}

			const auto& subMeshes = meshSource->GetSubMeshes();
			const auto& subMesh = subMeshes[subMeshIndex];

			vkCmdDrawIndexed(commandBuffer, subMesh.IndexCount, instanceCount, subMesh.BaseIndex, subMesh.BaseVertex, 0);
			s_Data->DrawCallCount++;
		});
	}

	void Renderer::RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<Material> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const glm::mat4& transform, uint32_t indexCount)
	{
		if (indexCount == 0)
			indexCount = indexBuffer->GetCount();

		Renderer::Submit([renderCommandBuffer, pipeline, material, vertexBuffer, indexBuffer, transform, indexCount]() mutable
		{
			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandbuffer = renderCommandBuffer->GetActiveCommandBuffer();
			VkPipelineLayout layout = pipeline->GetVulkanPipelineLayout();

			VkBuffer vbBuffer = vertexBuffer->GetVulkanBuffer();
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(commandbuffer, 0, 1, &vbBuffer, &offset);

			VkBuffer ibBuffer = indexBuffer->GetVulkanBuffer();
			vkCmdBindIndexBuffer(commandbuffer, ibBuffer, 0, VK_INDEX_TYPE_UINT32);

			VkDescriptorSet descSet = material->GetDescriptorSet(frameIndex);
			if (descSet)
				vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, material->GetFirstSetIndex(), 1, &descSet, 0, nullptr);

			vkCmdPushConstants(commandbuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
			Buffer uniformStorageBuffer = material->GetUniformStorageBuffer();
			if (uniformStorageBuffer)
				vkCmdPushConstants(commandbuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), static_cast<uint32_t>(uniformStorageBuffer.Size), uniformStorageBuffer.Data);

			vkCmdDrawIndexed(commandbuffer, indexCount, 1, 0, 0, 0);
			// NOTE: Here we do not increase the DrawCallCount since this is only called in the Renderer2D for now and that has its own draw call counter
		});
	}

	static VkCommandBuffer s_CurrentCommandBuffer = nullptr;

	void Renderer::BeginComputePass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass)
	{
		Renderer::Submit([renderCommandBuffer, computePass]() mutable
		{
			const uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			//VkCommandBuffer commandBuffer = renderCommandBuffer->GetActiveCommandBuffer();
			VkCommandBuffer commandBuffer = RendererContext::GetCurrentDevice()->GetCommandBuffer(true, true);
			s_CurrentCommandBuffer = commandBuffer;

			VkDebugUtilsLabelEXT debugLabel = {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
				.pLabelName = computePass->GetSpecification().DebugName.c_str()
			};
			std::memcpy(&debugLabel.color, glm::value_ptr(computePass->GetSpecification().MarkerColor), 4 * sizeof(float));
			fpCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);

			// Bind the pipeline
			Ref<ComputePipeline> pipeline = computePass->GetPipeline();
			//pipeline->RT_Begin(renderCommandBuffer); // TODO: This makes the compute pipeline use the Graphics queue instead of using the dedicated Compute queue
			pipeline->RT_Begin(commandBuffer);

			computePass->Prepare();
			if (computePass->HasDescriptorSets())
			{
				const auto& descriptorSets = computePass->GetDescriptorSets(frameIndex);
				vkCmdBindDescriptorSets(
					commandBuffer,
					VK_PIPELINE_BIND_POINT_COMPUTE,
					pipeline->GetPipelineLayout(),
					computePass->GetFirstSetIndex(),
					static_cast<uint32_t>(descriptorSets.size()),
					descriptorSets.data(),
					0,
					0
				);
			}
		});
	}

	void Renderer::EndComputePass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass)
	{
		Renderer::Submit([renderCommandBuffer, computePass]() mutable
		{
			computePass->GetPipeline()->End();
			RendererContext::GetCurrentDevice()->FlushCommandBuffer(s_CurrentCommandBuffer, true);
			fpCmdEndDebugUtilsLabelEXT(renderCommandBuffer->GetActiveCommandBuffer());
		});
	}

	void Renderer::DispatchComputePass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass, Ref<Material> material, const glm::uvec3& workGroups, Buffer constants)
	{
		Buffer pushConstantsBuffer;
		if (constants)
			pushConstantsBuffer = Buffer::Copy(constants);

		Renderer::Submit([renderCommandBuffer, computePass, material, workGroups, pushConstantsBuffer]() mutable
		{
			const uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			//VkCommandBuffer commandBuffer = renderCommandBuffer->GetActiveCommandBuffer();
			VkCommandBuffer commandBuffer = s_CurrentCommandBuffer;

			Ref<ComputePipeline> pipeline = computePass->GetPipeline();

			if (material)
			{
				VkDescriptorSet descriptorSet = material->GetDescriptorSet(frameIndex);
				if (descriptorSet)
					vkCmdBindDescriptorSets(
						commandBuffer,
						VK_PIPELINE_BIND_POINT_COMPUTE,
						pipeline->GetPipelineLayout(),
						material->GetFirstSetIndex(),
						1,
						&descriptorSet,
						0,
						nullptr
					);
			}

			if (pushConstantsBuffer)
			{
				pipeline->SetPushConstants(pushConstantsBuffer);
				pushConstantsBuffer.Release();
			}

			pipeline->Dispatch(workGroups);
		});
	}

	std::pair<Ref<TextureCube>, Ref<TextureCube>> Renderer::CreateEnvironmentMap(Ref<RenderCommandBuffer> renderCommandBuffer, const std::string& filepath)
	{
		/*
		 * TODO: Creating scene environment should not really be like this... the pipelines and compute passes should be created once and stored
		 * for use multiple times and we should run through descriptor set managers and baking the descriptor sets instead of manually setting descriptors...
		 * 
		 * - Maybe this Renderer could own those ComputePass/Pipeline objects and everytime we call this function we go through the different pipelines
		 *   to get at the end the desired result (mip filtered Radiance map, and and Irradiance map)
		 */

		if (!Renderer::GetConfig().ComputeEnvironmentMaps)
			return { Renderer::GetBlackCubeTexture(), Renderer::GetBlackCubeTexture() };
		
		const uint32_t cubemapSize = Renderer::GetConfig().EnvironmentMapResolution;
		constexpr uint32_t irradianceMapSize = 32;
		
		Ref<Texture2D> envEquirect = Texture2D::Create(TextureSpecification(), filepath);
		IR_VERIFY(envEquirect->GetFormat() == ImageFormat::RGBA32F, "Texture is not HDR!");
		
		TextureSpecification cubemapSpec;
		cubemapSpec.Format = ImageFormat::RGBA16F;
		cubemapSpec.Width = cubemapSize;
		cubemapSpec.Height = cubemapSize;
		
		Ref<TextureCube> envUnfiltered = TextureCube::Create(cubemapSpec);
		Ref<TextureCube> envFiltered = TextureCube::Create(cubemapSpec);
		
		s_Data->EquirectToCubemapMaterial->Set("o_OutputCubeMap", envUnfiltered);
		s_Data->EquirectToCubemapMaterial->Set("u_EquirectangularTexture", envEquirect);

		// Convert equirectangular to cubemap
		Renderer::BeginComputePass(renderCommandBuffer, s_Data->EquirectToCubemapPass);
		Renderer::DispatchComputePass(renderCommandBuffer, s_Data->EquirectToCubemapPass, s_Data->EquirectToCubemapMaterial, { cubemapSize / 32, cubemapSize / 32, 6 });

		Renderer::Submit([&commandBuffer = s_CurrentCommandBuffer, envUnfiltered]()
		{
			// NOTE: Here we need to set both src and dst pipeline stages to be COMPUTE_SHADER since the image that is created here is used in the upcoming two filtering stages
			Renderer::InsertImageMemoryBarrier(
				commandBuffer,
				envUnfiltered->GetVulkanImage(),
				VK_ACCESS_SHADER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 6 }
			);
		});

		Renderer::EndComputePass(renderCommandBuffer, s_Data->EquirectToCubemapPass);

		return { envUnfiltered, nullptr }; // TODO:
		// return {};
	}

	//Ref<TextureCube> Renderer::CreatePreethamSky(float turbidity, float azimuth, float inclination)
	//{
	//	const uint32_t cubemapSize = Renderer::GetConfig().EnvironmentMapResolution;

	//	TextureSpecification cubemapSpec;
	//	cubemapSpec.Format = ImageFormat::RGBA32F;
	//	cubemapSpec.Width = cubemapSize;
	//	cubemapSpec.Height = cubemapSize;

	//	Ref<TextureCube> environmentMap = TextureCube::Create(cubemapSpec);

	//	glm::vec3 params = { turbidity, azimuth, inclination };
	//	s_Data->PreethamSkyMaterial->Set("o_OutputCubeMap", environmentMap);
	//	s_Data->PreethamSkyMaterial->Set("u_Uniforms.TurbidityAzimuthInclination", params);

	//	Renderer::BeginComputePass(nullptr, s_Data->PreethamSkyPass);
	//	Renderer::DispatchComputePass(nullptr, s_Data->PreethamSkyPass, s_Data->PreethamSkyMaterial, { cubemapSize / 32, cubemapSize / 32, 6 });

	//	Renderer::Submit([&commandBuffer = s_CurrentCommandBuffer, environmentMap]()
	//	{
	//		// NOTE: Here we need to set both src and dst pipeline stages to be COMPUTE_SHADER since the image that is created here is used in the upcoming two filtering stages
	//		Renderer::InsertImageMemoryBarrier(
	//			commandBuffer,
	//			environmentMap->GetVulkanImage(),
	//			VK_ACCESS_SHADER_WRITE_BIT,
	//			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
	//			VK_IMAGE_LAYOUT_UNDEFINED,
	//			VK_IMAGE_LAYOUT_GENERAL,
	//			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	//			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	//			{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 6 }
	//		);
	//	});

	//	Renderer::EndComputePass(nullptr, s_Data->PreethamSkyPass);

	//	return environmentMap;
	//}

	VkDescriptorSet Renderer::RT_AllocateDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo)
	{
		VkDescriptorSet result;

		uint32_t bufferIndex = Renderer::RT_GetCurrentFrameIndex();
		allocInfo.descriptorPool = s_Data->DescriptorPools[bufferIndex];
		VK_CHECK_RESULT(vkAllocateDescriptorSets(RendererContext::GetCurrentDevice()->GetVulkanDevice(), &allocInfo, &result));
		s_Data->DescriptorPoolAllocationCount[bufferIndex] += allocInfo.descriptorSetCount;

		return result;
	}

	Ref<Texture2D> Renderer::GetWhiteTexture()
	{
		return s_Data->WhiteTexutre;
	}

	Ref<Texture2D> Renderer::GetBlackTexture()
	{
		return s_Data->BlackTexutre;
	}

	Ref<Texture2D> Renderer::GetErrorTexture()
	{
		return s_Data->ErrorTexture;
	}

	Ref<TextureCube> Renderer::GetBlackCubeTexture()
	{
		return s_Data->BlackCubeTexture;
	}

	Ref<Environment> Renderer::GetEmptyEnvironment()
	{
		return s_Data->EmptyEnvironment;
	}

	uint32_t Renderer::GetTotalDrawCallCount()
	{
		return s_Data->DrawCallCount;
	}

	RenderCommandQueue& Renderer::GetRenderCommandQueue()
	{
		return s_Data->CommandQueue[s_Data->RenderCommandQueueSubmissionIndex];
	}

	RenderCommandQueue& Renderer::GetRendererResourceReleaseQueue(uint32_t index)
	{
		return s_Data->RendererResourceFreeQueue[index];
	}

	void Renderer::RegisterShaderDependency(Ref<Shader> shader, Ref<Pipeline> pipeline)
	{
		s_ShaderDependencies[shader->GetHash()].Pipelines.push_back(pipeline);
	}

	void Renderer::RegisterShaderDependency(Ref<Shader> shader, Ref<ComputePipeline> computePipeline)
	{
		s_ShaderDependencies[shader->GetHash()].ComputePipelines.push_back(computePipeline);
	}

	void Renderer::RegisterShaderDependency(Ref<Shader> shader, Ref<Material> material)
	{
		s_ShaderDependencies[shader->GetHash()].Meterials.push_back(material);
	}

	void Renderer::OnShaderReloaded(std::size_t hash)
	{
		if (s_ShaderDependencies.contains(hash))
		{
			ShaderDependencies& deps = s_ShaderDependencies.at(hash);
			for (Ref<Pipeline>& pipeline : deps.Pipelines)
			{
				IR_CORE_TRACE_TAG("Renderer", "Invalidating pipeline ({0}) for shader ({1}) reload request", pipeline->GetSpecification().DebugName, pipeline->GetSpecification().Shader->GetName());
				pipeline->Invalidate();
			}

			for (Ref<ComputePipeline>& pipeline : deps.ComputePipelines)
			{
				IR_CORE_TRACE_TAG("Renderer", "Invalidating pipeline ({0}) for shader ({1}) reload request", pipeline->GetDebugName(), pipeline->GetShader()->GetName());
				pipeline->CreatePipeline();
			}

			for (Ref<Material>& material : deps.Meterials)
			{
				IR_CORE_TRACE_TAG("Renderer", "Reloading material ({0}) for shader ({1}) reload request", material->GetName(), material->GetShader()->GetName());
				material->OnShaderReloaded();
			}
		}
	}

	void Renderer::SetImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkPipelineStageFlags srcStageMask,
		VkPipelineStageFlags dstStageMask,
		VkImageSubresourceRange subresourceRange
	)
	{
		// Create an image barrier object
		VkImageMemoryBarrier imageMemoryBarrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.oldLayout = oldImageLayout,
			.newLayout = newImageLayout,
			.image = image,
			.subresourceRange = subresourceRange
		};
		imageMemoryBarrier.oldLayout = oldImageLayout;
		imageMemoryBarrier.newLayout = newImageLayout;
		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange = subresourceRange;

		// Source layouts (old)
		// Source access mask controls actions that have to be finished on the old layout
		// before it will be transitioned to the new layout
		switch (oldImageLayout)
		{
			case VK_IMAGE_LAYOUT_UNDEFINED:
				// Image layout is undefined (or does not matter)
				// Only valid as initial layout
				// No flags required, listed only for completeness
				imageMemoryBarrier.srcAccessMask = 0;
				break;

			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				// Image is preinitialized
				// Only valid as initial layout for linear images, preserves memory contents
				// Make sure host writes have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image is a color attachment
				// Make sure any writes to the color buffer have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image is a depth/stencil attachment
				// Make sure any writes to the depth/stencil buffer have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image is a transfer source
				// Make sure any reads from the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image is a transfer destination
				// Make sure any writes to the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image is read by a shader
				// Make sure any shader reads from the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
		}

		// Target layouts (new)
		// Destination access mask controls the dependency for the new image layout
		switch (newImageLayout)
		{
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image will be used as a transfer destination
				// Make sure any writes to the image have been finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image will be used as a transfer source
				// Make sure any reads from the image have been finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image will be used as a color attachment
				// Make sure any writes to the color buffer have been finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image layout will be used as a depth/stencil attachment
				// Make sure any writes to depth/stencil buffer have been finished
				imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image will be read in a shader (sampler, input attachment)
				// Make sure any writes to the image have been finished
				if (imageMemoryBarrier.srcAccessMask == 0)
				{
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
				}
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
		}

		// Put barrier inside setup command buffer
		vkCmdPipelineBarrier(
			cmdbuffer,
			srcStageMask,
			dstStageMask,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier
		);
	}

	void Renderer::InsertMemoryBarrier(
		VkCommandBuffer commandBuffer,
		VkAccessFlags srcAccessMask,
		VkAccessFlags dstAccessMask,
		VkPipelineStageFlags srcStageMask,
		VkPipelineStageFlags dstStageMask
	)
	{
		VkMemoryBarrier memoryBarrier = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = srcAccessMask,
			.dstAccessMask = dstAccessMask
		};

		vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	}

	void Renderer::InsertBufferMemoryBarrier(
		VkCommandBuffer commandBuffer,
		VkBuffer buffer,
		VkAccessFlags srcAccessMask,
		VkAccessFlags dstAccessMask,
		VkPipelineStageFlags srcStageMask,
		VkPipelineStageFlags dstStageMask
	)
	{
		VkBufferMemoryBarrier bufferMemBarrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = srcAccessMask,
			.dstAccessMask = dstAccessMask,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = buffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE
		};

		vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 1, &bufferMemBarrier, 0, nullptr);
	}

	void Renderer::InsertImageMemoryBarrier(
		VkCommandBuffer commandBuffer,
		VkImage image,
		VkAccessFlags srcAccessMask,
		VkAccessFlags dstAccessMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkPipelineStageFlags srcStageMask,
		VkPipelineStageFlags dstStageMask,
		VkImageSubresourceRange subresourceRange
	)
	{
		VkImageMemoryBarrier imageMemBarrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = srcAccessMask,
			.dstAccessMask = dstAccessMask,
			.oldLayout = oldImageLayout,
			.newLayout = newImageLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = subresourceRange
		};

		vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageMemBarrier);
	}

	uint32_t Renderer::GetCurrentFrameIndex()
	{
		return Application::Get().GetCurrentFrameIndex();
	}

	uint32_t Renderer::RT_GetCurrentFrameIndex()
	{
		return Application::Get().GetWindow().GetSwapChain().GetCurrentBufferIndex();
	}

}