#include <cstdarg>

#include "unity.h"

#include "platform.h"
#include "math_lib.h"
#include "memory_pool.h"
#include "bvh.h"
#include "sp_metrics.h"
#include "sp_scene.h"
#include "sp_material_system.h"
#include "tile.h"
#include "simd_path_tracer.h"

#include "simd.h"
#include "aabb.h"

#include "memory_pool.cpp"
#include "ray_intersection.cpp"
#include "bvh.cpp"
#include "sp_scene.cpp"
#include "sp_material_system.cpp"
#include "simd_path_tracer.cpp"

#include "mesh.h"
#include "mesh_generation.cpp"

#define MEMORY_ARENA_SIZE Megabytes(256)

MemoryArena memoryArena;

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
// The test is intended to simulate broadphase testing?
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
    u64 start = __rdtsc();
    bvh_Tree tree =
        bvh_CreateTree(&memoryArena, aabbMin, aabbMax, ArrayCount(aabbMin));
    LogMessage("bvh_CreateTree cyles elapsed: %llu", __rdtsc() - start);

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
    start = __rdtsc();
    for (u32 i = 0; i < RAY_COUNT; i++)
    {
        bvh_Node *intersectedNodes[64] = {};

        bvh_IntersectRayResult result = bvh_IntersectRay(&tree, rayOrigin[i],
                rayDirection[i], intersectedNodes, ArrayCount(intersectedNodes));
    }

    // Count avg number of cycles per intersection test, rays per second, etc
    u64 cyclesElapsed = __rdtsc() - start;
    LogMessage("Total cycles elapsed: %llu", cyclesElapsed);
    TEST_ASSERT_LESS_THAN_UINT64(36000000, cyclesElapsed);
    u64 cyclesPerRay = cyclesElapsed / (u64)RAY_COUNT;
    TEST_ASSERT_LESS_THAN_UINT64(4000, cyclesPerRay);
    LogMessage("Cycles per ray: %llu", cyclesPerRay);
}

void TestPathTraceTile()
{
#define IMAGE_WIDTH 4
#define IMAGE_HEIGHT 4
    vec4 pixels[IMAGE_WIDTH*IMAGE_HEIGHT] = {};
    ImagePlane imagePlane = {};
    imagePlane.width = IMAGE_WIDTH;
    imagePlane.height = IMAGE_HEIGHT;
    imagePlane.pixels = pixels;

    sp_Camera camera = {};
    vec3 cameraPosition = Vec3(0, 0, 10);
    quat cameraRotation = Quat();
    f32 filmDistance = 0.1f;
    sp_ConfigureCamera(
        &camera, &imagePlane, cameraPosition, cameraRotation, filmDistance);

    sp_Scene scene = {};
    sp_InitializeScene(&scene, &memoryArena);

    MeshData meshData = CreateIcosahedronMesh(3, &memoryArena);

    vec3 *vertices = AllocateArray(&memoryArena, vec3, meshData.vertexCount);
    for (u32 i = 0; i < meshData.vertexCount; i++)
    {
        vertices[i] = meshData.vertices[i].position;
    }

    sp_Mesh mesh = sp_CreateMesh(
        vertices, meshData.vertexCount, meshData.indices, meshData.indexCount);
    // FIXME: Need to build mesh midphase bvh tree!

    sp_MaterialSystem materialSystem = {};
    materialSystem.backgroundEmission = Vec3(1);

    sp_Material material = {};
    material.albedo = Vec3(0.18);

    u32 materialId = 123;
    sp_RegisterMaterial(&materialSystem, material, materialId);

    sp_AddObjectToScene(
        &scene, mesh, materialId, Vec3(0, -5, 0), Quat(), Vec3(1000));
    sp_BuildSceneBroadphase(&scene);

    // Create SIMD Path tracer
    sp_Context context = {};
    context.camera = &camera;
    context.scene = &scene;
    context.materialSystem = &materialSystem;

    RandomNumberGenerator rng = {};

    Tile tile = {};
    tile.minX = 0;
    tile.minY = 0;
    tile.maxX = IMAGE_WIDTH;
    tile.maxY = IMAGE_HEIGHT;

    sp_Metrics metrics = {};

    u64 start = __rdtsc();
    sp_PathTraceTile(&context, tile, &rng, &metrics);
    u64 cyclesElapsed =  __rdtsc() - start;
    LogMessage("sp_PathTraceTile: Cycles elapsed: %llu", cyclesElapsed);
    u64 cyclesPerPixel =
        cyclesElapsed / (u64)(imagePlane.width * imagePlane.height);
    LogMessage("Cycles per pixel : %llu", cyclesPerPixel);
}

struct EvaluateTreeResult
{
    u32 minDepth;
    u32 maxDepth;
};

internal EvaluateTreeResult EvaluateTree(bvh_Node *node, u32 currentDepth)
{
    EvaluateTreeResult result = {};
    if (node->children[0] != NULL)
    {
        // Assuming that this is always true?
        Assert(node->children[1] != NULL);
        EvaluateTreeResult a =
            EvaluateTree(node->children[0], currentDepth + 1);
        EvaluateTreeResult b =
            EvaluateTree(node->children[1], currentDepth + 1);
        result.minDepth = MinU32(a.minDepth, b.minDepth);
        result.maxDepth = MaxU32(a.maxDepth, b.maxDepth);
    }
    else
    {
        // Leaf node
        result.minDepth = currentDepth + 1;
        result.maxDepth = currentDepth + 1;
    }

    return result;
}

