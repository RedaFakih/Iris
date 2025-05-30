#include "IrisPCH.h"
#include "ShaderCompiler.h"

#include "Core/Hash.h"
#include "Renderer/Core/RendererContext.h"

#include "Renderer/Shaders/Shader.h"
#include "Renderer/Shaders/ShaderUtils.h"

#include "Renderer/Renderer.h"
#include "Renderer/Shaders/ShaderRegistry.h"

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_glsl.hpp>

namespace Iris {

	namespace Utils {

		inline static std::string CreateCacheDirIfNeeded()
		{
			std::string cacheDir = "Resources/Shaders/Cache";
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

		static ShaderUniformType SPIRTypeToShaderUniformType(spirv_cross::SPIRType type)
		{
			switch (type.basetype)
			{
				case spirv_cross::SPIRType::Boolean:  return ShaderUniformType::Bool;
				case spirv_cross::SPIRType::Int:
					if (type.vecsize == 1)            return ShaderUniformType::Int;
					if (type.vecsize == 2)            return ShaderUniformType::IVec2;
					if (type.vecsize == 3)            return ShaderUniformType::IVec3;
					if (type.vecsize == 4)            return ShaderUniformType::IVec4;

				case spirv_cross::SPIRType::UInt:     return ShaderUniformType::UInt;
				case spirv_cross::SPIRType::Float:
					if (type.columns == 3)            return ShaderUniformType::Mat3;
					if (type.columns == 4)            return ShaderUniformType::Mat4;

					if (type.vecsize == 1)            return ShaderUniformType::Float;
					if (type.vecsize == 2)            return ShaderUniformType::Vec2;
					if (type.vecsize == 3)            return ShaderUniformType::Vec3;
					if (type.vecsize == 4)            return ShaderUniformType::Vec4;
					break;
				}

			IR_ASSERT(false);
			return ShaderUniformType::None;
		}
	}

	// set -> binding point -> buffer
	static std::unordered_map<uint32_t, std::unordered_map<uint32_t, ShaderResources::UniformBuffer>> s_UniformBuffers;
	static std::unordered_map<uint32_t, std::unordered_map<uint32_t, ShaderResources::StorageBuffer>> s_StorageBuffers;

	ShaderCompiler::ShaderCompiler(const std::string& filePath, bool disableOptimization)
		: m_FilePath(filePath), m_DisableOptimizations(disableOptimization)
	{
	}

	Ref<ShaderCompiler> ShaderCompiler::Create(const std::string& filePath, bool disableOptimization)
	{
		return CreateRef<ShaderCompiler>(filePath, disableOptimization);
	}

