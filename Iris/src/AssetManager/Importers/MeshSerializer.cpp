#include "IrisPCH.h"
#include "MeshSerializer.h"

#include "AssetManager/Asset/MeshColliderAsset.h"
#include "AssetManager/AssetManager.h"
#include "MeshImporter.h"
#include "Project/Project.h"
#include "Utils/YAMLSerializationHelpers.h"

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

		std::filesystem::path serializePath = Project::GetAssetDirectory() / metaData.FilePath;
		std::ofstream fout(serializePath);
		IR_VERIFY(fout.good());
		fout << yamlString;
	}

	bool StaticMeshSerializer::TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const
	{
		std::filesystem::path filepath = Project::GetAssetDirectory() / metaData.FilePath;
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
			out << YAML::Key << "GenerateColliders" << YAML::Value << staticMesh->ShouldGenerateColliders();
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
		bool generateColliders = rootNode["GenerateColliders"].as<bool>(true); // TODO: For StaticMeshes we set to true and for dynamic meshes we want to set to false

		targetStaticMesh = StaticMesh::Create(meshSource, subMeshIndices, generateColliders);
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// MeshColliderSerializer
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	void MeshColliderSerializer::Serialize(const AssetMetaData& metaData, const Ref<Asset>& asset) const
	{
		Ref<MeshColliderAsset> meshCollider = asset.As<MeshColliderAsset>();

		std::string yamlString = SerializeToYAML(meshCollider);

		std::ofstream fout(Project::GetAssetDirectory() / metaData.FilePath);
		IR_VERIFY(fout.good());
		fout << yamlString;
	}

	bool MeshColliderSerializer::TryLoadData(const AssetMetaData& metaData, Ref<Asset>& asset) const
	{
		std::filesystem::path filepath = Project::GetAssetDirectory() / metaData.FilePath;
		std::ifstream stream(filepath);
		IR_ASSERT(stream);
		if (!stream)
		{
			asset->SetFlag(AssetFlag::Missing);
			return false;
		}

		std::stringstream strStream;
		strStream << stream.rdbuf();

		Ref<MeshColliderAsset> meshColliderAsset;
		bool success = DeserializeFromYAML(strStream.str(), meshColliderAsset);
		if (!success)
		{
			asset->SetFlag(AssetFlag::Invalid);
			return false;
		}

		meshColliderAsset->Handle = metaData.Handle;
		asset = meshColliderAsset;
		return true;
	}

	std::string MeshColliderSerializer::SerializeToYAML(Ref<MeshColliderAsset> meshCollider) const
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "MeshCollider" << YAML::Value;
		{
			out << YAML::BeginMap;

			out << YAML::Key << "ColliderMesh" << YAML::Value << meshCollider->ColliderMesh;
			out << YAML::Key << "EnableVertexWelding" << YAML::Value << meshCollider->EnableVertexWelding;
			out << YAML::Key << "VertexWeldTolerance" << YAML::Value << meshCollider->VertexWeldTolerance;
			out << YAML::Key << "CheckZeroAreaTriangles" << YAML::Value << meshCollider->CheckZeroAreaTriangles;
			out << YAML::Key << "AreaTestEpsilon" << YAML::Value << meshCollider->AreaTestEpsilon;
			out << YAML::Key << "FlipNormals" << YAML::Value << meshCollider->FlipNormals;
			out << YAML::Key << "ShiftVerticesToOrigin" << YAML::Value << meshCollider->ShiftVerticesToOrigin;
			out << YAML::Key << "AlwaysShareShape" << YAML::Value << meshCollider->AlwaysShareShape;
			out << YAML::Key << "CollisionComplexity" << YAML::Value << static_cast<uint8_t>(meshCollider->CollisionComplexity);
			out << YAML::Key << "ColliderScale" << YAML::Value << meshCollider->ColliderScale;

			out << YAML::Key << "ColliderMaterial" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Friction" << YAML::Value << meshCollider->Material.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << meshCollider->Material.Restitution;
			out << YAML::EndMap;

			out << YAML::EndMap;
		}
		out << YAML::EndMap;

		return std::string(out.c_str());
	}

	bool MeshColliderSerializer::DeserializeFromYAML(const std::string& yamlString, Ref<MeshColliderAsset>& targetMeshCollider) const
	{
		YAML::Node rootNode = YAML::Load(yamlString);
		YAML::Node colliderMeshNode = rootNode["MeshCollider"];

		if (!colliderMeshNode)
			return false;

		targetMeshCollider->ColliderMesh = colliderMeshNode["ColliderMesh"].as<AssetHandle>(AssetHandle(0));
		targetMeshCollider->EnableVertexWelding = colliderMeshNode["EnableVertexWelding"].as<bool>(true);
		targetMeshCollider->VertexWeldTolerance = colliderMeshNode["VertexWeldTolerance"].as<float>(0.1f);
		targetMeshCollider->CheckZeroAreaTriangles = colliderMeshNode["CheckZeroAreaTriangles"].as<bool>(true);
		targetMeshCollider->AreaTestEpsilon = colliderMeshNode["AreaTestEpsilon"].as<float>(0.06f);
		targetMeshCollider->FlipNormals = colliderMeshNode["FlipNormals"].as<bool>(false);
		targetMeshCollider->ShiftVerticesToOrigin = colliderMeshNode["ShiftVerticesToOrigin"].as<bool>(false);
		targetMeshCollider->AlwaysShareShape = colliderMeshNode["AlwaysShareShape"].as<bool>(false);
		targetMeshCollider->CollisionComplexity = static_cast<PhysicsCollisionComplexity>(colliderMeshNode["CollisionComplexity"].as<uint8_t>(0));
		targetMeshCollider->ColliderScale = colliderMeshNode["ColliderScale"].as<glm::vec3>(glm::vec3{ 1.0f, 1.0f, 1.0f });

		targetMeshCollider->Material.Friction = colliderMeshNode["ColliderMaterial"]["Friction"].as<float>(0.1f);
		targetMeshCollider->Material.Restitution = colliderMeshNode["ColliderMaterial"]["Restitution"].as<float>(0.05f);

		return true;
	}

}