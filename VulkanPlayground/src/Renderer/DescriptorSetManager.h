#pragma once

#include "Core/Base.h"

#include <vulkan/vulkan.h>

#include <set>
#include <map>

/*
 * The way that the descriptor sets will work is by their increasing update frequency
 * so:
 *	- Set 0 - 2: Global renderer stuff -> Least updated set (Global as in global for the current frame)
 *  - Set 3: Per draw stuff -> Most updated set (materials...)
 * 
 * In between sets will also contain Global/per draw renderer stuff however that are updated a bit more than set 0 and a bit less than set 3
 */

namespace vkPlayground {

	enum class DescriptorResourceType : uint8_t
	{
		None = 0,
		UniformBuffer,
		UniformBufferSet,
		Texture2D
		// NOTE: Add more stuff as things are expanded
	};

	enum class RenderPassInputType
	{
		None = 0,
		UniformBuffer,
		ImageSampler1D,
		ImageSampler2D,
		ImageSampler3D
		// NOTE: Add more stuff as things are expanded
	};

	namespace Utils {

		inline constexpr RenderPassInputType GetRenderPassInputTypeFromVkDescriptorType(VkDescriptorType type)
		{
			switch (type)
			{
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:			return RenderPassInputType::UniformBuffer;
				case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return RenderPassInputType::ImageSampler2D;
			}

			return RenderPassInputType::None;
		}

		inline constexpr DescriptorResourceType GetDefaultResourceType(VkDescriptorType type)
		{
			switch (type)
			{
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:			return DescriptorResourceType::UniformBuffer;
				case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return DescriptorResourceType::Texture2D;
			}

			return DescriptorResourceType::None;
		}

		inline constexpr const char* VkDescriptorTypeToString(VkDescriptorType type)
		{
			switch (type)
			{
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:			return "UniformBuffer";
				case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return "CombindImageSampler";
			}

			return "UNKNOWN";
		}

		inline constexpr const char* DescriptorResourceTypeToString(DescriptorResourceType type)
		{
			switch (type)
			{
				case DescriptorResourceType::UniformBuffer:
				case DescriptorResourceType::UniformBufferSet:  return "UniformBuffer";
				case DescriptorResourceType::Texture2D:			return "Texture2D";
			}

			return "UNKNOWN";
		}

		inline constexpr bool IsCompatibleInput(DescriptorResourceType type, VkDescriptorType vkType)
		{
			switch (vkType)
			{
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				{
					return (type == DescriptorResourceType::UniformBuffer) || (type == DescriptorResourceType::UniformBufferSet);
				}
				case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				{
					return type == DescriptorResourceType::Texture2D;
				}
			}

			return false;
		}

	}

	// Forward declares
	class Shader;
	class Texture2D;
	class UniformBuffer;
	class UniformBufferSet;

	struct RenderPassInput
	{
		DescriptorResourceType Type = DescriptorResourceType::None;
		std::vector<Ref<RefCountedObject>> Input; // vector since we could have array of textures in the shader

		RenderPassInput() = default;

		RenderPassInput(Ref<UniformBuffer> uniformBuffer)
			: Type(DescriptorResourceType::UniformBuffer), Input(std::vector<Ref<RefCountedObject>>(1, uniformBuffer))
		{
		}

		RenderPassInput(Ref<UniformBufferSet> uniformBufferSet)
			: Type(DescriptorResourceType::UniformBufferSet), Input(std::vector<Ref<RefCountedObject>>(1, uniformBufferSet))
		{
		}

		RenderPassInput(Ref<Texture2D> texture)
			: Type(DescriptorResourceType::Texture2D), Input(std::vector<Ref<RefCountedObject>>(1, texture))
		{
		}

		void Set(Ref<UniformBuffer> uniformBuffer, uint32_t index = 0)
		{
			Type = DescriptorResourceType::UniformBuffer;
			Input[index] = uniformBuffer;
		}

