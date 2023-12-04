#include "vkPch.h"
#include "Shader.h"

#include "Compiler/ShaderCompiler.h"
#include "Core/Hash.h"
#include "Renderer/Core/RendererContext.h"
#include "Renderer/Core/Vulkan.h"
#include "Renderer/Renderer.h"
#include "ShaderUtils.h"

#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan.h>

namespace vkPlayground {

	Ref<Shader> Shader::Create()
	{
		return CreateRef<Shader>();
	}

	Ref<Shader> Shader::Create(std::string_view path, bool forceCompile, bool disableOptimization)
	{
		return CreateRef<Shader>(path, forceCompile, disableOptimization);
	}

	Shader::Shader(std::string_view path, bool forceCompile, bool disableOptimization)
		: m_FilePath(path), m_DisableOptimizations(disableOptimization)
	{
		{
			size_t lastSlash = path.find_last_of("/\\");
			lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;

			size_t lastDot = path.rfind('.');
			size_t filenameSize = lastDot == std::string::npos ? path.size() - lastSlash : lastDot - lastSlash;
			m_Name = path.substr(lastSlash, filenameSize);
		}

		Reload();
	}

	Shader::~Shader()
	{
		std::vector<VkPipelineShaderStageCreateInfo>& pipelineCIs = m_PipelineShaderStageCreateInfos;
		std::vector<VkDescriptorSetLayout>& descriptorSetLayouts = m_DescriptorSetLayouts;
		Renderer::SubmitReseourceFree([pipelineCIs, descriptorSetLayouts]()
		{
			const VkDevice device = RendererContext::Get()->GetDevice()->GetVulkanDevice();

			for (const auto& pipelineShaderStageCI : pipelineCIs)
			{
				if (pipelineShaderStageCI.module)
					vkDestroyShaderModule(device, pipelineShaderStageCI.module, nullptr);
			}

			for (const auto& descriptorSetLayout : descriptorSetLayouts)
			{
				if (descriptorSetLayout)
					vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			}
		});
	}

	void Shader::Reload()
	{
		// TODO: READ:
		// This is actually a bug and needs to be done on a render thread!
		// This cant be done here on the main thread since we are passing the this pointer, which if this is being called from the constructor
		// is not fully initialized object yet so it will not work!
		// This has to be done on the render thread...
		// To properly initialize shaders at first we will need to use the ShaderCompiler::Compile method
		// Renderer::Submit([]() {});
		if (!ShaderCompiler::TryRecompile(this))
			PG_CORE_CRITICAL_TAG("Shader", "Failed to recompile shader!");
	}

	void Shader::Release()
	{
		auto& pipelineCIs = m_PipelineShaderStageCreateInfos;
		Renderer::SubmitReseourceFree([pipelineCIs]()
		{
			const VkDevice device = RendererContext::Get()->GetDevice()->GetVulkanDevice();

			for (const auto& pipelineShaderStageCI : pipelineCIs)
			{
				if (pipelineShaderStageCI.module)
					vkDestroyShaderModule(device, pipelineShaderStageCI.module, nullptr);
			}
		});

		for (auto& pipelineShaderStageCI : m_PipelineShaderStageCreateInfos)
			pipelineShaderStageCI.module = nullptr;

		m_PipelineShaderStageCreateInfos.clear();
		// NOTE: Cant clear those since they are needed for the DescriptorSetManager to create the DescriptorPool and other stuff
		// m_DescriptorSetLayouts.clear();
		// m_DescriptorPoolTypeCounts.clear();
	}

	std::size_t Shader::GetHash() const
	{
		return Hash::GenerateFNVHash(m_FilePath.string());
	}

	bool Shader::TryReadReflectionData(StreamReader* reader)
	{
		uint32_t shaderDescriptorSetCount;
		reader->ReadRaw<uint32_t>(shaderDescriptorSetCount);

		for (uint32_t i = 0; i < shaderDescriptorSetCount; i++)
		{
			ShaderResources::ShaderDescriptorSet& descriptorSet = m_ReflectionData.ShaderDescriptorSets.emplace_back();
			reader->ReadMap(descriptorSet.UniformBuffers);
			reader->ReadMap(descriptorSet.ImageSamplers);
			reader->ReadMap(descriptorSet.WriteDescriptorSets);
		}
		
		return true;
	}

