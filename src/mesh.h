#pragma once

#define MESH_PATH "../assets"

#pragma pack(push)
struct VertexPC
{
    vec3 position;
    vec3 color;
};
struct VertexPNT
{
    vec3 position;
    vec3 normal;
    vec2 textureCoord;
};
#pragma pack(pop)

struct MeshData
{
    VertexPNT *vertices;
    u32 *indices;
    u32 vertexCount;
    u32 indexCount;
};

