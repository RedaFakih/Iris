#include "IrisPCH.h"
#include "RenderPass.h"

#include "StorageBufferSet.h"
#include "UniformBufferSet.h"

namespace Iris {

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

	void RenderPass::SetInput(std::string_view name, Ref<StorageBuffer> storageBuffer)
	{
		m_DescriptorSetManager.SetInput(name, storageBuffer);
	}

	void RenderPass::SetInput(std::string_view name, Ref<StorageBufferSet> storageBufferSet)
	{
		m_DescriptorSetManager.SetInput(name, storageBufferSet);
	}

	void RenderPass::SetInput(std::string_view name, Ref<Texture2D> texture)
	{
		m_DescriptorSetManager.SetInput(name, texture);
	}

	void RenderPass::SetInput(std::string_view name, Ref<TextureCube> textureCube)
	{
		m_DescriptorSetManager.SetInput(name, textureCube);
	}

	void RenderPass::SetInput(std::string_view name, Ref<ImageView> imageView)
	{
		m_DescriptorSetManager.SetInput(name, imageView);
	}

	void RenderPass::SetInput(std::string_view name, Ref<StorageImage> image)
	{
		m_DescriptorSetManager.SetInput(name, image);
	}

	Ref<Texture2D> RenderPass::GetOutput(uint32_t index) const
	{
		Ref<Framebuffer> framebuffer = m_Specification.Pipeline->GetSpecification().TargetFramebuffer;
		if (index > framebuffer->GetColorAttachmentCount() + 1)
			return nullptr;

		if (index < framebuffer->GetColorAttachmentCount())
			return framebuffer->GetImage(index);

		return framebuffer->GetDepthImage();
	}

	Ref<Texture2D> RenderPass::GetDepthOutput() const
	{
		Ref<Framebuffer> framebuffer = m_Specification.Pipeline->GetSpecification().TargetFramebuffer;
		if (!framebuffer->HasDepthAttachment())
			return nullptr;

		return framebuffer->GetDepthImage();
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