	void Shader::SerializeReflectionData(StreamWriter* serializer)
	{
		serializer->WriteRaw<uint32_t>((uint32_t)m_ReflectionData.ShaderDescriptorSets.size());

		for (const auto& descriptorSet : m_ReflectionData.ShaderDescriptorSets)
		{
			serializer->WriteMap(descriptorSet.UniformBuffers);
			serializer->WriteMap(descriptorSet.ImageSamplers);
			serializer->WriteMap(descriptorSet.WriteDescriptorSets);
		}
	}

	std::vector<VkDescriptorSetLayout> Shader::GetAllDescriptorSetLayouts()
	{
		std::vector<VkDescriptorSetLayout> result;
		result.reserve(m_DescriptorSetLayouts.size());
		for (VkDescriptorSetLayout& layout : m_DescriptorSetLayouts)
			result.emplace_back(layout);

		return result;
	}

	ShaderResources::UniformBuffer& Shader::GetUniformBuffer(const uint32_t set, const uint32_t binding)
	{
		PG_ASSERT(m_ReflectionData.ShaderDescriptorSets.at(set).UniformBuffers.size() > binding, "");
		return m_ReflectionData.ShaderDescriptorSets.at(set).UniformBuffers.at(binding);
	}

	uint32_t Shader::GetUniformBufferCount(const uint32_t set)
	{
		// If we dont have enough sets as the one requested
		if (m_ReflectionData.ShaderDescriptorSets.size() < set)
			return 0;

		return static_cast<uint32_t>(m_ReflectionData.ShaderDescriptorSets.at(set).UniformBuffers.size());
	}

	const VkWriteDescriptorSet* Shader::GetDescriptorSet(const std::string& name, uint32_t set) const
	{
		PG_ASSERT(m_ReflectionData.ShaderDescriptorSets.size() > set, "");
		PG_ASSERT(m_ReflectionData.ShaderDescriptorSets[set], "");

		if (m_ReflectionData.ShaderDescriptorSets.at(set).WriteDescriptorSets.contains(name))
		{
			return &m_ReflectionData.ShaderDescriptorSets.at(set).WriteDescriptorSets.at(name);
		}
		else
		{
			PG_CORE_ERROR_TAG("Shader", "Shader {} does not contain the requested descriptor set {}", m_Name, name);
			return nullptr;
		}
	}

