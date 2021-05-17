#include "mesh.h"

// TODO: Copy raw mesh data into vertexUploadBuffer for vulkan renderer
// TODO: Copy into some sort of triangle mesh structure for our CPU ray tracer
internal MeshData LoadMesh()
{
    MeshData result = {};

    const struct aiScene *scene = aiImportFile("../assets/bunny.obj",
        aiProcess_CalcTangentSpace | aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);

    if (!scene)
    {
        LogMessage("Mesh import failed!\n%s\n", aiGetErrorString());
        return result;
    }

    Assert(scene->mNumMeshes > 0);
    aiMesh *mesh = scene->mMeshes[0];

    // TODO: Replace with AllocateMemory?
    VertexPNT *vertices = (VertexPNT *)calloc(mesh->mNumVertices, sizeof(VertexPNT));
    u32 indexCount = mesh->mNumFaces * 3; // We only support triangles
    u32 *indices = (u32 *)calloc(indexCount, sizeof(u32));

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

