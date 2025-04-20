#include "IrisPCH.h"
#include "DescriptorSetManager.h"

#include "Renderer.h"
#include "Renderer/Core/Vulkan.h"
#include "Shaders/Shader.h"

namespace Iris {

	namespace Utils {

		inline constexpr DescriptorResourceType GetDefaultResourceType(VkDescriptorType type, spv::Dim dimension = spv::Dim::DimMax)
		{
			if (dimension != spv::Dim::DimMax)
			{
				if (type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE && dimension == spv::Dim::DimCube)
					return DescriptorResourceType::TextureCube;
				else if (type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE && dimension == spv::Dim::Dim2D)
					return DescriptorResourceType::StorageImage;
			}
			else
			{
				switch (type)
				{
					case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:			return DescriptorResourceType::UniformBuffer;
					case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:			return DescriptorResourceType::StorageBuffer;
					case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
					case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return DescriptorResourceType::Texture2D;
					case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:			return DescriptorResourceType::StorageImage;
				}
			}

			IR_ASSERT(false);
			return DescriptorResourceType::None;
		}

	}

	DescriptorSetManager::DescriptorSetManager(const DescriptorSetManagerSpecification& spec)
		: m_Specification(spec)
	{
		Init();
		if (m_Specification.TriggerCopy)
		{
			m_InputResources = m_Specification.ExistingInputResources;
		}
	}

	DescriptorSetManager::~DescriptorSetManager()
	{
		Release();
	}

	void DescriptorSetManager::Release()
	{
		Renderer::SubmitReseourceFree([descriptorPool = m_DescriptorPool]()
		{
			VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
			if (descriptorPool)
				vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		});
	}

