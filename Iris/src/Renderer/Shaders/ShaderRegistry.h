#pragma once

#include "Compiler/ShaderCompiler.h"

#include <map>
#include <unordered_set>

namespace Iris {

	class ShaderRegistry 
	{
	public:
		static VkShaderStageFlagBits HasChanged(Ref<ShaderCompiler> shaderCompiler);
	private:
		// Shader filepath -> Stage(s)
		static void Serialize(const std::map<std::string, std::map<VkShaderStageFlagBits, StageData>>& shaderCache);
		static void Deserialize(std::map<std::string, std::map<VkShaderStageFlagBits, StageData>>& shaderCache);
	};

}