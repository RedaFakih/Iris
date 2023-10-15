#include "Shader.h"

#include "Renderer/Core/Vulkan.h"
#include "Renderer/Core/RendererContext.h"

#include <spirv_cross/spirv_glsl.hpp>
#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>

#include <chrono>

namespace vkPlayground {

	namespace Utils {

		constexpr const char* GetCacheDir()
		{
			return "Shaders/Cache";
		}

		void CreateCacheDirIfNeeded()
		{
			std::filesystem::path cacheDir = GetCacheDir();
			if (!std::filesystem::exists(cacheDir))
				std::filesystem::create_directories(cacheDir);
		}

		constexpr shaderc_shader_kind ShaderTypeFromString(const std::string& type)
		{
			if (type == "vertex")    return shaderc_vertex_shader;
			if (type == "fragment")  return shaderc_fragment_shader;
			if (type == "compute")   return shaderc_compute_shader;

			return (shaderc_shader_kind)-1;
		}

		constexpr VkShaderStageFlagBits VkShaderStageFromShadercShaderKind(shaderc_shader_kind type)
		{
			switch (type)
			{
				case shaderc_fragment_shader:   return VK_SHADER_STAGE_FRAGMENT_BIT;
				case shaderc_vertex_shader:		return VK_SHADER_STAGE_VERTEX_BIT;
				case shaderc_compute_shader:	return VK_SHADER_STAGE_COMPUTE_BIT;
			}

			return (VkShaderStageFlagBits)-1;
		}

		constexpr std::string FileExtensionFromType(shaderc_shader_kind type)
		{
			switch (type)
			{
				case shaderc_fragment_shader: return ".cachedVulkan.Frag";
				case shaderc_vertex_shader:   return ".cachedVulkan.Vert";
				case shaderc_compute_shader:  return ".cachedVulkan.Comp";
			}

			return "";
		}

		constexpr std::string ShaderTypeToString(shaderc_shader_kind type)
		{
			switch (type)
			{
				case shaderc_fragment_shader: return "Fragment";
				case shaderc_vertex_shader:   return "Vertex";
				case shaderc_compute_shader:  return "Compute";
			}

			return "";
		}

		std::string ReadTextFile(std::string_view filePath)
		{
			std::string result;
			
			FILE* f;
			// NOTE: It is safe to call `.data()` here since the file path will be provided via a string literal which is guaranteed to be null terminated
			fopen_s(&f, filePath.data(), "rb");
			if (f)
			{
				fseek(f, 0, SEEK_END);
				uint64_t size = ftell(f);
				fseek(f, 0, SEEK_SET);
				result.resize(size / sizeof(char));
				fread(result.data(), sizeof(char), size / sizeof(char), f);
				fclose(f);
			}

			return result;
		}
	}

	Ref<Shader> Shader::Create(std::string_view path)
	{
		return CreateRef<Shader>(path);
	}

	Shader::Shader(std::string_view path)
		: m_FilePath(path)
	{
		{
			size_t lastSlash = path.find_last_of("/\\");
			lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;

			size_t lastDot = path.rfind('.');
			size_t filenameSize = lastDot == std::string::npos ? path.size() - lastSlash : lastDot - lastSlash;
			m_Name = path.substr(lastSlash, filenameSize);
		}

		{
			std::string fullSource = Utils::ReadTextFile(path);
			LoadTwoStageShader(fullSource);
			// TODO: LoadCompute?
		}
	}

	Shader::~Shader()
	{
	}

	void Shader::LoadTwoStageShader(const std::string& source)
	{
		std::unordered_map<shaderc_shader_kind, std::string> vulkanSourceCode = PreProcess(source);

		bool forceCompile = GetLastTimeModified() < s_CompileTimeThreshold ? true : false;

		CompileVulkanBinaries(vulkanSourceCode, forceCompile);
		CreateProgram();
	}

	std::unordered_map<shaderc_shader_kind, std::string> Shader::PreProcess(const std::string& source)
	{
		// TODO: For compute shaders, there should be another specialized function for them...
		std::unordered_map<shaderc_shader_kind, std::string> shaderSources;

		constexpr const char* typeIdentifier = "#pragma";
		uint32_t typeIdentifierLength = (uint32_t)strlen(typeIdentifier);
		size_t pos = source.find(typeIdentifier, 0);

		while (pos != std::string::npos)
		{
			size_t eol = source.find_first_of("\r\n", pos);
			if (eol == std::string::npos)
			{
				PG_CORE_ERROR_TAG("ShaderCompiler", "Syntax Error!");
			}

			size_t typeBegin = pos + typeIdentifierLength + 1;
			std::string type = source.substr(typeBegin, eol - typeBegin);
			if (type != "vertex" && type != "fragment")
			{
				PG_CORE_ERROR_TAG("ShaderCompiler", "Unknown shader type!");
			}

			size_t nextLinePos = source.find_first_not_of("\r\n", eol);

			pos = source.find(typeIdentifier, nextLinePos);

			shaderc_shader_kind shaderType = Utils::ShaderTypeFromString(type);
			shaderSources[shaderType] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
		}

		return shaderSources;
	}

