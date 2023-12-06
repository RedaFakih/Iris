#include "vkPch.h"
#include "Mesh.h"

#include <fastgltf/parser.hpp>

namespace vkPlayground {

	MeshSource::MeshSource(const std::string& filepath)
		: m_AssetPath(filepath)
	{
		LoadFromFile();
	}

	void MeshSource::LoadFromFile()
	{
		fastgltf::Parser parser;
	}

}