	void DescriptorSetManager::Init()
	{
		const std::vector<ShaderResources::ShaderDescriptorSet>& shaderDescriptorSets = m_Specification.Shader->GetShaderDescriptorSets();
		uint32_t framesInFlight = Renderer::GetConfig().FramesInFlight;
		m_WriteDescriptorMap.resize(framesInFlight);

		for (uint32_t set = m_Specification.StartingSet; set <= m_Specification.EndingSet; set++)
		{
			// If the set does not exist in the shader...
			if (set >= shaderDescriptorSets.size())
				return;

			const ShaderResources::ShaderDescriptorSet& shaderDescriptorSet = shaderDescriptorSets[set];
			for (auto& [name, writeDescriptor] : shaderDescriptorSet.WriteDescriptorSets)
			{
				uint32_t binding = writeDescriptor.dstBinding;

				RenderPassInputDeclaration& inputDeclaration = m_InputDeclarations[name];
				inputDeclaration.Name = name;
				inputDeclaration.Type = Utils::GetRenderPassInputTypeFromVkDescriptorType(writeDescriptor.descriptorType);
				inputDeclaration.Set = set;
				inputDeclaration.Binding = binding;
				// The number of elements in the array or 1 if no array exists in the shader for this descriptor
				inputDeclaration.Count = writeDescriptor.descriptorCount;

				spv::Dim currentDimension = spv::Dim::DimMax;
				if (shaderDescriptorSet.ImageSamplers.contains(binding))
				{
					const ShaderResources::ImageSampler& imageSampler = shaderDescriptorSet.ImageSamplers.at(binding);
					spv::Dim dimension = imageSampler.Dimension;
					if (writeDescriptor.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || writeDescriptor.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
					{
						switch (dimension)
						{
							case spv::Dim1D:
								inputDeclaration.Type = RenderPassInputType::ImageSampler1D;
								break;
							case spv::Dim2D:
								inputDeclaration.Type = RenderPassInputType::ImageSampler2D;
								break;
							case spv::Dim3D:
								inputDeclaration.Type = RenderPassInputType::ImageSampler3D;
								break;
						}
					}
				}
				else if (shaderDescriptorSet.StorageImages.contains(binding))
				{
					const ShaderResources::ImageSampler& imageSampler = shaderDescriptorSet.StorageImages.at(binding);
					spv::Dim dimension = imageSampler.Dimension;
					currentDimension = dimension;
					if (writeDescriptor.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
					{
						switch (dimension)
						{
							case spv::Dim1D:
								inputDeclaration.Type = RenderPassInputType::StorageImage1D;
								break;
							case spv::Dim2D:
								inputDeclaration.Type = RenderPassInputType::StorageImage2D;
								break;
							case spv::Dim3D:
								inputDeclaration.Type = RenderPassInputType::StorageImage3D;
								break;
							case spv::DimCube:
								inputDeclaration.Type = RenderPassInputType::TextureCube;
								break;
						}
					}
				}

				if (m_Specification.DefaultResources)
				{
					RenderPassInput& input = m_InputResources[set][binding];
					input.Type = Utils::GetDefaultResourceType(writeDescriptor.descriptorType, currentDimension);
					input.Input.resize(writeDescriptor.descriptorCount);

					// Set default textures
					if (inputDeclaration.Type == RenderPassInputType::ImageSampler2D)
					{
						for (std::size_t i = 0; i < input.Input.size(); i++)
						{
							input.Input[i] = Renderer::GetWhiteTexture();
						}

						IR_CORE_WARN_TAG("Renderer", "[RenderPass ({})::Init] Setting {} to white 2D texture.", m_Specification.DebugName, name);
					}
					else if (inputDeclaration.Type == RenderPassInputType::StorageImage2D)
					{
						for (std::size_t i = 0; i < input.Input.size(); i++)
						{
							input.Input[i] = Renderer::GetStorageImage();
						}

						IR_CORE_WARN_TAG("Renderer", "[RenderPass ({})::Init] Setting {} to white 2D texture.", m_Specification.DebugName, name);
					}
					else if (inputDeclaration.Type == RenderPassInputType::ImageSampler3D)
					{
						for (std::size_t i = 0; i < input.Input.size(); i++)
						{
							input.Input[i] = Renderer::GetBlackCubeTexture();
						}
					}
					else if (inputDeclaration.Type == RenderPassInputType::TextureCube)
					{
						for (std::size_t i = 0; i < input.Input.size(); i++)
						{
							input.Input[i] = Renderer::GetBlackCubeTexture();
						}
					}
				}

				for (uint32_t frameIndex = 0; frameIndex < framesInFlight; frameIndex++)
					m_WriteDescriptorMap[frameIndex][set][binding] = {
						.WriteDescriptorSet = writeDescriptor,
						.ResourceHandles = std::vector<void*>(writeDescriptor.descriptorCount)
					};
			}
		}
	}

	bool DescriptorSetManager::Validate()
	{
		const std::vector<ShaderResources::ShaderDescriptorSet>& shaderDescriptorSets = m_Specification.Shader->GetShaderDescriptorSets();


		for (uint32_t set = m_Specification.StartingSet; set <= m_Specification.EndingSet; set++)
		{
			if (set >= shaderDescriptorSets.size())
				break;

			// If no descriptors are in the current set
			if (!shaderDescriptorSets.at(set))
				continue;

			// Check for set availablity
			if (!m_InputResources.contains(set))
			{
				IR_CORE_ERROR_TAG("Renderer", "[RenderPass: ({})::Validate] No input resources for set: {}", m_Specification.DebugName, set);
				return false;
			}

			const std::map<uint32_t, RenderPassInput>& setInputResources = m_InputResources.at(set);
			const ShaderResources::ShaderDescriptorSet& shaderDescriptor = shaderDescriptorSets.at(set);
			for (auto& [name, writeDescriptor] : shaderDescriptor.WriteDescriptorSets)
			{
				// Check for binding availability
				uint32_t binding = writeDescriptor.dstBinding;
				if (!setInputResources.contains(binding))
				{
					IR_CORE_ERROR_TAG("Renderer", "[RenderPass: ({})::Validate] No input resources for set: {} at binding: {}", m_Specification.DebugName, set, binding);
					IR_CORE_ERROR_TAG("Renderer", "[RenderPass: ({})::Validate] Required input resource is: {} ({})", m_Specification.DebugName, name, Utils::VkDescriptorTypeToString(writeDescriptor.descriptorType));
					return false;
				}

				const RenderPassInput& resource = setInputResources.at(binding);
				if (!Utils::IsCompatibleInput(resource.Type, writeDescriptor.descriptorType))
				{
					IR_CORE_ERROR_TAG("Renderer", "[RenderPass: ({})::Validate] Incompatible input resource type (Provided: {}, Needed: {})", m_Specification.DebugName, Utils::DescriptorResourceTypeToString(resource.Type), Utils::VkDescriptorTypeToString(writeDescriptor.descriptorType));
					return false;
				}

				if (resource.Input[0] == nullptr)
				{
					IR_CORE_ERROR_TAG("Renderer", "[RenderPass: ({})::Validate] Input resource is null! (name: {} set: {} binding: {})", m_Specification.DebugName, name, set, binding);
					return false;
				}
			}
		}

		// All resourcse present
		return true;
	}

	void DescriptorSetManager::Bake()
	{
		if (!Validate())
		{
			IR_CORE_ERROR_TAG("Renderer", "[RenderPass ({})::Bake] Validation failed!", m_Specification.DebugName);
			IR_VERIFY(false);
			return;
		}

		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
		// We want to have copies of each descriptor set for each frame in flight
		uint32_t descriptorSetCount = Renderer::GetConfig().FramesInFlight;

		// Create DescriptorPool
		{
			// Get the global amount of VkDescriptorPoolSize after all shaders have been reflected
			const std::unordered_map<VkDescriptorType, uint32_t> descriptorPoolSizes = m_Specification.Shader->GetDescriptorPoolSizes();

			std::vector<VkDescriptorPoolSize> sizes(descriptorPoolSizes.size());
			uint32_t m = 0;
			for (const auto& [type, size] : descriptorPoolSizes)
			{
				sizes[m] = VkDescriptorPoolSize{ .type = type, .descriptorCount = size };
				++m;
			}

			VkDescriptorPoolCreateInfo descPoolInfo = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				//  The flag is actually not needed for the descriptor set manager since it does not manually free descriptor sets which lowers performance...
				// .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
				.maxSets = 10 * descriptorSetCount, // frames in flight should contribute to this variable since we have 4 descriptor sets in total
				// however the Manager will be split between renderpass(manages 3 sets) and material(manages 1 set) and so ten is number higher than
				// the min required number of sets to be allocated by the renderpass(3 * 3) or the material(1 * 3).
				.poolSizeCount = static_cast<uint32_t>(sizes.size()),
				.pPoolSizes = sizes.data()
			};

			VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &m_DescriptorPool));
		}

