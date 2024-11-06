#include "IrisPCH.h"
#include "Mesh.h"

#include "AssetManager/AssetManager.h"
#include "Renderer/Shaders/Shader.h"

#include <assimp/Importer.hpp>

namespace Iris {

	///////////////////////////////////////////////////////////////////////////////////////////////////
	/// MeshSource
	///////////////////////////////////////////////////////////////////////////////////////////////////

	Ref<MeshSource> MeshSource::Create()
	{
		return CreateRef<MeshSource>();
	}

	Ref<MeshSource> MeshSource::Create(const std::vector<MeshUtils::Vertex>& vertices, const std::vector<MeshUtils::Index>& indices, const glm::mat4& transform)
	{
		return CreateRef<MeshSource>(vertices, indices, transform);
	}

	Ref<MeshSource> MeshSource::Create(const std::vector<MeshUtils::Vertex>& vertices, const std::vector<MeshUtils::Index>& indices, const std::vector<MeshUtils::SubMesh>& subMeshes)
	{
		return CreateRef<MeshSource>(vertices, indices, subMeshes);
	}

	MeshSource::MeshSource(const std::vector<MeshUtils::Vertex>& vertices, const std::vector<MeshUtils::Index>& indices, const glm::mat4& transform)
		: m_Vertices(vertices), m_Indices(indices)
	{
		MeshUtils::SubMesh subMesh = {
			.BaseVertex = 0,
			.VertexCount = static_cast<uint32_t>(vertices.size()),
			.BaseIndex = 0,
			.IndexCount = static_cast<uint32_t>(indices.size() * 3),
			.Transform = transform
		};
		m_SubMeshes.push_back(subMesh);

		m_VertexBuffer = VertexBuffer::Create(m_Vertices.data(), static_cast<uint32_t>(m_Vertices.size() * sizeof(MeshUtils::Vertex)));
		m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), static_cast<uint32_t>(m_Indices.size() * sizeof(MeshUtils::Index)));
	}

	MeshSource::MeshSource(const std::vector<MeshUtils::Vertex>& vertices, const std::vector<MeshUtils::Index>& indices, const std::vector<MeshUtils::SubMesh>& subMeshes)
		: m_Vertices(vertices), m_Indices(indices), m_SubMeshes(subMeshes)
	{
		m_VertexBuffer = VertexBuffer::Create(m_Vertices.data(), static_cast<uint32_t>(m_Vertices.size() * sizeof(MeshUtils::Vertex)));
		m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), static_cast<uint32_t>(m_Indices.size() * sizeof(MeshUtils::Index)));
	}

	MeshSource::~MeshSource()
	{
	}

	void MeshSource::DumpVertexBuffer()
	{
		// NOTE: This is for debugging...

		IR_CORE_DEBUG_TAG("Mesh", "-------------------------------------------------");
		IR_CORE_DEBUG_TAG("Mesh", "VertexBuffer dump...");
		IR_CORE_DEBUG_TAG("Mesh", "Mesh {0}", m_AssetPath);
		for (std::size_t i = 0; i < m_Vertices.size(); i++)
		{
			auto& vertex = m_Vertices[i];
			IR_CORE_DEBUG_TAG("Mesh", "Vertex {0}", i);
			IR_CORE_DEBUG_TAG("Mesh", "Position: {0} - {1} - {2}", vertex.Position.x, vertex.Position.y, vertex.Position.z);
			IR_CORE_DEBUG_TAG("Mesh", "Normal: {0} - {1} - {2}", vertex.Normal.x, vertex.Normal.y, vertex.Normal.z);
			IR_CORE_DEBUG_TAG("Mesh", "Tangent: {0} - {1} - {2}", vertex.Tangent.x, vertex.Tangent.y, vertex.Tangent.z);
			IR_CORE_DEBUG_TAG("Mesh", "Binormal: {0} - {1} - {2}", vertex.Binormal.x, vertex.Binormal.y, vertex.Binormal.z);
			IR_CORE_DEBUG_TAG("Mesh", "TexCoord: {0} - {1}", vertex.TexCoord.x, vertex.TexCoord.y);
		}
		IR_CORE_DEBUG_TAG("Mesh", "-------------------------------------------------");
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////
	/// StaticMesh
	///////////////////////////////////////////////////////////////////////////////////////////////////

	static std::vector<uint32_t> s_CurrentlyLoadingMeshSourceIndices;

	Ref<StaticMesh> StaticMesh::Create(AssetHandle meshSource)
	{
		return CreateRef<StaticMesh>(meshSource);
	}

	Ref<StaticMesh> StaticMesh::Create(AssetHandle meshSource, const std::vector<uint32_t>& subMeshes)
	{
		return CreateRef<StaticMesh>(meshSource, subMeshes);
	}

	StaticMesh::StaticMesh(AssetHandle meshSource)
		: m_MeshSource(meshSource)
	{
		Handle = {};

		Ref<MeshSource> meshSourceAsset = AssetManager::GetAsset<MeshSource>(meshSource);
		if (meshSourceAsset)
		{
			SetSubMeshes({}, meshSourceAsset);

			const std::vector<AssetHandle>& meshMaterials = meshSourceAsset->GetMaterials();
			m_MaterialTable = MaterialTable::Create(static_cast<uint32_t>(meshMaterials.size()));

			for (uint32_t i = 0; i < static_cast<uint32_t>(meshMaterials.size()); i++)
				m_MaterialTable->SetMaterial(i, meshMaterials[i]);
			// Memory only since the material table is just in memory and is not an asset
		}
	}

	StaticMesh::StaticMesh(AssetHandle meshSource, const std::vector<uint32_t>& subMeshes)
		: m_MeshSource(meshSource)
	{
		Handle = {};

		s_CurrentlyLoadingMeshSourceIndices = subMeshes;
		Ref<MeshSource> meshSourceAsset = AssetManager::GetAsset<MeshSource>(meshSource);
		s_CurrentlyLoadingMeshSourceIndices = {}; // Clear after the meshSource if loaded
		if (meshSourceAsset)
		{
			SetSubMeshes(subMeshes, meshSourceAsset);

			const std::vector<AssetHandle>& meshMaterials = meshSourceAsset->GetMaterials();
			m_MaterialTable = MaterialTable::Create(static_cast<uint32_t>(meshMaterials.size()));

			for (uint32_t i = 0; i < static_cast<uint32_t>(meshMaterials.size()); i++)
				m_MaterialTable->SetMaterial(i, meshMaterials[i]);
		}
	}

	void StaticMesh::SetSubMeshes(const std::vector<uint32_t>& subMeshes, Ref<MeshSource> meshSource)
	{
		if (!subMeshes.empty())
		{
			m_SubMeshes = subMeshes;
		}
		else
		{
			const std::vector<MeshUtils::SubMesh>& subMeshess = meshSource->GetSubMeshes();
			m_SubMeshes.resize(subMeshess.size());

			for (uint32_t i = 0; i < subMeshess.size(); i++)
				m_SubMeshes[i] = i;
		}
	}
	const std::vector<uint32_t>& StaticMesh::GetCurrentlyLoadingMeshSourceIndices()
	{
		return s_CurrentlyLoadingMeshSourceIndices;
	}
}