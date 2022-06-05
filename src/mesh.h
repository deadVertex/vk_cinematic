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
    f32 roughness;
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
    //Mesh_Bunny, // HACK to speed up startup time
    //Mesh_Monkey,
    Mesh_Plane,
    Mesh_Cube,
    Mesh_Triangle,
    Mesh_Sphere,
    Mesh_Disk,
    MAX_MESHES,
};

enum
{
    Material_Red,
    Material_Blue,
    Material_CheckerBoard,
    Material_White,
    Material_BlueLight,
    Material_WhiteLight,
    Material_Black,
    Material_OrangeLight,
    Material_HDRI,
    Material_WhiteR10,
    Material_WhiteR30,
    Material_WhiteR60,
    Material_WhiteR80,
    MAX_MATERIALS,
};
