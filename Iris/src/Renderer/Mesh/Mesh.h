#pragma once

#include "AssetManager/Asset/Asset.h"
#include "Core/AABB.h"
#include "MaterialAsset.h"
#include "Renderer/IndexBuffer.h"
#include "Renderer/VertexBuffer.h"

#include <glm/glm.hpp>

namespace Iris {

	/*
	 * TODO: When implementing mouse picking:
	 *	- If we implement using ray casting: Use mesh triangle cache
	 *	- If we implement using read back from UUID image then we dont need triangle caches and we should remove them since they provide alot of memory overhead
	 */

	namespace MeshUtils {

		struct Vertex
		{
			glm::vec3 Position;
			glm::vec3 Normal;
			glm::vec3 Tangent;
			glm::vec3 Binormal;
			glm::vec2 TexCoord;
		};

		static const int s_NumberOfAttributes = 5;

		struct Index
		{
			uint32_t V1, V2, V3;
		};

		static_assert(sizeof(Index) == 3 * sizeof(uint32_t));

		struct Triangle
		{
			Vertex V0, V1, V2;
		};

		struct SubMesh
		{
			uint32_t BaseVertex;
			uint32_t VertexCount;
			uint32_t BaseIndex;
			uint32_t IndexCount;
			uint32_t MaterialIndex;

			glm::mat4 Transform{ 1.0f }; // World Transform
			glm::mat4 LocalTransform{ 1.0f };
			AABB BoundingBox;

			std::string NodeName;
			std::string MeshName;

			// bool IsRigged = false; // For animation...

			// TODO: Add serialize and deserialize methods for asset serialization later...
		};

		struct MeshNode
		{
			uint32_t Parent = UINT32_MAX;
			std::vector<uint32_t> Children;
			std::vector<uint32_t> SubMeshes;

			std::string Name;
			glm::mat4 LocalTransform;

			inline bool IsRoot() const { return Parent == UINT32_MAX; }

			// TODO: Add serialize and deserialize methods for asset serialization later...
		};
		
	}

	class MeshSource : public Asset
	{
	public:
		MeshSource() = default;
		MeshSource(const std::vector<MeshUtils::Vertex>& vertices, const std::vector<MeshUtils::Index>& indices, const glm::mat4& transform);
		MeshSource(const std::vector<MeshUtils::Vertex>& vertices, const std::vector<MeshUtils::Index>& indices, const std::vector<MeshUtils::SubMesh>& subMeshes);
		virtual ~MeshSource();

		[[nodiscard]] static Ref<MeshSource> Create();
		[[nodiscard]] static Ref<MeshSource> Create(const std::vector<MeshUtils::Vertex>& vertices, const std::vector<MeshUtils::Index>& indices, const glm::mat4& transform);
		[[nodiscard]] static Ref<MeshSource> Create(const std::vector<MeshUtils::Vertex>& vertices, const std::vector<MeshUtils::Index>& indices, const std::vector<MeshUtils::SubMesh>& subMeshes);

		const std::string& GetAssetPath() const { return m_AssetPath; }

		std::vector<MeshUtils::SubMesh>& GetSubMeshes() { return m_SubMeshes; }
		const std::vector<MeshUtils::SubMesh>& GetSubMeshes() const { return m_SubMeshes; }

		Ref<VertexBuffer> GetVertexBuffer() const { return m_VertexBuffer; }
		Ref<IndexBuffer> GetIndexBuffer() const { return m_IndexBuffer; }

		const std::vector<MeshUtils::Vertex>& GetVertices() const { return m_Vertices; }
		const std::vector<MeshUtils::Index>& GetIndices() const { return m_Indices; }

		std::vector<Ref<Material>>& GetMaterials() { return m_Materials; }
		const std::vector<Ref<Material>>& GetMaterials() const { return m_Materials; }

		const std::vector<MeshUtils::Triangle>& GetTriangleCache(uint32_t index) const { IR_ASSERT(m_TriangleCache.contains(index)); return m_TriangleCache.at(index); }

		const AABB& GetBoundingBox() const { return m_BoundingBox; }

		const MeshUtils::MeshNode& GetRootNode() const { return m_Nodes[0]; }
		const std::vector<MeshUtils::MeshNode>& GetNodes() const { return m_Nodes; }

		// For debugging
		void DumpVertexBuffer();

		static AssetType GetStaticType() { return AssetType::MeshSource; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }

	private:
		std::string m_AssetPath;

		std::vector<MeshUtils::SubMesh> m_SubMeshes;

		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;

		std::vector<MeshUtils::Vertex> m_Vertices;
		std::vector<MeshUtils::Index> m_Indices;

		std::vector<Ref<Material>> m_Materials;

		std::unordered_map<uint32_t, std::vector<MeshUtils::Triangle>> m_TriangleCache;

		AABB m_BoundingBox;

		std::vector<MeshUtils::MeshNode> m_Nodes;

		friend class AssimpMeshImporter;

	};

	class StaticMesh : public Asset
	{
	public:
		explicit StaticMesh(AssetHandle meshSource);
		StaticMesh(AssetHandle meshSource, const std::vector<uint32_t>& subMeshes);
		virtual ~StaticMesh() = default;

		[[nodiscard]] static Ref<StaticMesh> Create(AssetHandle meshSource);
		[[nodiscard]] static Ref<StaticMesh> Create(AssetHandle meshSource, const std::vector<uint32_t>& subMeshes);

		AssetHandle GetMeshSource() const { return m_MeshSource; }
		void SetMeshSource(AssetHandle meshSource) { m_MeshSource = meshSource; }

		const std::vector<uint32_t>& GetSubMeshes() const { return m_SubMeshes; }
		void SetSubMeshes(const std::vector<uint32_t>& subMeshes, Ref<MeshSource> meshSource); // Pass empty vector to set all submeshes

		Ref<MaterialTable> GetMaterials() const { return m_MaterialTable; }

		static AssetType GetStaticType() { return AssetType::StaticMesh; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }

	private:
		AssetHandle m_MeshSource;
		std::vector<uint32_t> m_SubMeshes;

		Ref<MaterialTable> m_MaterialTable;
	};

}