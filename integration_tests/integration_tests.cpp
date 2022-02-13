#include <cstdarg>

#include "unity.h"

// For integration tests
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "platform.h"
#include "math_lib.h"
#include "tile.h"
#include "memory_pool.h"
#include "bvh.h"
#include "sp_scene.h"
#include "sp_material_system.h"
#include "sp_metrics.h"
#include "simd_path_tracer.h"

#include "simd.h"
#include "aabb.h"

#include "custom_assertions.h"

#include "cpu_ray_tracer.h"

#include "mesh.cpp"

#include "cmdline.cpp"

#include "ray_intersection.cpp"
#include "memory_pool.cpp"
#include "bvh.cpp"
#include "sp_scene.cpp"
#include "sp_material_system.cpp"
#include "simd_path_tracer.cpp"

#define MEMORY_ARENA_SIZE Megabytes(16)

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

// Integration tests
void TestLoadMesh()
{
    const char *meshName = "bunny.obj";
    MeshData meshData = LoadMesh(meshName, &memoryArena, g_AssetDir);
    TEST_ASSERT_GREATER_THAN_UINT(0, meshData.vertexCount);
}

void TestSimdPathTracer()
{
    vec4 pixels[1] = {};
    ImagePlane imagePlane = {};
    imagePlane.width = 1;
    imagePlane.height = 1;
    imagePlane.pixels = pixels;

    sp_Camera camera = {};
    vec3 cameraPosition = Vec3(0, 0, 10);
    quat cameraRotation = Quat();
    f32 filmDistance = 0.1f;
    sp_ConfigureCamera(
        &camera, &imagePlane, cameraPosition, cameraRotation, filmDistance);

    sp_Scene scene = {};
    sp_InitializeScene(&scene, &memoryArena);

    const char *meshName = "bunny.obj";
    MeshData meshData = LoadMesh(meshName, &memoryArena, g_AssetDir);

    vec3 *vertices = AllocateArray(&memoryArena, vec3, meshData.vertexCount);
    for (u32 i = 0; i < meshData.vertexCount; i++)
    {
        vertices[i] = meshData.vertices[i].position;
    }

    sp_Mesh mesh = sp_CreateMesh(
        vertices, meshData.vertexCount, meshData.indices, meshData.indexCount);
    MemoryArena bvhNodeArena = SubAllocateArena(&memoryArena, Megabytes(3));
    MemoryArena tempArena = SubAllocateArena(&memoryArena, Kilobytes(128));
    sp_BuildMeshMidphase(&mesh, &bvhNodeArena, &tempArena);

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

    sp_Metrics metrics = {};

    Tile tile = {};
    tile.minX = 0;
    tile.minY = 0;
    tile.maxX = 1;
    tile.maxY = 1;
    sp_PathTraceTile(&context, tile, &rng, &metrics);

    // TODO: Not sure what to assert since we don't know what the resulting
    // pixel color will be for an abitrary normal
    TEST_ASSERT_TRUE(pixels[0].r != 1.0f);
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
    ParseCommandLineArgs(argc, (const char **)argv, &g_AssetDir);
    LogMessage("Asset directory set to \"%s\".", g_AssetDir);

    UNITY_BEGIN();
    RUN_TEST(TestLoadMesh);
    RUN_TEST(TestSimdPathTracer);

    free(memoryArena.base);

    return UNITY_END();
}
