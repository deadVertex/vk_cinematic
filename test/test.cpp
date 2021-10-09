#include <cstdarg>

#include "unity.h"

// For integration tests
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#if 0
#include "platform.h"
#include "math_lib.h"
#include "mesh.h"
#include "profiler.h"
#include "cpu_ray_tracer.cpp"
#include "mesh.cpp"
#endif

#include "cpu_ray_tracer.h"

#include "cmdline.cpp"
#include "mesh.cpp"

void setUp(void)
{
    // set stuff up here
}

void tearDown(void)
{
    // clean stuff up here
}

#if 0
void test_repro_bug()
{
    MeshData bunnyMesh = LoadMesh();
    RayTracer rayTracer = {};
    rayTracer.nodes = (BvhNode *)malloc(sizeof(BvhNode) * MAX_BVH_NODES);
    rayTracer.meshData = bunnyMesh;
    rayTracer.useAccelerationStructure = true;
    BuildBvh(&rayTracer, bunnyMesh);

    vec3 rotation = Vec3(-0.145001, 0.25, 0);
    quat cameraRotation =
        Quat(Vec3(0, 1, 0), rotation.y) * Quat(Vec3(1, 0, 0), rotation.x);
    rayTracer.viewMatrix =
        Translate(Vec3(0.0141059, 0.106304, 0.163337)) * Rotate(cameraRotation);

    u32 width = RAY_TRACER_WIDTH;
    u32 height = RAY_TRACER_HEIGHT;

    CameraConstants camera =
        CalculateCameraConstants(rayTracer.viewMatrix, width, height);

    u32 x = 423 / 16;
    u32 y = 541 / 16;

    vec3 filmP = CalculateFilmP(&camera, width, height, x, y);

    vec3 rayOrigin = camera.cameraPosition;
    vec3 rayDirection = Normalize(filmP - camera.cameraPosition);

    RayHitResult rayHit =
        TraceRayThroughScene(&rayTracer, rayOrigin, rayDirection);

    TEST_ASSERT_TRUE(rayHit.isValid);
}
#endif

void TestComputeTiles()
{
    Tile tiles[64] = {};
    u32 tileCount =
        ComputeTiles(10, 10, 2, 2, tiles, ArrayCount(tiles));

    TEST_ASSERT_EQUAL_UINT32(tiles[0].minX, 0);
    TEST_ASSERT_EQUAL_UINT32(tiles[0].minY, 0);
    TEST_ASSERT_EQUAL_UINT32(tiles[0].maxX, 2);
    TEST_ASSERT_EQUAL_UINT32(tiles[0].maxY, 2);
    TEST_ASSERT_EQUAL_UINT32(tiles[1].minX, 2);
    TEST_ASSERT_EQUAL_UINT32(tiles[1].minY, 0);
    TEST_ASSERT_EQUAL_UINT32(tiles[1].maxX, 4);
    TEST_ASSERT_EQUAL_UINT32(tiles[1].maxY, 2);

    TEST_ASSERT_EQUAL_UINT32(tileCount, 5 * 5);
}

void TestComputeTilesNonDivisible()
{
    Tile tiles[64] = {};
    u32 tileCount =
        ComputeTiles(9, 9, 2, 2, tiles, ArrayCount(tiles));

    TEST_ASSERT_EQUAL_UINT32(tileCount, 5 * 5);

    TEST_ASSERT_EQUAL_UINT32(tiles[24].minX, 8);
    TEST_ASSERT_EQUAL_UINT32(tiles[24].minY, 8);
    TEST_ASSERT_EQUAL_UINT32(tiles[24].maxX, 9);
    TEST_ASSERT_EQUAL_UINT32(tiles[24].maxY, 9);
}

void TestComputeTilesInsufficientSpace()
{
    Tile tiles[10] = {};
    u32 tileCount =
        ComputeTiles(10, 10, 2, 2, tiles, ArrayCount(tiles));

    TEST_ASSERT_EQUAL_UINT32(tileCount, ArrayCount(tiles));
}

void TestWorkQueuePop()
{
    WorkQueue queue = {};
    queue.tasks[0].tile.minX = 1;
    queue.tasks[1].tile.minX = 2;

    queue.tail = 5;
    queue.head = 0;

    Task first = WorkQueuePop(&queue);
    Task second = WorkQueuePop(&queue);

    TEST_ASSERT_EQUAL_UINT32(first.tile.minX, 1);
    TEST_ASSERT_EQUAL_UINT32(second.tile.minX, 2);
}

void TestParseCommandLineArgs()
{
    char *assetDir = NULL;
    char *argv[] = {
        "/path/to/my/app",
        "--asset-dir",
        "/my/asset/dir"
    };
    int argc = ArrayCount(argv);

    TEST_ASSERT_TRUE(ParseCommandLineArgs(argc, argv, &assetDir));
    TEST_ASSERT_EQUAL_STRING(argv[2], assetDir);
}

void TestParseCommandLineArgsEmpty()
{
    char *assetDir = NULL;
    TEST_ASSERT_FALSE(ParseCommandLineArgs(0, NULL, &assetDir));
}

// Integration tests
void TestLoadMesh()
{
    MemoryArena memoryArena = {};
    u64 memoryCapacity = Megabytes(1);
    void *memory = malloc(memoryCapacity);
    InitializeMemoryArena(&memoryArena, memory, memoryCapacity);

    // TODO: This probably needs to be exposed as a commandline arg so we can run
    // our integration tests regardless of working dir.
    const char *assetDir = "../assets";

    const char *meshName = "bunny.obj";
    MeshData meshData = LoadMesh(meshName, &memoryArena, assetDir);
    TEST_ASSERT_GREATER_THAN_UINT(0, meshData.vertexCount);

    free(memory);
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

int main()
{
    LogMessage = LogMessage_;

    //RUN_TEST(test_repro_bug);
    //RUN_TEST(Test_BuildAabbTree);
    RUN_TEST(TestComputeTiles);
    RUN_TEST(TestComputeTilesNonDivisible);
    RUN_TEST(TestComputeTilesInsufficientSpace);
    RUN_TEST(TestWorkQueuePop);
    RUN_TEST(TestParseCommandLineArgs);
    RUN_TEST(TestParseCommandLineArgsEmpty);
    RUN_TEST(TestLoadMesh);
    return UNITY_END();
}
