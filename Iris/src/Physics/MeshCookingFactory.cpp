#include "IrisPCH.h"
#include "MeshCookingFactory.h"

#include "AssetManager/AssetManager.h"
#include "BinaryStream.h"
#include "Physics.h"

#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

namespace Iris {

    namespace Utils {

        static std::filesystem::path GetCacheDirectory()
        {
            return Project::GetAssetDirectory() / "Colliders";
        }

        static void CreateCacheDirectoryIfNeeded()
        {
            std::filesystem::path cacheDirectory = GetCacheDirectory();
            if (!std::filesystem::exists(cacheDirectory))
                std::filesystem::create_directories(cacheDirectory);
        }
    }

    std::pair<PhysicsCookingResult, PhysicsCookingResult> MeshCookingFactory::CookMesh(AssetHandle meshColliderAssetHandle, bool invalidateOld)
    {
        return CookMesh(AssetManager::GetAsset<MeshColliderAsset>(meshColliderAssetHandle), invalidateOld);
    }

    std::pair<PhysicsCookingResult, PhysicsCookingResult> MeshCookingFactory::CookMesh(Ref<MeshColliderAsset> meshColliderAsset, bool invalidateOld)
    {
        /*
         * TODO: Here we should not really serialize those meshes colliders
         */
        
        Timer timer;

        Utils::CreateCacheDirectoryIfNeeded();

        AssetHandle colliderHandle = meshColliderAsset->Handle;

        if (!AssetManager::IsAssetHandleValid(colliderHandle))
        {
            IR_CORE_ERROR_TAG("Physics", "Tried to cook mesh collider using an invalid mesh!");
            return { PhysicsCookingResult::InvalidMesh, PhysicsCookingResult::InvalidMesh };
        }

        Ref<StaticMesh> mesh = AssetManager::GetAsset<StaticMesh>(meshColliderAsset->ColliderMesh);

        if (!mesh)
        {
            IR_CORE_ERROR_TAG("Physics", "Tried to cook mesh collider using an invalid mesh!");
            return { PhysicsCookingResult::InvalidMesh, PhysicsCookingResult::InvalidMesh };
        }

        IR_ASSERT(mesh->GetAssetType() == AssetType::StaticMesh);

        std::string baseFileName = fmt::format("Mesh-{0}", colliderHandle);
        const bool isPhysicalAsset = !AssetManager::IsMemoryAsset(colliderHandle);

        std::filesystem::path simpleColliderFilePath = Utils::GetCacheDirectory() / fmt::format("{0}-simple.IJmc", baseFileName);
        std::filesystem::path complexColliderFilePath = Utils::GetCacheDirectory() / fmt::format("{0}-complex.IJmc", baseFileName);

        CachedColliderData colliderData;
        PhysicsCookingResult simpleMeshResult = PhysicsCookingResult::Failure;
        PhysicsCookingResult complexMeshResult = PhysicsCookingResult::Failure;

        Ref<MeshSource> meshSource = AssetManager::GetAsset<MeshSource>(mesh->GetMeshSource());
        if (meshSource)
        {
            const std::vector<uint32_t>& subMeshIndices = mesh->GetSubMeshes();

            // Cook or load the simple collider
            if (invalidateOld || !std::filesystem::exists(simpleColliderFilePath))
            {
                simpleMeshResult = CookConvexMesh(meshColliderAsset, meshSource, subMeshIndices, colliderData.SimpleColliderData);

                if (simpleMeshResult == PhysicsCookingResult::Success && !SerializeMeshCollider(simpleColliderFilePath, colliderData.SimpleColliderData))
                {
                    IR_CORE_ERROR_TAG("Physics", "Failed to cook simple collider mesh.");
                    simpleMeshResult = PhysicsCookingResult::Failure;
                }
            }
           else
           {
               colliderData.SimpleColliderData = DeserializeMeshCollider(simpleColliderFilePath);
               simpleMeshResult = PhysicsCookingResult::Success;
           }

            if (simpleMeshResult == PhysicsCookingResult::Success)
                GenerateDebugMesh(meshColliderAsset, subMeshIndices, colliderData.SimpleColliderData);

            // Cook or load the complex collider
            if (invalidateOld || !std::filesystem::exists(complexColliderFilePath))
            {
                complexMeshResult = CookTriangleMesh(meshColliderAsset, meshSource, subMeshIndices, colliderData.ComplexColliderData);

                if (complexMeshResult == PhysicsCookingResult::Success && !SerializeMeshCollider(complexColliderFilePath, colliderData.ComplexColliderData))
                {
                    IR_CORE_ERROR_TAG("Physics", "Failed to cook complex collider mesh");
                    complexMeshResult = PhysicsCookingResult::Failure;
                }
            }
            else
            {
                colliderData.ComplexColliderData = DeserializeMeshCollider(complexColliderFilePath);
                complexMeshResult = PhysicsCookingResult::Success;
            }

            if (complexMeshResult == PhysicsCookingResult::Success)
                GenerateDebugMesh(meshColliderAsset, subMeshIndices, colliderData.ComplexColliderData);

            if (simpleMeshResult == PhysicsCookingResult::Success || complexMeshResult == PhysicsCookingResult::Success)
            {
                // Add to cache
                Ref<MeshColliderCache>& meshCache = PhysicsSystem::GetMesheColliderCache();
                if (isPhysicalAsset)
                    meshCache->m_MeshData[meshColliderAsset->ColliderMesh][colliderHandle] = colliderData;
                else
                    meshCache->m_MeshData[meshColliderAsset->ColliderMesh][0] = colliderData;
            }
        }

        IR_CORE_DEBUG("CookingFactory::CookMesh took {}ms ", timer.ElapsedMillis());

        return { simpleMeshResult, complexMeshResult };
    }

