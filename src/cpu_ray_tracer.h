#pragma once

#include "platform.h"
#include "math_lib.h"
#include "mesh.h"
#include "debug.h"
#include "config.h"
#include "world.h"

#define RAY_TRACER_WIDTH (1024 / 4)
#define RAY_TRACER_HEIGHT (768 / 4)

#define MAX_AABB_TREE_NODES 0x10000

// TODO: Need to properly clusterize our triangles for this to not completely
// destroy our perf
#define MESHLET_SIZE 1

// TODO: This should be a parameter of the tree structure for iteration
#define STACK_SIZE 256

// TODO: This shouldn't be here
struct HdrImage
{
    f32 *pixels;
    u32 width;
    u32 height;
};

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
    Material materials[MAX_MATERIALS];
    AabbTree aabbTree;

    HdrImage image;
};

struct RayHitResult
{
    b32 isValid;
    f32 t;
    vec3 normal;
    u32 depth;
    u32 materialIndex;
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

struct Tile
{
    u32 minX;
    u32 minY;
    u32 maxX;
    u32 maxY;
};

inline u32 ComputeTiles(u32 totalWidth, u32 totalHeight, u32 tileWidth,
    u32 tileHeight, Tile *tiles, u32 maxTiles)
{
    u32 tileCountY = (u32)Ceil((f32)totalHeight / (f32)tileHeight);
    u32 tileCountX = (u32)Ceil((f32)totalWidth / (f32)tileWidth);

    for (u32 tileY = 0; tileY < tileCountY; ++tileY)
    {
        for (u32 tileX = 0; tileX < tileCountX; ++tileX)
        {
            u32 index = tileX + tileY * tileCountX;
            if (index >= maxTiles)
            {
                break;
            }

            u32 x = tileX * tileWidth;
            u32 y = tileY * tileHeight;

            Tile tile;
            tile.minX = tileX * tileWidth;
            tile.minY = tileY * tileHeight;
            tile.maxX = MinU32(tile.minX + tileWidth, totalWidth);
            tile.maxY = MinU32(tile.minY + tileHeight, totalHeight);

            tiles[index] = tile;
        }
    }

    u32 totalTileCount = MinU32(tileCountY * tileCountX, maxTiles);
    return totalTileCount;
}

struct ThreadData
{
    u32 width;
    u32 height;
    u32 *imageBuffer;
    RayTracer *rayTracer;
    World *world;
};

struct Task
{
    ThreadData *threadData;
    Tile tile;
};

#define MAX_TASKS 256

struct WorkQueue
{
    i32 volatile head;
    i32 volatile tail;
    Task tasks[MAX_TASKS];
};

inline Task WorkQueuePop(WorkQueue *queue)
{
    Assert(queue->head != queue->tail);
    //u32 index = queue->head++;
#ifdef PLATFORM_WINDOWS
    // FIXME: This is windows specific, need our intrinsics header
    i32 index = _InterlockedExchangeAdd((volatile long *)&queue->head, 1);
#elif defined(PLATFORM_LINUX)
    // TODO: Probably not the right memory model - https://gcc.gnu.org/wiki/Atomic/GCCMM/AtomicSync
    i32 index = __atomic_fetch_add(&queue->head, 1, __ATOMIC_SEQ_CST);
#else
#error "UNSUPPORTED PLATFORM"
#endif
    return queue->tasks[index];
}

struct PathVertex
{
    vec3 incomingDirection;
    vec3 outgoingDirection;
    vec3 surfaceNormal;
    u32 materialIndex;
};

inline vec4 SampleImage(HdrImage image, vec2 v)
{
    f32 fx = v.x * image.width;
    f32 fy = v.y * image.height;

    u32 x = (u32)Floor(fx);
    u32 y = (u32)Floor(fy);

    Assert(x <= image.width);
    Assert(y <= image.height);

    vec4 sample = *(((vec4 *)image.pixels) + (y * image.width + x));

    vec4 result = sample;
    return result;
}