		// - Allocate DescriptorSets
		// - Complete the write descriptors that were partially created in the shader and then call vkUpdateDescriptorSets

		// Returns the sets that contain a UniformBufferSet/StorageBufferSet resource
		std::set<uint32_t> bufferSets = HasBufferSets();

		// Add the per-frame DescriptorSet vectors...
		for (uint32_t i = 0; i < descriptorSetCount; i++)
			m_DescriptorSets.emplace_back();

		// Clear the per-frame DescriptorSet vectors...
		for (std::vector<VkDescriptorSet>& descriptorSet : m_DescriptorSets)
			descriptorSet.clear();

		for (const auto& [set, setData] : m_InputResources)
		{
			// NOTE: Need to duplicate all the sets to the number of frames in flight.
			// NOTE: In the future we should add the option to specify what sets should not be duplicated since they are static resources that
			// do not get updated throughout the lifetime of the application
			VkDescriptorSetLayout dsl = m_Specification.Shader->GetDescriptorSetLayout(set);
			for (uint32_t frameIndex = 0; frameIndex < descriptorSetCount; frameIndex++)
			{
				VkDescriptorSetAllocateInfo allocInfo = {
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
					.descriptorPool = m_DescriptorPool,
					.descriptorSetCount = 1,
					.pSetLayouts = &dsl
				};

				VkDescriptorSet descriptorSet = nullptr;
				VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
				m_DescriptorSets[frameIndex].emplace_back(descriptorSet); // Store the per-frame DescriptorSets

				std::map<uint32_t, WriteDescriptor>& writeDescriptorMap = m_WriteDescriptorMap[frameIndex].at(set);
				std::vector<std::vector<VkDescriptorImageInfo>> imageInfoStorage;
				uint32_t imageInfoStorageIndex = 0;

				for (const auto& [binding, input] : setData)
				{
					WriteDescriptor& storedWriteDescriptor = writeDescriptorMap.at(binding);

					// The write descriptor in the shader where filled with: `dstBinding`, `descriptorCount`, `descriptorType`
					VkWriteDescriptorSet& writeDescriptor = storedWriteDescriptor.WriteDescriptorSet;
					writeDescriptor.dstSet = descriptorSet;
					
					switch (input.Type)
					{
						case DescriptorResourceType::UniformBuffer:
						{
							Ref<UniformBuffer> uniformBuffer = input.Input[0].As<UniformBuffer>();
							writeDescriptor.pBufferInfo = &uniformBuffer->GetDescriptorBufferInfo();
							storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pBufferInfo->buffer;

							// Defer the resource if it does not exist yet...
							if (writeDescriptor.pBufferInfo->buffer == nullptr)
								m_InvalidatedInputResources[set][binding] = input;

							break;
						}
						case DescriptorResourceType::UniformBufferSet:
						{
							Ref<UniformBufferSet> uniformBufferSet = input.Input[0].As<UniformBufferSet>();
							writeDescriptor.pBufferInfo = &uniformBufferSet->Get(frameIndex)->GetDescriptorBufferInfo();
							storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pBufferInfo->buffer;

							// Defer the resource if it does not exist yet...
							if (writeDescriptor.pBufferInfo->buffer == nullptr)
								m_InvalidatedInputResources[set][binding] = input;

							break;
						}
						case DescriptorResourceType::StorageBuffer:
						{
							Ref<StorageBuffer> storageBuffer = input.Input[0].As<StorageBuffer>();
							writeDescriptor.pBufferInfo = &storageBuffer->GetDescriptorBufferInfo();
							storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pBufferInfo->buffer;

							// Defer the resource if it does not exist yet...
							if (writeDescriptor.pBufferInfo->buffer == nullptr)
								m_InvalidatedInputResources[set][binding] = input;

							break;
						}
						case DescriptorResourceType::StorageBufferSet:
						{
							Ref<StorageBufferSet> storageBufferSet = input.Input[0].As<StorageBufferSet>();
							writeDescriptor.pBufferInfo = &storageBufferSet->Get(frameIndex)->GetDescriptorBufferInfo();
							storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pBufferInfo->buffer;

							// Defer the resource if it does not exist yet...
							if (writeDescriptor.pBufferInfo->buffer == nullptr)
								m_InvalidatedInputResources[set][binding] = input;

							break;
						}
						case DescriptorResourceType::Texture2D:
						{
							if (input.Input.size() > 1)
							{
								imageInfoStorage.emplace_back(input.Input.size());
								for (uint32_t i = 0; i < input.Input.size(); i++)
								{
									Ref<Texture2D> texture = input.Input[i].As<Texture2D>();
									IR_ASSERT(texture->GetTextureSpecification().Usage == ImageUsage::Texture || texture->GetTextureSpecification().Usage == ImageUsage::Attachment);

									imageInfoStorage[imageInfoStorageIndex][i] = texture->GetDescriptorImageInfo();
								}
								writeDescriptor.pImageInfo = imageInfoStorage[imageInfoStorageIndex].data();
								++imageInfoStorageIndex;
							}
							else
							{
								Ref<Texture2D> texture = input.Input[0].As<Texture2D>();
								IR_ASSERT(texture->GetTextureSpecification().Usage == ImageUsage::Texture || texture->GetTextureSpecification().Usage == ImageUsage::Attachment);

								writeDescriptor.pImageInfo = &texture->GetDescriptorImageInfo();
							}

							storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pImageInfo->imageView;

							// Defer the resource if it does not exist yet...
							if (writeDescriptor.pImageInfo->imageView == nullptr)
								m_InvalidatedInputResources[set][binding] = input;

							break;
						}
						case DescriptorResourceType::TextureCube:
						{
							Ref<TextureCube> textureCube = input.Input[0].As<TextureCube>();
							writeDescriptor.pImageInfo = &textureCube->GetDescriptorImageInfo();
							storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pImageInfo->imageView;

							// Defer the resource if it does not exist...
							if (writeDescriptor.pImageInfo->imageView == nullptr)
								m_InvalidatedInputResources[set][binding] = input;

							break;
						}
						case DescriptorResourceType::ImageView:
						{
							Ref<ImageView> imageView = input.Input[0].As<ImageView>();

							// Defer the resource if it does not exist...
							if (imageView == nullptr)
							{
								m_InvalidatedInputResources[set][binding] = input;
								break;
							}

							writeDescriptor.pImageInfo = &imageView->GetDescriptorImageInfo();
							storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pImageInfo->imageView;

							// Defer the resource if it does not exist...
							if (writeDescriptor.pImageInfo->imageView == nullptr)
								m_InvalidatedInputResources[set][binding] = input;

							break;
						}
						case DescriptorResourceType::StorageImage:
						{
							Ref<Texture2D> storageImage = input.Input[0].As<Texture2D>();
							IR_ASSERT(storageImage->GetTextureSpecification().Usage == ImageUsage::Storage);

							writeDescriptor.pImageInfo = &storageImage->GetDescriptorImageInfo();
							storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pImageInfo->imageView;
						
							// Defer the resource if it does not exist...
							if (writeDescriptor.pImageInfo->imageView == nullptr)
								m_InvalidatedInputResources[set][binding] = input;
						
							break;
						}
					}
				}

				std::vector<VkWriteDescriptorSet> writeDescriptors;
				for (const auto& [binding, writeDescriptor] : writeDescriptorMap)
				{
					// Include if valid otherwise we defer it (untill when we want to use that way the resources are created)
					if (!IsInvalidated(set, binding))
						writeDescriptors.emplace_back(writeDescriptor.WriteDescriptorSet);
				}

				if (!writeDescriptors.empty())
				{
					IR_CORE_INFO_TAG("Renderer", "[RenderPass ({})::Bake] update {} descriptors in set {}", m_Specification.DebugName, writeDescriptors.size(), set);
					vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptors.size()), writeDescriptors.data(), 0, nullptr);
				}
			}
		}
	}

	void DescriptorSetManager::InvalidateAndUpdate()
	{
		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t currentFrameIndex = Renderer::RT_GetCurrentFrameIndex();

		// Check for invalidated resources...
		for (const auto& [set, inputs] : m_InputResources)
		{
			for (const auto& [binding, renderPassInput] : inputs)
			{
				switch (renderPassInput.Type)
				{
					case DescriptorResourceType::UniformBuffer:
					{
						Ref<UniformBuffer> uniformBuffer = renderPassInput.Input[0].As<UniformBuffer>();
						const VkDescriptorBufferInfo& bufferInfo = uniformBuffer->GetDescriptorBufferInfo();
						if (bufferInfo.buffer != m_WriteDescriptorMap[currentFrameIndex].at(set).at(binding).ResourceHandles[0])
							m_InvalidatedInputResources[set][binding] = renderPassInput;

						break;
					}
					case DescriptorResourceType::UniformBufferSet:
					{
						Ref<UniformBufferSet> uniformBufferSet = renderPassInput.Input[0].As<UniformBufferSet>();
						const VkDescriptorBufferInfo& bufferInfo = uniformBufferSet->Get(currentFrameIndex)->GetDescriptorBufferInfo();
						if (bufferInfo.buffer != m_WriteDescriptorMap[currentFrameIndex].at(set).at(binding).ResourceHandles[0])
							m_InvalidatedInputResources[set][binding] = renderPassInput;

						break;
					}
					case DescriptorResourceType::StorageBuffer:
					{
						Ref<StorageBuffer> storageBuffer = renderPassInput.Input[0].As<StorageBuffer>();
						const VkDescriptorBufferInfo& bufferInfo = storageBuffer->GetDescriptorBufferInfo();
						if (bufferInfo.buffer != m_WriteDescriptorMap[currentFrameIndex].at(set).at(binding).ResourceHandles[0])
							m_InvalidatedInputResources[set][binding] = renderPassInput;

						break;
					}
					case DescriptorResourceType::StorageBufferSet:
					{
						Ref<StorageBufferSet> storageBufferSet = renderPassInput.Input[0].As<StorageBufferSet>();
						const VkDescriptorBufferInfo& bufferInfo = storageBufferSet->Get(currentFrameIndex)->GetDescriptorBufferInfo();
						if (bufferInfo.buffer != m_WriteDescriptorMap[currentFrameIndex].at(set).at(binding).ResourceHandles[0])
							m_InvalidatedInputResources[set][binding] = renderPassInput;

						break;
					}
					case DescriptorResourceType::Texture2D:
					{
						for (std::size_t i = 0; i < renderPassInput.Input.size(); i++)
						{
							Ref<Texture2D> texture = renderPassInput.Input[i].As<Texture2D>();
							const VkDescriptorImageInfo& imageInfo = texture->GetDescriptorImageInfo();
							if (imageInfo.imageView != m_WriteDescriptorMap[currentFrameIndex].at(set).at(binding).ResourceHandles[i])
							{
								m_InvalidatedInputResources[set][binding] = renderPassInput;
								break;
							}
						}

						break;
					}
					case DescriptorResourceType::TextureCube:
					{
						Ref<TextureCube> textureCube = renderPassInput.Input[0].As<TextureCube>();
						const VkDescriptorImageInfo& imageInfo = textureCube->GetDescriptorImageInfo();
						if (imageInfo.imageView != m_WriteDescriptorMap[currentFrameIndex].at(set).at(binding).ResourceHandles[0])
							m_InvalidatedInputResources[set][binding] = renderPassInput;

						break;
					}
					case DescriptorResourceType::ImageView:
					{
						Ref<ImageView> imageView = renderPassInput.Input[0].As<ImageView>();
						const VkDescriptorImageInfo& imageInfo = imageView->GetDescriptorImageInfo();
						if (imageInfo.imageView != m_WriteDescriptorMap[currentFrameIndex].at(set).at(binding).ResourceHandles[0])
							m_InvalidatedInputResources[set][binding] = renderPassInput;

						break;
					}
					case DescriptorResourceType::StorageImage:
					{
						Ref<Texture2D> storageImage = renderPassInput.Input[0].As<Texture2D>();
						const VkDescriptorImageInfo& imageInfo = storageImage->GetDescriptorImageInfo();
						if (imageInfo.imageView != m_WriteDescriptorMap[currentFrameIndex].at(set).at(binding).ResourceHandles[0])
							m_InvalidatedInputResources[set][binding] = renderPassInput;
					
						break;
					}
				}
			}
		}

		// Nothing to do
		if (m_InvalidatedInputResources.empty())
			return;

		// Returns the sets that contain a UniformBufferSet/StorageBufferSet resource
		std::set<uint32_t> bufferSets = HasBufferSets();

		for (const auto& [set, setData] : m_InvalidatedInputResources)
		{
			std::vector<VkWriteDescriptorSet> writeDescriptorSetsToUpdate;
			writeDescriptorSetsToUpdate.reserve(setData.size());

			std::vector<std::vector<VkDescriptorImageInfo>> imageInfoStorage;
			uint32_t imageInfoStorageIndex = 0;
			for (const auto& [binding, input] : setData)
			{
				WriteDescriptor& storedWriteDescriptor = m_WriteDescriptorMap[currentFrameIndex].at(set).at(binding);

				VkWriteDescriptorSet& writeDescriptor = storedWriteDescriptor.WriteDescriptorSet;
				switch (input.Type)
				{
					case DescriptorResourceType::UniformBuffer:
					{
						Ref<UniformBuffer> uniformBuffer = input.Input[0].As<UniformBuffer>();
						writeDescriptor.pBufferInfo = &uniformBuffer->GetDescriptorBufferInfo();
						storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pBufferInfo->buffer;

						break;
					}
					case DescriptorResourceType::UniformBufferSet:
					{
						Ref<UniformBufferSet> uniformBufferSet = input.Input[0].As<UniformBufferSet>();
						writeDescriptor.pBufferInfo = &uniformBufferSet->Get(currentFrameIndex)->GetDescriptorBufferInfo();
						storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pBufferInfo->buffer;

						break;
					}
					case DescriptorResourceType::StorageBuffer:
					{
						Ref<StorageBuffer> storageBuffer = input.Input[0].As<StorageBuffer>();
						writeDescriptor.pBufferInfo = &storageBuffer->GetDescriptorBufferInfo();
						storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pBufferInfo->buffer;

						break;
					}
					case DescriptorResourceType::StorageBufferSet:
					{
						Ref<StorageBufferSet> storageBufferSet = input.Input[0].As<StorageBufferSet>();
						writeDescriptor.pBufferInfo = &storageBufferSet->Get(currentFrameIndex)->GetDescriptorBufferInfo();
						storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pBufferInfo->buffer;

						break;
					}
					case DescriptorResourceType::Texture2D:
					{
						if (input.Input.size() > 1)
						{
							imageInfoStorage.emplace_back(input.Input.size());
							for (uint32_t i = 0; i < input.Input.size(); i++)
							{
								Ref<Texture2D> texture = input.Input[i].As<Texture2D>();
								imageInfoStorage[imageInfoStorageIndex][i] = texture->GetDescriptorImageInfo();
								storedWriteDescriptor.ResourceHandles[i] = imageInfoStorage[imageInfoStorageIndex][i].imageView;
							}
							writeDescriptor.pImageInfo = imageInfoStorage[imageInfoStorageIndex].data();
							++imageInfoStorageIndex;
						}
						else
						{
							Ref<Texture2D> texture = input.Input[0].As<Texture2D>();
							writeDescriptor.pImageInfo = &texture->GetDescriptorImageInfo();
							storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pImageInfo->imageView;
						}

						break;
					}
					case DescriptorResourceType::TextureCube:
					{
						Ref<TextureCube> textureCube = input.Input[0].As<TextureCube>();
						writeDescriptor.pImageInfo = &textureCube->GetDescriptorImageInfo();
						storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pImageInfo->imageView;

						break;
					}
					case DescriptorResourceType::ImageView:
					{
						Ref<ImageView> imageView = input.Input[0].As<ImageView>();
						writeDescriptor.pImageInfo = &imageView->GetDescriptorImageInfo();
						IR_VERIFY(writeDescriptor.pImageInfo->imageView);
						storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pImageInfo->imageView;

						break;
					}
					case DescriptorResourceType::StorageImage:
					{
						Ref<Texture2D> storageImage = input.Input[0].As<Texture2D>();
						writeDescriptor.pImageInfo = &storageImage->GetDescriptorImageInfo();
						storedWriteDescriptor.ResourceHandles[0] = writeDescriptor.pImageInfo->imageView;
					
						break;
					}
				}

				writeDescriptorSetsToUpdate.emplace_back(writeDescriptor);
			}
			
			IR_CORE_INFO_TAG("Renderer", "[RenderPass ({})::InvalidateAndUpdate] updating {} descriptors in set {} (frameIndex = {})", m_Specification.DebugName, writeDescriptorSetsToUpdate.size(), set, currentFrameIndex);
			vkUpdateDescriptorSets(
				device, 
				static_cast<uint32_t>(writeDescriptorSetsToUpdate.size()),
				writeDescriptorSetsToUpdate.data(),
				0, nullptr
			);
		}

		m_InvalidatedInputResources.clear();
	}

	void DescriptorSetManager::SetInput(std::string_view name, Ref<UniformBuffer> uniformBuffer)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
			m_InputResources.at(decl->Set).at(decl->Binding).Set(uniformBuffer);
		else
			IR_CORE_ERROR_TAG("Renderer", "[RenderPass ({})::SetInput] Input {} not found!", m_Specification.DebugName, name);
	}

	void DescriptorSetManager::SetInput(std::string_view name, Ref<UniformBufferSet> uniformBufferSet)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
			m_InputResources.at(decl->Set).at(decl->Binding).Set(uniformBufferSet);
		else
			IR_CORE_ERROR_TAG("Renderer", "[RenderPass ({})::SetInput] Input {} not found!", m_Specification.DebugName, name);
	}

	void DescriptorSetManager::SetInput(std::string_view name, Ref<StorageBuffer> storageBuffer)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
			m_InputResources.at(decl->Set).at(decl->Binding).Set(storageBuffer);
		else
			IR_CORE_ERROR_TAG("Renderer", "[RenderPass ({})::SetInput] Input {} not found!", m_Specification.DebugName, name);
	}

	void DescriptorSetManager::SetInput(std::string_view name, Ref<StorageBufferSet> storageBufferSet)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);
		if (decl)
			m_InputResources.at(decl->Set).at(decl->Binding).Set(storageBufferSet);
		else
			IR_CORE_ERROR_TAG("Renderer", "[RenderPass ({})::SetInput] Input {} not found!", m_Specification.DebugName, name);
	}

	void DescriptorSetManager::SetInput(std::string_view name, Ref<Texture2D> texture, uint32_t index)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);

		IR_ASSERT(index < decl->Count);

		if (decl)
			m_InputResources.at(decl->Set).at(decl->Binding).Set(texture, index);
		else
			IR_CORE_ERROR_TAG("Renderer", "[RenderPass ({})::SetInput] Input {} not found!", m_Specification.DebugName, name);
	}

	void DescriptorSetManager::SetInput(std::string_view name, Ref<TextureCube> textureCube)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);

		if (decl)
			m_InputResources.at(decl->Set).at(decl->Binding).Set(textureCube);
		else
			IR_CORE_ERROR_TAG("Renderer", "[RenderPass ({})::SetInput] Input {} not found!", m_Specification.DebugName, name);
	}

	void DescriptorSetManager::SetInput(std::string_view name, Ref<ImageView> imageView)
	{
		const RenderPassInputDeclaration* decl = GetInputDeclaration(name);

		if (decl)
			m_InputResources.at(decl->Set).at(decl->Binding).Set(imageView);
		else
			IR_CORE_ERROR_TAG("Renderer", "[RenderPass ({})::SetInput] Input {} not found!", m_Specification.DebugName, name);
	}

	bool DescriptorSetManager::IsInvalidated(uint32_t set, uint32_t binding) const
	{
		if (m_InvalidatedInputResources.contains(set))
		{
			const auto& resource = m_InvalidatedInputResources.at(set);
			return resource.contains(binding);
		}

		return false;
	}

	std::set<uint32_t> DescriptorSetManager::HasBufferSets() const
	{
		std::set<uint32_t> result;

		for (auto& [set, data] : m_InputResources)
		{
			for (auto& [binding, input] : data)
			{
				if (input.Type == DescriptorResourceType::UniformBufferSet || input.Type == DescriptorResourceType::StorageBufferSet)
				{
					result.insert(set);
					break;
				}
			}
		}

		return result;
	}

	bool DescriptorSetManager::HasDescriptorSets() const
	{
		return !m_DescriptorSets.empty() && !m_DescriptorSets[0].empty();
	}

	uint32_t DescriptorSetManager::GetFirstSetIndex() const
	{
		if (m_InputResources.empty())
			return UINT32_MAX;

		return m_InputResources.begin()->first; // returning the first key which is the first set index
	}

	const std::vector<VkDescriptorSet>& DescriptorSetManager::GetDescriptorSets(uint32_t frameIndex) const
	{
		IR_ASSERT(!m_DescriptorSets.empty());

		if (frameIndex > 0 && m_DescriptorSets.size() == 1)
			return m_DescriptorSets[0]; // Frame index is irrelevant for this type of render pass since it has only one descriptor set

		return m_DescriptorSets[frameIndex];
	}

	const RenderPassInputDeclaration* DescriptorSetManager::GetInputDeclaration(std::string_view name) const
	{
		std::string nameStr = std::string(name);

		if (!m_InputDeclarations.contains(nameStr))
			return nullptr;

		const RenderPassInputDeclaration* declaration = &(m_InputDeclarations.at(nameStr));
		return declaration;
	}
}