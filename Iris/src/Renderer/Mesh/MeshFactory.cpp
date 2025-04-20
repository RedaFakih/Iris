#include "IrisPCH.h"
#include "MeshFactory.h"

#include "AssetManager/AssetManager.h"
#include "Mesh.h"

namespace Iris {

	namespace Utils {

		static void CalculateRing(std::size_t segments, float radius, float y, float dy, float height, float actualRadius, std::vector<MeshUtils::Vertex>& vertices)
		{
			float segIncr = 1.0f / static_cast<float>(segments - 1);
			for (std::size_t s = 0; s < segments; s++)
			{
				float x = glm::cos((glm::pi<float>() * 2.0f) * s * segIncr) * radius;
				float z = glm::sin((glm::pi<float>() * 2.0f) * s * segIncr) * radius;

				MeshUtils::Vertex& vertex = vertices.emplace_back();
				vertex.Position = glm::vec3(actualRadius * x, actualRadius * y + height * dy, actualRadius * z);
			}
		}

		// For the cylinder
		static void CalculateRing(std::size_t segments, float radius, float y, std::vector<MeshUtils::Vertex>& vertices)
		{
			float segIncr = 1.0f / static_cast<float>(segments - 1);
			for (std::size_t s = 0; s < segments; s++)
			{
				float x = glm::cos((glm::pi<float>() * 2.0f) * s * segIncr) * radius;
				float z = glm::sin((glm::pi<float>() * 2.0f) * s * segIncr) * radius;

				MeshUtils::Vertex& vertex = vertices.emplace_back();
				vertex.Position = glm::vec3(x, y, z);
			}
		}

	}

	AssetHandle MeshFactory::CreateBox(const glm::vec3& size)
	{
		std::vector<MeshUtils::Vertex> vertices;
		vertices.resize(8);
		vertices[0].Position = { -size.x / 2.0f, -size.y / 2.0f,  size.z / 2.0f };
		vertices[1].Position = { size.x / 2.0f, -size.y / 2.0f,  size.z / 2.0f };
		vertices[2].Position = { size.x / 2.0f,  size.y / 2.0f,  size.z / 2.0f };
		vertices[3].Position = { -size.x / 2.0f,  size.y / 2.0f,  size.z / 2.0f };
		vertices[4].Position = { -size.x / 2.0f, -size.y / 2.0f, -size.z / 2.0f };
		vertices[5].Position = { size.x / 2.0f, -size.y / 2.0f, -size.z / 2.0f };
		vertices[6].Position = { size.x / 2.0f,  size.y / 2.0f, -size.z / 2.0f };
		vertices[7].Position = { -size.x / 2.0f,  size.y / 2.0f, -size.z / 2.0f };

		vertices[0].Normal = { -1.0f, -1.0f,  1.0f };
		vertices[1].Normal = { 1.0f, -1.0f,  1.0f };
		vertices[2].Normal = { 1.0f,  1.0f,  1.0f };
		vertices[3].Normal = { -1.0f,  1.0f,  1.0f };
		vertices[4].Normal = { -1.0f, -1.0f, -1.0f };
		vertices[5].Normal = { 1.0f, -1.0f, -1.0f };
		vertices[6].Normal = { 1.0f,  1.0f, -1.0f };
		vertices[7].Normal = { -1.0f,  1.0f, -1.0f };

		std::vector<MeshUtils::Index> indices;
		indices.resize(12);
		indices[0] = { 0, 1, 2 };
		indices[1] = { 2, 3, 0 };
		indices[2] = { 1, 5, 6 };
		indices[3] = { 6, 2, 1 };
		indices[4] = { 7, 6, 5 };
		indices[5] = { 5, 4, 7 };
		indices[6] = { 4, 0, 3 };
		indices[7] = { 3, 7, 4 };
		indices[8] = { 4, 5, 1 };
		indices[9] = { 1, 0, 4 };
		indices[10] = { 3, 2, 6 };
		indices[11] = { 6, 7, 3 };

		AssetHandle meshSource = AssetManager::CreateMemoryOnlyAsset<MeshSource>(vertices, indices, glm::mat4(1.0f));
		return AssetManager::CreateMemoryOnlyAsset<StaticMesh>(meshSource, false);
	}

