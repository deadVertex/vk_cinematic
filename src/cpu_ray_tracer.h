#pragma once

#include "platform.h"
#include "math_lib.h"
#include "mesh.h"
#include "debug.h"
#include "config.h"
#include "world.h"

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
    vec3 worldPoint;
    u32 depth;
    u32 materialIndex;
    vec2 uv;
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
    vec4 *imageBuffer;
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
    vec3 surfacePointLocal;
    vec3 surfacePointWorld;
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

inline vec4 SampleImageBilinear(HdrImage image, vec2 uv)
{
    // Compute pixel coordinates to sample
    f32 px = (uv.x * image.width) - 0.5f;
    f32 py = (uv.y * image.height) - 0.5f;

    u32 x0 = (u32)Floor(px);
    u32 x1 = x0 + 1;

    u32 y0 = (u32)Floor(py);
    u32 y1 = y0 + 1;

    f32 fx = px - (f32)x0;
    f32 fy = py - (f32)y0;

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
