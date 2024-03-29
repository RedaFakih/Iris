#pragma once

#include "Renderer/Mesh/Mesh.h"

namespace vkPlayground {

	class AssimpMeshImporter
	{
	public:
		AssimpMeshImporter(const std::string& assetPath);

		Ref<MeshSource> ImportToMeshSource();

	private:
		// level is for debugging
		void TraverseNodes(Ref<MeshSource> meshSource, void* assimpNode, uint32_t nodeIndex, const glm::mat4& parentTransform = glm::mat4(1.0f), uint32_t level = 0);

	private:
		const std::filesystem::path m_AssetPath;

	};

}