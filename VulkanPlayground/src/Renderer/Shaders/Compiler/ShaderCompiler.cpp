#include "ShaderCompiler.h"

#include "Core/Hash.h"
#include "Renderer/Core/RendererContext.h"

#include "Renderer/Shaders/Shader.h"
#include "Renderer/Shaders/ShaderUtils.h"

#include "Renderer/Renderer.h"
#include "Renderer/Shaders/ShaderRegistry.h"

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include <fstream>

namespace vkPlayground {

	namespace Utils {

		inline static std::filesystem::path CreateCacheDirIfNeeded()
		{
			std::filesystem::path cacheDir = "Shaders/Cache";
			if (!std::filesystem::exists(cacheDir))
				std::filesystem::create_directories(cacheDir);

			return cacheDir;
		}

		// Skip the Byte Order Mark if it is present since UTF-8 does not care about it
		inline static int SkipBOM(std::istream& inputStream)
		{
			char bom[4] = { 0 };
			inputStream.seekg(0, std::ios::beg);
			inputStream.read(bom, 3);
			if (strcmp(bom, "\xEF\xBB\xBF") == 0)
			{
				inputStream.seekg(3, std::ios::beg);
				return 3;
			}

			inputStream.seekg(0, std::ios::beg);
			return 0;
		}

		static std::string ReadTextFileWithoutBOM(const std::filesystem::path& filePath)
		{
			std::string result;

			std::ifstream f(filePath, std::ios::in | std::ios::binary);
			if (f)
			{
				f.seekg(0, std::ios::end);
				std::size_t fileSize = f.tellg();
				const int skippedChars = SkipBOM(f);

				fileSize -= skippedChars - 1;
				result.resize(fileSize);
				f.read(result.data() + 1, fileSize);

				// Add a dummy tab to the beginning of the file
				result[0] = '\t';
			}

			f.close();
			return result;
		}
	}

	// set -> binding point -> buffer
	static std::unordered_map<uint32_t, std::unordered_map<uint32_t, ShaderResources::UniformBuffer>> s_UniformBuffers;

	ShaderCompiler::ShaderCompiler(const std::filesystem::path& filePath, bool disableOptimization)
		: m_FilePath(filePath), m_DisableOptimizations(disableOptimization)
	{
	}

	Ref<ShaderCompiler> ShaderCompiler::Create(const std::filesystem::path& filePath, bool disableOptimization)
	{
		return CreateRef<ShaderCompiler>(filePath, disableOptimization);
	}

	bool ShaderCompiler::Reload(bool forceCompile)
	{
		m_ShaderSource.clear();
		m_StagesMetadata.clear();
		m_SPIRVData.clear();

		Utils::CreateCacheDirIfNeeded();
		const std::string source = Utils::ReadTextFileWithoutBOM(m_FilePath.string());
		PG_ASSERT(source.size(), "Failed to load shader source text file!");

		PG_CORE_DEBUG_TAG("ShaderCompiler", "Compiling shader: {}", m_FilePath.string());
		m_ShaderSource = PreProcess(source);
		const VkShaderStageFlagBits stagesChanged = ShaderRegistry::HasChanged(this);
		
		std::map<VkShaderStageFlagBits, std::vector<uint32_t>> spirvDebugData;
		bool compileSucceeded = CompileOrGetVulkanBinaries(spirvDebugData, m_SPIRVData, stagesChanged, forceCompile);
		if (!compileSucceeded)
		{
			PG_ASSERT(false, "");
			return false;
		}

		if (forceCompile || stagesChanged || !TryReadCachedReflectionData())
		{
			ReflectAllShaderStages(spirvDebugData);
			SerializeReflectinoData();
		}

		return true;
	}

	void ShaderCompiler::ClearUniformAndStorageBuffers()
	{
		s_UniformBuffers.clear();
	}

	Ref<Shader> ShaderCompiler::Compile(const std::filesystem::path& filePath, bool forceCompile, bool disableOptimizations)
	{
		// Set name for the shader
		std::string filePathString = filePath.string();
		std::size_t lastSlash = filePathString.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;

		std::size_t lastDot = filePathString.rfind(".");
		std::size_t filenameSize = lastDot == std::string::npos ? filePathString.size() - lastSlash : lastDot - lastSlash;
		std::string name = filePathString.substr(lastSlash, filenameSize);

		Ref<Shader> shader = Shader::Create();
		shader->m_FilePath = filePath;
		shader->m_Name = name;
		shader->m_DisableOptimizations = disableOptimizations;

		Ref<ShaderCompiler> compiler = ShaderCompiler::Create(filePath, disableOptimizations);
		compiler->Reload(forceCompile);

		shader->LoadAndCreateShaders(compiler->GetSPIRVData());
		shader->SetReflectionData(compiler->m_ReflectionData);
		shader->CreateDescriptors();

		Renderer::OnShaderReloaded(shader->GetHash());

		return shader;
	}