	bool ShaderCompiler::Reload(bool forceCompile)
	{
		m_ShaderSource.clear();
		m_StagesMetadata.clear();
		m_SPIRVData.clear();

		Utils::CreateCacheDirIfNeeded();
		const std::string source = Utils::ReadTextFileWithoutBOM(m_FilePath);
		IR_ASSERT(source.size(), "Failed to load shader source text file!");

		IR_CORE_DEBUG_TAG("ShaderCompiler", "Compiling shader: {}", m_FilePath);
		m_ShaderSource = PreProcess(source);
		// Compare shadersource hashes to see if the shader has changed
		const VkShaderStageFlagBits stagesChanged = ShaderRegistry::HasChanged(this);
		
		std::map<VkShaderStageFlagBits, std::vector<uint32_t>> spirvDebugData;
		bool compileSucceeded = CompileOrGetVulkanBinaries(spirvDebugData, m_SPIRVData, stagesChanged, forceCompile);
		if (!compileSucceeded)
		{
			IR_ASSERT(false);
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
		s_StorageBuffers.clear();
	}

	Ref<Shader> ShaderCompiler::Compile(const std::string& filePath, bool forceCompile, bool disableOptimizations)
	{
		// Set name for the shader
		std::size_t lastSlash = filePath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;

		std::size_t lastDot = filePath.rfind(".");
		std::size_t filenameSize = lastDot == std::string::npos ? filePath.size() - lastSlash : lastDot - lastSlash;
		std::string name = filePath.substr(lastSlash, filenameSize);

		Ref<Shader> shader = Shader::Create();
		shader->m_FilePath = filePath;
		shader->m_Name = name;
		shader->m_DisableOptimizations = disableOptimizations;

		Ref<ShaderCompiler> compiler = ShaderCompiler::Create(filePath, disableOptimizations);
		shader->m_CompilationStatus = compiler->Reload(forceCompile);

		shader->LoadAndCreateShaders(compiler->GetSPIRVData());
		shader->SetReflectionData(compiler->m_ReflectionData);
		shader->BuildWriteDescriptors();

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
		shader->BuildWriteDescriptors();

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

			const shaderc::PreprocessedSourceCompilationResult res = compiler.PreprocessGlsl(shaderSource, ShaderUtils::VkShaderStageToShaderc(stage), m_FilePath.c_str(), options);
			if (res.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				IR_CORE_ERROR_TAG("ShaderCompiler", "Failed to preprocess `{}`s {} shader.\nError: {}", m_FilePath, ShaderUtils::VkShaderStageToString(stage), res.GetErrorMessage());
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

		const shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, ShaderUtils::VkShaderStageToShaderc(stage), m_FilePath.c_str(), shadercOptions);
		if (module.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			return fmt::format("Could not compile shader file: {}, at stage: {}.\nError: {}", m_FilePath, ShaderUtils::VkShaderStageToString(stage), module.GetErrorMessage());
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
		std::string cacheDirectory = Utils::CreateCacheDirIfNeeded();

		// Check if we have a cached version if we dont need to force compile
		const char* extension = ShaderUtils::ShaderStageCachedFileExtension(stage, debug);
		if (!forceCompile && stage & ~stagesChanged) // Per-stage cache is found and not changed
		{
			TryGetVulkanCachedBinary(cacheDirectory, extension, outputBin);
		}

		// Couldnt get it from cache now we compile
		if (outputBin.empty())
		{
			// IR_CORE_DEBUG_TAG("ShaderCompiler", "Shader cache not found! Compiling: {}, stage {}", m_FilePath.string(), ShaderUtils::VkShaderStageToString(stage));
			CompilationOptions options;
			if (debug)
			{
				options.GenerateDebugInfo = true;
				options.Optimize = false;
			}
			else
			{
				options.GenerateDebugInfo = false;
				// NOTE: Shaderc internal error with optimizing compute shaders...
				// options.Optimize = !m_DisableOptimizations && stage != VK_SHADER_STAGE_COMPUTE_BIT;
				options.Optimize = true;
			}

			std::string error = Compile(outputBin, stage, options);
			if (error.size())
			{
				IR_CORE_ERROR_TAG("ShaderCompiler", "Error compiling shader: {} stage: {}.\nError: {}", m_FilePath, ShaderUtils::VkShaderStageToString(stage), error);
				// Fallback to still run on the old shader
				TryGetVulkanCachedBinary(cacheDirectory, extension, outputBin);
				if (outputBin.empty())
				{
					IR_CORE_FATAL_TAG("ShaderCompiler", "Couldnt compile shader and did not find a cached version!");
				}
				else
				{
					IR_CORE_FATAL_TAG("ShaderCompiler", "Failed to compile shader: {}, stage: {} so a cached version was loaded instead!", m_FilePath, ShaderUtils::VkShaderStageToString(stage));
				}

				return false;
			}
			else
			{
				const std::filesystem::path filePathPath = m_FilePath;
				const std::filesystem::path path = std::filesystem::path(cacheDirectory) / (filePathPath.filename().string() + extension);
				const std::string cachedFilePath = path.string();

				FILE* f = fopen(cachedFilePath.c_str(), "wb");
				if (f)
				{
					fwrite(outputBin.data(), sizeof(uint32_t), outputBin.size(), f);
					fclose(f);
				}
				else
				{
					IR_CORE_ERROR_TAG("ShaderCompiler", "Failed to cache shader: {}, stage: {} binary", m_FilePath, ShaderUtils::VkShaderStageToString(stage));
					return false;
				}
			}
		}

		return true;
	}

	void ShaderCompiler::TryGetVulkanCachedBinary(const std::string& cacheDirectory, const std::string& extension, std::vector<uint32_t>& outputBinary) const
	{
		const std::filesystem::path filePathPath = m_FilePath;
		const std::filesystem::path path = std::filesystem::path(cacheDirectory) / (filePathPath.filename().string() + extension);
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
		m_ReflectionData.ConstantBuffers.clear();
		m_ReflectionData.PushConstantRanges.clear();
	}

	bool ShaderCompiler::TryReadCachedReflectionData()
	{
		char header[4] = { 'I', 'R', 'S', 'R' };

		std::filesystem::path cacheDir = Utils::CreateCacheDirIfNeeded();
		const std::filesystem::path filePathPath = m_FilePath;
		const auto reflectedPath = cacheDir / (filePathPath.filename().string() + ".cachedVulkan.refl");

		FileStreamReader reader(reflectedPath);
		if (!reader)
			return false;

		reader.ReadRaw(header);

		bool validHeader = memcmp(header, "IRSR", 4) == 0;
		IR_VERIFY(validHeader, "Header is not valid!");
		if (!validHeader)
			return false;

		ClearReflectionData();

		uint32_t shaderDescriptorSetCount;
		reader.ReadRaw<uint32_t>(shaderDescriptorSetCount);

		for (uint32_t i = 0; i < shaderDescriptorSetCount; i++)
		{
			ShaderResources::ShaderDescriptorSet& descriptorSet = m_ReflectionData.ShaderDescriptorSets.emplace_back();
			reader.ReadMap(descriptorSet.UniformBuffers);
			reader.ReadMap(descriptorSet.StorageBuffers);
			reader.ReadMap(descriptorSet.ImageSamplers);
			reader.ReadMap(descriptorSet.StorageImages);
			reader.ReadMap(descriptorSet.WriteDescriptorSets);
		}

		reader.ReadMap(m_ReflectionData.ConstantBuffers);
		reader.ReadArray(m_ReflectionData.PushConstantRanges);

		return true;
	}

	void ShaderCompiler::SerializeReflectinoData()
	{
		char header[4] = { 'I', 'R', 'S', 'R' };

		std::filesystem::path cacheDir = Utils::CreateCacheDirIfNeeded();
		const std::filesystem::path filePathPath = m_FilePath;
		const auto reflectedPath = cacheDir / (filePathPath.filename().string() + ".cachedVulkan.refl");

		FileStreamWriter serializer(reflectedPath);
		if (!serializer)
			return;

		serializer.WriteRaw(header);

		serializer.WriteRaw<uint32_t>((uint32_t)m_ReflectionData.ShaderDescriptorSets.size());
		for (const auto& descriptorSet : m_ReflectionData.ShaderDescriptorSets)
		{
			serializer.WriteMap(descriptorSet.UniformBuffers);
			serializer.WriteMap(descriptorSet.StorageBuffers);
			serializer.WriteMap(descriptorSet.ImageSamplers);
			serializer.WriteMap(descriptorSet.StorageImages);
			serializer.WriteMap(descriptorSet.WriteDescriptorSets);
		}

		serializer.WriteMap(m_ReflectionData.ConstantBuffers);
		serializer.WriteArray(m_ReflectionData.PushConstantRanges);
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
		IR_CORE_WARN_TAG("ShaderCompiler", "============================");
		IR_CORE_WARN_TAG("ShaderCompiler", "  Vulkan Shader Reflection  ");
		IR_CORE_WARN_TAG("ShaderCompiler", "============================");

		spirv_cross::Compiler compiler(shaderData);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		// General Info
		IR_CORE_TRACE_TAG("ShaderCompiler", "{0} - {1}", m_FilePath, ShaderUtils::VkShaderStageToString(stage));
		IR_CORE_TRACE_TAG("ShaderCompiler", "\t{0} Uniform Buffers", resources.uniform_buffers.size());
		IR_CORE_TRACE_TAG("ShaderCompiler", "\t{0} Push Constant Buffers", resources.push_constant_buffers.size());
		IR_CORE_TRACE_TAG("ShaderCompiler", "\t{0} Storage Buffers", resources.storage_buffers.size());
		IR_CORE_TRACE_TAG("ShaderCompiler", "\t{0} Sampled Images", resources.sampled_images.size());
		IR_CORE_TRACE_TAG("ShaderCompiler", "\t{0} Storage Images", resources.storage_images.size());

		IR_CORE_TRACE_TAG("ShaderCompiler", "============================");
		IR_CORE_WARN_TAG("ShaderCompiler", "Uniform Buffers:");
		for (const spirv_cross::Resource& res : resources.uniform_buffers)
		{
			spirv_cross::SmallVector activeBuffer = compiler.get_active_buffer_ranges(res.id);

			if (activeBuffer.size())
			{
				const std::string& name = res.name;
				const spirv_cross::SPIRType& bufferType = compiler.get_type(res.base_type_id);
				uint32_t memberCount = static_cast<uint32_t>(bufferType.member_types.size());

				uint32_t descriptorSet = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
				uint32_t binding = compiler.get_decoration(res.id, spv::DecorationBinding);
				uint32_t bufferSize = static_cast<uint32_t>(compiler.get_declared_struct_size(bufferType));

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

				IR_CORE_TRACE_TAG("ShaderCompiler", " \tName: {0} (Set: {1}, Binding: {2})", name, descriptorSet, binding);
				IR_CORE_TRACE_TAG("ShaderCompiler", " \tMember Count: {0}", memberCount);
				IR_CORE_TRACE_TAG("ShaderCompiler", " \tSize: {0}", bufferSize);
				IR_CORE_TRACE_TAG("ShaderCompiler", "-------------------");
			}
		}

		IR_CORE_TRACE_TAG("ShaderCompiler", "============================");
		IR_CORE_WARN_TAG("ShaderCompiler", "Storage Buffers:");
		for (const spirv_cross::Resource& res : resources.storage_buffers)
		{
			spirv_cross::SmallVector activeBuffer = compiler.get_active_buffer_ranges(res.id);

			if (activeBuffer.size())
			{
				const std::string& name = res.name;
				const spirv_cross::SPIRType& bufferType = compiler.get_type(res.base_type_id);
				uint32_t memberCount = static_cast<uint32_t>(bufferType.member_types.size());

				uint32_t descriptorSet = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
				uint32_t binding = compiler.get_decoration(res.id, spv::DecorationBinding);
				uint32_t bufferSize = static_cast<uint32_t>(compiler.get_declared_struct_size(bufferType));

				if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
					m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

				ShaderResources::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
				if (s_StorageBuffers[descriptorSet].contains(shaderDescriptorSet))
				{
					ShaderResources::StorageBuffer& sbo = s_StorageBuffers.at(descriptorSet).at(binding);
					if (bufferSize > sbo.Size)
						sbo.Size = bufferSize;
				}
				else
				{
					ShaderResources::StorageBuffer& sbo = s_StorageBuffers.at(descriptorSet)[binding];
					sbo.Name = name;
					sbo.BindingPoint = binding;
					sbo.Size = bufferSize;
					sbo.ShaderStage = VK_SHADER_STAGE_ALL;
					// NOTE: Here we set `VK_SHADER_STAGE_ALL` since `contains` only tries to compare the keys of the map so if we set a specific
					// stage, say a uniform buffer was shared in 2 stages then for the fragment shader the uniformbuffer would be already cached
					// and the shader stage would be `VK_SHADER_STAGE_VERTEX` which is obviously wrong so we just set `VK_SHADER_STAGE_ALL`
				}

				shaderDescriptorSet.StorageBuffers[binding] = s_StorageBuffers.at(descriptorSet).at(binding);

				IR_CORE_TRACE_TAG("ShaderCompiler", " \tName: {0} (Set: {1}, Binding: {2})", name, descriptorSet, binding);
				IR_CORE_TRACE_TAG("ShaderCompiler", " \tMember Count: {0}", memberCount);
				IR_CORE_TRACE_TAG("ShaderCompiler", " \tSize: {0}", bufferSize);
				IR_CORE_TRACE_TAG("ShaderCompiler", "-------------------");
			}
		}

		IR_CORE_TRACE_TAG("ShaderCompiler", "============================");
		IR_CORE_WARN_TAG("ShaderCompiler", "Push Constant Buffers:");
		for (const spirv_cross::Resource& res : resources.push_constant_buffers)
		{
			const std::string& bufferName = res.name;
			const spirv_cross::SPIRType& bufferType = compiler.get_type(res.base_type_id);
			std::size_t bufferSize = compiler.get_declared_struct_size(bufferType);
			uint32_t memberCount = static_cast<uint32_t>(bufferType.member_types.size());
			uint32_t bufferOffset = 0;

			if (m_ReflectionData.PushConstantRanges.size())
				bufferOffset = m_ReflectionData.PushConstantRanges.back().Offset + m_ReflectionData.PushConstantRanges.back().Size;

			ShaderResources::PushConstantRange& pushConstantRange = m_ReflectionData.PushConstantRanges.emplace_back();
			pushConstantRange.ShaderStage = stage;
			pushConstantRange.Size = static_cast<uint32_t>(bufferSize);
			pushConstantRange.Offset = bufferOffset;

			// Skip empty push constant buffers
			if (bufferName.empty() || bufferName == "u_Renderer")
				continue;

			ShaderBuffer& shaderBuffer = m_ReflectionData.ConstantBuffers[bufferName];
			shaderBuffer.Name = bufferName;
			shaderBuffer.Size = static_cast<uint32_t>(bufferSize - bufferOffset);

			IR_CORE_TRACE_TAG("ShaderCompiler", " \tName: {0}", bufferName);
			IR_CORE_TRACE_TAG("ShaderCompiler", " \tMember Count: {0}", memberCount);
			IR_CORE_TRACE_TAG("ShaderCompiler", " \tSize: {0}", bufferSize);
			IR_CORE_TRACE_TAG("ShaderCompiler", "-------------------");

			for (uint32_t i = 0; i < memberCount; i++)
			{
				const spirv_cross::SPIRType& type = compiler.get_type(bufferType.member_types[i]);
				const std::string& name = compiler.get_member_name(bufferType.self, i);
				uint32_t size = static_cast<uint32_t>(compiler.get_declared_struct_member_size(bufferType, i));
				uint32_t offset = compiler.type_struct_member_offset(bufferType, i) - bufferOffset;

				std::string uniformName = fmt::format("{}.{}", bufferName, name);
				shaderBuffer.Uniforms[uniformName] = ShaderUniform(uniformName, Utils::SPIRTypeToShaderUniformType(type), size, offset);
			}
		}

		IR_CORE_TRACE_TAG("ShaderCompiler", "============================");
		IR_CORE_WARN_TAG("ShaderCompiler", "Sampled Images:");
		for (const spirv_cross::Resource& res : resources.sampled_images)
		{
			const std::string& name = res.name;
			const spirv_cross::SPIRType& baseType = compiler.get_type(res.base_type_id);
			const spirv_cross::SPIRType& type = compiler.get_type(res.type_id);

			uint32_t binding = compiler.get_decoration(res.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
			spv::Dim dimension = baseType.image.dim;
			uint32_t arraySize = 0;
			if (type.array.size())
				arraySize = type.array[0]; // NOTE: We only consider the first elements since we do not have arrays or arrays of textures...

			// Array size in case we have an array of samplers (2D batch rendering texture array) (sampler2D arr[32], NOT A sampler2DArray arr)
			// in case the array size was 0 meaning there is no array we set it to 1 since there is 1 single image attachment instead of an array
			if (arraySize == 0)
				arraySize = 1;
			if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
				m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

			ShaderResources::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
			shaderDescriptorSet.ImageSamplers[binding] = ShaderResources::ImageSampler{
				.Name = name,
				.DescriptorSet = descriptorSet,
				.BindingPoint = binding,
				.Dimension = dimension,
				.ArraySize = arraySize, 
				.ShaderStage = stage
			};

			IR_CORE_TRACE_TAG("ShaderCompiler", " \tName: {0} (Set: {1}, Binding: {2})", name, descriptorSet, binding);
		}

		IR_CORE_TRACE_TAG("ShaderCompiler", "============================");
		IR_CORE_WARN_TAG("ShaderCompiler", "Storages Images:");
		for (const spirv_cross::Resource& res : resources.storage_images)
		{
			const std::string& name = res.name;
			const spirv_cross::SPIRType& baseType = compiler.get_type(res.base_type_id);
			const spirv_cross::SPIRType& type = compiler.get_type(res.type_id);

			uint32_t binding = compiler.get_decoration(res.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
			spv::Dim dimension = baseType.image.dim;

			// Array size in case we have an array of samplers (2D batch rendering texture array) (sampler2D arr[32], NOT A sampler2DArray arr)
			// in case the array size was 0 meaning there is no array we set it to 1 since there is 1 single image attachment instead of an array
			uint32_t arraySize = 1;
			if (type.array.size())
				arraySize = type.array[0]; // NOTE: We only consider the first elements since we do not have arrays or arrays of textures...

			if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
				m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

			ShaderResources::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
			shaderDescriptorSet.StorageImages[binding] = ShaderResources::ImageSampler{
				.Name = name,
				.DescriptorSet = descriptorSet,
				.BindingPoint = binding,
				.Dimension = dimension,
				.ArraySize = arraySize,
				.ShaderStage = stage
			};

			IR_CORE_TRACE_TAG("ShaderCompiler", " \tName: {0} (Set: {1}, Binding: {2})", name, descriptorSet, binding);
		}
	}

}