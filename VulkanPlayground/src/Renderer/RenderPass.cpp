#include "RenderPass.h"

namespace vkPlayground {

	Ref<RenderPass> RenderPass::Create(const RenderPassSpecification& spec)
	{
		return CreateRef<RenderPass>(spec);
	}

	RenderPass::RenderPass(const RenderPassSpecification& spec)
		: m_Specification(spec)
	{
		// The render passes will handle descriptor sets 0, 1, 2 and the material will handle descriptor set 3 since it is the per-draw
		DescriptorSetManagerSpecification managerSpec = {
			.Shader = spec.Pipeline->GetShader(),
			.DebugName = spec.DebugName,
			.StartingSet = 0,
			.EndingSet = 2
		};
		m_DescriptorSetManager = DescriptorSetManager(managerSpec);
	}

	RenderPass::~RenderPass()
	{
	}

	bool RenderPass::Validate()
	{
		return m_DescriptorSetManager.Validate();
	}

	void RenderPass::Bake()
	{
		m_DescriptorSetManager.Bake();
	}

	void RenderPass::Prepare()
	{
		m_DescriptorSetManager.InvalidateAndUpdate();
	}

	void RenderPass::SetInput(std::string_view name, Ref<UniformBuffer> uniformBuffer)
	{
		m_DescriptorSetManager.SetInput(name, uniformBuffer);
	}

	void RenderPass::SetInput(std::string_view name, Ref<UniformBufferSet> uniformBufferSet)
	{
		m_DescriptorSetManager.SetInput(name, uniformBufferSet);
	}

	void RenderPass::SetInput(std::string_view name, Ref<Texture2D> texture)
	{
		m_DescriptorSetManager.SetInput(name, texture);
	}

	uint32_t RenderPass::GetFirstSetIndex() const
	{
		return m_DescriptorSetManager.GetFirstSetIndex();
	}

	bool RenderPass::HasDescriptorSets() const
	{
		return m_DescriptorSetManager.HasDescriptorSets();
	}

	const std::vector<VkDescriptorSet>& RenderPass::GetDescriptorSets(uint32_t frameIndex) const
	{
		return m_DescriptorSetManager.GetDescriptorSets(frameIndex);
	}

	bool RenderPass::IsInputValid(std::string_view name) const
	{
		std::string nameStr = std::string(name);
		return m_DescriptorSetManager.m_InputDeclarations.contains(nameStr);
	}

}