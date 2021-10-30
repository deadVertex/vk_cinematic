#include "mesh.h"

#define MESH_PATH "broken"

internal MeshData LoadMesh(
    const char *path, MemoryArena *arena, const char *assetDir)
{
    MeshData result = {};

    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", assetDir, path);

    const struct aiScene *scene = aiImportFile(fullPath,
        aiProcess_CalcTangentSpace | aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices | aiProcess_SortByPType |
            aiProcess_ImproveCacheLocality);

    if (!scene)
    {
        LogMessage("Mesh import failed!\n%s\n", aiGetErrorString());
        return result;
    }

    Assert(scene->mNumMeshes > 0);
    aiMesh *mesh = scene->mMeshes[0];

    VertexPNT *vertices = AllocateArray(arena, VertexPNT, mesh->mNumVertices);
    u32 indexCount = mesh->mNumFaces * 3; // We only support triangles
    u32 *indices = AllocateArray(arena, u32, indexCount);

    // NOTE: mesh->mVertices is always present
    Assert(mesh->mNormals);

    for (u32 vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
    {
        VertexPNT *vertex = vertices + vertexIndex;
        vertex->position.x = mesh->mVertices[vertexIndex].x;
        vertex->position.y = mesh->mVertices[vertexIndex].y;
        vertex->position.z = mesh->mVertices[vertexIndex].z;

        vertex->normal.x = mesh->mNormals[vertexIndex].x;
        vertex->normal.y = mesh->mNormals[vertexIndex].y;
        vertex->normal.z = mesh->mNormals[vertexIndex].z;
    }

    for (u32 triangleIndex = 0; triangleIndex < mesh->mNumFaces; ++triangleIndex)
    {
        aiFace *face = mesh->mFaces + triangleIndex;
        indices[triangleIndex*3 + 0] = face->mIndices[0];
        indices[triangleIndex*3 + 1] = face->mIndices[1];
        indices[triangleIndex*3 + 2] = face->mIndices[2];
    }

    result.vertices = vertices;
    result.indices= indices;
    result.vertexCount = mesh->mNumVertices;
    result.indexCount = indexCount;

    aiReleaseImport(scene);

    return result;
}

internal MeshData CreatePlaneMesh(MemoryArena *arena)
{
    VertexPNT planeVertices[4] = {
        {Vec3(-0.5, -0.5, 0), Vec3(0, 0, 1), Vec2(0, 0)},
        {Vec3(0.5, -0.5, 0), Vec3(0, 0, 1), Vec2(1, 0)},
        {Vec3(0.5, 0.5, 0), Vec3(0, 0, 1), Vec2(1, 1)},
        {Vec3(-0.5, 0.5, 0), Vec3(0, 0, 1), Vec2(0, 1)}};

    u32 planeIndices[6] = {0, 1, 2, 2, 3, 0};

    MeshData meshData = {};
    meshData.vertices = (VertexPNT *)AllocateBytes(arena, sizeof(planeVertices));
    meshData.vertexCount = 4;
    CopyMemory(meshData.vertices, planeVertices, sizeof(planeVertices));

    meshData.indices = (u32 *)AllocateBytes(arena, sizeof(planeIndices));
    meshData.indexCount = 6;
    CopyMemory(meshData.indices, planeIndices, sizeof(planeIndices));

    return meshData;
}

internal MeshData CreateTriangleMeshData(MemoryArena *arena)
{
    VertexPNT triangleVertices[3] = {
        {Vec3(-0.5, -0.5, 0), Vec3(0, 0, 1), Vec2(0, 0)},
        {Vec3(0.5, -0.5, 0), Vec3(0, 0, 1), Vec2(1, 0)},
        {Vec3(0.0, 0.5, 0), Vec3(0, 0, 1), Vec2(1, 1)}
    };

    u32 triangleIndices[3] = {0, 1, 2};

    MeshData meshData = {};
    meshData.vertices = (VertexPNT *)AllocateBytes(arena, sizeof(triangleVertices));
    meshData.vertexCount = 3;
    CopyMemory(meshData.vertices, triangleVertices, sizeof(triangleVertices));

    meshData.indices = (u32 *)AllocateBytes(arena, sizeof(triangleIndices));
    meshData.indexCount = 3;
    CopyMemory(meshData.indices, triangleIndices, sizeof(triangleIndices));

    return meshData;
}

internal MeshData CreateCubeMesh(MemoryArena *arena)
{
    // clang-format off
    VertexPNT cubeVertices[] = {
        // Top
        {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},

        // Bottom
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},

        // Back
        {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
        {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},

        // Front
        {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},

        // Left
        {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},

        // Right
        {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    };

    u32 cubeIndices[] = {
        2, 1, 0,
        0, 3, 2,

        4, 5, 6,
        6, 7, 4,

        8, 9, 10,
        10, 11, 8,

        14, 13, 12,
        12, 15, 14,

        16, 17, 18,
        18, 19, 16,

        22, 21, 20,
        20, 23, 22
    };
    // clang-format on

    MeshData meshData = {};
    meshData.vertices = (VertexPNT *)AllocateBytes(arena, sizeof(cubeVertices));
    meshData.vertexCount = ArrayCount(cubeVertices);
    CopyMemory(meshData.vertices, cubeVertices, sizeof(cubeVertices));

    meshData.indices = (u32 *)AllocateBytes(arena, sizeof(cubeIndices));
    meshData.indexCount = ArrayCount(cubeIndices);
    CopyMemory(meshData.indices, cubeIndices, sizeof(cubeIndices));

    return meshData;
}

struct TriangleFace
{
    u32 i, j, k;
};

inline VertexPNT CreateUnitVertex(f32 x, f32 y, f32 z)
{
    VertexPNT result = {};
    result.position = Normalize(Vec3(x, y, z));
    result.normal = result.position;
    result.textureCoord = Vec2(result.position.x, result.position.y);
    return result;
}

// FIXME: Super inefficient with memory! Needs temp allocators? Pre-calculate
// number of vertices and indices for each tesselation level?
internal MeshData CreateIcosahedronMesh(u32 tesselationLevel, MemoryArena *arena)
{
    MeshData result = {};
    // TODO: Actually calculate how many vertices and indices
    u32 maxVertices = 2048;
    u32 maxIndices = 4096;
    result.vertices = AllocateArray(arena, VertexPNT, maxVertices);
    result.indices = AllocateArray(arena, u32, maxIndices);

    f32 t = (1.0f + Sqrt(5.0f)) * 0.5f;

    result.vertices[0] = CreateUnitVertex(-1, t, 0);
    result.vertices[1] = CreateUnitVertex(1, t, 0);
    result.vertices[2] = CreateUnitVertex(-1, -t, 0);
    result.vertices[3] = CreateUnitVertex(1, -t, 0);

    result.vertices[4] = CreateUnitVertex(0, -1, t);
    result.vertices[5] = CreateUnitVertex(0, 1, t);
    result.vertices[6] = CreateUnitVertex(0, -1, -t);
    result.vertices[7] = CreateUnitVertex(0, 1, -t);

    result.vertices[8] = CreateUnitVertex(t, 0, -1);
    result.vertices[9] = CreateUnitVertex(t, 0, 1);
    result.vertices[10] = CreateUnitVertex(-t, 0, -1);
    result.vertices[11] = CreateUnitVertex(-t, 0, 1);

    u32 vertexCount = 12;

    u32 initialFaceCount = 20;
    TriangleFace *initialFaces =
        AllocateArray(arena, TriangleFace, initialFaceCount);

    initialFaces[0] = TriangleFace{0, 11, 5};
    initialFaces[1] = TriangleFace{0, 5, 1};
    initialFaces[2] = TriangleFace{0, 1, 7};
    initialFaces[3] = TriangleFace{0, 7, 10};
    initialFaces[4] = TriangleFace{0, 10, 11};

    initialFaces[5] = TriangleFace{1, 5, 9};
    initialFaces[6] = TriangleFace{5, 11, 4};
    initialFaces[7] = TriangleFace{11, 10, 2};
    initialFaces[8] = TriangleFace{10, 7, 6};
    initialFaces[9] = TriangleFace{7, 1, 8};

    initialFaces[10] = TriangleFace{3, 9, 4};
    initialFaces[11] = TriangleFace{3, 4, 2};
    initialFaces[12] = TriangleFace{3, 2, 6};
    initialFaces[13] = TriangleFace{3, 6, 8};
    initialFaces[14] = TriangleFace{3, 8, 9};

    initialFaces[15] = TriangleFace{4, 9, 5};
    initialFaces[16] = TriangleFace{2, 4, 11};
    initialFaces[17] = TriangleFace{6, 2, 10};
    initialFaces[18] = TriangleFace{8, 6, 7};
    initialFaces[19] = TriangleFace{9, 8, 1};

    u32 currentFaceCount = initialFaceCount;
    TriangleFace *currentFaces = initialFaces;
    for (u32 i = 0; i < tesselationLevel; ++i)
    {
        TriangleFace *newFaces =
            AllocateArray(arena, TriangleFace, currentFaceCount * 4);
        u32 newFaceCount = 0;

        for (size_t j = 0; j < currentFaceCount; ++j)
        {
            TriangleFace currentFace = currentFaces[j];

            // 3 edges in total comprised of 2 indices each
            u32 edges[6] = {currentFace.i, currentFace.j, currentFace.j,
                currentFace.k, currentFace.k, currentFace.i};

            u32 indicesAdded[3];

            for (u32 k = 0; k < 3; ++k)
            {
                // Retrieve vertices for each edge
                u32 idx0 = edges[k * 2];
                u32 idx1 = edges[k * 2 + 1];
                VertexPNT v0 = result.vertices[idx0];
                VertexPNT v1 = result.vertices[idx1];

                // Add mid point vertex
                vec3 u = v0.position + v1.position;

                Assert(vertexCount < maxVertices);
                result.vertices[vertexCount] = CreateUnitVertex(u.x, u.y, u.z);

                indicesAdded[k] = vertexCount++;
            }

            newFaces[newFaceCount++] =
                TriangleFace{currentFace.i, indicesAdded[0], indicesAdded[2]};
            newFaces[newFaceCount++] =
                TriangleFace{currentFace.j, indicesAdded[1], indicesAdded[0]};
            newFaces[newFaceCount++] =
                TriangleFace{currentFace.k, indicesAdded[2], indicesAdded[1]};
            newFaces[newFaceCount++] =
                TriangleFace{indicesAdded[0], indicesAdded[1], indicesAdded[2]};
        }

        currentFaces = newFaces;
        currentFaceCount = newFaceCount;
    }

    Assert(currentFaceCount * 3 < maxIndices);
    CopyMemory(result.indices, currentFaces, currentFaceCount * sizeof(TriangleFace));

    // Also frees the allocated faces for all tesselation levels
    //MemoryArenaFree(arena, initialFaces);

    result.vertexCount = vertexCount;
    result.indexCount = currentFaceCount * 3;

    //CalculateTangents(result);

    return result;
}