    PhysicsCookingResult MeshCookingFactory::CookConvexMesh(const Ref<MeshColliderAsset>& meshColliderAsset, const Ref<MeshSource>& meshSource, const std::vector<uint32_t>& subMeshIndices, MeshColliderData& outData)
    {
        PhysicsCookingResult cookingResult = PhysicsCookingResult::Failure;

        const std::vector<MeshUtils::Vertex>& vertices = meshSource->GetVertices();
        const std::vector<MeshUtils::Index>& indices = meshSource->GetIndices();
        const std::vector<MeshUtils::SubMesh>& subMeshes = meshSource->GetSubMeshes();

        for (uint32_t subMeshIndex : subMeshIndices)
        {
            const MeshUtils::SubMesh& subMesh = subMeshes[subMeshIndex];

            if (subMesh.VertexCount < 3)
            {
                outData.SubMeshes.emplace_back();
                continue;
            }

            JPH::Array<JPH::Vec3> positions;

            for (uint32_t i = subMesh.BaseIndex / 3; i < (subMesh.BaseIndex / 3) + (subMesh.IndexCount / 3); i++)
            {
                const MeshUtils::Index& vertexIndex = indices[i];
                const MeshUtils::Vertex& v0 = vertices[vertexIndex.V1];
                positions.push_back(JPH::Vec3(v0.Position.x, v0.Position.y, v0.Position.z));

                const MeshUtils::Vertex& v1 = vertices[vertexIndex.V2];
                positions.push_back(JPH::Vec3(v1.Position.x, v1.Position.y, v1.Position.z));

                const MeshUtils::Vertex& v2 = vertices[vertexIndex.V3];
                positions.push_back(JPH::Vec3(v2.Position.x, v2.Position.y, v2.Position.z));
            }

            JPH::RefConst<JPH::ConvexHullShapeSettings> meshSettings = new JPH::ConvexHullShapeSettings(positions);
            JPH::Shape::ShapeResult result = meshSettings->Create();

            if (result.HasError())
            {
                IR_CORE_ERROR_TAG("Physics", "Failed to cook convex mesh {}. Error: {}", subMesh.MeshName, result.GetError());

                for (SubMeshColliderData& existingSubMesh : outData.SubMeshes)
                    existingSubMesh.ColliderData.Release();

                outData.SubMeshes.clear();

                cookingResult = PhysicsCookingResult::Failure;
                break;
            }

            JPH::RefConst<JPH::Shape> shape = result.Get();

            JoltBinaryStreamWriter joltBufferWriter;
            shape->SaveBinaryState(joltBufferWriter);

            SubMeshColliderData& data = outData.SubMeshes.emplace_back();
            data.ColliderData = joltBufferWriter.ToBuffer();
            data.Transform = subMesh.Transform * glm::scale(glm::mat4(1.0f), meshColliderAsset->ColliderScale);
            cookingResult = PhysicsCookingResult::Success;
        }

        outData.Type = MeshColliderType::Convex;

        return cookingResult;
    }

