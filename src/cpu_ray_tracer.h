#pragma once

#include "platform.h"
#include "intrinsics.h"
#include "math_lib.h"
#include "mesh.h"
#include "debug.h"
#include "config.h"
#include "scene.h"
#include "tile.h"

#include "asset_loader/asset_loader.h"

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
    i32 index = AtomicExchangeAdd(&queue->head, 1);
    return queue->tasks[index];
}

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

inline vec4 SampleImageNearest(HdrImage image, vec2 v)
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

inline vec4 GetPixel(HdrImage image, u32 x, u32 y)
{
    // TODO: Wrapping modes?
    Assert(x < image.width);
    Assert(y < image.height);

    u32 index = y * image.width + x;

    vec4 *pixels = (vec4 *)image.pixels;
    vec4 result = pixels[index];
    return result;
}

// TODO: Would be nice not to do 2 stages of clamping
inline vec4 SampleImageBilinear(HdrImage image, vec2 uv)
{
    Assert(uv.x >= 0.0f && uv.x <= 1.0f);
    Assert(uv.y >= 0.0f && uv.y <= 1.0f);

    // Compute pixel coordinates to sample
    f32 px = (uv.x * image.width) - 0.5f;
    f32 py = (uv.y * image.height) - 0.5f;

    // Clamp sample coords to valid range i.e. ClampToEdge wrapping mode
    px = Max(px, 0.0f);
    py = Max(py, 0.0f);

    u32 x0 = (u32)Floor(px);
    u32 x1 = x0 + 1;

    u32 y0 = (u32)Floor(py);
    u32 y1 = y0 + 1;

    f32 fx = px - (f32)x0;
    f32 fy = py - (f32)y0;

    // Clamp sample coords to valid range i.e. ClampToEdge wrapping mode
    x1 = MinU32(x1, image.width - 1);
    y1 = MinU32(y1, image.height - 1);

    // Sample 4 pixels
    vec4 samples[4];
    samples[0] = GetPixel(image, x0, y0);
    samples[1] = GetPixel(image, x1, y0);
    samples[2] = GetPixel(image, x0, y1);
    samples[3] = GetPixel(image, x1, y1);

    // Blend them together
    vec4 t0 = Lerp(samples[0], samples[1], fx);
    vec4 t1 = Lerp(samples[2], samples[3], fx);

    vec4 result = Lerp(t0, t1, fy);
    return result;
}

// NOTE: dir must be normalized
// TODO: Nothing uses this because it hard-codes the sampler method to nearest
inline vec4 SampleEquirectangularImage(HdrImage image, vec3 sampleDir)
{
    vec2 sphereCoords = ToSphericalCoordinates(sampleDir);
    vec2 uv = MapToEquirectangular(sphereCoords);
    //uv.y = 1.0f - uv.y; // Flip Y axis as usual
    vec4 sample = SampleImageNearest(image, uv);
    return sample;
}

// TODO: Don't assume 4 color channels
inline HdrImage AllocateImage(u32 width, u32 height, MemoryArena *arena)
{
    HdrImage image = {};
    image.width = width;
    image.height = height;
    image.pixels = AllocateArray(arena, f32, image.width * image.height * 4);

    return image;
}

inline void SetPixel(HdrImage *image, u32 x, u32 y, vec4 color)
{
    Assert(x < image->width);
    Assert(y < image->height);

    vec4 *pixels = (vec4 *)image->pixels;
    u32 index = y * image->width + x;
    pixels[index] = color;
}
