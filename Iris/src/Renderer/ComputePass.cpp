#include "IrisPCH.h"
#include "ComputePass.h"

#include "StorageBufferSet.h"
#include "UniformBufferSet.h"

namespace Iris {

	Ref<ComputePass> ComputePass::Create(const ComputePassSpecification& spec)
	{
		return CreateRef<ComputePass>(spec);
	}

	ComputePass::ComputePass(const ComputePassSpecification& spec)
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

	bool ComputePass::Validate()
	{
		return m_DescriptorSetManager.Validate();
	}

	void ComputePass::Bake()
	{
		m_DescriptorSetManager.Bake();
	}

	void ComputePass::Prepare()
	{
		m_DescriptorSetManager.InvalidateAndUpdate();
	}

	void ComputePass::SetInput(std::string_view name, Ref<UniformBuffer> uniformBuffer)
	{
		m_DescriptorSetManager.SetInput(name, uniformBuffer);
	}

	void ComputePass::SetInput(std::string_view name, Ref<UniformBufferSet> uniformBufferSet)
	{
		m_DescriptorSetManager.SetInput(name, uniformBufferSet);
	}

	void ComputePass::SetInput(std::string_view name, Ref<StorageBuffer> storageBuffer)
	{
		m_DescriptorSetManager.SetInput(name, storageBuffer);
	}

	void ComputePass::SetInput(std::string_view name, Ref<StorageBufferSet> storageBufferSet)
	{
		m_DescriptorSetManager.SetInput(name, storageBufferSet);
	}

	void ComputePass::SetInput(std::string_view name, Ref<Texture2D> texture)
	{
		m_DescriptorSetManager.SetInput(name, texture);
	}

	void ComputePass::SetInput(std::string_view name, Ref<TextureCube> textureCube)
	{
		m_DescriptorSetManager.SetInput(name, textureCube);
	}

	// TODO:
	// void ComputePass::SetInput(std::string_view name, Ref<StorageImage> storageImage)
	// {
	//		m_DescriptorSetManager.SetInput(name, storageImage);
	// }

	uint32_t ComputePass::GetFirstSetIndex() const
	{
		return m_DescriptorSetManager.GetFirstSetIndex();
	}

	bool ComputePass::HasDescriptorSets() const
	{
		return m_DescriptorSetManager.HasDescriptorSets();
	}

	const std::vector<VkDescriptorSet>& ComputePass::GetDescriptorSets(uint32_t frameIndex) const
	{
		return m_DescriptorSetManager.GetDescriptorSets(frameIndex);
	}

	bool ComputePass::IsInputValid(std::string_view name) const
	{
		std::string nameStr = std::string(name);
		return m_DescriptorSetManager.m_InputDeclarations.contains(nameStr);
	}

}