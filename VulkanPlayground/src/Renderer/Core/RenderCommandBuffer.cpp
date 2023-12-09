#include "vkPch.h"
#include "RenderCommandBuffer.h"

#include "Renderer/Renderer.h"

namespace vkPlayground {

	Ref<RenderCommandBuffer> RenderCommandBuffer::Create(uint32_t count, const std::string& debugName)
	{
		return CreateRef<RenderCommandBuffer>(count, debugName);
	}

	Ref<RenderCommandBuffer> RenderCommandBuffer::CreateFromSwapChain(const std::string& debugName)
	{
		return CreateRef<RenderCommandBuffer>(debugName);
	}

	RenderCommandBuffer::RenderCommandBuffer(uint32_t count, const std::string& debugName)
		: m_DebugName(debugName)
	{
		Ref<VulkanDevice> logicalDevice = RendererContext::GetCurrentDevice();
		VkDevice device = logicalDevice->GetVulkanDevice();
		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;
		count = count == 0 ? framesInFlight : count;

		// Create command pool
		VkCommandPoolCreateInfo commandPoolInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			.queueFamilyIndex = static_cast<uint32_t>(logicalDevice->GetPhysicalDevice()->GetQueueFamilyIndices().Graphics)
		};

		// Allocate command buffers...
		VkCommandBufferAllocateInfo cbAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};

		m_CommandBuffers.resize(count);
		int i = 0;
		for (auto& commandBuffer : m_CommandBuffers)
		{
			VK_CHECK_RESULT(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandBuffer.CommandPool));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_COMMAND_POOL, m_DebugName, commandBuffer.CommandPool);

			cbAllocateInfo.commandPool = commandBuffer.CommandPool;
			VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cbAllocateInfo, &commandBuffer.CommandBuffer));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_COMMAND_BUFFER, fmt::format("{0} (frame in flight: {1})", m_DebugName, i), commandBuffer.CommandBuffer);
			i++;
		}

		// Create fences
		VkFenceCreateInfo fenceCI = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
		};
		m_WaitFences.resize(count);
		for (uint32_t i = 0; i < count; i++)
		{
			VK_CHECK_RESULT(vkCreateFence(device, &fenceCI, nullptr, &m_WaitFences[i]));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_FENCE, fmt::format("{0} fence (frame in flight {1})", m_DebugName, i), m_WaitFences[i]);
		}

		m_PipelineQueryCount = 7;
		VkQueryPoolCreateInfo queryPoolCI = {
			.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
			.pNext = nullptr,
			.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS,
			.queryCount = m_PipelineQueryCount,
			.pipelineStatistics = 
				VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT     |
				VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT   |
				VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT   |
				VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT        |
				VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT         |
				VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
				VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT
		};
		m_PipelineStatisticsQueryPools.resize(count);
		for (VkQueryPool& pipelineStatisticsQueryPool : m_PipelineStatisticsQueryPools)
			VK_CHECK_RESULT(vkCreateQueryPool(device, &queryPoolCI, nullptr, &pipelineStatisticsQueryPool));

		m_PipelineStatisticsQueryResults.resize(count);
	}

	RenderCommandBuffer::RenderCommandBuffer(const std::string& debugName, bool swapchain)
		: m_DebugName(debugName), m_OwnedBySwapChain(true)
	{
		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;

		m_PipelineQueryCount = 7;
		VkQueryPoolCreateInfo queryPoolCI = {
			.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
			.pNext = nullptr,
			.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS,
			.queryCount = m_PipelineQueryCount,
			.pipelineStatistics =
				VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT     |
				VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT   |
				VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT   |
				VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT        |
				VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT         |
				VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
				VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT
		};
		m_PipelineStatisticsQueryPools.resize(framesInFlight);
		for (VkQueryPool& pipelineStatisticsQueryPool : m_PipelineStatisticsQueryPools)
			VK_CHECK_RESULT(vkCreateQueryPool(device, &queryPoolCI, nullptr, &pipelineStatisticsQueryPool));

		m_PipelineStatisticsQueryResults.resize(framesInFlight);
	}

	RenderCommandBuffer::~RenderCommandBuffer()
	{
		if (m_OwnedBySwapChain)
			return;

		Renderer::SubmitReseourceFree([commandbuffers = m_CommandBuffers, fences = m_WaitFences, queryPools = m_PipelineStatisticsQueryPools]()
		{
			VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();

			for (const RenderCommandBuffer::PerPoolCommandBuffer& commandBuffer : commandbuffers)
				vkDestroyCommandPool(device, commandBuffer.CommandPool, nullptr);
			
			for (const VkFence& fence : fences)
				vkDestroyFence(device, fence, nullptr);

			for (const VkQueryPool& queryPool : queryPools)
				vkDestroyQueryPool(device, queryPool, nullptr);
		});
	}

	void RenderCommandBuffer::Begin()
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();

		if (m_CommandBuffers.size())
			VK_CHECK_RESULT(vkResetCommandPool(device, m_CommandBuffers[frameIndex].CommandPool, 0));

		VkCommandBufferBeginInfo beginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};

		if (m_OwnedBySwapChain)
			m_ActiveCommandBuffer = Application::Get().GetWindow().GetSwapChain().GetDrawCommandBuffer(frameIndex);
		else
			m_ActiveCommandBuffer = m_CommandBuffers[frameIndex].CommandBuffer;

		VK_CHECK_RESULT(vkBeginCommandBuffer(m_ActiveCommandBuffer, &beginInfo));

		// Pipeline Statistics
		vkCmdResetQueryPool(m_ActiveCommandBuffer, m_PipelineStatisticsQueryPools[frameIndex], 0, m_PipelineQueryCount);
		vkCmdBeginQuery(m_ActiveCommandBuffer, m_PipelineStatisticsQueryPools[frameIndex], 0, 0);
	}

	void RenderCommandBuffer::End()
	{
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
		vkCmdEndQuery(m_ActiveCommandBuffer, m_PipelineStatisticsQueryPools[frameIndex], 0);
		VK_CHECK_RESULT(vkEndCommandBuffer(m_ActiveCommandBuffer));

		m_ActiveCommandBuffer = nullptr;
	}

	void RenderCommandBuffer::Submit()
	{
		if (m_OwnedBySwapChain)
			return;

		Ref<VulkanDevice> logicalDevice = RendererContext::GetCurrentDevice();
		VkDevice device = logicalDevice->GetVulkanDevice();
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		VkSubmitInfo submitInfo = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &m_CommandBuffers[frameIndex].CommandBuffer
		};
		VK_CHECK_RESULT(vkWaitForFences(device, 1, &m_WaitFences[frameIndex], VK_TRUE, UINT64_MAX));
		VK_CHECK_RESULT(vkResetFences(device, 1, &m_WaitFences[frameIndex]));

		// PG_CORE_TRACE_TAG("Renderer", "Submitting Render Command Buffer {}", m_DebugName);

		VK_CHECK_RESULT(vkQueueSubmit(logicalDevice->GetGraphicsQueue(), 1, &submitInfo, m_WaitFences[frameIndex]));

		vkGetQueryPoolResults(
			device, 
			m_PipelineStatisticsQueryPools[frameIndex], 
			0, 
			1, 
			sizeof(PipelineStatistics), 
			&m_PipelineStatisticsQueryResults[frameIndex], 
			sizeof(uint64_t),
			VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
		);
	}

}