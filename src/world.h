#pragma once

struct Entity
{
    vec3 position;
    vec3 scale;
    quat rotation;
    u32 mesh;
};

#define MAX_ENTITIES 64

struct World
{
    Entity entities[MAX_ENTITIES];
    u32 count;
};