	AssetHandle MeshFactory::CreateSphere(float radius)
	{
		std::vector<MeshUtils::Vertex> vertices;
		std::vector<MeshUtils::Index> indices;

		constexpr float latitudeBands = 30;
		constexpr float longitudeBands = 30;

		for (float latitude = 0.0f; latitude <= latitudeBands; latitude++)
		{
			const float theta = latitude * glm::pi<float>() / latitudeBands;
			const float sinTheta = glm::sin(theta);
			const float cosTheta = glm::cos(theta);

			for (float longitude = 0.0f; longitude <= longitudeBands; longitude++)
			{
				const float phi = longitude * 2.f * glm::pi<float>() / longitudeBands;
				const float sinPhi = glm::sin(phi);
				const float cosPhi = glm::cos(phi);

				MeshUtils::Vertex vertex;
				vertex.Normal = { cosPhi * sinTheta, cosTheta, sinPhi * sinTheta };
				vertex.Position = { radius * vertex.Normal.x, radius * vertex.Normal.y, radius * vertex.Normal.z };
				vertices.push_back(vertex);
			}
		}

		for (uint32_t latitude = 0; latitude < static_cast<uint32_t>(latitudeBands); latitude++)
		{
			for (uint32_t longitude = 0; longitude < static_cast<uint32_t>(longitudeBands); longitude++)
			{
				const uint32_t first = (latitude * (static_cast<uint32_t>(longitudeBands) + 1)) + longitude;
				const uint32_t second = first + static_cast<uint32_t>(longitudeBands) + 1;

				indices.push_back({ first, first + 1, second });
				indices.push_back({ second, first + 1, second + 1 });
			}
		}

		AssetHandle meshSource = AssetManager::CreateMemoryOnlyAsset<MeshSource>(vertices, indices, glm::mat4(1.0f));
		return AssetManager::CreateMemoryOnlyAsset<StaticMesh>(meshSource, false);
	}

	AssetHandle MeshFactory::CreateCylinder(float radius, float height)
	{
		constexpr std::size_t numSegments = 12;
		constexpr std::size_t ringsHeight = 8;
		constexpr float radiusModifier = 0.021f;

		std::vector<MeshUtils::Vertex> vertices;
		std::vector<MeshUtils::Index> indices;

		vertices.reserve(numSegments * ringsHeight);
		indices.reserve((numSegments - 1) * (ringsHeight - 1) * 2);

		float ringIncr = height / static_cast<float>(ringsHeight - 1);

		// Generate rings along the cylinder's height
		for (int r = 0; r < ringsHeight; r++)
		{
			float y = -height / 2 + r * ringIncr;
			Utils::CalculateRing(numSegments, radius + radiusModifier, y, vertices);
		}

		// Generate indices to form quads along the height
		for (int r = 0; r < ringsHeight - 1; r++)
		{
			for (int s = 0; s < numSegments - 1; s++)
			{
				MeshUtils::Index& index1 = indices.emplace_back();
				index1.V1 = static_cast<uint32_t>(r * numSegments + s + 1);
				index1.V2 = static_cast<uint32_t>(r * numSegments + s + 0);
				index1.V3 = static_cast<uint32_t>((r + 1) * numSegments + s + 1);

				MeshUtils::Index& index2 = indices.emplace_back();
				index2.V1 = static_cast<uint32_t>((r + 1) * numSegments + s + 0);
				index2.V2 = static_cast<uint32_t>((r + 1) * numSegments + s + 1);
				index2.V3 = static_cast<uint32_t>(r * numSegments + s);
			}
		}

		// Reserve additional space for the extra indices (optional)
		indices.reserve(indices.size() + (2 * numSegments));

		// Add a center vertex for the top face (using the top ring's y-value)
		std::size_t topCenterIndex = vertices.size();
		MeshUtils::Vertex topCenter;
		topCenter.Position = { 0.0f, height / 2.0f, 0.0f };
		vertices.push_back(topCenter);

		// Add a center vertex for the bottom face (using the bottom ring's y-value)
		std::size_t bottomCenterIndex = vertices.size();
		MeshUtils::Vertex bottomCenter;
		bottomCenter.Position = { 0.0f, -height / 2.0f, 0.0f };
		vertices.push_back(bottomCenter);

		// Create the top face using a triangle fan from the top center vertex.
		// The top ring is the last ring we generated.
		std::size_t topRingStart = (ringsHeight - 1) * numSegments;
		for (std::size_t s = 0; s < numSegments; s++)
		{
			// Wrap around using modulus for the last triangle.
			std::size_t next = (s + 1) % numSegments;
			MeshUtils::Index topTri;
			topTri.V1 = static_cast<uint32_t>(topCenterIndex);
			topTri.V2 = static_cast<uint32_t>(topRingStart + s);
			topTri.V3 = static_cast<uint32_t>(topRingStart + next);
			indices.push_back(topTri);
		}

		// Create the bottom face using a triangle fan from the bottom center vertex.
		// The bottom ring is the first ring we generated.
		std::size_t bottomRingStart = 0;
		for (std::size_t s = 0; s < numSegments; s++)
		{
			std::size_t next = (s + 1) % numSegments;
			MeshUtils::Index bottomTri;
			// Note: Reverse the order to ensure the correct winding (and face normal direction)
			bottomTri.V1 = static_cast<uint32_t>(bottomCenterIndex);
			bottomTri.V2 = static_cast<uint32_t>(bottomRingStart + next);
			bottomTri.V3 = static_cast<uint32_t>(bottomRingStart + s);
			indices.push_back(bottomTri);
		}

		AssetHandle meshSource = AssetManager::CreateMemoryOnlyAsset<MeshSource>(vertices, indices, glm::mat4(1.0f));
		return AssetManager::CreateMemoryOnlyAsset<StaticMesh>(meshSource, false);
	}