	bool Shader::CompileVulkanBinaries(const std::unordered_map<shaderc_shader_kind, std::string>& source, bool forceCompile)
	{
		Utils::CreateCacheDirIfNeeded();
		std::filesystem::path cacheDir = Utils::GetCacheDir();

		for (const auto& [type, spirv] : source)
		{
			std::filesystem::path cachedPath = cacheDir / (m_FilePath.filename().string() + Utils::FileExtensionFromType(type));
			std::string p = cachedPath.string();

			FILE* f;
			fopen_s(&f, p.c_str(), "rb");
			if (f && !forceCompile)
			{
				PG_CORE_DEBUG_TAG("Shader", "Found cache, loading shader cache...");

				fseek(f, 0, SEEK_END);
				uint64_t size = ftell(f);
				fseek(f, 0, SEEK_SET);
				m_VulkanSPIRV[type].resize(size / sizeof(uint32_t));
				fread(m_VulkanSPIRV[type].data(), sizeof(uint32_t), size / sizeof(uint32_t), f);
				fclose(f);
			}
			else
			{
				PG_CORE_DEBUG_TAG("Shader", "No cache found! Compiling shaders...");

				if (f)
					fclose(f); // Stop the file leak in-case force compile was set to true!

				shaderc::Compiler compiler;
				shaderc::CompileOptions options;

				options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
				options.AddMacroDefinition("PG_DEBUG");
				//options.SetGenerateDebugInfo();
				//options.SetAutoMapLocations(true);
				//options.SetAutoSampledTextures(true);

				constexpr bool optimize = false;
				if (optimize)
					options.SetOptimizationLevel(shaderc_optimization_level_performance);

				shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(spirv, type, m_FilePath.string().c_str(), options);
				if (result.GetCompilationStatus() != shaderc_compilation_status_success)
				{
					PG_CORE_ERROR_TAG("ShaderCompiler", "Compilation failed in stage: {0}", Utils::ShaderTypeToString(type));
					PG_CORE_ERROR_TAG("ShaderCompiler", "\tError message: {0}", result.GetErrorMessage());

					return false;
				}

				m_VulkanSPIRV[type] = std::vector<uint32_t>(result.cbegin(), result.cend());

				// Cache the compiled shader...
				FILE* ftoCache;
				fopen_s(&ftoCache, p.c_str(), "wb");
				if (ftoCache)
				{
					fwrite(m_VulkanSPIRV[type].data(), sizeof(uint32_t), m_VulkanSPIRV[type].size(), ftoCache);
					fclose(ftoCache);
				}
			}
		}
		
		return true;
	}

	void Shader::CreateProgram()
	{
		VkDevice device = RendererContext::Get()->GetDevice()->GetVulkanDevice();

		for (const auto& [type, spirvBin] : m_VulkanSPIRV)
		{
			VkShaderModuleCreateInfo shaderCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.codeSize = spirvBin.size() * sizeof(uint32_t),
				.pCode = spirvBin.data()
			};

			VkShaderModule shaderModule;
			VK_CHECK_RESULT(vkCreateShaderModule(device, &shaderCreateInfo, nullptr, &shaderModule));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_SHADER_MODULE, std::format("{}.{}", m_Name, Utils::ShaderTypeToString(type)), shaderModule);
			
			m_PipelineShaderStageCreateInfos.emplace_back(VkPipelineShaderStageCreateInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = Utils::VkShaderStageFromShadercShaderKind(type),
				.module = shaderModule,
				.pName = "main"
			});
		}
	}

	void Shader::Release()
	{
		VkDevice device = RendererContext::Get()->GetDevice()->GetVulkanDevice();
		for (const auto& pipelineShaderStageCIs : m_PipelineShaderStageCreateInfos)
		{
			if (pipelineShaderStageCIs.module)
				vkDestroyShaderModule(device, pipelineShaderStageCIs.module, nullptr);
		}

		for (auto& pipelineShaderStageCIs : m_PipelineShaderStageCreateInfos)
			pipelineShaderStageCIs.module = nullptr;

		m_PipelineShaderStageCreateInfos.clear();
	}

	uint32_t Shader::GetLastTimeModified() const
	{
		// Reference: https://stackoverflow.com/questions/61030383/how-to-convert-stdfilesystemfile-time-type-to-time-t
		std::filesystem::file_time_type lastTime = std::filesystem::last_write_time(m_FilePath);
		uint64_t ticks = lastTime.time_since_epoch().count() - std::filesystem::__std_fs_file_time_epoch_adjustment;

		// create a time_point from ticks
		std::chrono::system_clock::time_point tp{ std::chrono::system_clock::time_point::duration(ticks) };

		return std::chrono::duration_cast<std::chrono::minutes>(std::chrono::system_clock::now() - tp).count();
	}

}