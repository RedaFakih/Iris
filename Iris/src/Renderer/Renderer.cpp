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
		Ref<Texture2D> BRDFLutTexture;
		Ref<TextureCube> BlackCubeTexture;
		Ref<Environment> EmptyEnvironment;

		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<IndexBuffer> QuadIndexBuffer;

		// ComputePasses and ComputePipelines
		Ref<ComputePass> EquirectToCubemapPass;
		Ref<Material> EquirectToCubemapMaterial;

		// No material for this pass since this pass will be per mip and we have to handle the descriptors manually since we generate the image views for each image on the fly
		Ref<ComputePass> MipChainFilterPass;

		Ref<ComputePass> IrradianceMapPass;
		Ref<Material> IrradianceMapMaterial;

		Ref<ComputePass> PreethamSkyPass;
		Ref<Material> PreethamSkyMaterial;

		std::vector<VkDescriptorPool> DescriptorPools;
		std::vector<uint32_t> DescriptorPoolAllocationCount;

		// Rendering Command Queue (Also works fine if we only had 1 but we create 2 just to make sure nothing can get out of sync
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
			constexpr VkDescriptorPoolSize poolSizes[] =
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
				.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
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

		constexpr QuadVertex data[4] = {
			{ .Position = { -1.0f, -1.0f, 0.0f }, .TexCoord = { 0.0f, 0.0f } },
			{ .Position = {  1.0f, -1.0f, 0.0f }, .TexCoord = { 1.0f, 0.0f } },
			{ .Position = {  1.0f,  1.0f, 0.0f }, .TexCoord = { 1.0f, 1.0f } },
			{ .Position = { -1.0f,  1.0f, 0.0f }, .TexCoord = { 0.0f, 1.0f } }
		};

		s_Data->QuadVertexBuffer = VertexBuffer::Create(reinterpret_cast<const void*>(data), 4 * sizeof(QuadVertex));
		uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };
		s_Data->QuadIndexBuffer = IndexBuffer::Create(indices, 6 * sizeof(uint32_t));

		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/2D/Renderer2D_Line.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/2D/Renderer2D_Quad.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/2D/Renderer2D_Text.glsl");

		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/Compositing/Compositing.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/Compositing/DepthOfField.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/Compositing/Grid.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/Compositing/TexturePass.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/Compositing/WireFrame.glsl");

		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/EnvironmentMapping/EnvironmentIrradiance.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/EnvironmentMapping/EnvironmentMipChainFilter.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/EnvironmentMapping/EquirectangularToCubemap.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/EnvironmentMapping/PreethamSky.glsl");

		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/Geometry/DirectionalShadow.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/Geometry/IrisPBRStatic.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/Geometry/PreDepth.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/Geometry/Skybox.glsl");

		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/JumpFlood/JumpFloodComposite.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/JumpFlood/JumpFloodInit.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/JumpFlood/JumpFloodPass.glsl");
		Renderer::GetShadersLibrary()->Load("Resources/Shaders/Src/JumpFlood/SelectedGeometry.glsl");

		constexpr uint32_t whiteTextureData = 0xffffffff;
		TextureSpecification spec = {
			.DebugName = "WhiteTexture",
			.Width = 1,
			.Height = 1,
			.Format = ImageFormat::RGBA
		};
		s_Data->WhiteTexutre = Texture2D::Create(spec, Buffer(reinterpret_cast<const uint8_t*>(&whiteTextureData), sizeof(uint32_t)));

		constexpr uint32_t blackTextureData = 0xff000000;
		spec.DebugName = "BlackTexture";
		s_Data->BlackTexutre = Texture2D::Create(spec, Buffer(reinterpret_cast<const uint8_t*>(&blackTextureData), sizeof(uint32_t)));
		
		constexpr uint32_t errorTextureData = 0xff0000ff;
		spec.DebugName = "ErrorTexture";
		s_Data->ErrorTexture = Texture2D::Create(spec, Buffer(reinterpret_cast<const uint8_t*>(&errorTextureData), sizeof(uint32_t)));

		TextureSpecification brdfTextureSpec = {
			.DebugName = "BRDFLutTexture",
			.WrapMode = TextureWrap::Clamp,
			.GenerateMips = false
		};
		s_Data->BRDFLutTexture = Texture2D::Create(brdfTextureSpec, std::string("Resources/Renderer/BRDF_LUT.png"));

		constexpr uint32_t blackCubeTextureData[6] = { 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000 };
		spec.DebugName = "BlackCubeTexture";
		s_Data->BlackCubeTexture = TextureCube::Create(spec, Buffer(reinterpret_cast<const uint8_t*>(&blackCubeTextureData), sizeof(blackCubeTextureData)));

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
		
			Ref<Shader> mipChainFilterShader = Renderer::GetShadersLibrary()->Get("EnvironmentMipChainFilter");
			ComputePassSpecification mipChainFilterPassSpec = {
				.DebugName = "MipChainFilterPass",
				.Pipeline = ComputePipeline::Create(mipChainFilterShader, "MipChainFilterPipeline"),
				.MarkerColor = { 0.3f, 0.85f, 0.3f, 1.0f }
			};
			s_Data->MipChainFilterPass = ComputePass::Create(mipChainFilterPassSpec);

			s_Data->MipChainFilterPass->Bake();

			Ref<Shader> irradianceShader = Renderer::GetShadersLibrary()->Get("EnvironmentIrradiance");
			ComputePassSpecification envIrradiancePassSpec = {
				.DebugName = "EnvIrradiancePass",
				.Pipeline = ComputePipeline::Create(irradianceShader, "EnvIrradiancePipeline"),
				.MarkerColor = { 0.4f, 0.9f, 0.4f, 1.0f }
			};
			s_Data->IrradianceMapPass = ComputePass::Create(envIrradiancePassSpec);
			s_Data->IrradianceMapMaterial = Material::Create(irradianceShader, "EnvIrradianceMaterial");

			s_Data->IrradianceMapPass->Bake();

			Ref<Shader> preethamSkyShader = Renderer::GetShadersLibrary()->Get("PreethamSky");
			ComputePassSpecification preethamSkyPassSpec = {
				.DebugName = "PreethamSkyPass",
				.Pipeline = ComputePipeline::Create(preethamSkyShader, "PreethamSkyPipeline"),
				.MarkerColor = { 0.3f, 0.15f, 0.8f, 1.0f }
			};
			s_Data->PreethamSkyPass = ComputePass::Create(preethamSkyPassSpec);
			s_Data->PreethamSkyMaterial = Material::Create(preethamSkyShader, "PreethamSkyMaterial");
			
			s_Data->PreethamSkyPass->Bake();
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
		s_Data->BRDFLutTexture.Reset();
		s_Data->BlackCubeTexture.Reset();
		s_Data->EmptyEnvironment.Reset();
		s_Data->QuadVertexBuffer.Reset();
		s_Data->QuadIndexBuffer.Reset();
		s_Data->EquirectToCubemapPass.Reset();
		s_Data->EquirectToCubemapMaterial.Reset();
		s_Data->MipChainFilterPass.Reset();
		s_Data->IrradianceMapPass.Reset();
		s_Data->IrradianceMapMaterial.Reset();
		s_Data->PreethamSkyPass.Reset();
		s_Data->PreethamSkyMaterial.Reset();

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
			RenderCommandQueue& renderCommandQueue = GetRenderCommandQueue();
			renderCommandQueue.Execute();
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

			VkRenderingInfo renderingInfo = {
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.pNext = nullptr,
				.renderArea = {
					.offset = {.x = 0, .y = 0 },
					.extent = {.width = width, .height = height }
				}
			};

			if (fbSpec.SwapchainTarget)
			{
			 	SwapChain& swapChain = Application::Get().GetWindow().GetSwapChain();
			 	width = swapChain.GetWidth();
			 	height = swapChain.GetHeight();

				VkRenderingAttachmentInfo imageAttachmentInfo = swapChain.GetCurrentImageInfo();
				renderingInfo.layerCount = 1;
				renderingInfo.colorAttachmentCount = 1;
				renderingInfo.pColorAttachments = &imageAttachmentInfo;
				renderingInfo.pDepthAttachment = nullptr;
				renderingInfo.pStencilAttachment = nullptr;
				
			 	// Here we flip the viewport so that the image to display is not flipped
			 	viewport.x = 0.0f;
			 	viewport.y = static_cast<float>(height);
			 	viewport.width = static_cast<float>(width);
			 	viewport.height = -static_cast<float>(height);
			}
			else
			{
				width = framebuffer->GetWidth();
				height = framebuffer->GetHeight();

				renderingInfo.layerCount = 1; // NOTE: We will be rendering into only one layer of the referenced image...
				renderingInfo.colorAttachmentCount = static_cast<uint32_t>(framebuffer->GetColorAttachmentCount());
				renderingInfo.pColorAttachments = framebuffer->GetColorAttachmentInfos().data();
				renderingInfo.pDepthAttachment = framebuffer->HasDepthAttachment() ? &framebuffer->GetDepthAttachmentInfo() : nullptr;
				renderingInfo.pStencilAttachment = framebuffer->HasStencilComponent() ? &framebuffer->GetDepthAttachmentInfo() : nullptr;

				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = static_cast<float>(width);
				viewport.height = static_cast<float>(height);
			}

			vkCmdBeginRendering(commandBuffer, &renderingInfo);

			if (explicitClear)
			{
				const uint32_t colorAttachmentCount = static_cast<uint32_t>(framebuffer->GetColorAttachmentCount());
				const uint32_t totalAttachmentCount = colorAttachmentCount + (framebuffer->HasDepthAttachment() ? 1 : 0);
			
				std::vector<VkClearAttachment> attachments(totalAttachmentCount);
				std::vector<VkClearRect> clearRects(totalAttachmentCount);
				for (uint32_t i = 0; i < colorAttachmentCount; i++)
				{
					attachments[i] = VkClearAttachment{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.colorAttachment = i,
						.clearValue = framebuffer->GetColorAttachmentInfos()[i].clearValue
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
						.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
						.clearValue = framebuffer->GetColorAttachmentInfos()[colorAttachmentCount].clearValue
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

			vkCmdEndRendering(commandBuffer);

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

	void Renderer::RenderStaticMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource, uint32_t subMeshIndex, Ref<MaterialTable> materialTable, Ref<VertexBuffer> transformBuffer, uint32_t transformOffset, uint32_t instanceCount)
	{
		IR_VERIFY(staticMesh);
		IR_VERIFY(meshSource);
		IR_VERIFY(materialTable);

		Renderer::Submit([renderCommandBuffer, pipeline, staticMesh, meshSource, subMeshIndex, materialTable, transformBuffer, transformOffset, instanceCount]() mutable
		{
			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer->GetActiveCommandBuffer();

			Ref<VertexBuffer> meshVB = meshSource->GetVertexBuffer();

			VkBuffer vertexBuffers[] = { meshVB->GetVulkanBuffer(), transformBuffer->GetVulkanBuffer() };
			VkDeviceSize offsets[] = { 0, transformOffset };
			vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);

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

			VkBuffer vertexBuffers[] = { meshVB->GetVulkanBuffer(), transformBuffer->GetVulkanBuffer() };
			VkDeviceSize offsets[] = { 0, transformOffset };
			vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);

			Ref<IndexBuffer> meshIB = meshSource->GetIndexBuffer();
			VkBuffer vulkanMeshIB = meshIB->GetVulkanBuffer();
			vkCmdBindIndexBuffer(commandBuffer, vulkanMeshIB, 0, VK_INDEX_TYPE_UINT32);

			VkPipelineLayout layout = pipeline->GetVulkanPipelineLayout();

			VkDescriptorSet descriptorSet = material->GetDescriptorSet(frameIndex);
			if (descriptorSet)
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, material->GetFirstSetIndex(), 1, &descriptorSet, 0, nullptr);

			Buffer uniformStorageBuffer = material->GetUniformStorageBuffer();
			if (uniformStorageBuffer)
			{
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, static_cast<uint32_t>(uniformStorageBuffer.Size), uniformStorageBuffer.Data);
			}

			const auto& subMeshes = meshSource->GetSubMeshes();
			const auto& subMesh = subMeshes[subMeshIndex];

			vkCmdDrawIndexed(commandBuffer, subMesh.IndexCount, instanceCount, subMesh.BaseIndex, subMesh.BaseVertex, 0);
			s_Data->DrawCallCount++;
		});
	}

	void Renderer::RenderStaticMeshWithMaterial(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource, uint32_t subMeshIndex, Ref<Material> material, Ref<VertexBuffer> transformBuffer, uint32_t transformOffset, uint32_t instanceCount, Buffer vertexShaderOverrides)
	{
		IR_VERIFY(staticMesh);
		IR_VERIFY(meshSource);
		IR_VERIFY(material);

		Buffer pushConstantBuffer;
		if (vertexShaderOverrides.Size)
		{
			pushConstantBuffer.Allocate(vertexShaderOverrides.Size);
			if (vertexShaderOverrides.Size)
				pushConstantBuffer.Write(vertexShaderOverrides.Data, vertexShaderOverrides.Size);
		}

		Renderer::Submit([renderCommandBuffer, pipeline, staticMesh, meshSource, subMeshIndex, material, transformBuffer, transformOffset, instanceCount, pushConstantBuffer]() mutable
		{
			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			VkCommandBuffer commandBuffer = renderCommandBuffer->GetActiveCommandBuffer();

			Ref<VertexBuffer> meshVB = meshSource->GetVertexBuffer();

			VkBuffer vertexBuffers[] = { meshVB->GetVulkanBuffer(), transformBuffer->GetVulkanBuffer() };
			VkDeviceSize offsets[] = { 0, transformOffset };
			vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);

			Ref<IndexBuffer> meshIB = meshSource->GetIndexBuffer();
			VkBuffer vulkanMeshIB = meshIB->GetVulkanBuffer();
			vkCmdBindIndexBuffer(commandBuffer, vulkanMeshIB, 0, VK_INDEX_TYPE_UINT32);

			VkPipelineLayout layout = pipeline->GetVulkanPipelineLayout();

			VkDescriptorSet descriptorSet = material->GetDescriptorSet(frameIndex);
			if (descriptorSet)
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, material->GetFirstSetIndex(), 1, &descriptorSet, 0, nullptr);

			if (pushConstantBuffer.Size)
			{
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<uint32_t>(pushConstantBuffer.Size), pushConstantBuffer.Data);
			}

			Buffer uniformStorageBuffer = material->GetUniformStorageBuffer();
			if (uniformStorageBuffer)
			{
				vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT, static_cast<uint32_t>(pushConstantBuffer.Size), static_cast<uint32_t>(uniformStorageBuffer.Size), uniformStorageBuffer.Data);
			}

			const auto& subMeshes = meshSource->GetSubMeshes();
			const auto& subMesh = subMeshes[subMeshIndex];

			vkCmdDrawIndexed(commandBuffer, subMesh.IndexCount, instanceCount, subMesh.BaseIndex, subMesh.BaseVertex, 0);
			s_Data->DrawCallCount++;

			pushConstantBuffer.Release();
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

	void Renderer::BeginComputePass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<ComputePass> computePass)
	{
		Renderer::Submit([renderCommandBuffer, computePass]() mutable
		{
			const uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();

			VkCommandBuffer commandBuffer = renderCommandBuffer->GetActiveCommandBuffer();

			VkDebugUtilsLabelEXT debugLabel = {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
				.pLabelName = computePass->GetSpecification().DebugName.c_str()
			};
			std::memcpy(&debugLabel.color, glm::value_ptr(computePass->GetSpecification().MarkerColor), 4 * sizeof(float));
			fpCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);

			// Bind the pipeline
			Ref<ComputePipeline> pipeline = computePass->GetPipeline();
			pipeline->RT_Begin(renderCommandBuffer);

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

			VkCommandBuffer commandBuffer = renderCommandBuffer->GetActiveCommandBuffer();
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

	std::pair<Ref<TextureCube>, Ref<TextureCube>> Renderer::CreateEnvironmentMap(const std::string& filepath)
	{
		if (!Renderer::GetConfig().ComputeEnvironmentMaps)
			return { Renderer::GetBlackCubeTexture(), Renderer::GetBlackCubeTexture() };
		
		const uint32_t cubemapSize = Renderer::GetConfig().EnvironmentMapResolution;
		constexpr uint32_t irradianceMapSize = 32;

		TextureSpecification equiRect2DTextureSpec = {
			.DebugName = "EquiRectangularTempTexture",
			.GenerateMips = false
		};
		Ref<Texture2D> envEquirect = Texture2D::Create(equiRect2DTextureSpec, filepath);
		IR_VERIFY(envEquirect->GetFormat() == ImageFormat::RGBA32F, "Texture is not HDR!");
		
		TextureSpecification cubemapSpec = {
			.DebugName = "UnFilteredCubemap",
			.Width = cubemapSize,
			.Height = cubemapSize,
			.Format = ImageFormat::RGBA32F
		};
		Ref<TextureCube> envUnfiltered = TextureCube::Create(cubemapSpec);
		cubemapSpec.DebugName = "RadianceMap";
		Ref<TextureCube> envFiltered = TextureCube::Create(cubemapSpec);
		cubemapSpec.DebugName = "IrradianceMap";
		cubemapSpec.Width = irradianceMapSize;
		cubemapSpec.Height = irradianceMapSize;
		Ref<TextureCube> irradianceMap = TextureCube::Create(cubemapSpec);

		// 1. Convert equirectangular to cubemap
		{
			Renderer::Submit([envUnfiltered, envEquirect, cubemapSize]() mutable
			{
				s_Data->EquirectToCubemapMaterial->Set("o_OutputCubeMap", envUnfiltered);
				s_Data->EquirectToCubemapMaterial->Set("u_EquirectangularTexture", envEquirect);

				const uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();

				Ref<ComputePipeline> pipeline = s_Data->EquirectToCubemapPass->GetPipeline();
				pipeline->RT_Begin();

				s_Data->EquirectToCubemapPass->Prepare();
				if (s_Data->EquirectToCubemapPass->HasDescriptorSets())
				{
					const auto& descriptorSets = s_Data->EquirectToCubemapPass->GetDescriptorSets(frameIndex);
					vkCmdBindDescriptorSets(
						pipeline->GetActiveCommandBuffer(),
						VK_PIPELINE_BIND_POINT_COMPUTE,
						pipeline->GetPipelineLayout(),
						s_Data->EquirectToCubemapPass->GetFirstSetIndex(),
						static_cast<uint32_t>(descriptorSets.size()),
						descriptorSets.data(),
						0,
						0
					);
				}

				VkDescriptorSet descriptorSet = s_Data->EquirectToCubemapMaterial->GetDescriptorSet(frameIndex);
				if (descriptorSet)
					vkCmdBindDescriptorSets(
						pipeline->GetActiveCommandBuffer(),
						VK_PIPELINE_BIND_POINT_COMPUTE,
						pipeline->GetPipelineLayout(),
						s_Data->EquirectToCubemapMaterial->GetFirstSetIndex(),
						1,
						&descriptorSet,
						0,
						nullptr
					);

				pipeline->Dispatch({ cubemapSize / 32, cubemapSize / 32, 6 });

				pipeline->End();

				envUnfiltered->GenerateMips(true);
			});
		}

		// 2. Then we need to rerun the same pipeline on the filtered image since the first mip will be an AS-IS copy of the cubemap which is used for fully reflective materials
		{
			Renderer::Submit([envUnfiltered, envFiltered, cubemapSize]() mutable
			{
				// Change render target for next pass since we are using the same compute pass
				s_Data->EquirectToCubemapMaterial->Set("o_OutputCubeMap", envFiltered);

				const uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();

				Ref<ComputePipeline> pipeline = s_Data->EquirectToCubemapPass->GetPipeline();
				pipeline->RT_Begin();

				s_Data->EquirectToCubemapPass->Prepare();
				if (s_Data->EquirectToCubemapPass->HasDescriptorSets())
				{
					const auto& descriptorSets = s_Data->EquirectToCubemapPass->GetDescriptorSets(frameIndex);
					vkCmdBindDescriptorSets(
						pipeline->GetActiveCommandBuffer(),
						VK_PIPELINE_BIND_POINT_COMPUTE,
						pipeline->GetPipelineLayout(),
						s_Data->EquirectToCubemapPass->GetFirstSetIndex(),
						static_cast<uint32_t>(descriptorSets.size()),
						descriptorSets.data(),
						0,
						0
					);
				}

				VkDescriptorSet descriptorSet = s_Data->EquirectToCubemapMaterial->GetDescriptorSet(frameIndex);
				if (descriptorSet)
					vkCmdBindDescriptorSets(
						pipeline->GetActiveCommandBuffer(),
						VK_PIPELINE_BIND_POINT_COMPUTE,
						pipeline->GetPipelineLayout(),
						s_Data->EquirectToCubemapMaterial->GetFirstSetIndex(),
						1,
						&descriptorSet,
						0,
						nullptr
					);

				pipeline->Dispatch({ cubemapSize / 32, cubemapSize / 32, 6 });

				pipeline->End();
			});
		}
		
		// 3. Filter mip chain of the cubemap
		{
			Renderer::Submit([envFiltered, envUnfiltered, cubemapSize]() mutable
			{
				uint32_t mipCount = Utils::CalculateMipCount(cubemapSize, cubemapSize);

				std::vector<VkDescriptorSet> descriptorSet;
				descriptorSet.reserve(mipCount);

				std::vector<VkDescriptorSetLayout> dsls = s_Data->MipChainFilterPass->GetPipeline()->GetShader()->GetAllDescriptorSetLayouts();
				for (uint32_t i = 0; i < mipCount; i++)
				{
					VkDescriptorSetAllocateInfo info = {
						.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
						.descriptorSetCount = 1,
						.pSetLayouts = &dsls[3],
					};

					descriptorSet.push_back(Renderer::RT_AllocateDescriptorSet(info));
				}

				std::vector<VkWriteDescriptorSet> writeDescriptors(mipCount * 2);
				std::vector<VkDescriptorImageInfo> mipImageInfos(mipCount);
				for (uint32_t i = 0; i < mipCount; i++)
				{
					VkDescriptorImageInfo& mipImageInfo = mipImageInfos[i];
					mipImageInfo = envFiltered->GetDescriptorImageInfo();
					mipImageInfo.imageView = envFiltered->CreateImageViewSingleMip(i)->GetVulkanImageView();

					writeDescriptors[i * 2 + 0] = *s_Data->MipChainFilterPass->GetPipeline()->GetShader()->GetDescriptorSet("o_OutputCubeMap", 3);
					writeDescriptors[i * 2 + 0].dstSet = descriptorSet[i];
					writeDescriptors[i * 2 + 0].pImageInfo = &mipImageInfo;

					writeDescriptors[i * 2 + 1] = *s_Data->MipChainFilterPass->GetPipeline()->GetShader()->GetDescriptorSet("u_InputCubeMap", 3);
					writeDescriptors[i * 2 + 1].dstSet = descriptorSet[i];
					writeDescriptors[i * 2 + 1].pImageInfo = &envUnfiltered->GetDescriptorImageInfo();
				}

				vkUpdateDescriptorSets(RendererContext::GetCurrentDevice()->GetVulkanDevice(), static_cast<uint32_t>(writeDescriptors.size()), writeDescriptors.data(), 0, nullptr);

				Ref<ComputePipeline> pipeline = s_Data->MipChainFilterPass->GetPipeline();
				pipeline->RT_Begin();

				const float deltaRoughness = 1.0f / glm::max(static_cast<float>(envFiltered->GetMipLevelCount()) - 1.0f, 1.0f);

				// Start from second mip since first mip is a copy of the cubemap
				for (uint32_t i = 1, size = cubemapSize; i < mipCount; i++, size /= 2)
				{
					vkCmdBindDescriptorSets(pipeline->GetActiveCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE, s_Data->MipChainFilterPass->GetPipeline()->GetPipelineLayout(), 3, 1, &descriptorSet[i], 0, nullptr);

					float roughness = i * deltaRoughness;
					uint32_t numGroups = glm::max(1u, size / 32);
					pipeline->SetPushConstants(Buffer(reinterpret_cast<const uint8_t*>(&roughness), sizeof(float)));
					pipeline->Dispatch({ numGroups, numGroups, 6 });
				}

				pipeline->End();
			});
		}

		// 4. Generate Irradiance map
		{
			Renderer::Submit([irradianceMap, envFiltered]() mutable
			{
				s_Data->IrradianceMapMaterial->Set("o_OutputCubeMap", irradianceMap);
				s_Data->IrradianceMapMaterial->Set("u_RadianceMap", envFiltered);

				const uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();

				Ref<ComputePipeline> pipeline = s_Data->IrradianceMapPass->GetPipeline();
				pipeline->RT_Begin();

				s_Data->IrradianceMapPass->Prepare();
				if (s_Data->IrradianceMapPass->HasDescriptorSets())
				{
					const auto& descriptorSets = s_Data->IrradianceMapPass->GetDescriptorSets(frameIndex);
					vkCmdBindDescriptorSets(
						pipeline->GetActiveCommandBuffer(),
						VK_PIPELINE_BIND_POINT_COMPUTE,
						pipeline->GetPipelineLayout(),
						s_Data->IrradianceMapPass->GetFirstSetIndex(),
						static_cast<uint32_t>(descriptorSets.size()),
						descriptorSets.data(),
						0,
						0
					);
				}

				VkDescriptorSet descriptorSet = s_Data->IrradianceMapMaterial->GetDescriptorSet(frameIndex);
				if (descriptorSet)
					vkCmdBindDescriptorSets(
						pipeline->GetActiveCommandBuffer(),
						VK_PIPELINE_BIND_POINT_COMPUTE,
						pipeline->GetPipelineLayout(),
						s_Data->IrradianceMapMaterial->GetFirstSetIndex(),
						1,
						&descriptorSet,
						0,
						nullptr
					);

				pipeline->SetPushConstants(Buffer(reinterpret_cast<const uint8_t*>(&Renderer::GetConfig().IrradianceMapComputeSamples), sizeof(uint32_t)));
				pipeline->Dispatch({ irradianceMapSize / 32, irradianceMapSize / 32, 6 });

				pipeline->End();

				irradianceMap->GenerateMips(true);

				// Release the reference held by the DescriptorSetManager to the equirectangular image so that we can delete it
				s_Data->EquirectToCubemapMaterial->Set("u_EquirectangularTexture", Renderer::GetWhiteTexture());
			});
		}

		return { envFiltered, irradianceMap };
	}

	Ref<TextureCube> Renderer::CreatePreethamSky(float turbidity, float azimuth, float inclination, float sunSize)
	{
		const uint32_t cubemapSize = Renderer::GetConfig().EnvironmentMapResolution;

		TextureSpecification cubemapSpec = {
			.DebugName = "PreethamSky",
			.Width = cubemapSize,
			.Height = cubemapSize,
			.Format = ImageFormat::RGBA32F
		};
		Ref<TextureCube> environmentMap = TextureCube::Create(cubemapSpec);

		glm::vec4 params = { turbidity, azimuth, inclination, sunSize };
		Renderer::Submit([environmentMap, params, cubemapSize]() mutable
		{
			s_Data->PreethamSkyMaterial->Set("o_OutputCubeMap", environmentMap);

			const uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();

			Ref<ComputePipeline> pipeline = s_Data->PreethamSkyPass->GetPipeline();
			pipeline->RT_Begin();

			s_Data->PreethamSkyPass->Prepare();
			if (s_Data->PreethamSkyPass->HasDescriptorSets())
			{
				const auto& descriptorSets = s_Data->PreethamSkyPass->GetDescriptorSets(frameIndex);
				vkCmdBindDescriptorSets(
					pipeline->GetActiveCommandBuffer(),
					VK_PIPELINE_BIND_POINT_COMPUTE,
					pipeline->GetPipelineLayout(),
					s_Data->PreethamSkyPass->GetFirstSetIndex(),
					static_cast<uint32_t>(descriptorSets.size()),
					descriptorSets.data(),
					0,
					0
				);
			}

			VkDescriptorSet descriptorSet = s_Data->PreethamSkyMaterial->GetDescriptorSet(frameIndex);
			if (descriptorSet)
				vkCmdBindDescriptorSets(
					pipeline->GetActiveCommandBuffer(),
					VK_PIPELINE_BIND_POINT_COMPUTE,
					pipeline->GetPipelineLayout(),
					s_Data->PreethamSkyMaterial->GetFirstSetIndex(),
					1,
					&descriptorSet,
					0,
					nullptr
				);

			pipeline->SetPushConstants(Buffer(reinterpret_cast<const uint8_t*>(&params), sizeof(glm::vec4)));
			pipeline->Dispatch({ cubemapSize / 32, cubemapSize / 32, 6 });

			pipeline->End();

			environmentMap->GenerateMips(true);
		});

		return environmentMap;
	}

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

	Ref<Texture2D> Renderer::GetBRDFLutTexture()
	{
		return s_Data->BRDFLutTexture;
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

	void Renderer::InsertMemoryBarrier(
		VkCommandBuffer commandBuffer,
		VkAccessFlags2 srcAccessMask,
		VkAccessFlags2 dstAccessMask,
		VkPipelineStageFlags2 srcStageMask,
		VkPipelineStageFlags2 dstStageMask
	)
	{
		VkMemoryBarrier2 memoryBarrier = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
			.pNext = nullptr,
			.srcStageMask = srcStageMask,
			.srcAccessMask = srcAccessMask,
			.dstStageMask = dstStageMask,
			.dstAccessMask = dstAccessMask
		};

		VkDependencyInfo info = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.pNext = nullptr,
			.memoryBarrierCount = 1,
			.pMemoryBarriers = &memoryBarrier
		};

		vkCmdPipelineBarrier2(commandBuffer, &info);
	}

	void Renderer::InsertBufferMemoryBarrier(
		VkCommandBuffer commandBuffer,
		VkBuffer buffer,
		VkAccessFlags2 srcAccessMask,
		VkAccessFlags2 dstAccessMask,
		VkPipelineStageFlags2 srcStageMask,
		VkPipelineStageFlags2 dstStageMask
	)
	{
		VkBufferMemoryBarrier2 bufferMemBarrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext = nullptr,
			.srcStageMask = srcStageMask,
			.srcAccessMask = srcAccessMask,
			.dstStageMask = dstStageMask,
			.dstAccessMask = dstAccessMask,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = buffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE
		};

		VkDependencyInfo info = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.pNext = nullptr,
			.bufferMemoryBarrierCount = 1,
			.pBufferMemoryBarriers = &bufferMemBarrier
		};

		vkCmdPipelineBarrier2(commandBuffer, &info);
	}

	void Renderer::InsertImageMemoryBarrier(
		VkCommandBuffer commandBuffer,
		VkImage image,
		VkAccessFlags2 srcAccessMask,
		VkAccessFlags2 dstAccessMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkPipelineStageFlags2 srcStageMask,
		VkPipelineStageFlags2 dstStageMask,
		VkImageSubresourceRange subresourceRange
	)
	{
		VkImageMemoryBarrier2 imageMemBarrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext = nullptr,
			.srcStageMask = srcStageMask,
			.srcAccessMask = srcAccessMask,
			.dstStageMask = dstStageMask,
			.dstAccessMask = dstAccessMask,
			.oldLayout = oldImageLayout,
			.newLayout = newImageLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = subresourceRange
		};

		VkDependencyInfo info = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.pNext = nullptr,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &imageMemBarrier
		};

		vkCmdPipelineBarrier2(commandBuffer, &info);
	}

	void Renderer::TransferImageQueueOwnership(
		bool computeToGraphics,
		VkImage image,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkPipelineStageFlags2 srcSrcStage,
		VkPipelineStageFlags2 srcDstStage,
		VkPipelineStageFlags2 dstSrcStage,
		VkPipelineStageFlags2 dstDstStage,
		VkImageSubresourceRange subresourceRange
	)
	{
		if (computeToGraphics)
		{
			VkImageMemoryBarrier2 releaseImageMemBarrier = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.pNext = nullptr,
				.srcStageMask = srcSrcStage,
				.srcAccessMask = 0,
				.dstStageMask = srcDstStage,
				.dstAccessMask = 0,
				.oldLayout = oldImageLayout,
				.newLayout = newImageLayout,
				.srcQueueFamilyIndex = static_cast<uint32_t>(RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetQueueFamilyIndices().Compute),
				.dstQueueFamilyIndex = static_cast<uint32_t>(RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetQueueFamilyIndices().Graphics),
				.image = image,
				.subresourceRange = subresourceRange
			};

			VkDependencyInfo info = {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &releaseImageMemBarrier
			};

			VkCommandBuffer cmdBuff = RendererContext::GetCurrentDevice()->GetCommandBuffer(true, true);
			vkCmdPipelineBarrier2(cmdBuff, &info);
			RendererContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuff, true);

			VkImageMemoryBarrier2 acquireImageMemBarrier = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.pNext = nullptr,
				.srcStageMask = srcSrcStage,
				.srcAccessMask = 0,
				.dstStageMask = srcDstStage,
				.dstAccessMask = 0,
				.oldLayout = oldImageLayout,
				.newLayout = newImageLayout,
				.srcQueueFamilyIndex = static_cast<uint32_t>(RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetQueueFamilyIndices().Compute),
				.dstQueueFamilyIndex = static_cast<uint32_t>(RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetQueueFamilyIndices().Graphics),
				.image = image,
				.subresourceRange = subresourceRange
			};

			info = {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &acquireImageMemBarrier
			};

			cmdBuff = RendererContext::GetCurrentDevice()->GetCommandBuffer(true);
			vkCmdPipelineBarrier2(cmdBuff, &info);
			RendererContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuff);
		}
		else
		{
			VkImageMemoryBarrier2 releaseImageMemBarrier = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.pNext = nullptr,
				.srcStageMask = srcSrcStage,
				.srcAccessMask = 0,
				.dstStageMask = srcDstStage,
				.dstAccessMask = 0,
				.oldLayout = oldImageLayout,
				.newLayout = newImageLayout,
				.srcQueueFamilyIndex = static_cast<uint32_t>(RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetQueueFamilyIndices().Graphics),
				.dstQueueFamilyIndex = static_cast<uint32_t>(RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetQueueFamilyIndices().Compute),
				.image = image,
				.subresourceRange = subresourceRange
			};

			VkDependencyInfo info = {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &releaseImageMemBarrier
			};

			VkCommandBuffer cmdBuff = RendererContext::GetCurrentDevice()->GetCommandBuffer(true);
			vkCmdPipelineBarrier2(cmdBuff, &info);
			RendererContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuff);

			VkImageMemoryBarrier2 acquireImageMemBarrier = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.pNext = nullptr,
				.srcStageMask = dstSrcStage,
				.srcAccessMask = 0,
				.dstStageMask = dstDstStage,
				.dstAccessMask = 0,
				.oldLayout = oldImageLayout,
				.newLayout = newImageLayout,
				.srcQueueFamilyIndex = static_cast<uint32_t>(RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetQueueFamilyIndices().Graphics),
				.dstQueueFamilyIndex = static_cast<uint32_t>(RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetQueueFamilyIndices().Compute),
				.image = image,
				.subresourceRange = subresourceRange
			};

			info = {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &acquireImageMemBarrier
			};

			cmdBuff = RendererContext::GetCurrentDevice()->GetCommandBuffer(true, true);
			vkCmdPipelineBarrier2(cmdBuff, &info);
			RendererContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuff, true);
		}
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