		void Set(Ref<UniformBufferSet> uniformBufferSet, uint32_t index = 0)
		{
			Type = DescriptorResourceType::UniformBufferSet;
			Input[index] = uniformBufferSet;
		}

		void Set(Ref<Texture2D> texture, uint32_t index = 0)
		{
			Type = DescriptorResourceType::Texture2D;
			Input[index] = texture;
		}
	};

	struct RenderPassInputDeclaration
	{
		std::string Name;
		RenderPassInputType Type = RenderPassInputType::None;
		uint32_t Set = 0;
		uint32_t Binding = 0;
		uint32_t Count = 0;
	};

	struct DescriptorSetManagerSpecification
	{
		Ref<Shader> Shader;
		std::string DebugName;
		
		// TODO: Create some sort of flag here to specify if a descriptor set should be updated or not, because for something like BRDFLut that
		// will not change through out the program it will only be updated once and therefore we could have only one shared descriptor set for it
		// and we dont have to duplicate its sets to frame in flights duplicates

		// Leave this on true... better to have default placeholder resources
		bool DefaultResources = true;

		// Sets to be managed by the Manager instance
		uint32_t StartingSet = 0;
		uint32_t EndingSet = 3;
	};

	class DescriptorSetManager
	{
	public:
		struct WriteDescriptor
		{
			VkWriteDescriptorSet WriteDescriptorSet;
			// This is used to cache data about resources before (if any) where invalidated and determines whether we need to update the write descriptors
			std::vector<void*> ResourceHandles;
		};

	public:
		DescriptorSetManager() = default;
		DescriptorSetManager(const DescriptorSetManagerSpecification& spec) noexcept;
		~DescriptorSetManager();

		bool Validate();
		void Bake();
		void InvalidateAndUpdate();

		void SetInput(std::string_view name, Ref<UniformBuffer> uniformBuffer);
		void SetInput(std::string_view name, Ref<UniformBufferSet> uniformBufferSet);
		void SetInput(std::string_view name, Ref<Texture2D> texture, uint32_t index = 0);

		template<typename T>
		Ref<T> GetInput(std::string_view name)
		{
			const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
			if (decl)
			{
				auto setIt = m_InputResources.find(decl->Set);
				if (setIt != m_InputResources.end())
				{
					auto resourceIt = setIt->second.find(decl->Binding);
					if (resourceIt != setIt->second.end())
						return resourceIt->second.Input[0];
				}
			}

			return nullptr;
		}

		bool IsInvalidated(uint32_t set, uint32_t binding) const;

		// Finds descriptor sets that have a set of buffers (UniformBufferSet, ...) descriptors
		std::set<uint32_t> HasBufferSets() const;
		bool HasDescriptorSets() const;
		uint32_t GetFirstSetIndex() const;
		const std::vector<VkDescriptorSet>& GetDescriptorSets(uint32_t frameIndex) const;

		VkDescriptorPool GetDescriptorPool() const { return m_DescriptorPool; }

	private:
		void Init();
		const RenderPassInputDeclaration* GetInputDeclaration(std::string_view name) const;

	private:
		DescriptorSetManagerSpecification m_Specification;
		VkDescriptorPool m_DescriptorPool = nullptr;

		// Frame in flight -> set
		std::vector<std::vector<VkDescriptorSet>> m_DescriptorSets;

		// Input resources (Set -> Binding -> Resource)
		std::map<uint32_t, std::map<uint32_t, RenderPassInput>> m_InputResources;
		// This is to store whether any resources where invalidated during the frame and whether we need to update the write descriptors
		std::map<uint32_t, std::map<uint32_t, RenderPassInput>> m_InvalidatedInputResources;
		std::map<std::string, RenderPassInputDeclaration> m_InputDeclarations;

		// Frame in flight -> set -> binding -> WriteDescriptor
		std::vector<std::map<uint32_t, std::map<uint32_t, WriteDescriptor>>> m_WriteDescriptorMap;

		friend class RenderPass;

	};

}