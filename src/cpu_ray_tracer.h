#pragma once

#define RAY_TRACER_WIDTH (1024 / 4)
#define RAY_TRACER_HEIGHT (768 / 4)

#define MAX_AABB_TREE_NODES 0x10000

// TODO: Need to properly clusterize our triangles for this to not completely
// destroy our perf
#define MESHLET_SIZE 1

// TODO: This should be a parameter of the tree structure for iteration
#define STACK_SIZE 256

// TODO: Probably want multiple triangles per leaf node
struct AabbTreeNode
{
    vec3 min;
    vec3 max;
    AabbTreeNode *children[2];
    u32 leafIndex;
};

struct AabbTree
{
    AabbTreeNode *root;
    AabbTreeNode *nodes;
    u32 count;
    u32 max;
};

struct RayTracerMesh
{
    AabbTree aabbTree;
    MeshData meshData;
};

struct RayTracer
{
    mat4 viewMatrix;
    DebugDrawingBuffer *debugDrawBuffer;
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

    u64 triangleTestCount;
    u64 triangleHitCount;

    u64 meshTestCount;
    u64 meshHitCount;

    u64 rayCount;
    u64 totalSampleCount;
    u64 totalPixelCount;

    u64 broadphaseCycleCount;
    u64 buildModelMatricesCycleCount;
    u64 transformRayCycleCount;
    u64 rayIntersectMeshCycleCount;
    u64 rayTraceSceneCycleCount;
};

global Metrics g_Metrics;

