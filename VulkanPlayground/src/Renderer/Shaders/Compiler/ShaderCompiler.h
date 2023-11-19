#pragma once

#include "Core/Base.h"
#include "Renderer/Shaders/Shader.h"
#include "ShaderPreprocessor.h"

#include <vulkan/vulkan.h>

#include <filesystem>
#include <map>

namespace vkPlayground {

	struct StageData
	{
		uint32_t Hash = 0;
		bool operator!=(const StageData& other) const noexcept { return Hash != other.Hash; }
	};

	class ShaderCompiler : public RefCountedObject
	{
	public:
		ShaderCompiler(const std::filesystem::path& filePath, bool disableOptimization = false);

		[[nodiscard]] static Ref<ShaderCompiler> Create(const std::filesystem::path& filePath, bool disableOptimization = false);

		bool Reload(bool forceCompile = false);

		const std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& GetSPIRVData() const { return m_SPIRVData; }

		// If multiple shaders reference the same buffers that we dont have define on every shader compilation rather they are cached
		static void ClearUniformAndStorageBuffers();

		static Ref<Shader> Compile(const std::filesystem::path& filepath, bool forceCompile = false, bool disableOptimizations = false);
		static bool TryRecompile(Ref<Shader> shader);

	private:
		struct CompilationOptions
		{
			bool GenerateDebugInfo = false;
			bool Optimize = true;
		};

		std::map<VkShaderStageFlagBits, std::string> PreProcess(const std::string& source);

		std::string Compile(std::vector<uint32_t>& outputBin, VkShaderStageFlagBits stage, CompilationOptions options) const;
		bool CompileOrGetVulkanBinaries(std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& outputDebugBin, std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& outputBin, VkShaderStageFlagBits stagesChanged, bool forceCompile);
		bool CompileOrGetVulkanBinary(VkShaderStageFlagBits stage, std::vector<uint32_t>& outputBin, bool debug, VkShaderStageFlagBits stagesChanged, bool forceCompile);
		void TryGetVulkanCachedBinary(const std::filesystem::path& cacheDirectory, const std::string& extension, std::vector<uint32_t>& outputBinary) const;

		void ClearReflectionData();

		bool TryReadCachedReflectionData();
		void SerializeReflectinoData();

		void ReflectAllShaderStages(const std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData);
		void Reflect(VkShaderStageFlagBits stage, const std::vector<uint32_t>& shaderData);

	private:
		std::filesystem::path m_FilePath;
		bool m_DisableOptimizations = false;

		std::map<VkShaderStageFlagBits, std::string> m_ShaderSource;
		std::map<VkShaderStageFlagBits, std::vector<uint32_t>> m_SPIRVData;

		Shader::ReflectionData m_ReflectionData;

		std::map<VkShaderStageFlagBits, StageData> m_StagesMetadata;

		friend class ShaderRegistry;
	};

}