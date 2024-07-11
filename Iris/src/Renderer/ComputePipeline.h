#pragma once

#include "Shaders/Shader.h"
#include "Renderer/Core/RenderCommandBuffer.h"

namespace Iris {

	class ComputePipeline : public RefCountedObject
	{
	public:
		ComputePipeline(Ref<Shader> shader, std::string_view debugName = "");
		~ComputePipeline();

		[[nodiscard]] inline static Ref<ComputePipeline> Create(Ref<Shader> shader, std::string_view debugName = "")
		{
			return CreateRef<ComputePipeline>(shader, debugName);
		}

		void Begin(VkCommandBuffer commandBuffer = nullptr);
		void RT_Begin(VkCommandBuffer commandBuffer = nullptr);
		void End();

		void Dispatch(const glm::uvec3& workGroups) const;

		void SetPushConstants(Buffer pushConstants) const;
		void CreatePipeline();

		bool IsUsingComputeQueue() const { return m_UsingGraphicsQueue == false; }

		VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
		VkCommandBuffer GetActiveCommandBuffer() const { return m_ActiveComputeCommandBuffer; }
		Ref<Shader> GetShader() const { return m_Shader; }
		std::string_view GetDebugName() const { return m_DebugName; }

	private:
		void RT_CreatePipeline();
		void Release();

	private:
		Ref<Shader> m_Shader;
		std::string m_DebugName;

		VkPipeline m_Pipeline = nullptr;
		VkPipelineLayout m_PipelineLayout = nullptr;
		VkFence m_ComputeFence = nullptr;

		VkCommandBuffer m_ActiveComputeCommandBuffer = nullptr;

		bool m_UsingGraphicsQueue = false;

	};

}