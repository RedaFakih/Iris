#include "IrisPCH.h"
#include "ComputePipeline.h"

#include "Renderer.h"

namespace Iris {

	ComputePipeline::ComputePipeline(Ref<Shader> shader, std::string_view debugName)
		: m_Shader(shader), m_DebugName(debugName)
	{
		Ref<ComputePipeline> instance = this;
		Renderer::Submit([instance]() mutable
		{
			instance->RT_CreatePipeline();
		});

		Renderer::RegisterShaderDependency(shader, this);
	}

	ComputePipeline::~ComputePipeline()
	{
		Release();
	}

	void ComputePipeline::Begin(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		IR_ASSERT(!m_ActiveCommandBuffer, "A command buffer is currently active");

		if (renderCommandBuffer)
		{
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
			m_ActiveCommandBuffer = renderCommandBuffer->GetActiveCommandBuffer();
			m_UsingGraphicsQueue = true;
		}
		else
		{
			//m_ActiveCommandBuffer = RendererContext::GetCurrentDevice()->GetCommandBuffer(true, true);
			m_ActiveCommandBuffer = RendererContext::GetCurrentDevice()->GetCommandBuffer(true);
			m_UsingGraphicsQueue = false;
		}

		vkCmdBindPipeline(m_ActiveCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
	}

	void ComputePipeline::RT_Begin(Ref<RenderCommandBuffer> renderCommandBuffer)
	{
		IR_ASSERT(!m_ActiveCommandBuffer, "A command buffer is currently active");

		if (renderCommandBuffer)
		{
			uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
			m_ActiveCommandBuffer = renderCommandBuffer->GetActiveCommandBuffer();
			m_UsingGraphicsQueue = true;
		}
		else
		{
			//m_ActiveCommandBuffer = RendererContext::GetCurrentDevice()->GetCommandBuffer(true, true);
			m_ActiveCommandBuffer = RendererContext::GetCurrentDevice()->GetCommandBuffer(true);
			m_UsingGraphicsQueue = false;
		}

		vkCmdBindPipeline(m_ActiveCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
	}

	void ComputePipeline::End()
	{
		IR_ASSERT(m_ActiveCommandBuffer);

		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
		if (!m_UsingGraphicsQueue)
		{
			//VkQueue computeQueue = RendererContext::GetCurrentDevice()->GetComputeQueue();
			VkQueue graphicsQueue = RendererContext::GetCurrentDevice()->GetGraphicsQueue();

			vkEndCommandBuffer(m_ActiveCommandBuffer);

			vkWaitForFences(device, 1, &m_ComputeFence, VK_TRUE, UINT64_MAX);
			vkResetFences(device, 1, &m_ComputeFence);

			VkSubmitInfo submitInfo = {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.commandBufferCount = 1,
				.pCommandBuffers = &m_ActiveCommandBuffer
			};
			//VK_CHECK_RESULT(vkQueueSubmit(computeQueue, 1, &submitInfo, m_ComputeFence));
			VK_CHECK_RESULT(vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_ComputeFence));

			// Wait for compute shader execution for safety reasons...
			{
				Timer timer;
				vkWaitForFences(device, 1, &m_ComputeFence, VK_TRUE, UINT64_MAX);
				IR_CORE_INFO("Compute shader execution: {}", timer.ElapsedMillis());
			}
		}

		m_ActiveCommandBuffer = nullptr;
	}

	void ComputePipeline::Dispatch(const glm::uvec3& workGroups) const
	{
		IR_ASSERT(m_ActiveCommandBuffer);

		vkCmdDispatch(m_ActiveCommandBuffer, workGroups.x, workGroups.y, workGroups.z);
	}

	void ComputePipeline::SetPushConstants(Buffer pushConstants) const
	{
		vkCmdPushConstants(m_ActiveCommandBuffer, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, static_cast<uint32_t>(pushConstants.Size), pushConstants.Data);
	}

	void ComputePipeline::CreatePipeline()
	{
		Ref<ComputePipeline> instance = this;
		Renderer::Submit([instance]() mutable
		{
			instance->RT_CreatePipeline();
		});
	}

	void ComputePipeline::RT_CreatePipeline()
	{
		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();

		Release();

		std::vector<VkDescriptorSetLayout> descriptorSetLayouts = m_Shader->GetAllDescriptorSetLayouts();

		VkPipelineLayoutCreateInfo pipelineLayoutCI = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
			.pSetLayouts = descriptorSetLayouts.data()
		};

		const std::vector<ShaderResources::PushConstantRange>& pushConstantRanges = m_Shader->GetPushConstantRanges();
		std::vector<VkPushConstantRange> vulkanPushConstantRanges(pushConstantRanges.size());

		for (uint32_t i = 0; i < pushConstantRanges.size(); i++)
		{
			const auto& pushConstantRange = pushConstantRanges[i];
			auto& vulkanPushConstantRange = vulkanPushConstantRanges[i];

			vulkanPushConstantRange.stageFlags = pushConstantRange.ShaderStage;
			vulkanPushConstantRange.offset = pushConstantRange.Offset;
			vulkanPushConstantRange.size = pushConstantRange.Size;
		}

		if (pushConstantRanges.size())
		{
			pipelineLayoutCI.pushConstantRangeCount = static_cast<uint32_t>(vulkanPushConstantRanges.size());
			pipelineLayoutCI.pPushConstantRanges = vulkanPushConstantRanges.data();
		}

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &m_PipelineLayout));

		const std::vector<VkPipelineShaderStageCreateInfo>& pipelineShaderStages = m_Shader->GetPipelineShaderStageCreateInfos();
		VkComputePipelineCreateInfo pipelineCI = {
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.flags = 0,
			.stage = pipelineShaderStages[0],
			.layout = m_PipelineLayout
		};
		VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &pipelineCI, nullptr, &m_Pipeline));
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_PIPELINE, std::string(m_Shader->GetName()), m_Pipeline);

		VkFenceCreateInfo fenceCI = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
		};
		VK_CHECK_RESULT(vkCreateFence(device, &fenceCI, nullptr, &m_ComputeFence));
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_FENCE, "Compute Pipeline Fence", m_ComputeFence);
	}

	void ComputePipeline::Release()
	{
		if (m_Pipeline && m_PipelineLayout)
		{
			Renderer::SubmitReseourceFree([pipelineLayout = m_PipelineLayout, pipeline = m_Pipeline, fence = m_ComputeFence]()
			{
				VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
				vkDestroyPipeline(device, pipeline, nullptr);
				vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
				vkDestroyFence(device, fence, nullptr);
			});

			m_Pipeline = nullptr;
			m_PipelineLayout = nullptr;
		}
	}

}