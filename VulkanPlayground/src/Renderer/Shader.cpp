#include "Shader.h"

#include <spirv_cross/spirv_glsl.hpp>

#include <chrono>

namespace vkPlayground::Renderer {

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

		shaderc_shader_kind ShaderTypeFromString(const std::string& type)
		{
			if (type == "vertex")    return shaderc_vertex_shader;
			if (type == "fragment")  return shaderc_fragment_shader;
			if (type == "compute")   return shaderc_compute_shader;

			return (shaderc_shader_kind)-1;
		}

		std::string FileExtensionFromType(shaderc_shader_kind type)
		{
			switch (type)
			{
				case shaderc_fragment_shader: return ".cachedVulkan.Frag";
				case shaderc_vertex_shader:   return ".cachedVulkan.Vert";
				case shaderc_compute_shader:  return ".cachedVulkan.Comp";
			}

			return "";
		}

		std::string ShaderTypeToString(shaderc_shader_kind type)
		{
			switch (type)
			{
				case shaderc_fragment_shader: return "Fragment";
				case shaderc_vertex_shader:   return "Vertex";
				case shaderc_compute_shader:  return "Compute";
			}

			return "";
		}

		std::string ReadTextFile(const std::string& filePath)
		{
			std::string result;
			
			FILE* f;
			fopen_s(&f, filePath.c_str(), "rb");
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

	Ref<Shader> Shader::Create(const std::string& path)
	{
		return CreateRef<Shader>(path);
	}

	Shader::Shader(const std::string& path)
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
				std::cerr << "Syntax Error!" << std::endl;
			}

			size_t typeBegin = pos + typeIdentifierLength + 1;
			std::string type = source.substr(typeBegin, eol - typeBegin);
			if (type != "vertex" && type != "fragment")
			{
				std::cerr << "Unknown shader type!" << std::endl;
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
				std::cout << "[Shader]: Found cache, loading shader cache..." << std::endl;

				fseek(f, 0, SEEK_END);
				uint64_t size = ftell(f);
				fseek(f, 0, SEEK_SET);
				m_VulkanSPIRV[type].resize(size / sizeof(uint32_t));
				fread(m_VulkanSPIRV[type].data(), sizeof(uint32_t), size / sizeof(uint32_t), f);
				fclose(f);
			}
			else
			{
				std::cout << "[Shader]: No cache found! Compiling shaders..." << std::endl;

				if (f)
				{
					fclose(f); // Stop the file leak in-case force compile was set to true!
				}

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
					std::cerr << "Compilation failed in stage: " << Utils::ShaderTypeToString(type) << std::endl;
					std::cerr << "\tError Message: " << result.GetErrorMessage();

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