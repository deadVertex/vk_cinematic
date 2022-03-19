#pragma once

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

struct Material
{
    vec3 baseColor;
    vec3 emission;
};
#pragma pack(pop)

struct MeshData
{
    VertexPNT *vertices;
    u32 *indices;
    u32 vertexCount;
    u32 indexCount;
};

enum
{
    Mesh_Bunny,
    Mesh_Monkey,
    Mesh_Plane,
    Mesh_Cube,
    Mesh_Triangle,
    Mesh_Sphere,
    MAX_MESHES,
};

enum
{
    Material_Red,
    Material_Blue,
    Material_Background,
    Material_CheckerBoard,
    Material_White,
    Material_BlueLight,
    Material_WhiteLight,
    Material_Black,
    MAX_MATERIALS,
};
