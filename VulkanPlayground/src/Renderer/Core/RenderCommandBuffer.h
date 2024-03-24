#pragma once

#include "Renderer/Pipeline.h"

#include <vulkan/vulkan.h>


namespace vkPlayground {

	// TODO: Timestamps

	class RenderCommandBuffer : public RefCountedObject
	{
	public:
		RenderCommandBuffer(uint32_t count = 0, const std::string& debugName = "");
		// This creates the command buffer from the sawpchain owned draw buffer
		RenderCommandBuffer(const std::string& debugName, [[maybe_unused]] bool swapchain = true);
		~RenderCommandBuffer();

		[[nodiscard]] static Ref<RenderCommandBuffer> Create(uint32_t count = 0, const std::string& debugName = "");
		[[nodiscard]] static Ref<RenderCommandBuffer> CreateFromSwapChain(const std::string& debugName);

		void Begin();
		void End();
		void Submit();

		VkCommandBuffer GetActiveCommandBuffer() const { return m_ActiveCommandBuffer; }
		VkCommandBuffer GetCommandBuffer(uint32_t frameIndex) const { VKPG_ASSERT(frameIndex < m_CommandBuffers.size()); return m_CommandBuffers[frameIndex].CommandBuffer; }

		const PipelineStatistics& GetPipelineStatistics(uint32_t frameIndex) const { return m_PipelineStatisticsQueryResults[frameIndex]; }

	private:
		std::string m_DebugName;

		// Per-pool command buffer (So that we can have transient command pools)
		struct PerPoolCommandBuffer
		{
			VkCommandPool CommandPool = nullptr;
			VkCommandBuffer CommandBuffer = nullptr;
		};
		std::vector<PerPoolCommandBuffer> m_CommandBuffers;

		VkCommandBuffer m_ActiveCommandBuffer = nullptr; // Only a ref

		std::vector<VkFence> m_WaitFences;

		bool m_OwnedBySwapChain = false;

		std::vector<VkQueryPool> m_PipelineStatisticsQueryPools;
		uint32_t m_PipelineQueryCount = 0;
		std::vector<PipelineStatistics> m_PipelineStatisticsQueryResults;		
	};

}