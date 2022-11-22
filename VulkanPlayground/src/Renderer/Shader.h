#pragma once

#include "Core/Base.h"

#include <shaderc/shaderc.hpp>

#include <string>
#include <filesystem>
#include <unordered_map>

namespace vkPlayground::Renderer {

	class Shader
	{
	public:
		Shader(const std::string& path);
		~Shader();

		static Ref<Shader> Create(const std::string& path);

		uint32_t GetLastTimeModified() const;

		const std::string& GetName() const { return m_Name; }
		const std::filesystem::path& GetFilePath() const { return m_FilePath; }

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

		inline static uint16_t s_CompileTimeThreshold = 5;

	};

}