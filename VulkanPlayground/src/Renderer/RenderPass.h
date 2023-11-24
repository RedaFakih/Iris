#pragma once

#include "Core/Base.h"
#include "DescriptorSetManager.h"
#include "Pipeline.h"

#include <glm/glm.hpp>

namespace vkPlayground {

	struct RenderPassSpecification
	{
		std::string DebugName;
		Ref<Pipeline> Pipeline;
		glm::vec4 MarkerColor;
	};

	class RenderPass : public RefCountedObject
	{
	public:
		RenderPass(const RenderPassSpecification& spec);
		~RenderPass();

		[[nodiscard]] static Ref<RenderPass> Create(const RenderPassSpecification& spec);

		bool Validate();
		void Bake();
		void Prepare();

		void SetInput(std::string_view name, Ref<UniformBuffer> uniformBuffer);
		void SetInput(std::string_view name, Ref<UniformBufferSet> uniformBufferSet);
		void SetInput(std::string_view name, Ref<Texture2D> texture);

		// TODO: Getting output from framebuffer however that requires that the framebuffers are implemented first xD...

		uint32_t GetFirstSetIndex() const;

		// We can just check if the pointer for the descriptor pool exists since that is only created in `DescriptorSetManager::Bake`
		bool Baked() const { return (bool)m_DescriptorSetManager.GetDescriptorPool(); }

		bool HasDescriptorSets() const;
		const std::vector<VkDescriptorSet>& GetDescriptorSets(uint32_t frameIndex) const;
		
		bool IsInputValid(std::string_view name) const;

		Ref<Framebuffer> GetTargetFramebuffer() const { return m_Specification.Pipeline->GetSpecification().TargetFramebuffer; }
		Ref<Pipeline> GetPipeline() const { return m_Specification.Pipeline; }

		RenderPassSpecification& GetSpec() { return m_Specification; }
		const RenderPassSpecification& GetSpec() const { return m_Specification; }

	private:
		RenderPassSpecification m_Specification;
		DescriptorSetManager m_DescriptorSetManager;

	};

}