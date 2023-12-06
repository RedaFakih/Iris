#pragma once

#include "IndexBuffer.h"
#include "VertexBuffer.h"

#include <glm/glm.hpp>

#include <string>

namespace vkPlayground {

	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Color;
		glm::vec2 TexCoord;
	};

	struct Index
	{
		uint32_t V1, V2, V3;
	};

	class MeshSource : public RefCountedObject
	{
	public:
		MeshSource(const std::string& filepath);

		Ref<VertexBuffer> GetVertexBuffer() const { return m_VertexBuffer; }
		Ref<IndexBuffer> GetIndexBuffer() const { return m_IndexBuffer; }

	private:
		void LoadFromFile();

	private:
		std::string m_AssetPath;

		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;

	};

}