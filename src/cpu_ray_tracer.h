#pragma once

#include "platform.h"
#include "intrinsics.h"
#include "math_lib.h"
#include "mesh.h"
#include "debug.h"
#include "scene.h"
#include "tile.h"

#include "asset_loader/asset_loader.h"
#include "image.h"

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
    b32 useSmoothShading;
};

struct RayTracer
{
    mat4 viewMatrix;
    DebugDrawingBuffer *debugDrawBuffer;
    b32 useAccelerationStructure;
    u32 maxDepth;
    RayTracerMesh meshes[MAX_MESHES];
    Material materials[MAX_MATERIALS];
    AabbTree aabbTree;

    HdrImage image;
    HdrImage checkerBoardImage;
};

struct RayHitResult
{
    b32 isValid;
    f32 t;
    vec3 normal;
    vec3 localPoint;
    vec3 scenePoint;
    u32 depth;
    u32 materialIndex;
    vec2 uv;
};

enum
{
    CycleCount_Broadphase,
    CycleCount_BuildModelMatrices,
    CycleCount_TransformRay,
    CycleCount_RayIntersectMesh,
    CycleCount_Midphase,
    CycleCount_TriangleIntersect,
    CycleCount_ConsumeMidPhaseResults,
    MAX_CYCLE_COUNTS,
};

struct LocalMetrics
{
    i64 meshTestCount;
    i64 midphaseAabbTestCount;
    i64 cycleCounts[MAX_CYCLE_COUNTS];
};

struct Metrics
{
    i64 rayCount;
    i64 sampleCount;
    i64 pixelCount;
    i64 cycleCount;

    i64 meshTestCount;
    i64 midphaseAabbTestCount;

    i64 cycleCounts[MAX_CYCLE_COUNTS];

#if 0
    // Old
    i64 broadphaseCycleCount;
    i64 buildModelMatricesCycleCount;
    i64 transformRayCycleCount;
    i64 rayIntersectMeshCycleCount;

    i64 broadPhaseTestCount;
    i64 broadPhaseHitCount;

    i64 midPhaseTestCount;
    i64 midPhaseHitCount;

    i64 triangleTestCount;
    i64 triangleHitCount;

    i64 meshTestCount;
    i64 meshHitCount;
#endif
};

global Metrics g_Metrics;

struct ThreadData
{
    u32 width;
    u32 height;
    vec4 *imageBuffer;
    RayTracer *rayTracer;
    Scene *scene;
};

struct Task
{
    ThreadData *threadData;
    Tile tile;
};

struct PathVertex
{
    vec3 incomingDirection;
    vec3 outgoingDirection;
    vec3 surfaceNormal;
    vec3 surfacePointLocal;
    vec3 surfacePointScene;
    vec2 uv;
    u32 materialIndex;
};