    PhysicsCookingResult MeshCookingFactory::CookTriangleMesh(const Ref<MeshColliderAsset>& meshColliderAsset, const Ref<MeshSource>& meshSource, const std::vector<uint32_t>& subMeshIndices, MeshColliderData& outData)
    {
        PhysicsCookingResult cookingResult = PhysicsCookingResult::Failure;

        const std::vector<MeshUtils::Vertex>& vertices = meshSource->GetVertices();
        const std::vector<MeshUtils::Index>& indices = meshSource->GetIndices();
        const std::vector<MeshUtils::SubMesh>& subMeshes = meshSource->GetSubMeshes();

        for (uint32_t subMeshIndex : subMeshIndices)
        {
            const MeshUtils::SubMesh& subMesh = subMeshes[subMeshIndex];

            if (subMesh.VertexCount < 3)
            {
                outData.SubMeshes.emplace_back();
                continue;
            }

            JPH::VertexList vertexList;
            JPH::IndexedTriangleList triangleList;

            for (uint32_t vertexIndex = subMesh.BaseVertex; vertexIndex < subMesh.BaseVertex + subMesh.VertexCount; vertexIndex++)
            {
                const MeshUtils::Vertex& v = vertices[vertexIndex];
                vertexList.push_back(JPH::Float3(v.Position.x, v.Position.y, v.Position.z));
            }

            for (uint32_t triangleIndex = subMesh.BaseIndex / 3; triangleIndex < (subMesh.BaseIndex / 3) + (subMesh.IndexCount / 3); triangleIndex++)
            {
                const MeshUtils::Index& i = indices[triangleIndex];
                triangleList.push_back(JPH::IndexedTriangle(i.V1, i.V2, i.V3, 0));
            }

            JPH::RefConst<JPH::MeshShapeSettings> meshSettings = new JPH::MeshShapeSettings(vertexList, triangleList);
            JPH::Shape::ShapeResult result = meshSettings->Create();

            if (result.HasError())
            {
                IR_CORE_ERROR_TAG("Physics", "Failed to cook triangle mesh {}. Error: {}", subMesh.MeshName, result.GetError());

                for (SubMeshColliderData& existingSubMesh : outData.SubMeshes)
                    existingSubMesh.ColliderData.Release();

                outData.SubMeshes.clear();

                cookingResult = PhysicsCookingResult::Failure;
                break;
            }

            JPH::RefConst<JPH::Shape> shape = result.Get();

            JoltBinaryStreamWriter joltBufferWriter;
            shape->SaveBinaryState(joltBufferWriter);

            SubMeshColliderData& data = outData.SubMeshes.emplace_back();
            data.ColliderData = joltBufferWriter.ToBuffer();
            data.Transform = subMesh.Transform * glm::scale(glm::mat4(1.0f), meshColliderAsset->ColliderScale);
            cookingResult = PhysicsCookingResult::Success;
        }

        outData.Type = MeshColliderType::Triangle;

        return cookingResult;
    }

