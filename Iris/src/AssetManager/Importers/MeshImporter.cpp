#include "IrisPCH.h"
#include "MeshImporter.h"

#include "AssetManager/AssetManager.h"
#include "ImGui/Themes.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Renderer.h"
#include "Renderer/Shaders/Shader.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"
#include "Utils/AssimpLogStream.h"
#include "Utils/TextureImporter.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace Iris {

	namespace Utils {

		glm::mat4 Mat4FromAIMat4(const aiMatrix4x4& matrix)
		{
			glm::mat4 result;
			result[0][0] = static_cast<float>(matrix.a1); result[1][0] = static_cast<float>(matrix.a2); result[2][0] = static_cast<float>(matrix.a3); result[3][0] = static_cast<float>(matrix.a4);
			result[0][1] = static_cast<float>(matrix.b1); result[1][1] = static_cast<float>(matrix.b2); result[2][1] = static_cast<float>(matrix.b3); result[3][1] = static_cast<float>(matrix.b4);
			result[0][2] = static_cast<float>(matrix.c1); result[1][2] = static_cast<float>(matrix.c2); result[2][2] = static_cast<float>(matrix.c3); result[3][2] = static_cast<float>(matrix.c4);
			result[0][3] = static_cast<float>(matrix.d1); result[1][3] = static_cast<float>(matrix.d2); result[2][3] = static_cast<float>(matrix.d3); result[3][3] = static_cast<float>(matrix.d4);
		
			return result;
		}

	}

	static const uint32_t s_MeshImporterFlags =
		aiProcess_CalcTangentSpace      |  // Create tangent and binormal just in case they are not generated
		aiProcess_Triangulate           |  // Make sure the format of vertices is triangles
		aiProcess_SortByPType           |  // Split meshes by primitive types
		aiProcess_GenNormals            |  // Double check normals
		aiProcess_GenUVCoords           |  // Generate UV coords if not generated
		// aiProcess_OptimizeGraph      |
		aiProcess_OptimizeMeshes        |  // minimize draws where possible
		aiProcess_JoinIdenticalVertices |
		aiProcess_LimitBoneWeights      |  // If more than N (= 4) bond weights, discard least influencing bones and renormalize to 1
		aiProcess_ValidateDataStructure |  // Validation
		aiProcess_GlobalScale;			   // e.g. convert cm to m for fbx import (and other formats where cm is native)
		
	AssimpMeshImporter::AssimpMeshImporter(const std::string& assetPath)
		: m_AssetPath(assetPath)
	{
		AssimpLogStream::Init();
	}

	Ref<MeshSource> AssimpMeshImporter::ImportToMeshSource()
	{
		Ref<MeshSource> meshSource = MeshSource::Create();

		IR_CORE_INFO_TAG("Mesh", "Loading mesh: {0}", m_AssetPath);

		Assimp::Importer importer;
		importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);

		const aiScene* scene = importer.ReadFile(m_AssetPath.string(), s_MeshImporterFlags);
		if (!scene)
		{
			IR_CORE_ERROR_TAG("Mesh", "Failed to load mesh file: {0}", m_AssetPath);
			meshSource->SetFlag(AssetFlag::Invalid);
			return nullptr;
		}

		// Meshes
		if (scene->HasMeshes())
		{
			uint32_t vertexCount = 0;
			uint32_t indexCount = 0;

			meshSource->m_BoundingBox.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
			meshSource->m_BoundingBox.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

			// TODO:
			// const std::vector<uint32_t>& subMeshIndices = StaticMesh::GetCurrentlyLoadingMeshSourceIndices();
			// bool loadAllSubMeshes = subMeshIndices.empty();
			// IR_VERIFY(subMeshIndices.size() <= scene->mNumMeshes); // TODO: Or equal?

			//meshSource->m_SubMeshes.reserve(scene->mNumMeshes);
			for (uint32_t m = 0; m < scene->mNumMeshes; m++)
			{
				// TODO: Skip if we do not want to load it... TODO: Optimize?
				// if (!loadAllSubMeshes && std::find(subMeshIndices.begin(), subMeshIndices.end(), m) == subMeshIndices.end())
				// 	continue;

				aiMesh* mesh = scene->mMeshes[m];

				if (!mesh->HasPositions())
				{
					IR_CORE_ERROR_TAG("Mesh", "Mesh index {0} with name `{1}` has no vertex positions... skipping import!", m, mesh->mName.C_Str());
				}
				if (!mesh->HasNormals())
				{
					IR_CORE_ERROR_TAG("Mesh", "Mesh index {0} with name `{1}` has no vertex normals, and they could not be computed... skipping import!", m, mesh->mName.C_Str());
				}

				bool skip = !mesh->HasPositions() || !mesh->HasNormals();

				// We still have to create a submesh even if we have to skip so that the TraverseNodes function works...
				MeshUtils::SubMesh& subMesh = meshSource->m_SubMeshes.emplace_back();
				subMesh = {
					.BaseVertex = vertexCount,
					.VertexCount = skip ? 0 : mesh->mNumVertices,
					.BaseIndex = indexCount,
					.IndexCount = skip ? 0 : mesh->mNumFaces * 3,
					.MaterialIndex = mesh->mMaterialIndex,
					.MeshName = mesh->mName.C_Str()
				};

				if (skip)
					continue;

				vertexCount += mesh->mNumVertices;
				indexCount += mesh->mNumFaces * 3;

				// Vertices...
				AABB& aabb = subMesh.BoundingBox;
				aabb.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
				aabb.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
				for (std::size_t i = 0; i < mesh->mNumVertices; i++)
				{
					MeshUtils::Vertex vertex;
					vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
					vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

					if (mesh->HasTangentsAndBitangents())
					{
						vertex.Tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
						vertex.Binormal = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
					}

					if (mesh->HasTextureCoords(0))
					{
						vertex.TexCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
					}

					aabb.Min.x = glm::min(vertex.Position.x, aabb.Min.x);
					aabb.Min.y = glm::min(vertex.Position.y, aabb.Min.y);
					aabb.Min.z = glm::min(vertex.Position.z, aabb.Min.z);
					aabb.Max.x = glm::max(vertex.Position.x, aabb.Max.x);
					aabb.Max.y = glm::max(vertex.Position.y, aabb.Max.y);
					aabb.Max.z = glm::max(vertex.Position.z, aabb.Max.z);

					meshSource->m_Vertices.push_back(vertex);
				}

				// Indices...
				for (std::size_t i = 0; i < mesh->mNumFaces; i++)
				{
					IR_ASSERT(mesh->mFaces[i].mNumIndices == 3, "Must have 3 indices since we are using aiProcess_Triangulate");
					MeshUtils::Index index = { mesh->mFaces[i].mIndices[0], mesh->mFaces[i].mIndices[1], mesh->mFaces[i].mIndices[2] };
					meshSource->m_Indices.push_back(index);

					meshSource->m_TriangleCache[m].emplace_back(meshSource->m_Vertices[index.V1 + subMesh.BaseVertex], meshSource->m_Vertices[index.V2 + subMesh.BaseVertex], meshSource->m_Vertices[index.V3 + subMesh.BaseVertex]);
				}
			}

			meshSource->m_Nodes.emplace_back();
			TraverseNodes(meshSource, scene->mRootNode, 0);

			for (const MeshUtils::SubMesh& subMesh : meshSource->m_SubMeshes)
			{
				AABB transformedSubMeshAABB = subMesh.BoundingBox;
				glm::vec3 min = glm::vec3(subMesh.Transform * glm::vec4(transformedSubMeshAABB.Min, 1.0f));
				glm::vec3 max = glm::vec3(subMesh.Transform * glm::vec4(transformedSubMeshAABB.Max, 1.0f));

				meshSource->m_BoundingBox.Min.x = glm::min(meshSource->m_BoundingBox.Min.x, min.x);
				meshSource->m_BoundingBox.Min.y = glm::min(meshSource->m_BoundingBox.Min.y, min.y);
				meshSource->m_BoundingBox.Min.z = glm::min(meshSource->m_BoundingBox.Min.z, min.z);

				meshSource->m_BoundingBox.Max.x = glm::max(meshSource->m_BoundingBox.Max.x, max.x);
				meshSource->m_BoundingBox.Max.y = glm::max(meshSource->m_BoundingBox.Max.y, max.y);
				meshSource->m_BoundingBox.Max.z = glm::max(meshSource->m_BoundingBox.Max.z, max.z);
			}
		}

		// Materials
		Ref<Texture2D> whiteTexture = Renderer::GetWhiteTexture();
		if (scene->HasMaterials())
		{
			IR_CORE_TRACE_TAG("Mesh", "----- Materials - {0} -----", m_AssetPath);

			meshSource->m_Materials.resize(scene->mNumMaterials);
			for (uint32_t i = 0; i < scene->mNumMaterials; i++)
			{
				aiMaterial* aiMaterial = scene->mMaterials[i];
				aiString aiMaterialName = aiMaterial->GetName();
				Ref<Material> material = Material::Create(Renderer::GetShadersLibrary()->Get("IrisPBRStatic"), aiMaterialName.C_Str());

				AssetHandle materialAssetHandle = AssetManager::CreateMemoryOnlyAsset<MaterialAsset>(material);
				meshSource->m_Materials[i] = materialAssetHandle;

				Ref<MaterialAsset> ma = AssetManager::GetAsset<MaterialAsset>(materialAssetHandle);

				IR_CORE_TRACE_TAG("Mesh", "\t   {0} (index = {1})", aiMaterialName.data, i);

				aiString aiTexPath;

				// Albedo
				glm::vec3 albedoColor = glm::vec3{ 0.8f };
				float emission = 0.0f;
				aiColor3D aiColor, aiEmission;
				if (aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == AI_SUCCESS)
					albedoColor = { aiColor.r, aiColor.g, aiColor.b };

				if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, aiColor) == AI_SUCCESS)
					emission = static_cast<float>(aiEmission.r);

				ma->SetAlbedoColor(albedoColor);
				ma->SetEmission(emission);

				float roughness, metalness;
				if (aiMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) != AI_SUCCESS)
					roughness = 0.5f; // Default value

				// NOTE: Here maybe we should check for AI_MATKEY_REFLECTIVITY?
				if (aiMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metalness) != AI_SUCCESS)
					metalness = 0.0f; // Default value

				// Physically realistic materials are either metals or they are not
				metalness = (metalness < 0.9f) ? 0.0f : 1.0f;

				IR_CORE_TRACE_TAG("Mesh", "\t   COLOR = {0} - {1} - {2}", aiColor.r, aiColor.g, aiColor.b);
				IR_CORE_TRACE_TAG("Mesh", "\t   ROUGHNESS = {0}", roughness);
				IR_CORE_TRACE_TAG("Mesh", "\t   METALNESS = {0}", metalness);

				// Albedo maps
				bool hasAlbedoMap = aiMaterial->GetTexture(AI_MATKEY_BASE_COLOR_TEXTURE, &aiTexPath) == AI_SUCCESS;
				if (!hasAlbedoMap)
				{
					// no PBR color, try old diffuse
					hasAlbedoMap = aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &aiTexPath) == AI_SUCCESS;
				}

				if (hasAlbedoMap)
				{
					AssetHandle textureHandle = 0;
					TextureSpecification spec = {
						.DebugName = aiTexPath.C_Str(),
						.Format = ImageFormat::SRGBA
					};

					if (auto aiTexEmbedded = scene->GetEmbeddedTexture(aiTexPath.C_Str()))
					{
						spec.Width = aiTexEmbedded->mWidth;
						spec.Height = aiTexEmbedded->mHeight;

						textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(reinterpret_cast<uint8_t*>(aiTexEmbedded->pcData), 1));
					}
					else
					{
						auto parentPath = m_AssetPath.parent_path();
						auto texturePath = parentPath / aiTexPath.C_Str();
						if (!FileSystem::Exists(texturePath))
						{
							IR_CORE_WARN_TAG("Mesh", "\t   Albedo map path: {0} --> NOT FOUND!", texturePath);
							texturePath = parentPath / texturePath.filename();
						}
						IR_CORE_TRACE_TAG("Mesh", "\t   Albedo map path: {0}{1}", texturePath, FileSystem::Exists(texturePath) ? "" : "--> NOT FOUND!");
						
						textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, texturePath);
					}

					ma->SetAlbedoMap(textureHandle);
					ma->SetAlbedoColor(glm::vec3{ 1.0f });
				}

				// Normal maps
				bool hasNormalMap = aiMaterial->GetTexture(aiTextureType_NORMALS, 0, &aiTexPath) == AI_SUCCESS;
				if (hasNormalMap)
				{
					AssetHandle textureHandle = 0;
					TextureSpecification spec = {
						.DebugName = aiTexPath.C_Str(),
						.Format = ImageFormat::RGBA
					};

					if (auto aiTexEmbedded = scene->GetEmbeddedTexture(aiTexPath.C_Str()))
					{
						spec.Width = aiTexEmbedded->mWidth;
						spec.Height = aiTexEmbedded->mHeight;

						textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(reinterpret_cast<uint8_t*>(aiTexEmbedded->pcData), 1));
					}
					else
					{
						auto parentPath = m_AssetPath.parent_path();
						auto texturePath = parentPath / aiTexPath.C_Str();
						if (!FileSystem::Exists(texturePath))
						{
							IR_CORE_WARN_TAG("Mesh", "\t   Normal map path: {0} --> NOT FOUND!", texturePath);
							texturePath = parentPath / texturePath.filename();
						}
						IR_CORE_TRACE_TAG("Mesh", "\t   Normal map path: {0}{1}", texturePath, FileSystem::Exists(texturePath) ? "" : "--> NOT FOUND!");

						textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, texturePath);
					}

					ma->SetNormalMap(textureHandle);
					// NOTE: Needs to be false if we were not able to load the normal map?
					ma->SetUseNormalMap(true);
				}

				// Roughness maps
				bool hasRoughnessMap = aiMaterial->GetTexture(AI_MATKEY_ROUGHNESS_TEXTURE, &aiTexPath) == AI_SUCCESS;
				bool invertRoughness = false;
				if (!hasRoughnessMap)
				{
					// no PBR roughness, try to find shininess and then roughness = (1 - shininess)
					hasRoughnessMap = aiMaterial->GetTexture(aiTextureType_SHININESS, 0, &aiTexPath) == AI_SUCCESS;
					invertRoughness = true;
				}

				if (hasRoughnessMap)
				{
					AssetHandle textureHandle = 0;
					TextureSpecification spec = {
						.DebugName = aiTexPath.C_Str(),
						.Format = ImageFormat::RGBA
					};

					if (auto aiTexEmbedded = scene->GetEmbeddedTexture(aiTexPath.C_Str()))
					{
						spec.Width = aiTexEmbedded->mWidth;
						spec.Height = aiTexEmbedded->mHeight;

						aiTexel* texels = aiTexEmbedded->pcData;
						if (invertRoughness)
						{
							if (spec.Height == 0)
							{
								auto buffer = Utils::TextureImporter::LoadImageFromMemory(Buffer(reinterpret_cast<uint8_t*>(aiTexEmbedded->pcData), spec.Width), spec.Format, spec.Width, spec.Height);
								texels = reinterpret_cast<aiTexel*>(buffer.Data);
							}

							for (uint32_t i = 0; i < spec.Width * spec.Height; i++)
							{
								aiTexel& texel = texels[i];
								texel.r = 255 - texel.r;
								texel.g = 255 - texel.g;
								texel.b = 255 - texel.b;
							}
						}

						textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(reinterpret_cast<uint8_t*>(texels), 1));
					}
					else
					{
						auto parentPath = m_AssetPath.parent_path();
						auto texturePath = parentPath / aiTexPath.C_Str();
						if (!FileSystem::Exists(texturePath))
						{
							IR_CORE_WARN_TAG("Mesh", "\t   Roughness map path: {0} --> NOT FOUND!", texturePath);
							texturePath = parentPath / texturePath.filename();
						}
						IR_CORE_TRACE_TAG("Mesh", "\t   Roughness map path: {0}{1}", texturePath, FileSystem::Exists(texturePath) ? "" : "--> NOT FOUND!");

						Buffer buffer = Utils::TextureImporter::LoadImageFromFile(texturePath.string(), spec.Format, spec.Width, spec.Height);

						aiTexel* texels = reinterpret_cast<aiTexel*>(buffer.Data);
						if (invertRoughness)
						{
							for (uint32_t i = 0; i < spec.Width * spec.Height; i++)
							{
								aiTexel& texel = texels[i];
								texel.r = 255 - texel.r;
								texel.g = 255 - texel.g;
								texel.b = 255 - texel.b;
							}
						}

						textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, buffer);
					}

					ma->SetRoughnessMap(textureHandle);
					ma->SetRoughness(1.0f);
				}

				// Metalness maps
				bool hasMetalnessMap = aiMaterial->GetTexture(AI_MATKEY_METALLIC_TEXTURE, &aiTexPath) == AI_SUCCESS;
				if (hasMetalnessMap)
				{
					AssetHandle textureHandle = 0;
					TextureSpecification spec = {
						.DebugName = aiTexPath.C_Str(),
						.Format = ImageFormat::RGBA
					};

					if (auto aiTexEmbedded = scene->GetEmbeddedTexture(aiTexPath.C_Str()))
					{
						spec.Width = aiTexEmbedded->mWidth;
						spec.Height = aiTexEmbedded->mHeight;

						textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(reinterpret_cast<uint8_t*>(aiTexEmbedded->pcData), 1));
					}
					else
					{
						auto parentPath = m_AssetPath.parent_path();
						auto texturePath = parentPath / aiTexPath.C_Str();
						if (!FileSystem::Exists(texturePath))
						{
							IR_CORE_WARN_TAG("Mesh", "\t   Metalness map path: {0} --> NOT FOUND!", texturePath);
							texturePath = parentPath / texturePath.filename();
						}
						IR_CORE_TRACE_TAG("Mesh", "\t   Metalness map path: {0}{1}", texturePath, FileSystem::Exists(texturePath) ? "" : "--> NOT FOUND!");

						textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, texturePath);
					}

					ma->SetMetalnessMap(textureHandle);
					ma->SetMetalness(1.0f);
				}

				ma->SetLit();
			}

			IR_CORE_TRACE_TAG("Mesh", "---------------------------");
		}
		else
		{
			if (scene->HasMeshes())
			{
				Ref<Material> material = Material::Create(Renderer::GetShadersLibrary()->Get("IrisPBRStatic"), "IrisDefault");
				AssetHandle materialAssetHandle = AssetManager::CreateMemoryOnlyAsset<MaterialAsset>(material);
				meshSource->m_Materials.push_back(materialAssetHandle);
			}
		}

		if (meshSource->m_Vertices.size())
			meshSource->m_VertexBuffer = VertexBuffer::Create(meshSource->m_Vertices.data(), static_cast<uint32_t>(meshSource->m_Vertices.size() * sizeof(MeshUtils::Vertex)));

		if (meshSource->m_Indices.size())
			meshSource->m_IndexBuffer = IndexBuffer::Create(meshSource->m_Indices.data(), static_cast<uint32_t>(meshSource->m_Indices.size() * sizeof(MeshUtils::Index)));

		return meshSource;
	}

	void AssimpMeshImporter::TraverseNodes(Ref<MeshSource> meshSource, void* assimpNode, uint32_t nodeIndex, const glm::mat4& parentTransform, uint32_t level)
	{
		aiNode* aNode = reinterpret_cast<aiNode*>(assimpNode);

		MeshUtils::MeshNode& node = meshSource->m_Nodes[nodeIndex];
		node.Name = aNode->mName.C_Str();
		node.LocalTransform = Utils::Mat4FromAIMat4(aNode->mTransformation);

		glm::mat4 transform = parentTransform * node.LocalTransform;
		for (uint32_t i = 0; i < aNode->mNumMeshes; i++)
		{
			uint32_t submeshIndex = aNode->mMeshes[i];
			auto& submesh = meshSource->m_SubMeshes[submeshIndex];
			submesh.NodeName = aNode->mName.C_Str();
			submesh.Transform = transform;
			submesh.LocalTransform = node.LocalTransform;

			node.SubMeshes.push_back(submeshIndex);
		}

		uint32_t parentNodeIndex = (uint32_t)meshSource->m_Nodes.size() - 1;
		node.Children.resize(aNode->mNumChildren);
		for (uint32_t i = 0; i < aNode->mNumChildren; i++)
		{
			MeshUtils::MeshNode& child = meshSource->m_Nodes.emplace_back();
			uint32_t childIndex = static_cast<uint32_t>(meshSource->m_Nodes.size()) - 1;
			child.Parent = parentNodeIndex;
			meshSource->m_Nodes[nodeIndex].Children[i] = childIndex;
			TraverseNodes(meshSource, aNode->mChildren[i], childIndex, transform, level + 1);
		}
	}

}