void TestMeshMidphase()
{
    RandomNumberGenerator rng = { 0x1A34C249 };

    MeshData meshData = CreateIcosahedronMesh(3, &memoryArena);

    vec3 *vertices = AllocateArray(&memoryArena, vec3, meshData.vertexCount);
    for (u32 i = 0; i < meshData.vertexCount; i++)
    {
        vertices[i] = meshData.vertices[i].position;
    }

    sp_Mesh mesh = sp_CreateMesh(
        vertices, meshData.vertexCount, meshData.indices, meshData.indexCount);
    MemoryArena bvhNodeArena = SubAllocateArena(&memoryArena, Megabytes(1));
    MemoryArena tempArena = SubAllocateArena(&memoryArena, Kilobytes(64));

    // TODO: Measure how long this takes
    sp_BuildMeshMidphase(&mesh, &bvhNodeArena, &tempArena);

    EvaluateTreeResult treeResult = EvaluateTree(mesh.midphaseTree.root, 0);
    LogMessage("Midphase tree maxDepth: %u minDepth: %u",
            treeResult.maxDepth, treeResult.minDepth);
    u32 triangleCount = mesh.indexCount / 3;
    LogMessage("Midphase tree leaf node count: %u", triangleCount);

    vec3 min = mesh.midphaseTree.root->min;
    vec3 max = mesh.midphaseTree.root->max;

    sp_Metrics metrics = {};

    u32 bucketMax = 250;
    u32 bucketCount = 10;
    u32 bucketWidth = bucketMax / bucketCount;
    u32 buckets[10] = {};

    u32 rayCount = 1024*8192;
    u64 cyclesElapsed = 0;
    u32 hitCount = 0;
    u32 missCount = 0;
    for (u32 ray = 0; ray < rayCount; ++ray)
    {
        f32 x0 = Lerp(min.x, max.x, RandomBilateral(&rng));
        f32 y0 = Lerp(min.y, max.y, RandomBilateral(&rng));
        f32 z0 = Lerp(min.z, max.z, RandomBilateral(&rng));

        vec3 p = Vec3(x0, y0, z0);

        f32 x1 = Lerp(min.x, max.x, RandomBilateral(&rng));
        f32 y1 = Lerp(min.y, max.y, RandomBilateral(&rng));
        f32 z1 = Lerp(min.z, max.z, RandomBilateral(&rng));

        vec3 q = Vec3(x1, y1, z1);

        vec3 rayOrigin = p;
        vec3 rayDirection = Normalize(q - p);

        u64 prev =
            metrics.values[sp_Metric_RayIntersectMesh_MidphaseAabbTestCount];

        u64 start = __rdtsc();

        RayIntersectTriangleResult result =
            sp_RayIntersectMesh(mesh, rayOrigin, rayDirection, &metrics);

        cyclesElapsed += __rdtsc() - start;

        hitCount = (result.t >= 0.0f) ? hitCount + 1 : hitCount;
        missCount = (result.t < 0.0f) ? missCount + 1 : missCount;

        u32 aabbCount = (u32)(
            metrics.values[sp_Metric_RayIntersectMesh_MidphaseAabbTestCount] -
            prev);
        u32 bucketIndex = MinU32(aabbCount / bucketWidth, bucketCount - 1);
        buckets[bucketIndex]++;
    }

    LogMessage("Cycles elapsed %llu (%.8g per ray)",
            cyclesElapsed, (f64)cyclesElapsed / (f64)rayCount);
    LogMessage("Midphase cycles elapsed: %llu (%.8g per ray)",
        metrics.values[sp_Metric_CyclesElapsed_RayIntersectMeshMidphase],
        (f64)metrics.values[sp_Metric_CyclesElapsed_RayIntersectMeshMidphase] /
            (f64)rayCount);
    LogMessage("RayIntersectTriangle cycles elapsed: %llu (%.8g per ray)",
        metrics.values[sp_Metric_CyclesElapsed_RayIntersectTriangle],
        (f64)metrics.values[sp_Metric_CyclesElapsed_RayIntersectTriangle] /
            (f64)rayCount);
    LogMessage("Midphase AABB test count: %llu (%.8g per ray)",
        metrics.values[sp_Metric_RayIntersectMesh_MidphaseAabbTestCount],
        (f64)metrics.values[sp_Metric_RayIntersectMesh_MidphaseAabbTestCount] /
            (f64)rayCount);
    LogMessage("Ray hit count: %u", hitCount);
    LogMessage("Ray miss count: %u", missCount);
    LogMessage("Histogram of midphase AABB test counts");
    for (u32 i = 0; i < bucketCount; ++i)
    {
        u32 start = bucketWidth * i;
        u32 end = start + bucketWidth;
        LogMessage("%u - %u : %u", start, end, buckets[i]);
    }
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
    RUN_TEST(TestPathTraceTile);
    RUN_TEST(TestMeshMidphase);

    free(memoryArena.base);

    return UNITY_END();
}