    void MeshCookingFactory::GenerateDebugMesh(const Ref<MeshColliderAsset>& meshColliderAsset, const std::vector<uint32_t>& subMeshIndices, const MeshColliderData& colliderData)
    {
        std::vector<MeshUtils::Vertex> vertices;
        std::vector<MeshUtils::Index> indices;
        std::vector<MeshUtils::SubMesh> subMeshes;

        for (std::size_t i = 0; i < colliderData.SubMeshes.size(); i++)
        {
            const SubMeshColliderData& subMeshData = colliderData.SubMeshes[i];

            // retrieve the shape by restoring from the binary data
            JoltBinaryStreamReader bufferReader(subMeshData.ColliderData);
            JPH::Shape::ShapeResult shapeResult = JPH::Shape::sRestoreFromBinaryState(bufferReader);
            if (!shapeResult.HasError())
            {
                JPH::Ref<JPH::Shape> shape = shapeResult.Get();

                glm::vec3 centerOfMass = Utils::FromJoltVec3(shape->GetCenterOfMass());
                JPH::Shape::VisitedShapes ioVisitedShapes;
                JPH::Shape::Stats stats = shape->GetStatsRecursive(ioVisitedShapes);

                if (stats.mNumTriangles > 0)
                {
                    MeshUtils::SubMesh& subMesh = subMeshes.emplace_back();
                    subMesh.BaseVertex = static_cast<uint32_t>(vertices.size());
                    subMesh.VertexCount = stats.mNumTriangles * 3;
                    subMesh.BaseIndex = static_cast<uint32_t>(indices.size()) * 3;
                    subMesh.IndexCount = stats.mNumTriangles * 3; // There are three indices per Iris::MeshUtils::Index
                    subMesh.MaterialIndex = 0;
                    subMesh.Transform = subMeshData.Transform;
                    vertices.reserve(vertices.size() + stats.mNumTriangles * 3);
                    indices.reserve(indices.size() + stats.mNumTriangles);

                    JPH::Shape::GetTrianglesContext ioContext;

                    // NOTE: Here we have to initialize the Bounding Box we want to provide for the triangle mesh to cull against. For convex meshes the physics engine uses the
                    // provided AABB to cull against however Concave shapes do not come with a default shape.
                    JPH::AABox inBox;
                    if (colliderData.Type == MeshColliderType::Triangle)
                    {
                        inBox = shape->GetLocalBounds();
                        inBox.Translate(shape->GetCenterOfMass());
                    }
                    JPH::Vec3 inPositionCOM = JPH::Vec3::sZero();
                    JPH::Quat inRotation = JPH::Quat::sIdentity();
                    JPH::Vec3 inScale = JPH::Vec3::sReplicate(1.0f);
                    shape->GetTrianglesStart(ioContext, inBox, inPositionCOM, inRotation, inScale);

                    JPH::Float3 jphVertices[100 * 3];
                    int count = 0;
                    while ((count = shape->GetTrianglesNext(ioContext, 100, jphVertices)) > 0)
                    {
                        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
                        for (int i = 0; i < count; ++i)
                        {
                            MeshUtils::Vertex& v1 = vertices.emplace_back();
                            v1.Position = { jphVertices[3 * i].x, jphVertices[3 * i].y, jphVertices[3 * i].z };
                            v1.Position += centerOfMass;

                            MeshUtils::Vertex& v2 = vertices.emplace_back();
                            v2.Position = { jphVertices[3 * i + 1].x, jphVertices[3 * i + 1].y, jphVertices[3 * i + 1].z };
                            v2.Position += centerOfMass;

                            MeshUtils::Vertex& v3 = vertices.emplace_back();
                            v3.Position = { jphVertices[3 * i + 2].x, jphVertices[3 * i + 2].y, jphVertices[3 * i + 2].z };
                            v3.Position += centerOfMass;

                            MeshUtils::Index& index = indices.emplace_back();
                            index.V1 = static_cast<uint32_t>(baseIndex + 3 * i);
                            index.V2 = static_cast<uint32_t>(baseIndex + 3 * i + 1);
                            index.V3 = static_cast<uint32_t>(baseIndex + 3 * i + 2);
                        }
                    }
                }
            }
        }

        if (vertices.size() > 0)
        {
            Ref<MeshSource> meshSource = MeshSource::Create(vertices, indices, subMeshes);
            AssetManager::AddMemoryOnlyAsset(meshSource);

            if (colliderData.Type == MeshColliderType::Convex)
            {
                PhysicsSystem::GetMesheColliderCache()->AddSimpleDebugStaticMesh(meshColliderAsset, StaticMesh::Create(meshSource->Handle, subMeshIndices, false));
            }
            else
            {
                PhysicsSystem::GetMesheColliderCache()->AddComplexDebugStaticMesh(meshColliderAsset, StaticMesh::Create(meshSource->Handle, subMeshIndices, false));
            }
        }
    }

