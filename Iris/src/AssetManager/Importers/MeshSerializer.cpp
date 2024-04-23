#include "IrisPCH.h"
#include "MeshSerializer.h"

#include "AssetManager/AssetManager.h"
#include "MeshImporter.h"
#include "Project/Project.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

#include <yaml-cpp/yaml.h>

namespace YAML {

	template<>
	struct convert<std::vector<uint32_t>>
	{
		static Node encode(const std::vector<uint32_t>& value)
		{
			Node node;
			for (uint32_t element : value)
				node.push_back(element);
			return node;
		}

		static bool decode(const Node& node, std::vector<uint32_t>& result)
		{
			if (!node.IsSequence())
				return false;

			result.resize(node.size());
			for (size_t i = 0; i < node.size(); i++)
				result[i] = node[i].as<uint32_t>();

			return true;
		}
	};

}

namespace Iris {

	YAML::Emitter& operator<<(YAML::Emitter& out, const std::vector<uint32_t>& value)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq;
		for (uint32_t element : value)
			out << element;
		out << YAML::EndSeq;
		return out;
	}

	/////////////////////////////////////////
	/// MeshSource
	/////////////////////////////////////////

	bool MeshSourceSerializer::TryLoadData(const AssetMetaData& metadata, Ref<Asset>& asset) const
	{
		AssimpMeshImporter importer(Project::GetEditorAssetManager()->GetFileSystemPathString(metadata));
		Ref<MeshSource> meshSource = importer.ImportToMeshSource();
		if (!meshSource)
			return false;

		asset = meshSource;
		asset->Handle = metadata.Handle;
		return true;
	}

	/////////////////////////////////////////
	/// StaticMesh
	/////////////////////////////////////////

	void StaticMeshSerializer::Serialize(const AssetMetaData& metadata, const Ref<Asset>& asset) const
	{
		Ref<StaticMesh> staticMesh = asset.As<StaticMesh>();

		std::string yamlString = SerializeToYAML(staticMesh);

		auto serializePath = Project::GetActive()->GetAssetDirectory() / metadata.FilePath;
		std::ofstream fout(serializePath);
		IR_VERIFY(fout.good());
		fout << yamlString;
	}

	bool StaticMeshSerializer::TryLoadData(const AssetMetaData& metadata, Ref<Asset>& asset) const
	{
		// TODO: this needs to open up a Hazel Mesh file and make sure the MeshSource file is also loaded

		auto filepath = Project::GetAssetDirectory() / metadata.FilePath;
		std::ifstream stream(filepath);
		IR_ASSERT(stream);
		std::stringstream strStream;
		strStream << stream.rdbuf();

		Ref<StaticMesh> staticMesh;
		bool success = DeserializeFromYAML(strStream.str(), staticMesh);
		if (!success)
			return false;

		staticMesh->Handle = metadata.Handle;
		asset = staticMesh;
		return true;
	}

	std::string StaticMeshSerializer::SerializeToYAML(Ref<StaticMesh> staticMesh) const
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "StaticMesh";
		{
			out << YAML::BeginMap;
			out << YAML::Key << "MeshSource" << YAML::Value << staticMesh->GetMeshSource();
			out << YAML::Key << "SubMeshIndices" << YAML::Flow << YAML::Value << staticMesh->GetSubMeshes();
			out << YAML::EndMap;
		}
		out << YAML::EndMap;

		return std::string(out.c_str());
	}

	bool StaticMeshSerializer::DeserializeFromYAML(const std::string& yamlString, Ref<StaticMesh>& targetStaticMesh) const
	{
		YAML::Node data = YAML::Load(yamlString);
		if (!data["StaticMesh"])
			return false;

		YAML::Node rootNode = data["StaticMesh"];
		if (!rootNode["MeshSource"])
			return false;

		AssetHandle meshSource = rootNode["MeshSource"].as<uint64_t>();
		std::vector<uint32_t> subMeshIndices = rootNode["SubMeshIndices"].as<std::vector<uint32_t>>();

		targetStaticMesh = StaticMesh::Create(meshSource, subMeshIndices);
		return true;
	}

}