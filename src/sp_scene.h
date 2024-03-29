#pragma once

struct sp_Mesh
{
    VertexPNT *vertices; // Where dis memory at?
    u32 *indices;
    u32 vertexCount;
    u32 indexCount;

    bvh_Tree midphaseTree;
    b32 useSmoothShading;
};

// TODO: Switch to dynamic arrays in the future
#define SP_SCENE_MAX_OBJECTS 32
struct sp_Scene
{
    // TODO: We could build the AABB arrays in temp memory in BuildBroadphaseTree
    vec3 aabbMin[SP_SCENE_MAX_OBJECTS];
    vec3 aabbMax[SP_SCENE_MAX_OBJECTS];
    sp_Mesh meshes[SP_SCENE_MAX_OBJECTS];
    u32 materials[SP_SCENE_MAX_OBJECTS];
    mat4 invModelMatrices[SP_SCENE_MAX_OBJECTS];
    mat4 modelMatrices[SP_SCENE_MAX_OBJECTS];
    u32 objectCount;

    MemoryArena memoryArena;
    bvh_Tree broadphaseTree;
};

struct sp_RayIntersectMeshResult
{
    RayIntersectTriangleResult triangleIntersection;
#if SP_DEBUG_MIDPHASE_INTERSECTION_COUNT
    u32 midphaseIntersectionCount;
#endif
};

struct sp_RayIntersectSceneResult
{
    f32 t;
    u32 materialId;
    vec3 normal;
    vec2 uv;

#if SP_DEBUG_BROADPHASE_INTERSECTION_COUNT
    u32 broadphaseIntersectionCount;
#endif

#if SP_DEBUG_MIDPHASE_INTERSECTION_COUNT
    u32 midphaseIntersectionCount;
#endif
};