	AssetHandle MeshFactory::CreateCapsule(float radius, float height)
	{
		constexpr std::size_t subdivisionsHeight = 8;
		constexpr std::size_t ringsBody = subdivisionsHeight + 1;
		constexpr std::size_t ringsTotal = subdivisionsHeight + ringsBody;
		constexpr std::size_t numSegments = 12;
		constexpr float radiusModifier = 0.021f; // Needed to ensure that the wireframe is always visible

		std::vector<MeshUtils::Vertex> vertices;
		std::vector<MeshUtils::Index> indices;

		vertices.reserve(numSegments * ringsTotal);
		indices.reserve((numSegments - 1) * (ringsTotal - 1) * 2);

		float bodyIncr = 1.0f / static_cast<float>(ringsBody - 1);
		float ringIncr = 1.0f / static_cast<float>(subdivisionsHeight - 1);

		for (int r = 0; r < subdivisionsHeight / 2; r++)
			Utils::CalculateRing(numSegments, glm::sin(glm::pi<float>() * r * ringIncr), glm::sin(glm::pi<float>() * (r * ringIncr - 0.5f)), -0.5f, height, radius + radiusModifier, vertices);

		for (int r = 0; r < ringsBody; r++)
			Utils::CalculateRing(numSegments, 1.0f, 0.0f, r * bodyIncr - 0.5f, height, radius + radiusModifier, vertices);

		for (int r = subdivisionsHeight / 2; r < subdivisionsHeight; r++)
			Utils::CalculateRing(numSegments, glm::sin(glm::pi<float>() * r * ringIncr), glm::sin(glm::pi<float>() * (r * ringIncr - 0.5f)), 0.5f, height, radius + radiusModifier, vertices);

		for (int r = 0; r < ringsTotal - 1; r++)
		{
			for (int s = 0; s < numSegments - 1; s++)
			{
				MeshUtils::Index& index1 = indices.emplace_back();
				index1.V1 = static_cast<uint32_t>(r * numSegments + s + 1);
				index1.V2 = static_cast<uint32_t>(r * numSegments + s + 0);
				index1.V3 = static_cast<uint32_t>((r + 1) * numSegments + s + 1);

				MeshUtils::Index& index2 = indices.emplace_back();
				index2.V1 = static_cast<uint32_t>((r + 1) * numSegments + s + 0);
				index2.V2 = static_cast<uint32_t>((r + 1) * numSegments + s + 1);
				index2.V3 = static_cast<uint32_t>(r * numSegments + s);
			}
		}

		AssetHandle meshSource = AssetManager::CreateMemoryOnlyAsset<MeshSource>(vertices, indices, glm::mat4(1.0f));
		return AssetManager::CreateMemoryOnlyAsset<StaticMesh>(meshSource, false);
	}

}