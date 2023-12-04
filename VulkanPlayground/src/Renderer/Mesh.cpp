#include "vkPch.h"
#include "Mesh.h"

namespace vkPlayground {

	Mesh::Mesh(Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer)
		: m_VertexBuffer(vertexBuffer), m_IndexBuffer(indexBuffer)
	{
	}

}