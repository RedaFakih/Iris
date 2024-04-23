#pragma once

#include "Scene.h"

namespace Iris {

	struct SceneSerializer
	{
		static void Serialize(Ref<Scene> scene, const std::filesystem::path& filePath);
		static bool Deserialize(Ref<Scene>& scene, const std::filesystem::path& filePath);
	};

}