#include "Renderer.h"

namespace vkPlayground {

	static RendererConfiguration s_RendererConfig;
	// We create 3 which is corresponding with how many frames in flight we have
	static RenderCommandQueue s_RendererResourceFreeQueue[3];

	void Renderer::Init()
	{
		// TODO:
	}

	void Renderer::Shutdown()
	{
		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
		vkDeviceWaitIdle(device);

		// Clean all resources that were queued for destruction in the last frame
		for (uint32_t i = 0; i < 3; i++)
		{
			RenderCommandQueue& resourceReleaseQueue = GetRendererResourceReleaseQueue(i);
			resourceReleaseQueue.Execute();
		}

		// Any more resource freeing will be here...
	}

	RenderCommandQueue& Renderer::GetRendererResourceReleaseQueue(uint32_t index)
	{
		return s_RendererResourceFreeQueue[index];
	}

	uint32_t Renderer::GetCurrentFrameIndex()
	{
		return Application::Get().GetWindow().GetSwapChain().GetCurrentBufferIndex();
	}

	RendererConfiguration& Renderer::GetConfig()
	{
		return s_RendererConfig;
	}

	void Renderer::SetConfig(const RendererConfiguration& config)
	{
		s_RendererConfig = config;
	}

}