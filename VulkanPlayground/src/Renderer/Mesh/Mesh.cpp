#include "vkPch.h"
#include "Mesh.h"

#include "Renderer/Shaders/Shader.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

#include <assimp/Importer.hpp>

namespace vkPlayground {

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

		VKPG_CORE_DEBUG_TAG("Mesh", "-------------------------------------------------");
		VKPG_CORE_DEBUG_TAG("Mesh", "VertexBuffer dump...");
		VKPG_CORE_DEBUG_TAG("Mesh", "Mesh {0}", m_AssetPath);
		for (std::size_t i = 0; i < m_Vertices.size(); i++)
		{
			auto& vertex = m_Vertices[i];
			VKPG_CORE_DEBUG_TAG("Mesh", "Vertex {0}", i);
			VKPG_CORE_DEBUG_TAG("Mesh", "Position: {0} - {1} - {2}", vertex.Position.x, vertex.Position.y, vertex.Position.z);
			VKPG_CORE_DEBUG_TAG("Mesh", "Normal: {0} - {1} - {2}", vertex.Normal.x, vertex.Normal.y, vertex.Normal.z);
			VKPG_CORE_DEBUG_TAG("Mesh", "Tangent: {0} - {1} - {2}", vertex.Tangent.x, vertex.Tangent.y, vertex.Tangent.z);
			VKPG_CORE_DEBUG_TAG("Mesh", "Binormal: {0} - {1} - {2}", vertex.Binormal.x, vertex.Binormal.y, vertex.Binormal.z);
			VKPG_CORE_DEBUG_TAG("Mesh", "TexCoord: {0} - {1}", vertex.TexCoord.x, vertex.TexCoord.y);
		}
		VKPG_CORE_DEBUG_TAG("Mesh", "-------------------------------------------------");
	}

	Ref<StaticMesh> StaticMesh::Create(Ref<MeshSource> meshSource)
	{
		return CreateRef<StaticMesh>(meshSource);
	}

	Ref<StaticMesh> StaticMesh::Create(Ref<MeshSource> meshSource, const std::vector<uint32_t>& subMeshes)
	{
		return CreateRef<StaticMesh>(meshSource, subMeshes);
	}

	StaticMesh::StaticMesh(Ref<MeshSource> meshSource)
		: m_MeshSource(meshSource)
	{
		if (meshSource)
		{
			SetSubMeshes({}, meshSource);

			const std::vector<Ref<Material>>& meshMaterials = meshSource->GetMaterials();
			uint32_t numOfMaterials = (uint32_t)meshMaterials.size();
			m_MaterialTable = MaterialTable::Create(numOfMaterials);

			for (uint32_t i = 0; i < numOfMaterials; i++)
				m_MaterialTable->SetMaterial(i, MaterialAsset::Create(meshMaterials[i]));
		}
	}

	StaticMesh::StaticMesh(Ref<MeshSource> meshSource, const std::vector<uint32_t>& subMeshes)
		: m_MeshSource(meshSource)
	{
		if (meshSource)
		{
			SetSubMeshes(subMeshes, meshSource);

			const std::vector<Ref<Material>>& meshMaterials = meshSource->GetMaterials();
			uint32_t numOfMaterials = (uint32_t)meshMaterials.size();
			m_MaterialTable = MaterialTable::Create(numOfMaterials);

			for (uint32_t i = 0; i < numOfMaterials; i++)
				m_MaterialTable->SetMaterial(i, MaterialAsset::Create(meshMaterials[i]));
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
			const std::vector<MeshUtils::SubMesh>& subMeshes = meshSource->GetSubMeshes();
			m_SubMeshes.resize(subMeshes.size());

			for (uint32_t i = 0; i < subMeshes.size(); i++)
				m_SubMeshes[i] = i;
		}
	}
}