#pragma once

#include "Scene.h"

namespace YAML {

	class Emitter;
	class Node;

}

namespace Iris {

	class SceneSerializer
	{
	public:
		static void Serialize(Ref<Scene> scene, const std::filesystem::path& filePath);
		static bool Deserialize(Ref<Scene>& scene, const std::filesystem::path& filePath);

	private:
		static void SerializeEntity(YAML::Emitter& out, Entity entity, Ref<Scene> scene);
		static void DeserializeEntity(YAML::Node& entitiesNode, Ref<Scene> scene, const std::vector<UUID>& visibleEntities);

	};

}