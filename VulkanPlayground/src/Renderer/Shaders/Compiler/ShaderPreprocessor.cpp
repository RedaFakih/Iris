#include "vkPch.h"
#include "ShaderPreprocessor.h"

#include "Core/Base.h"
#include "Renderer/Shaders/ShaderUtils.h"

namespace vkPlayground {

	std::map<VkShaderStageFlagBits, std::string> ShaderPreProcessor::PreprocessShader(const std::string& source) const
	{
		std::string shaderSource;
		{
			std::stringstream ss;
			CopyWithoutComments(source.begin(), source.end(), std::ostream_iterator<char>(ss));
			shaderSource = ss.str();
		}
		VKPG_VERIFY(shaderSource.size(), "Shader has no code...");

		std::map<VkShaderStageFlagBits, std::string> shaderSources;
		std::vector<std::pair<VkShaderStageFlagBits, std::size_t>> stagePositions;

		std::size_t startOfStage = 0;
		std::size_t pos = shaderSource.find('#');

		// Check the version statement
		std::size_t eol = shaderSource.find_first_of("\r\n", pos) + 1; // Add 1 since we would want to keep the new lines in the preprocessed string
		std::vector<std::string> tokens = TokenizeString(shaderSource.substr(pos, eol - pos));
		VKPG_VERIFY((tokens.size() >= 3) && (tokens[1] == "version"), "Invalid `#version` expression or not found!");
		pos = shaderSource.find('#', pos + 1);

		while (pos != std::string::npos)
		{
			eol = shaderSource.find_first_of("\r\n", pos) + 1; // Add 1 since we would want to keep the new lines in the preprocessed string
			tokens = TokenizeString(shaderSource.substr(pos, eol - pos));

			std::size_t index = 1; // Skip '#'

			// Parse the stage. Example: #pragma vertex
			if (tokens[index] == "stage")
			{
				++index;
				const std::string_view stage = tokens[index];
				VKPG_VERIFY(stage == "vertex" || stage == "fragment" || stage == "compute", "Invalid shader type/stage specified!");
				VkShaderStageFlagBits vkStage = ShaderUtils::VkShaderStageFromString(stage);

				stagePositions.emplace_back(std::pair{ vkStage, startOfStage });
				shaderSource.erase(pos, eol - pos);
			}

			// NOTE: If we want to add more stuff to handle such as #if defined() we can do that here:
			// else if (tokens[index] == "if")
			// {
			// 	++index;
			// 	if (tokens[index] == "defined")
			// 	{
			// 		++index;
			// 		VKPG_VERIFY(tokens[index] == "(", "whatever");
			// 		++index;
			// 		// do stuff with your expression while increasing the index after each token
			// 		++index;
			// 		VKPG_VERIFY(tokens[index] == ")", "whatever");
			// 	}
			// }

			if (tokens[index] == "version")
			{
				startOfStage = pos;
			}

			pos = shaderSource.find('#', pos + 1);
		}

		VKPG_VERIFY(stagePositions.size(), "Could not preprocess the shader since there are no recognized stages in the file!");
		auto& [firstStage, firstStagePos] = stagePositions[0];
		if (stagePositions.size() > 1)
		{
			// In case we have more than one stage (vertex, fragment shaders)

			// Get first stage
			const std::string firstStageStr = shaderSource.substr(0, stagePositions[1].second);
			std::size_t lineCount = std::count(firstStageStr.begin(), firstStageStr.end(), '\n') + 1;
			shaderSources[firstStage] = firstStageStr;

			// Get middle stages
			for (std::size_t i = 1; i < stagePositions.size() - 1; i++)
			{
				auto& [stage, stagePos] = stagePositions[i];
				std::string stageStr = shaderSource.substr(stagePos, stagePositions[i + 1].second - stagePos);
				const std::size_t secondLinePos = stageStr.find_first_of('\n', 1) + 1;
				stageStr.insert(secondLinePos, fmt::format("#line {}\n", lineCount));
				lineCount += std::count(stageStr.begin(), stageStr.end(), '\n') + 1;
			}

			// Get last stage
			auto& [stage, stagePos] = stagePositions[stagePositions.size() - 1];
			std::string lastStageStr = shaderSource.substr(stagePos);
			const std::size_t secondLinePos = lastStageStr.find_first_of('\n', 1) + 1;
			lastStageStr.insert(secondLinePos, fmt::format("#line {}\n", lineCount + 1));
			shaderSources[stage] = lastStageStr;
		}
		else
		{
			// In case we have only one stage (compute shaders)
			shaderSources[firstStage] = shaderSource;
		}

		return shaderSources;
	}

	std::vector<std::string> ShaderPreProcessor::TokenizeString(const std::string& str) const
	{
		std::vector<std::string> result;
		std::regex re(R"((^\W|^\w+)|(\w+)|[:()])", std::regex_constants::optimize);

		std::regex_iterator<std::string::const_iterator> rit(str.begin(), str.cend(), re);
		std::regex_iterator<std::string::const_iterator> rend;

		while (rit != rend)
		{
			result.emplace_back(rit->str());
			++rit;
		}

		return result;
	}

}