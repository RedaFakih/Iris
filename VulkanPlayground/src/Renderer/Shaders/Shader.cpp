#include "Shader.h"

#include "Compiler/ShaderCompiler.h"
#include "Core/Hash.h"
#include "Renderer/Core/RendererContext.h"
#include "Renderer/Core/Vulkan.h"
#include "Renderer/Renderer.h"
#include "ShaderUtils.h"

#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan.h>

#include <chrono>

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
		m_DescriptorSetLayout.clear();
		m_TypeCounts.clear();
	}

	std::size_t Shader::GetHash() const
	{
		return Hash::GenerateFNVHash(m_FilePath.string());
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
		// TODO: ...
	}

	Ref<ShadersLibrary> ShadersLibrary::Create()
	{
		return CreateRef<ShadersLibrary>();
	}

	void ShadersLibrary::Add(Ref<Shader> shader)
	{
		const std::string name(shader->GetName());
		PG_ASSERT(m_Library.find(name) == m_Library.end(), "Shader already loaded!");
		m_Library[name] = shader;
	}

	void ShadersLibrary::Load(std::string_view filePath, bool forceCompile, bool disableOptimizations)
	{
		Ref<Shader> shader;
		shader = ShaderCompiler::Compile(filePath, forceCompile, disableOptimizations);

		const std::string name(shader->GetName());
		PG_ASSERT(m_Library.find(name) == m_Library.end(), "Shader already loaded!");
		m_Library[name] = shader;
	}

	void ShadersLibrary::Load(std::string_view name, std::string_view filePath)
	{
		PG_ASSERT(m_Library.find(std::string(name)) == m_Library.end(), "Shader already loaded!");
		m_Library[std::string(name)] = Shader::Create(filePath);
	}

	const Ref<Shader> ShadersLibrary::Get(const std::string& name) const
	{
		PG_ASSERT(m_Library.find(name) != m_Library.end(), "Cant find shader with name: {}", name);
		return m_Library.at(name);
	}

}