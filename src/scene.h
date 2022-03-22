#pragma once

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
};
