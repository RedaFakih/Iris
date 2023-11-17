#include "Renderer.h"

#include <glm/glm.hpp>

namespace vkPlayground {

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

		Renderer::GetShadersLibrary()->Load("Shaders/SimpleShader.glsl");
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

	Ref<ShadersLibrary> Renderer::GetShadersLibrary()
	{
		return s_Data->m_ShaderLibrary;
	}

	RenderCommandQueue& Renderer::GetRendererResourceReleaseQueue(uint32_t index)
	{
		return s_RendererResourceFreeQueue[index];
	}

	void Renderer::OnShaderReloaded(std::size_t hash)
	{
		// TODO: Invalidate all the dependencies...
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