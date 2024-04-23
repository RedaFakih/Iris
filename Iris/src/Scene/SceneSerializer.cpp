#include "IrisPCH.h"
#include "SceneSerializer.h"

#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

#include <yaml-cpp/yaml.h>

namespace Iris {

	// TODO:

	void SceneSerializer::Serialize(Ref<Scene> scene, const std::filesystem::path& filePath)
	{

	}

	bool SceneSerializer::Deserialize(Ref<Scene>& scene, const std::filesystem::path& filePath)
	{
		return false;
	}

}