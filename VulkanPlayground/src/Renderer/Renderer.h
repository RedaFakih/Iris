#pragma once

#include "Core/Application.h"
#include "Renderer/Core/GPUMemoryStats.h"
#include "Renderer/Core/RenderCommandQueue.h"
#include "Renderer/Core/RendererContext.h"
#include "RendererConfiguration.h"

namespace vkPlayground {

	// Forward declares
	class Shader;
	class ShadersLibrary;
	class Texture2D;
	class RenderPass;
	class Pipeline;

	class Renderer
	{
	public:
		static Ref<RendererContext> GetContext() { return Application::Get().GetWindow().GetRendererContext(); }

		static void Init();
		static void Shutdown();

		static Ref<ShadersLibrary> GetShadersLibrary();
		static GPUMemoryStats GetGPUMemoryStats();
		static RendererConfiguration& GetConfig();
		static void SetConfig(const RendererConfiguration& config);

		// template<typename FuncT>
		// static void Submit(FuncT&& func)

		template<typename FuncT>
		static void SubmitReseourceFree(FuncT&& func)
		{
			auto renderCmd = [](void* ptr)
			{
				FuncT* pFunc = (FuncT*)ptr;
				(*pFunc)();

				// NOTE: Instead of destroying we could try to ensure all items are trivially destructible
				// however some items may contains std::string (uniforms).
				// static_assert(std::is_trivially_destructible_v<FuncT>, "FuncT must be trivially destructible");
				pFunc->~FuncT();
			};

			// TODO: Should be done on the render thread by wrapping it in an another Renderer::Submit(...); and then we wrap the execute call
			// that is inside `SwapChain::BeginFrame` in a `Renderer::Submit`
			const uint32_t index = Renderer::GetCurrentFrameIndex();
			void* storageBuffer = Renderer::GetRendererResourceReleaseQueue(index).Allocate(renderCmd, sizeof(func));
			new(storageBuffer) FuncT(std::forward<FuncT>((FuncT&&)func));
		}

		static void BeginRenderPass(VkCommandBuffer commandBuffer, Ref<RenderPass> renderPass, bool explicitClear = false);
		static void EndRenderPass(VkCommandBuffer commandBuffer);

		// static void SubmitFullScreenQuad(VkCommandBuffer commandBuffer, Ref<Pipeline> pipeline /* TODO: Material */);

		static Ref<Texture2D> GetWhiteTexture();
		static Ref<Texture2D> GetBlackTexture();

		static RenderCommandQueue& GetRendererResourceReleaseQueue(uint32_t index);

		// TODO: Register all the dependencies (Pipelines, Materials)
		static void RegisterShaderDependency(Ref<Shader> shader, Ref<Pipeline> pipeline);
		static void OnShaderReloaded(std::size_t hash);

		static void InsertImageMemoryBarrier(
			VkCommandBuffer commandBuffer,
			VkImage image,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask,
			VkImageSubresourceRange subresourceRange
		);

		static uint32_t GetCurrentFrameIndex();

	private:

	};

}