#pragma once

#include "Core/Application.h"
#include "Core/Buffer.h"
#include "Renderer/Core/GPUMemoryStats.h"
#include "Renderer/Core/RenderCommandQueue.h"
#include "Renderer/Core/RendererContext.h"
#include "Renderer/Core/RenderThread.h"
#include "RendererCapabilities.h"
#include "RendererConfiguration.h"

#include <glm/glm.hpp>

namespace Iris {

	// Forward declares
	class Shader;
	class ShadersLibrary;
	class Texture2D;
	class RenderPass;
	class Pipeline;
	class Material;
	class MaterialTable;
	class StaticMesh;
	class MeshSource;
	class RenderCommandBuffer;
	class VertexBuffer;
	class IndexBuffer;

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

		template<typename FuncT>
		static void Submit(FuncT&& func)
		{
			auto renderCmd = [](void* ptr)
			{
				FuncT* pFunc = reinterpret_cast<FuncT*>(ptr);
				(*pFunc)();

				// NOTE: Instead of destroying we could try to ensure all items are trivially destructible
				// however some items may contains std::string (uniforms).
				// static_assert(std::is_trivially_destructible_v<FuncT>, "FuncT must be trivially destructible");
				pFunc->~FuncT();
			};

			void* storageBuffer = Renderer::GetRenderCommandQueue().Allocate(renderCmd, sizeof(func));
			new(storageBuffer) FuncT(std::forward<FuncT>((FuncT&&)func));
		}

		template<typename FuncT>
		static void SubmitReseourceFree(FuncT&& func)
		{
			auto renderCmd = [](void* ptr)
			{
				FuncT* pFunc = reinterpret_cast<FuncT*>(ptr);
				(*pFunc)();

				// NOTE: Instead of destroying we could try to ensure all items are trivially destructible
				// however some items may contains std::string (uniforms).
				// static_assert(std::is_trivially_destructible_v<FuncT>, "FuncT must be trivially destructible");
				pFunc->~FuncT();
			};

			if (RenderThread::IsCurrentThreadRT())
			{
				const uint32_t index = Renderer::RT_GetCurrentFrameIndex();
				void* storageBuffer = Renderer::GetRendererResourceReleaseQueue(index).Allocate(renderCmd, sizeof(func));
				new(storageBuffer) FuncT(std::forward<FuncT>((FuncT&&)func));
			}
			else
			{
				const uint32_t index = Renderer::GetMainThreadResourceFreeingQueueIndex();
				Submit([renderCmd, index, func]()
				{
					void* storageBuffer = Renderer::GetRendererResourceReleaseQueue(index).Allocate(renderCmd, sizeof(func));
					new(storageBuffer) FuncT(std::forward<FuncT>((FuncT&&)func));
				});
			}
		}

		static void ExecuteAllRenderCommandQueues();

		static void SwapQueues();
		static void WaitAndRender(RenderThread* renderThread);

		static void RenderThreadFunc(RenderThread* renderThread);
		static uint32_t GetRenderQueueIndex();
		static uint32_t GetRenderQueueSubmissionIndex();
		static uint32_t GetMainThreadResourceFreeingQueueIndex();

		static void BeginFrame();
		static void EndFrame();

		static void BeginRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<RenderPass> renderPass, bool explicitClear = false);
		static void EndRenderPass(Ref<RenderCommandBuffer> renderCommandBuffer);

		static void SubmitFullScreenQuad(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<Material> material);
		// This is for shaders that have u_Renderer since they are ignored in the reflection
		static void SubmitFullScreenQuadWithOverrides(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<Material> material, Buffer vertexShaderOverrides, Buffer fragmentShaderOverrides);

		static void RenderStaticMesh(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource, uint32_t subMeshIndex, Ref<MaterialTable> materialTable, Ref<VertexBuffer> transformBuffer, uint32_t transformOffset, uint32_t instanceCount);
		static void RenderStaticMeshWithMaterial(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<StaticMesh> staticMesh, Ref<MeshSource> meshSource, uint32_t subMeshIndex, Ref<Material> material, Ref<VertexBuffer> transformBuffer, uint32_t transformOffset, uint32_t instanceCount);
		static void RenderGeometry(Ref<RenderCommandBuffer> renderCommandBuffer, Ref<Pipeline> pipeline, Ref<Material> material, Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer, const glm::mat4& transform, uint32_t indexCount = 0);

		static VkDescriptorSet RT_AllocateDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo);

		static Ref<Texture2D> GetWhiteTexture();
		static Ref<Texture2D> GetBlackTexture();
		static Ref<Texture2D> GetErrorTexture();

		static RenderCommandQueue& GetRenderCommandQueue();
		static RenderCommandQueue& GetRendererResourceReleaseQueue(uint32_t index);

		// TODO: Register all the dependencies (Compute Pipelines)
		static void RegisterShaderDependency(Ref<Shader> shader, Ref<Pipeline> pipeline);
		static void RegisterShaderDependency(Ref<Shader> shader, Ref<Material> material);
		static void OnShaderReloaded(std::size_t hash);

		static void InsertMemoryBarrier(VkCommandBuffer commandBuffer,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask
		);

		static void InsertBufferMemoryBarrier(
			VkCommandBuffer commandBuffer,
			VkBuffer buffer,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask
		);

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
		static uint32_t RT_GetCurrentFrameIndex();

	};

}