    bool MeshCookingFactory::SerializeMeshCollider(const std::filesystem::path& filepath, MeshColliderData& meshData)
    {
        IrisPhysicsMeshHeader Imc;
        Imc.Type = meshData.Type;
        Imc.SubMeshCount = static_cast<uint32_t>(meshData.SubMeshes.size());

        std::ofstream stream(filepath, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            stream.close();
            IR_CORE_ERROR_TAG("Physics", "Failed to write collider to {0}", filepath.string());
            for (SubMeshColliderData& submesh : meshData.SubMeshes)
                submesh.ColliderData.Release();
            meshData.SubMeshes.clear();
            return false;
        }

        stream.write(reinterpret_cast<char*>(&Imc), sizeof(IrisPhysicsMeshHeader));
        for (SubMeshColliderData& submesh : meshData.SubMeshes)
        {
            stream.write(reinterpret_cast<char*>(glm::value_ptr(submesh.Transform)), sizeof(submesh.Transform));
            stream.write(reinterpret_cast<char*>(&submesh.ColliderData.Size), sizeof(submesh.ColliderData.Size));
            stream.write(reinterpret_cast<char*>(submesh.ColliderData.Data), submesh.ColliderData.Size);
        }

        return true;
    }

    MeshColliderData MeshCookingFactory::DeserializeMeshCollider(const std::filesystem::path& filepath)
    {
        Buffer colliderBuffer = FileSystem::ReadBytes(filepath);
        IrisPhysicsMeshHeader& Imc = *reinterpret_cast<IrisPhysicsMeshHeader*>(colliderBuffer.Data);
        IR_VERIFY(strcmp(Imc.Header, IrisPhysicsMeshHeader().Header) == 0);

        MeshColliderData meshData;
        meshData.Type = Imc.Type;
        meshData.SubMeshes.resize(Imc.SubMeshCount);

        uint8_t* buffer = reinterpret_cast<uint8_t*>(colliderBuffer.Data) + sizeof(IrisPhysicsMeshHeader);
        for (uint32_t i = 0; i < Imc.SubMeshCount; i++)
        {
            SubMeshColliderData& subMeshData = meshData.SubMeshes[i];

            // Transform
            memcpy(glm::value_ptr(subMeshData.Transform), buffer, sizeof(glm::mat4));
            buffer += sizeof(glm::mat4);

            // Data
            auto size = subMeshData.ColliderData.Size = *reinterpret_cast<uint64_t*>(buffer);
            buffer += sizeof(size);
            subMeshData.ColliderData = Buffer::Copy(buffer, size);
            buffer += size;
        }

        colliderBuffer.Release();
        return meshData;
    }

}