	bool ShaderCompiler::TryRecompile(Ref<Shader> shader)
	{
		Ref<ShaderCompiler> compiler = ShaderCompiler::Create(shader->m_FilePath, shader->m_DisableOptimizations);
		bool compileSucceeded = compiler->Reload(true); // force compile to reload
		if (!compileSucceeded)
			return false;

		shader->Release();

		shader->LoadAndCreateShaders(compiler->GetSPIRVData());
		shader->SetReflectionData(compiler->m_ReflectionData);
		shader->CreateDescriptors();

		Renderer::OnShaderReloaded(shader->GetHash());

		return true;
	}

	std::map<VkShaderStageFlagBits, std::string> ShaderCompiler::PreProcess(const std::string& source)
	{
		ShaderPreProcessor preprocessor;
		std::map<VkShaderStageFlagBits, std::string> shaderSources = preprocessor.PreprocessShader(source);

		static shaderc::Compiler compiler;
		for (auto& [stage, shaderSource] : shaderSources)
		{
			shaderc::CompileOptions options;
			options.AddMacroDefinition(ShaderUtils::VkShaderStageToMacroString(stage));

			const shaderc::PreprocessedSourceCompilationResult res = compiler.PreprocessGlsl(shaderSource, ShaderUtils::VkShaderStageToShaderc(stage), m_FilePath.string().c_str(), options);
			if (res.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				PG_CORE_ERROR_TAG("ShaderCompiler", "Failed to preprocess `{}`s {} shader.\nError: {}", m_FilePath.string(), ShaderUtils::VkShaderStageToString(stage), res.GetErrorMessage());
			}

			m_StagesMetadata[stage].Hash = Hash::GenerateFNVHash(shaderSource);

			shaderSource = std::string(res.begin(), res.end());
		}

		return shaderSources;
	}

	std::string ShaderCompiler::Compile(std::vector<uint32_t>& outputBin, VkShaderStageFlagBits stage, CompilationOptions options) const
	{
		const std::string& source = m_ShaderSource.at(stage);

		static shaderc::Compiler compiler;
		shaderc::CompileOptions shadercOptions;
		shadercOptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
		shadercOptions.SetWarningsAsErrors();
		if (options.GenerateDebugInfo)
			shadercOptions.SetGenerateDebugInfo();

		if (options.Optimize)
			shadercOptions.SetOptimizationLevel(shaderc_optimization_level_performance);

		const shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, ShaderUtils::VkShaderStageToShaderc(stage), m_FilePath.string().c_str(), shadercOptions);
		if (module.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			return fmt::format("Could not compile shader file: {}, at stage: {}.\nError: {}", m_FilePath.string(), ShaderUtils::VkShaderStageToString(stage), module.GetErrorMessage());
		}

