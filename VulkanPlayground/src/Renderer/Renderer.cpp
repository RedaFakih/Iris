#include "Renderer.h"

#include "Shaders/Compiler/ShaderCompiler.h"

#include <glm/glm.hpp>

namespace vkPlayground {

	struct ShaderDependencies
	{
		std::vector<Ref<Pipeline>> Pipelines;
	};

	static std::unordered_map<std::size_t, ShaderDependencies> s_ShaderDependencies;

	struct RendererData
	{
		Ref<ShadersLibrary> m_ShaderLibrary;
	};

	static RendererData* s_Data = nullptr;
	static RendererConfiguration s_RendererConfig;
	// We create 3 which is corresponding with how many frames in flight we have
	static RenderCommandQueue s_RendererResourceFreeQueue[3];

	void Renderer::Init()
	{
		s_Data = new RendererData();

		s_RendererConfig.FramesInFlight = glm::min<uint32_t>(s_RendererConfig.FramesInFlight, Application::Get().GetWindow().GetSwapChain().GetImageCount());

		s_Data->m_ShaderLibrary = ShadersLibrary::Create();

		Renderer::GetShadersLibrary()->Load("Shaders/Src/SimpleShader.glsl");
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

		// Any more resource freeing will be here...
		// Delete render command queues
	}

	Ref<ShadersLibrary> Renderer::GetShadersLibrary()
	{
		return s_Data->m_ShaderLibrary;
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