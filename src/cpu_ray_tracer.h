#pragma once

#define RAY_TRACER_WIDTH (1024 / 4)
#define RAY_TRACER_HEIGHT (768 / 4)

#define MAX_BVH_NODES 0x10000

// TODO: Probably want multiple triangles per leaf node
struct BvhNode
{
    vec3 min;
    vec3 max;
    BvhNode *children[2];
    u32 triangleIndex;
};

struct RayTracerMesh
{
    BvhNode *root;
    MeshData meshData;
};

#define MAX_AABB_TREE_NODES 2048

struct AabbTreeNode
{
    vec3 min;
    vec3 max;
    AabbTreeNode *children[2];
    u32 entityIndex;
};

struct AabbTree
{
    AabbTreeNode *root;
    AabbTreeNode *nodes;
    u32 count;
    u32 max;
};

struct RayTracer
{
    mat4 viewMatrix;
    DebugDrawingBuffer *debugDrawBuffer;
    BvhNode *nodes;
    u32 nodeCount;
    b32 useAccelerationStructure;
    u32 maxDepth;
    RayTracerMesh meshes[MAX_MESHES];
    RandomNumberGenerator rng;
    AabbTree aabbTree;
};

struct RayHitResult
{
    b32 isValid;
    f32 t;
    vec3 normal;
    u32 depth;
};

struct Metrics
{
    u64 broadPhaseTestCount;
    u64 broadPhaseHitCount;
    u64 midPhaseTestCount;
    u64 midPhaseHitCount;
    u64 aabbTestCount;
    u64 triangleTestCount;
    u64 rayCount;
    u64 totalSampleCount;
    u64 totalPixelCount;
    u64 triangleHitCount;
};

global Metrics g_Metrics;

