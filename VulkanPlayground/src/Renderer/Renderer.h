#pragma once

#include "Core/Application.h"
#include "Renderer/Core/GPUMemoryStats.h"
#include "Renderer/Core/RenderCommandQueue.h"
#include "Renderer/Core/RendererContext.h"
#include "RendererConfiguration.h"
#include "RendererCapabilities.h"

namespace vkPlayground {

	// Forward declares
	class Shader;
	class ShadersLibrary;
	class Texture2D;
	class RenderPass;
	class Pipeline;
	class Material;
	class RenderCommandBuffer;

	class Renderer
	{
	public:
		static Ref<RendererContext> GetContext() { return Application::Get().GetWindow().GetRendererContext(); }

		static void Init();
		static void Shutdown();

		static Ref<ShadersLibrary> GetShadersLibrary();
		static GPUMemoryStats GetGPUMemoryStats();
		static RendererCapabilities& GetCapabilities();
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

		static void BeginFrame();
		static void EndFrame();

		static void BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RenderPass> renderPass, bool explicitClear = false);
		static void EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer);

		static void SubmitFullScreenQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<Material> material);

		static VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo);

		static Ref<Texture2D> GetWhiteTexture();
		static Ref<Texture2D> GetBlackTexture();

		static RenderCommandQueue& GetRendererResourceReleaseQueue(uint32_t index);

		// TODO: Register all the dependencies (Compute Pipelines)
		static void RegisterShaderDependency(Ref<Shader> shader, Ref<Pipeline> pipeline);
		static void RegisterShaderDependency(Ref<Shader> shader, Ref<Material> material);
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