#include <cstdarg>

#include "unity.h"

#include "platform.h"
#include "math_lib.h"
#include "memory_pool.h"
#include "bvh.h"

#include "memory_pool.cpp"
#include "ray_intersection.cpp"
#include "bvh.cpp"

#define MEMORY_ARENA_SIZE Megabytes(256)

MemoryArena memoryArena;

global const char *g_AssetDir = "../assets";

void setUp(void)
{
    // set stuff up here
    ClearToZero(memoryArena.base, (u32)memoryArena.size);
    ResetMemoryArena(&memoryArena);
}

void tearDown(void)
{
    // clean stuff up here
}

// Performance tests
void TestBvh()
{
#define AABB_COUNT 2048
    vec3 aabbMin[AABB_COUNT];
    vec3 aabbMax[AABB_COUNT];

    RandomNumberGenerator rng = { 0x1A34C249 };

    f32 s = 2000.0f;
    for (u32 i = 0; i < AABB_COUNT; i++)
    {
        vec3 center = Vec3(s * RandomBilateral(&rng), s * RandomBilateral(&rng),
            s * RandomBilateral(&rng));

        f32 radius = 500.0f * RandomBilateral(&rng);
        aabbMin[i] = center - Vec3(radius);
        aabbMax[i] = center + Vec3(radius);
    }

    // Build BVH tree with lots of leaves say 1024?
    bvh_Tree tree =
        bvh_CreateTree(&memoryArena, aabbMin, aabbMax, ArrayCount(aabbMin));

    vec3 min = tree.root->min;
    vec3 max = tree.root->max;

    // Generate random rays within the AABB of the BVH tree
#define RAY_COUNT 8192
    vec3 rayOrigin[RAY_COUNT];
    vec3 rayDirection[RAY_COUNT];
    for (u32 i = 0; i < RAY_COUNT; i++)
    {
        f32 x0 = Lerp(min.x, max.x, RandomBilateral(&rng));
        f32 y0 = Lerp(min.y, max.y, RandomBilateral(&rng));
        f32 z0 = Lerp(min.z, max.z, RandomBilateral(&rng));

        vec3 p = Vec3(x0, y0, z0);

        f32 x1 = Lerp(min.x, max.x, RandomBilateral(&rng));
        f32 y1 = Lerp(min.y, max.y, RandomBilateral(&rng));
        f32 z1 = Lerp(min.z, max.z, RandomBilateral(&rng));

        vec3 q = Vec3(x1, y1, z1);

        rayOrigin[i] = p;
        rayDirection[i] = Normalize(q - p);
    }

    // Perform ray intersection tests
    u64 start = __rdtsc();
    for (u32 i = 0; i < RAY_COUNT; i++)
    {
        bvh_Node *intersectedNodes[64] = {};

        bvh_IntersectRayResult result = bvh_IntersectRay(&tree, rayOrigin[i],
                rayDirection[i], intersectedNodes, ArrayCount(intersectedNodes));
    }

    // Count avg number of cycles per intersection test, rays per second, etc
    u64 cyclesElapsed = __rdtsc() - start;
    LogMessage("Total cycles elapsed: %llu", cyclesElapsed);
    u64 cyclesPerRay = cyclesElapsed / (u64)RAY_COUNT;
    LogMessage("Cycles per ray: %llu", cyclesPerRay);
}

// FIXME: Copied from main.cpp
internal DebugLogMessage(LogMessage_)
{
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    puts(buffer);
}

int main(int argc, char **argv)
{
    InitializeMemoryArena(
        &memoryArena, calloc(1, MEMORY_ARENA_SIZE), MEMORY_ARENA_SIZE);

    LogMessage = LogMessage_;

    UNITY_BEGIN();
    RUN_TEST(TestBvh);

    free(memoryArena.base);

    return UNITY_END();
}
