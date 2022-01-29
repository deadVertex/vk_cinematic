#pragma once

struct sp_Mesh
{
    vec3 *vertices; // Where dis memory at?
    u32 *indices;
    u32 vertexCount;
    u32 indexCount;
};

// TODO: Switch to dynamic arrays in the future
#define SP_SCENE_MAX_OBJECTS 32
struct sp_Scene
{
    // TODO: We could build the AABB arrays in temp memory in BuildBroadphaseTree
    vec3 aabbMin[SP_SCENE_MAX_OBJECTS];
    vec3 aabbMax[SP_SCENE_MAX_OBJECTS];
    sp_Mesh meshes[SP_SCENE_MAX_OBJECTS];
    mat4 invModelMatrices[SP_SCENE_MAX_OBJECTS];
    mat4 modelMatrices[SP_SCENE_MAX_OBJECTS];
    u32 objectCount;

    MemoryArena memoryArena;
    bvh_Tree broadphaseTree;
};

struct sp_RayIntersectSceneResult
{
    f32 t;
};
