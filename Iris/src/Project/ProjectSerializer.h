#pragma once

#include "Project.h"

namespace Iris {

	struct ProjectSerializer
	{
		static void Serialize(Ref<Project> project, const std::filesystem::path& filePath);
		static bool Deserialize(Ref<Project>& project, const std::filesystem::path& filePath);
	};

}