	void Shader::LoadAndCreateShaders(const std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData)
	{
		VkDevice device = RendererContext::Get()->GetDevice()->GetVulkanDevice();
		m_VulkanSPIRV = shaderData;
		m_PipelineShaderStageCreateInfos.clear();

		for (const auto& [stage, spirvBin] : m_VulkanSPIRV)
		{
			PG_ASSERT(spirvBin.size(), "");

			VkShaderModuleCreateInfo shaderModuleCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.codeSize = spirvBin.size() * sizeof(uint32_t),
				.pCode = spirvBin.data()
			};

			VkShaderModule shaderModule;
			VK_CHECK_RESULT(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_SHADER_MODULE, std::format("{}:{}", m_Name, ShaderUtils::VkShaderStageToString(stage)), shaderModule);

			m_PipelineShaderStageCreateInfos.emplace_back(VkPipelineShaderStageCreateInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = stage,
				.module = shaderModule,
				.pName = "main"
			});
		}
	}

	void Shader::CreateDescriptors()
	{
		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();

		/////////////////////////////////////////////////////////////////////////////////////////////////////////
		/// Descriptor Pool
		/////////////////////////////////////////////////////////////////////////////////////////////////////////

		// Resize the DSL vector to the number of sets we have in the shader
		m_DescriptorSetLayouts.resize(m_ReflectionData.ShaderDescriptorSets.size());
		for (uint32_t set = 0; set < m_ReflectionData.ShaderDescriptorSets.size(); set++)
		{
			auto& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[set];

			// Add to the global VkDescriptorPoolSize for the global descriptor pool in the DescriptorSetManager
			if (shaderDescriptorSet.UniformBuffers.size())
			{
				m_DescriptorPoolTypeCounts[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER] += (uint32_t)shaderDescriptorSet.UniformBuffers.size();
			}

			if (shaderDescriptorSet.ImageSamplers.size())
			{
				// TODO: Maybe also do it for `VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE`
				m_DescriptorPoolTypeCounts[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] += (uint32_t)shaderDescriptorSet.ImageSamplers.size();
			}

			/////////////////////////////////////////////////////////////////////////////////////////////////////
			/// Descriptor Set Layout
			/////////////////////////////////////////////////////////////////////////////////////////////////////

			std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
			for (auto& [binding, uniformBuffer] : shaderDescriptorSet.UniformBuffers)
			{
				VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.emplace_back();
				layoutBinding.binding = binding;
				layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				layoutBinding.descriptorCount = 1;
				layoutBinding.stageFlags = uniformBuffer.ShaderStage;
				layoutBinding.pImmutableSamplers = nullptr;

				// All the other files will be filled inside the DescriptorSetManager class which will be owned by a renderpass
				shaderDescriptorSet.WriteDescriptorSets[uniformBuffer.Name] = {
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstBinding = layoutBinding.binding,
					.descriptorCount = 1,
					.descriptorType = layoutBinding.descriptorType
				};
			}

			for (auto& [binding, imageSampler] : shaderDescriptorSet.ImageSamplers)
			{
				VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.emplace_back();
				layoutBinding.binding = binding;
				layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				layoutBinding.descriptorCount = imageSampler.ArraySize;
				layoutBinding.stageFlags = imageSampler.ShaderStage;
				layoutBinding.pImmutableSamplers = nullptr;

				PG_ASSERT(shaderDescriptorSet.UniformBuffers.contains(binding) == false, "Binding already present!");

				// All the other files will be filled inside the DescriptorSetManager class which will be owned by a renderpass
				shaderDescriptorSet.WriteDescriptorSets[imageSampler.Name] = {
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstBinding = layoutBinding.binding,
					.descriptorCount = layoutBinding.descriptorCount,
					.descriptorType = layoutBinding.descriptorType,
				};
			}

			VkDescriptorSetLayoutCreateInfo descriptorSetLaytoutCI = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.bindingCount = static_cast<uint32_t>(layoutBindings.size()),
				.pBindings = layoutBindings.data()
			};

			PG_CORE_INFO_TAG("Shader", "Creating descriptor set {} with {} ubo's, {} samplers", set, 
				shaderDescriptorSet.UniformBuffers.size(),
				shaderDescriptorSet.ImageSamplers.size()
			);

			m_ExistingSets.insert(set);

			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLaytoutCI, nullptr, &m_DescriptorSetLayouts[set]));
		}
	}

	Ref<ShadersLibrary> ShadersLibrary::Create()
	{
		return CreateRef<ShadersLibrary>();
	}

	void ShadersLibrary::Add(Ref<Shader> shader)
	{
		const std::string name(shader->GetName());
		PG_ASSERT(!m_Library.contains(name), "Shader already loaded!");
		m_Library[name] = shader;
	}

	void ShadersLibrary::Load(std::string_view filePath, bool forceCompile, bool disableOptimizations)
	{
		Ref<Shader> shader;
		shader = ShaderCompiler::Compile(filePath, forceCompile, disableOptimizations);

		const std::string name(shader->GetName());
		PG_ASSERT(!m_Library.contains(name), "Shader already loaded!");
		m_Library[name] = shader;
	}

	void ShadersLibrary::Load(std::string_view name, std::string_view filePath)
	{
		PG_ASSERT(!m_Library.contains(std::string(name)), "Shader already loaded!");
		m_Library[std::string(name)] = Shader::Create(filePath);
	}

	const Ref<Shader> ShadersLibrary::Get(const std::string& name) const
	{
		PG_ASSERT(m_Library.contains(name), fmt::format("Cant find shader with name: {}", name));
		return m_Library.at(name);
	}

}