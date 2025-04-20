#pragma once

#include "Core/Base.h"
#include "DescriptorSetManager.h"
#include "Pipeline.h"

#include <glm/glm.hpp>

namespace Iris {

	struct RenderPassSpecification
	{
		std::string DebugName;
		Ref<Pipeline> Pipeline;
		glm::vec4 MarkerColor;
	};

	// Forward declares
	class UniformBuffer;
	class UniformBufferSet;
	class StorageBuffer;
	class StorageBufferSet;
	class Texture2D;
	class TextureCube;

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
		void SetInput(std::string_view name, Ref<StorageBuffer> storageBuffer);
		void SetInput(std::string_view name, Ref<StorageBufferSet> storageBufferSet);
		void SetInput(std::string_view name, Ref<Texture2D> texture);
		void SetInput(std::string_view name, Ref<TextureCube> textureCube);
		void SetInput(std::string_view name, Ref<ImageView> imageView);

		// NOTE: ignoreMultisampled ignores whether the framebuffer is multisampled or not and return the original images even if resolve ones exist...
		Ref<Texture2D> GetOutput(uint32_t index) const;
		Ref<Texture2D> GetDepthOutput() const;

		uint32_t GetFirstSetIndex() const;

		// We can just check if the pointer for the descriptor pool exists since that is only created in `DescriptorSetManager::Bake`
		bool Baked() const { return static_cast<bool>(m_DescriptorSetManager.GetDescriptorPool()); }

		bool HasDescriptorSets() const;
		const std::vector<VkDescriptorSet>& GetDescriptorSets(uint32_t frameIndex) const;
		
		bool IsInputValid(std::string_view name) const;

		Ref<Framebuffer> GetTargetFramebuffer() const { return m_Specification.Pipeline->GetSpecification().TargetFramebuffer; }
		Ref<Pipeline> GetPipeline() const { return m_Specification.Pipeline; }

		RenderPassSpecification& GetSpecification() { return m_Specification; }
		const RenderPassSpecification& GetSpecification() const { return m_Specification; }

	private:
		RenderPassSpecification m_Specification;
		DescriptorSetManager m_DescriptorSetManager;

	};

}