		outputBin = std::vector<uint32_t>(module.begin(), module.end());
		return {};
	}

	bool ShaderCompiler::CompileOrGetVulkanBinaries(std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& outputDebugBin, std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& outputBin, VkShaderStageFlagBits stagesChanged, bool forceCompile)
	{
		for (auto& [stage, source] : m_ShaderSource)
		{
			// Debug
			if (!CompileOrGetVulkanBinary(stage, outputDebugBin[stage], true, stagesChanged, forceCompile))
				return false;
			// Release
			if (!CompileOrGetVulkanBinary(stage, outputBin[stage], false, stagesChanged, forceCompile))
				return false;
		}

		return true;
	}

	bool ShaderCompiler::CompileOrGetVulkanBinary(VkShaderStageFlagBits stage, std::vector<uint32_t>& outputBin, bool debug, VkShaderStageFlagBits stagesChanged, bool forceCompile)
	{
		std::filesystem::path cacheDirectory = Utils::CreateCacheDirIfNeeded();

		// Check if we have a cached version if we dont need to force compile
		const char* extension = ShaderUtils::ShaderStageCachedFileExtension(stage, debug);
		if (!forceCompile && stage & ~stagesChanged) // Per-stage cache is found and not changed
		{
			TryGetVulkanCachedBinary(cacheDirectory, extension, outputBin);
		}

		// Couldnt get it from cache now we compile
		if (outputBin.empty())
		{
			// PG_CORE_DEBUG_TAG("ShaderCompiler", "Shader cache not found! Compiling: {}, stage {}", m_FilePath.string(), ShaderUtils::VkShaderStageToString(stage));
			CompilationOptions options;
			if (debug)
			{
				options.GenerateDebugInfo = true;
				options.Optimize = false;
			}
			else
			{
				options.GenerateDebugInfo = false;
				// NOTE: There may be an internal error with shaderc optimizing compute shaders...
				options.Optimize = true;
			}

			std::string error = Compile(outputBin, stage, options);
			if (error.size())
			{
				PG_CORE_ERROR_TAG("ShaderCompiler", "Error compiling shader: {} stage: {}.\nError: {}", m_FilePath.string(), ShaderUtils::VkShaderStageToString(stage), error);
				// Fallback to still run on the old shader
				TryGetVulkanCachedBinary(cacheDirectory, extension, outputBin);
				if (outputBin.empty())
				{
					PG_CORE_CRITICAL_TAG("ShaderCompiler", "Couldnt compile shader and did not find a cached version!");
				}
				else
				{
					PG_CORE_CRITICAL_TAG("ShaderCompiler", "Failed to compile shader: {}, stage: {} so a cached version was loaded instead!", m_FilePath.string(), ShaderUtils::VkShaderStageToString(stage));
				}

				return false;
			}
			else
			{
				const std::filesystem::path path = cacheDirectory / (m_FilePath.filename().string() + extension);
				const std::string cachedFilePath = path.string();

				FILE* f = fopen(cachedFilePath.c_str(), "wb");
				if (f)
				{
					fwrite(outputBin.data(), sizeof(uint32_t), outputBin.size(), f);
					fclose(f);
				}
				else
				{
					PG_CORE_ERROR_TAG("ShaderCompiler", "Failed to cache shader: {}, stage: {} binary", m_FilePath.string(), ShaderUtils::VkShaderStageToString(stage));
					return false;
				}
			}
		}

		return true;
	}

	void ShaderCompiler::TryGetVulkanCachedBinary(const std::filesystem::path& cacheDirectory, const std::string& extension, std::vector<uint32_t>& outputBinary) const
	{
		const std::filesystem::path path = cacheDirectory / (m_FilePath.filename().string() + extension);
		const std::string cachedFilePath = path.string();

		FILE* f = fopen(cachedFilePath.c_str(), "rb");
		if (!f)
			return;

		fseek(f, 0, SEEK_END);
		uint64_t size = ftell(f);
		fseek(f, 0, SEEK_SET);
		outputBinary = std::vector<uint32_t>(size / sizeof(uint32_t));
		fread(outputBinary.data(), sizeof(uint32_t), outputBinary.size(), f);
		fclose(f);
	}

	void ShaderCompiler::ClearReflectionData()
	{
		m_ReflectionData.ShaderDescriptorSets.clear();
		// TODO: Add the other stuff..
	}

	bool ShaderCompiler::TryReadCachedReflectionData()
	{
		char header[6] = { 'V', 'K', 'P', 'G', 'S', 'R' };

		std::filesystem::path cacheDir = Utils::CreateCacheDirIfNeeded();
		const auto reflectedPath = cacheDir / (m_FilePath.filename().string() + ".cachedVulkan.refl");

		FileStreamReader reader(reflectedPath);
		if (!reader)
			return false;

		reader.ReadRaw(header);

		bool validHeader = memcmp(header, "VKPGSR", 6) == 0;
		PG_ASSERT(validHeader, "Header is not valid!");
		if (!validHeader)
			return false;

		ClearReflectionData();

		uint32_t shaderDescriptorSetCount;
		reader.ReadRaw<uint32_t>(shaderDescriptorSetCount);

		for (uint32_t i = 0; i < shaderDescriptorSetCount; i++)
		{
			ShaderResources::ShaderDescriptorSet& descriptorSet = m_ReflectionData.ShaderDescriptorSets.emplace_back();
			reader.ReadMap(descriptorSet.UniformBuffers);
		}

		return true;
	}

	void ShaderCompiler::SerializeReflectinoData()
	{
		char header[6] = { 'V', 'K', 'P', 'G', 'S', 'R' };

		std::filesystem::path cacheDir = Utils::CreateCacheDirIfNeeded();
		const auto reflectedPath = cacheDir / (m_FilePath.filename().string() + ".cachedVulkan.refl");

		FileStreamWriter serializer(reflectedPath);
		if (!serializer)
			return;

		serializer.WriteRaw(header);

		serializer.WriteRaw<uint32_t>((uint32_t)m_ReflectionData.ShaderDescriptorSets.size());
		for (const auto& descriptorSet : m_ReflectionData.ShaderDescriptorSets)
		{
			serializer.WriteMap(descriptorSet.UniformBuffers);
		}
	}

	void ShaderCompiler::ReflectAllShaderStages(const std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData)
	{
		ClearReflectionData();

		for (auto& [stage, data] : shaderData)
		{
			Reflect(stage, data);
		}
	}

	void ShaderCompiler::Reflect(VkShaderStageFlagBits stage, const std::vector<uint32_t>& shaderData)
	{
		PG_CORE_WARN_TAG("ShaderCompiler", "============================");
		PG_CORE_WARN_TAG("ShaderCompiler", "  Vulkan Shader Reflection  ");
		PG_CORE_WARN_TAG("ShaderCompiler", "============================");

		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();

		spirv_cross::Compiler compiler(shaderData);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		// General Info
		PG_CORE_TRACE_TAG("ShaderCompiler", "{0} - {1}", m_FilePath.string(), ShaderUtils::VkShaderStageToString(stage));
		PG_CORE_TRACE_TAG("ShaderCompiler", "\t{0} Uniform Buffers", resources.uniform_buffers.size());
		// TODO: For now all we care about is the uniform buffers
		// PG_CORE_TRACE_TAG("Renderer", "\t{0} Push Constant Buffers", resources.push_constant_buffers.size());
		// PG_CORE_TRACE_TAG("Renderer", "\t{0} Storage Buffers", resources.storage_buffers.size());
		// PG_CORE_TRACE_TAG("Renderer", "\t{0} Sampled Images", resources.sampled_images.size());
		// PG_CORE_TRACE_TAG("Renderer", "\t{0} Storage Images", resources.storage_images.size());

		PG_CORE_TRACE_TAG("ShaderCompiler", "============================");
		PG_CORE_WARN_TAG("ShaderCompiler", "Uniform Buffers:");
		for (const spirv_cross::Resource& res : resources.uniform_buffers)
		{
			spirv_cross::SmallVector activeBuffer = compiler.get_active_buffer_ranges(res.id);

			if (activeBuffer.size())
			{
				const std::string& name = res.name;
				const spirv_cross::SPIRType& bufferType = compiler.get_type(res.base_type_id);
				uint32_t memberCount = (uint32_t)bufferType.member_types.size();

				uint32_t descriptorSet = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
				uint32_t binding = compiler.get_decoration(res.id, spv::DecorationBinding);
				uint32_t bufferSize = (uint32_t)compiler.get_declared_struct_size(bufferType);

				// Create/cache the resource in case other shaders reference the same resource that way it is readily available
				if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
					m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

				ShaderResources::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
				if (s_UniformBuffers[descriptorSet].contains(binding))
				{
					ShaderResources::UniformBuffer& ubo = s_UniformBuffers.at(descriptorSet).at(binding);
					if (bufferSize > ubo.Size)
						ubo.Size = bufferSize;
				}
				else
				{
					ShaderResources::UniformBuffer& ubo = s_UniformBuffers.at(descriptorSet)[binding];
					ubo.Name = name;
					ubo.BindingPoint = binding;
					ubo.Size = bufferSize;
					ubo.ShaderStage = VK_SHADER_STAGE_ALL;
					// NOTE: Here we set `VK_SHADER_STAGE_ALL` since `contains` only tries to compare the keys of the map so if we set a specific
					// stage, say a uniform buffer was shared in 2 stages then for the fragment shader the uniformbuffer would be already cached
					// and the shader stage would be `VK_SHADER_STAGE_VERTEX` which is obviously wrong so we just set `VK_SHADER_STAGE_ALL`
				}

				shaderDescriptorSet.UniformBuffers[binding] = s_UniformBuffers.at(descriptorSet).at(binding);

				PG_CORE_TRACE_TAG("ShaderCompiler", " \tName: {0} (Set: {1}, Binding: {2})", name, descriptorSet, binding);
				PG_CORE_TRACE_TAG("ShaderCompiler", " \tMember Count: {0}", memberCount);
				PG_CORE_TRACE_TAG("ShaderCompiler", " \tSize: {0}", bufferSize);
				PG_CORE_TRACE_TAG("ShaderCompiler", "-------------------");
			}
		}
	}

}