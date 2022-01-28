#pragma once

struct CollisionMesh
{
    vec3 *vertices; // Where dis memory at?
    u32 *indices;
    u32 vertexCount;
    u32 indexCount;
};

// TODO: Switch to dynamic arrays in the future
#define COLLISION_WORLD_MAX_OBJECTS 32
struct CollisionWorld
{
    // TODO: We could build the AABB arrays in temp memory in BuildBroadphaseTree
    vec3 aabbMin[COLLISION_WORLD_MAX_OBJECTS];
    vec3 aabbMax[COLLISION_WORLD_MAX_OBJECTS];
    CollisionMesh collisionMeshes[COLLISION_WORLD_MAX_OBJECTS];
    mat4 invModelMatrices[COLLISION_WORLD_MAX_OBJECTS];
    mat4 modelMatrices[COLLISION_WORLD_MAX_OBJECTS];
    u32 objectCount;

    MemoryArena memoryArena;
    bvh_Tree broadphaseTree;
};

struct RayIntersectCollisionWorldResult
{
    f32 t;
};
