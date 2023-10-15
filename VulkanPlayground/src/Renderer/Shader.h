#pragma once

#include "Core/Base.h"

#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan.h>

#include <string>
#include <string_view>
#include <filesystem>
#include <unordered_map>

namespace vkPlayground {

	class Shader  // : public RefCountedObject
	{
	public:
		Shader(std::string_view path);
		~Shader();

		[[nodiscard]] static Ref<Shader> Create(std::string_view path);

		// Release the shader module after the pipelines have been created
		void Release();

		uint32_t GetLastTimeModified() const;

		const std::string_view GetName() const { return m_Name; }
		const std::filesystem::path& GetFilePath() const { return m_FilePath; }

		// Vulkan-specific: To be used in the pipeline creation
		const std::vector<VkPipelineShaderStageCreateInfo>& GetPipelineShaderStageCreateInfos() const { return m_PipelineShaderStageCreateInfos; }

	private:
		void LoadTwoStageShader(const std::string& source);
		std::unordered_map<shaderc_shader_kind, std::string> PreProcess(const std::string& source);
		bool CompileVulkanBinaries(const std::unordered_map<shaderc_shader_kind, std::string>& source, bool forceCompile);
		void CreateProgram();

	private:
		uint32_t m_ShaderID = 0;

		std::string m_Name;
		std::filesystem::path m_FilePath;

		std::unordered_map<shaderc_shader_kind, std::vector<uint32_t>> m_VulkanSPIRV;
		std::vector<VkPipelineShaderStageCreateInfo> m_PipelineShaderStageCreateInfos;

		// Minutes
		inline static uint8_t s_CompileTimeThreshold = 5;

	};

}