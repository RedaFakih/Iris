#pragma once

#include "ComputePipeline.h"
#include "Core/Base.h"
#include "DescriptorSetManager.h"

namespace Iris {

	/*
	 * NOTE: Currently compute shader are dispatched on the graphics queue to avoid queue ownership transfer of the used resources
	 */

	struct ComputePassSpecification
	{
		std::string DebugName;
		Ref<ComputePipeline> Pipeline;
		glm::vec4 MarkerColor;
	};

	class ComputePass : public RefCountedObject
	{
	public:
		ComputePass(const ComputePassSpecification& spec);
		~ComputePass() = default;

		[[nodiscard]] static Ref<ComputePass> Create(const ComputePassSpecification& spec);

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
		void SetInput(std::string_view name, Ref<StorageImage> storageImage);

		uint32_t GetFirstSetIndex() const;

		// We can just check if the pointer for the descriptor pool exists since that is only created in `DescriptorSetManager::Bake`
		bool Baked() const { return static_cast<bool>(m_DescriptorSetManager.GetDescriptorPool()); }

		bool HasDescriptorSets() const;
		const std::vector<VkDescriptorSet>& GetDescriptorSets(uint32_t frameIndex) const;

		bool IsInputValid(std::string_view name) const;

		Ref<ComputePipeline> GetPipeline() const { return m_Specification.Pipeline; }

		ComputePassSpecification& GetSpecification() { return m_Specification; }
		const ComputePassSpecification& GetSpecification() const { return m_Specification; }

	private:
		ComputePassSpecification m_Specification;
		DescriptorSetManager m_DescriptorSetManager;

	};

}