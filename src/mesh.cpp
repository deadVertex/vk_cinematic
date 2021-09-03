#include "mesh.h"

internal MeshData LoadMesh(const char *path, MemoryArena *arena)
{
    MeshData result = {};

    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", MESH_PATH, path);

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

