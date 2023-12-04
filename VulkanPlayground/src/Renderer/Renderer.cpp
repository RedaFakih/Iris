#include "vkPch.h"
#include "Renderer.h"

#include "RenderPass.h"
#include "Shaders/Compiler/ShaderCompiler.h"
#include "Shaders/Shader.h"
#include "Texture.h"
#include "UniformBufferSet.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace vkPlayground {

	struct ShaderDependencies
	{
		std::vector<Ref<Pipeline>> Pipelines;
	};

	static std::unordered_map<std::size_t, ShaderDependencies> s_ShaderDependencies;

	struct RendererData
	{
		Ref<ShadersLibrary> ShaderLibrary;

		Ref<Texture2D> BlackTexutre;
		Ref<Texture2D> WhiteTexutre;
	};

	static RendererData* s_Data = nullptr;
	static RendererConfiguration s_RendererConfig;
	// We create 3 which is corresponding with how many frames in flight we have
	static RenderCommandQueue s_RendererResourceFreeQueue[3];

	void Renderer::Init()
	{
		s_Data = new RendererData();

		s_RendererConfig.FramesInFlight = glm::min<uint32_t>(s_RendererConfig.FramesInFlight, Application::Get().GetWindow().GetSwapChain().GetImageCount());

		s_Data->ShaderLibrary = ShadersLibrary::Create();

		Renderer::GetShadersLibrary()->Load("Shaders/Src/SimpleShader.glsl");
		Renderer::GetShadersLibrary()->Load("Shaders/Src/TexturePass.glsl");

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
	}

	void Renderer::Shutdown()
	{
		s_ShaderDependencies.clear();

		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
		vkDeviceWaitIdle(device);

		ShaderCompiler::ClearUniformAndStorageBuffers();

		delete s_Data;

		// Clean all resources that were queued for destruction in the last frame
		for (uint32_t i = 0; i < 3; i++)
		{
			RenderCommandQueue& resourceReleaseQueue = GetRendererResourceReleaseQueue(i);
			resourceReleaseQueue.Execute();
		}

		// NOTE: Any more resource freeing will be here...
		// TODO: Delete Render command queues
	}

	Ref<ShadersLibrary> Renderer::GetShadersLibrary()
	{
		return s_Data->ShaderLibrary;
	}

	GPUMemoryStats Renderer::GetGPUMemoryStats()
	{
		return VulkanAllocator::GetStats();
	}

	RendererConfiguration& Renderer::GetConfig()
	{
		return s_RendererConfig;
	}

	void Renderer::SetConfig(const RendererConfiguration& config)
	{
		s_RendererConfig = config;
	}

	void Renderer::BeginRenderPass(VkCommandBuffer commandBuffer, Ref<RenderPass> renderPass, bool explicitClear)
	{
		// PG_CORE_TRACE_TAG("Renderer", "BeginRenderPass - {}", renderPass->GetSpecification().DebugName);

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

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
		if (fbSpec.SwapchainTarget)
		{
			SwapChain& swapChain = Application::Get().GetWindow().GetSwapChain();
			width = swapChain.GetWidth();
			height = swapChain.GetHeight();
			renderPassBeginInfo.renderPass = framebuffer->GetVulkanRenderPass();
			renderPassBeginInfo.framebuffer = swapChain.GetCurrentFramebuffer();
			renderPassBeginInfo.renderArea = {
				.offset = { .x = 0, .y = 0 },
				.extent = { .width = width, .height = height }
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
				.offset = { .x = 0, .y = 0 },
				.extent = { .width = width, .height = height }
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
			PG_ASSERT(clearValues.size() == totalAttachmentCount, "");

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
						.extent = { .width = width, .height = height }
					},
					.baseArrayLayer = 0,
					.layerCount = 1
				};
			}

			if (framebuffer->HasDepthAttachment())
			{
				attachments[colorAttachmentCount] = VkClearAttachment{
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
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
			.extent = { .width = width, .height = height }
		};
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		// TODO: Auto layout transitions for input resources

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
	}

	void Renderer::EndRenderPass(VkCommandBuffer commandBuffer)
	{
		vkCmdEndRenderPass(commandBuffer);
		fpCmdEndDebugUtilsLabelEXT(commandBuffer);
	}

	Ref<Texture2D> Renderer::GetWhiteTexture()
	{
		return s_Data->WhiteTexutre;
	}

	Ref<Texture2D> Renderer::GetBlackTexture()
	{
		return s_Data->BlackTexutre;
	}

	RenderCommandQueue& Renderer::GetRendererResourceReleaseQueue(uint32_t index)
	{
		return s_RendererResourceFreeQueue[index];
	}

	void Renderer::RegisterShaderDependency(Ref<Shader> shader, Ref<Pipeline> pipeline)
	{
		s_ShaderDependencies[shader->GetHash()].Pipelines.push_back(pipeline);
	}

	void Renderer::OnShaderReloaded(std::size_t hash)
	{
		if (s_ShaderDependencies.contains(hash))
		{
			ShaderDependencies& deps = s_ShaderDependencies.at(hash);
			for (Ref<Pipeline>& pipeline : deps.Pipelines)
			{
				pipeline->Invalidate();
			}
		}
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
		return Application::Get().GetWindow().GetSwapChain().GetCurrentBufferIndex();
	}

}