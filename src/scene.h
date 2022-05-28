#pragma once

#include "aabb.h"

// TODO: Find a better place for this!
// NOTE: Needs to be kept in sync with shaders
#define MAX_SPHERE_LIGHTS 20
#define MAX_DISK_LIGHTS 4

struct SphereLightData
{
    vec3 position;
    vec3 radiance;
    f32 radius;
};

struct AmbientLightData
{
    vec3 radiance;
};

struct DiskLightData
{
    vec3 normal;
    vec3 position;
    vec3 radiance;
    float radius;
};

struct LightData
{
    u32 sphereLightCount;
    SphereLightData sphereLights[MAX_SPHERE_LIGHTS];
    AmbientLightData ambientLight;
    u32 diskLightCount;
    DiskLightData diskLights[MAX_DISK_LIGHTS];
};

struct Entity
{
    vec3 position;
    vec3 scale;
    quat rotation;
    u32 mesh;
    u32 material;
    vec3 aabbMin;
    vec3 aabbMax;
};

#define MAX_ENTITIES 1024

struct Scene
{
    Entity *entities;
    u32 count;
    u32 max;

    u32 backgroundMaterial;

    Aabb *meshAabbs;
    LightData *lightData;
};
