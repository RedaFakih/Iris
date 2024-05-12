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

	bool MeshSourceSerializer::TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const
	{
		AssimpMeshImporter importer(Project::GetEditorAssetManager()->GetFileSystemPathString(metaData));
		Ref<MeshSource> meshSource = importer.ImportToMeshSource();
		if (!meshSource)
		{
			asset->SetFlag(AssetFlag::Invalid);
			return false;
		}

		asset = meshSource;
		asset->Handle = metaData.Handle;
		return true;
	}

	/////////////////////////////////////////
	/// StaticMesh
	/////////////////////////////////////////

	void StaticMeshSerializer::Serialize(const AssetMetaData& metaData, const Ref<Asset>& asset) const
	{
		Ref<StaticMesh> staticMesh = asset.As<StaticMesh>();

		std::string yamlString = SerializeToYAML(staticMesh);

		auto serializePath = Project::GetActive()->GetAssetDirectory() / metaData.FilePath;
		std::ofstream fout(serializePath);
		IR_VERIFY(fout.good());
		fout << yamlString;
	}

	bool StaticMeshSerializer::TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const
	{
		auto filepath = Project::GetAssetDirectory() / metaData.FilePath;
		std::ifstream stream(filepath);
		IR_ASSERT(stream);
		if (!stream)
		{
			asset->SetFlag(AssetFlag::Missing);
			return false;
		}
		std::stringstream strStream;
		strStream << stream.rdbuf();

		Ref<StaticMesh> staticMesh;
		bool success = DeserializeFromYAML(strStream.str(), staticMesh);
		if (!success)
		{
			asset->SetFlag(AssetFlag::Invalid);
			return false;
		}

		staticMesh->Handle = metaData.Handle;
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
			out << YAML::Key << "SubMeshIndices" << YAML::Flow;
			Ref<MeshSource> meshSource = AssetManager::GetAsset<MeshSource>(staticMesh->GetMeshSource());
			if (meshSource && meshSource->GetSubMeshes().size() == staticMesh->GetSubMeshes().size())
				out << YAML::Value << std::vector<uint32_t>();
			else
				out << YAML::Value << staticMesh->GetSubMeshes();
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