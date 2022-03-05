#pragma once

struct sp_Material
{
    vec3 albedo;
    u32 albedoTexture;

    vec3 emission;
    u32 emissionTexture;
};

// Output of sp_EvaluateMaterial
struct sp_MaterialOutput
{
    vec3 albedo;
    vec3 emission;
};

struct sp_PathVertex
{
    u32 materialId;
    vec3 worldPosition;
    vec3 outgoingDir;
    vec3 incomingDir;
    vec3 normal;
    vec2 uv;
};

#define SP_MAX_MATERIALS 32
#define SP_MAX_IMAGES 16

struct sp_MaterialSystem
{
    u32 keys[SP_MAX_MATERIALS];
    sp_Material materials[SP_MAX_MATERIALS];
    u32 count;

    u32 imageKeys[SP_MAX_IMAGES];
    HdrImage images[SP_MAX_IMAGES];
    u32 imageCount;

    u32 backgroundMaterialId;
};
