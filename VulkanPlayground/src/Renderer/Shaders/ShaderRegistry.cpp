#include "ShaderRegistry.h"

#include "ShaderUtils.h"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace vkPlayground {

	constexpr static const char* s_ShaderRegistryPath = "Shaders/ShaderRegistry.cache";

	VkShaderStageFlagBits ShaderRegistry::HasChanged(Ref<ShaderCompiler> shaderCompiler)
	{
		std::map<std::string, std::map<VkShaderStageFlagBits, StageData>> shaderCache;

		Deserialize(shaderCache);

		VkShaderStageFlagBits changedState = {};
		const bool shaderNotCached = !shaderCache.contains(shaderCompiler->m_FilePath.string());

		for (const auto& [stage, source] : shaderCompiler->m_ShaderSource)
		{
			// We use the std::map's `[]` operator so it inserts if it is not there...
			if (shaderNotCached || shaderCompiler->m_StagesMetadata.at(stage) != shaderCache[shaderCompiler->m_FilePath.string()][stage])
			{
				shaderCache[shaderCompiler->m_FilePath.string()][stage] = shaderCompiler->m_StagesMetadata.at(stage);
				*(int*)&changedState |= stage;
			}
		}

		// Update cache in case we added a stage but didn't remove the deleted (in file) stages
		shaderCache.at(shaderCompiler->m_FilePath.string()) = shaderCompiler->m_StagesMetadata;

		if (changedState)
		{
			Serialize(shaderCache);
		}

		return changedState;
	}

	void ShaderRegistry::Serialize(const std::map<std::string, std::map<VkShaderStageFlagBits, StageData>>& shaderCache)
	{
		YAML::Emitter out;

		out << YAML::BeginMap;

		out << YAML::Key << "ShaderRegistry" << YAML::BeginSeq;
		for (const auto&[filePath, stages] : shaderCache)
		{
			out << YAML::BeginMap;

			out << YAML::Key << "Filepath" << YAML::Value << filePath;

			out << YAML::Key << "Stages" << YAML::BeginSeq;
			for (const auto&[stage, data] : stages)
			{
				out << YAML::BeginMap;

				out << YAML::Key << "Stage" << YAML::Value << ShaderUtils::VkShaderStageToString(stage);
				out << YAML::Key << "Hash" << YAML::Value << data.Hash;

				out << YAML::EndMap;
			}
			out << YAML::EndSeq;

			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		out << YAML::EndMap;

		std::ofstream fout(s_ShaderRegistryPath);
		fout << out.c_str();
	}

	void ShaderRegistry::Deserialize(std::map<std::string, std::map<VkShaderStageFlagBits, StageData>>& shaderCache)
	{
		std::ifstream fin(s_ShaderRegistryPath);
		if (!fin.good())
			return;

		std::stringstream sstr;
		sstr << fin.rdbuf();

		YAML::Node data = YAML::Load(sstr.str());
		YAML::Node handles = data["ShaderRegistry"];
		if (handles.IsNull())
		{
			PG_CORE_ERROR_TAG("ShaderCompiler", "ShaderRegistry is invalid!");
			return;
		}

		for (auto shader : handles)
		{
			std::string path = shader["Filepath"].as<std::string>("");

			for (auto stage : shader["Stages"])
			{
				std::string stageType = stage["Stage"].as<std::string>();
				uint32_t stageHash = stage["Hash"].as<uint32_t>(0);

				StageData& stageCache = shaderCache[path][ShaderUtils::VkShaderStageFromString(stageType)];
				stageCache.Hash = stageHash;
			}
		}
	}

}