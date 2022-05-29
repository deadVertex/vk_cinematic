#include "unity.h"

#include "platform.h"
#include "math_lib.h"
#include "tile.h"
#include "memory_pool.h"
#include "bvh.h"
#include "ray_intersection.h"
#include "asset_loader/asset_loader.h"
#include "image.h"
#include "mesh.h"
#include "sp_scene.h"
#include "sp_material_system.h"
#include "simd_path_tracer.h"
#include "sp_metrics.h"

#include "simd.h"
#include "aabb.h"

#include "custom_assertions.h"

#include "memory_pool.cpp"
#include "ray_intersection.cpp"

#define SAMPLES_PER_PIXEL 1

// Include cpp file for faster unity build
#include "bvh.cpp"
#include "sp_scene.cpp"
#include "sp_material_system.cpp"
#include "simd_path_tracer.cpp"

#define MEMORY_ARENA_SIZE Megabytes(1)

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

void TestPathTraceSingleColor()
{
    // Given a context
    vec4 pixels[16] = {};
    ImagePlane imagePlane = {};
    imagePlane.pixels = pixels;
    imagePlane.width = 4;
    imagePlane.height = 4;

    sp_Camera camera = {};
    camera.imagePlane = &imagePlane;

    sp_Scene scene = {};

    sp_MaterialSystem materialSystem = {};

    sp_Context ctx = {};
    ctx.camera = &camera;
    ctx.scene = &scene;
    ctx.materialSystem = &materialSystem;

    RandomNumberGenerator rng = {};

    sp_Metrics metrics = {};

    // When we path trace a tile
    Tile tile = {};
    tile.minX = 0;
    tile.minY = 0;
    tile.maxX = 4;
    tile.maxY = 4;
    sp_PathTraceTile(&ctx, tile, &rng, &metrics);

    // Then all of the pixels are set to magenta (no background material set)
    for (u32 i = 0; i < 16; i++)
    {
        AssertWithinVec4(EPSILON, Vec4(1, 0, 1, 1), pixels[i]);
    }
}

void TestPathTraceTile()
{
    // Given a context
    vec4 pixels[16] = {};
    ImagePlane imagePlane = {};
    imagePlane.pixels = pixels;
    imagePlane.width = 4;
    imagePlane.height = 4;

    sp_Camera camera = {};
    camera.imagePlane = &imagePlane;

    sp_Scene scene = {};

    sp_MaterialSystem materialSystem = {};

    sp_Context ctx = {};
    ctx.camera = &camera;
    ctx.scene = &scene;
    ctx.materialSystem = &materialSystem;

    RandomNumberGenerator rng = {};

    sp_Metrics metrics = {};

    // When we path trace a tile
    Tile tile = {};
    tile.minX = 1;
    tile.minY = 1;
    tile.maxX = 3;
    tile.maxY = 3;
    sp_PathTraceTile(&ctx, tile, &rng, &metrics);

    // Then only the pixels within that tile are updated
    // clang-format off
    vec4 expectedPixels[16] = {
        Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 0),
        Vec4(0, 0, 0, 0), Vec4(1, 0, 1, 1), Vec4(1, 0, 1, 1), Vec4(0, 0, 0, 0),
        Vec4(0, 0, 0, 0), Vec4(1, 0, 1, 1), Vec4(1, 0, 1, 1), Vec4(0, 0, 0, 0),
        Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 0),
    };
    // clang-format on
    for (u32 i = 0; i < 16; i++)
    {
        AssertWithinVec4(EPSILON, expectedPixels[i], pixels[i]);
    }
}

void TestConfigureCamera()
{
    vec4 pixels[4*4] = {};

    ImagePlane imagePlane = {};
    imagePlane.width = 4;
    imagePlane.height = 2;
    imagePlane.pixels = pixels;

    vec3 position = Vec3(0, 2, 0);
    quat rotation = Quat();
    f32 filmDistance = 0.1f;

    sp_Camera camera = {};
    sp_ConfigureCamera(&camera, &imagePlane, position, rotation, filmDistance);

    TEST_ASSERT_TRUE(camera.imagePlane == &imagePlane);

    AssertWithinVec3(EPSILON, position, camera.position);
    // Compute basis vectors
    AssertWithinVec3(EPSILON, Vec3(1, 0, 0), camera.basis.right);
    AssertWithinVec3(EPSILON, Vec3(0, 1, 0), camera.basis.up);
    AssertWithinVec3(EPSILON, Vec3(0, 0, -1), camera.basis.forward);
    // Compute filmCenter
    AssertWithinVec3(EPSILON, Vec3(0, 2, -0.1f), camera.filmCenter);
    // Compute halfPixelWidth/halfPixelHeight
    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 0.125f, camera.halfPixelWidth);
    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 0.25f, camera.halfPixelHeight);
    // Compute halfFilmWidth/halfFilmHeight;
    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 0.5f, camera.halfFilmWidth);
    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 0.25f, camera.halfFilmHeight);
}

void TestCalculateFilmP()
{
    vec4 pixels[4*4] = {};

    ImagePlane imagePlane = {};
    imagePlane.width = 4;
    imagePlane.height = 4;
    imagePlane.pixels = pixels;

    vec3 position = Vec3(0, 2, 0);
    quat rotation = Quat();
    f32 filmDistance = 0.1f;

    sp_Camera camera = {};
    sp_ConfigureCamera(&camera, &imagePlane, position, rotation, filmDistance);

    vec3 filmPositions[4] = {};

    vec2 pixelPositions[4] = {
        Vec2(0.5, 0.5),
        Vec2(1.5, 0.5),
        Vec2(0.5, 1.5),
        Vec2(3.5, 3.5),
    };

    u32 count =
        sp_CalculateFilmPositions(&camera, filmPositions, pixelPositions, 4);
    TEST_ASSERT_EQUAL_UINT32(4, count);
    AssertWithinVec3(EPSILON, Vec3(-0.375, 2.375, -0.1), filmPositions[0]);
    AssertWithinVec3(EPSILON, Vec3(0.375, 1.625, -0.1), filmPositions[3]);
}

void TestTransformAabb()
{
    // Given an Aabb
    vec3 boxMin = Vec3(-0.5);
    vec3 boxMax = Vec3(0.5);

    // When it is transformed by a model matrix?
    Aabb result = TransformAabb(boxMin, boxMax, Vec3(5, 0, 0), Quat(), Vec3(1));

    // Then the min and max are updated
    AssertWithinVec3(EPSILON, Vec3(4.5, -0.5, -0.5), result.min);
    AssertWithinVec3(EPSILON, Vec3(5.5, 0.5, 0.5), result.max);
}

void TestRayIntersectScene()
{
    // Given a scene with multiple objects
    sp_Scene scene = {};
    sp_InitializeScene(&scene, &memoryArena);

    sp_Mesh meshes[2];
    vec3 positions[2] = {
        Vec3(0, 2, -5),
        Vec3(0, 2, -15),
    };

    quat orientations[2] = {
        Quat(),
        Quat(Vec3(0, 1, 0), PI * 0.25f),
    };

    vec3 scale[2] = {
        Vec3(1),
        Vec3(2),
    };

    VertexPNT vertices[] = {
        {Vec3(-0.5, -0.5, 0), Vec3(0, 0, 1), Vec2(0, 0)},
        {Vec3(0.5, -0.5, 0), Vec3(0, 0, 1), Vec2(0, 0)},
        {Vec3(0.0, 0.5, 0), Vec3(0, 0, 1), Vec2(0, 0)},
    };

    u32 indices[] = { 0, 1, 2 };

    u32 material = 53;

    // TODO: Do we copy the vertex data and store it in the collision world?
    meshes[0] = sp_CreateMesh(
        vertices, ArrayCount(vertices), indices, ArrayCount(indices));
    meshes[1] = sp_CreateMesh(
        vertices, ArrayCount(vertices), indices, ArrayCount(indices));

    MemoryArena bvhNodeArena = SubAllocateArena(&memoryArena, Kilobytes(4));
    MemoryArena tempArena = SubAllocateArena(&memoryArena, Kilobytes(8));
    sp_BuildMeshMidphase(&meshes[0], &bvhNodeArena, &tempArena);
    sp_BuildMeshMidphase(&meshes[1], &bvhNodeArena, &tempArena);

    sp_AddObjectToScene(
        &scene, meshes[0], material, positions[0], orientations[0], scale[0]);
    sp_AddObjectToScene(
        &scene, meshes[1], material, positions[1], orientations[1], scale[1]);
    sp_BuildSceneBroadphase(&scene);

    // When we test if a ray intersects with the collision world
    vec3 rayOrigin = Vec3(0, 2, 0);
    vec3 rayDirection = Vec3(0, 0, -1);
    sp_Metrics metrics = {};
    sp_RayIntersectSceneResult result =
        sp_RayIntersectScene(&scene, rayOrigin, rayDirection, &metrics);

    // Then we find an intersection
    TEST_ASSERT_TRUE(result.t >= 0.0f);
    TEST_ASSERT_EQUAL_UINT32(material, result.materialId);
    // TODO: Check other properties of the intersection
}

void TestEvaluateLightPath()
{
    sp_MaterialSystem materialSystem = {};

    sp_Material backgroundMaterial = {};
    backgroundMaterial.emission = Vec3(1);
    backgroundMaterial.emissionTexture = U32_MAX;
    backgroundMaterial.albedoTexture = U32_MAX;
    u32 backgroundMaterialId = 0;
    sp_RegisterMaterial(
        &materialSystem, backgroundMaterial, backgroundMaterialId);

    sp_Material material = {};
    material.albedo = Vec3(0.18, 0.18, 0.18);
    material.albedoTexture = U32_MAX;
    material.emissionTexture = U32_MAX;

    u32 materialId = 1;
    sp_RegisterMaterial(&materialSystem, material, materialId);

    sp_PathVertex path[2] = {};
    path[0].materialId = materialId;
    path[0].normal = Vec3(0, 1, 0);
    path[0].incomingDir = Vec3(0, 1, 0);
    path[1].materialId = backgroundMaterialId;
    vec3 radiance = ComputeRadianceForPath(
        path, ArrayCount(path), &materialSystem);
    AssertWithinVec3(EPSILON, Vec3(0.18, 0.18, 0.18), radiance);
}

void TestMaterialAlbedoTexture()
{
    // Creat image
    HdrImage image = AllocateImage(1, 1, &memoryArena);
    SetPixel(&image, 0, 0, Vec4(1, 0, 0, 1));

    sp_MaterialSystem materialSystem = {};

    u32 imageId = 1;
    sp_RegisterTexture(&materialSystem, image, imageId);

    // Give a material
    sp_Material material = {};
    material.albedoTexture = imageId;
    material.emissionTexture = U32_MAX;

    sp_PathVertex vertex = {};

    // Then we can retrieve the albedo term
    sp_MaterialOutput output =
        sp_EvaluateMaterial(&materialSystem, &material, &vertex);

    AssertWithinVec3(EPSILON, Vec3(1, 0, 0), output.albedo);
}

void TestMetrics()
{
    // Given a context
    vec4 pixels[16] = {};
    ImagePlane imagePlane = {};
    imagePlane.pixels = pixels;
    imagePlane.width = 4;
    imagePlane.height = 4;

    sp_Camera camera = {};
    camera.imagePlane = &imagePlane;

    sp_Scene scene = {};

    sp_MaterialSystem materialSystem = {};

    sp_Context ctx = {};
    ctx.camera = &camera;
    ctx.scene = &scene;
    ctx.materialSystem = &materialSystem;

    RandomNumberGenerator rng = {};

    sp_Metrics metrics = {};

    // When we path trace a tile
    Tile tile = {};
    tile.minX = 1;
    tile.minY = 1;
    tile.maxX = 3;
    tile.maxY = 3;
    sp_PathTraceTile(&ctx, tile, &rng, &metrics);

    // Then per-thread performance metrics are computed
    TEST_ASSERT_GREATER_THAN_UINT32(0, metrics.values[sp_Metric_CyclesElapsed]);
}

void TestRayIntersectMesh()
{
    // Given a mesh
    VertexPNT vertices[] = {
        {Vec3(-0.5, -0.5, 0), Vec3(0, 0, 1), Vec2(0, 0)},
        {Vec3(0.5, -0.5, 0), Vec3(0, 0, 1), Vec2(0, 0)},
        {Vec3(0.0, 0.5, 0), Vec3(0, 0, 1), Vec2(0, 0)},
    };

    u32 indices[] = { 0, 1, 2 };
    sp_Mesh mesh = sp_CreateMesh(
        vertices, ArrayCount(vertices), indices, ArrayCount(indices));

    MemoryArena bvhNodeArena = SubAllocateArena(&memoryArena, Kilobytes(1));
    MemoryArena tempArena = SubAllocateArena(&memoryArena, Kilobytes(4));
    sp_BuildMeshMidphase(&mesh, &bvhNodeArena, &tempArena);

    // When we intersect a ray against it
    vec3 rayOrigin = Vec3(0, 0, 10);
    vec3 rayDirection = Vec3(0, 0, -1);
    sp_Metrics metrics = {};
    sp_RayIntersectMeshResult result =
        sp_RayIntersectMesh(mesh, rayOrigin, rayDirection, &metrics);

    // Then we get the intersection t value
    TEST_ASSERT_TRUE(result.triangleIntersection.t >= 0.0f);
}

void TestCreateMeshBuildsBvhTreeSingleTriangle()
{
    // Given vertices and indices for a single triangle
    VertexPNT vertices[] = {
        {Vec3(-0.5, -0.5, 0), Vec3(0, 0, 1), Vec2(0, 0)},
        {Vec3(0.5, -0.5, 0), Vec3(0, 0, 1), Vec2(0, 0)},
        {Vec3(0.0, 0.5, 0), Vec3(0, 0, 1), Vec2(0, 0)},
    };

    u32 indices[] = { 0, 1, 2 };

    // When we create a mesh
    sp_Mesh mesh = sp_CreateMesh(
        vertices, ArrayCount(vertices), indices, ArrayCount(indices));

    MemoryArena bvhNodeArena = SubAllocateArena(&memoryArena, Kilobytes(1));
    MemoryArena tempArena = SubAllocateArena(&memoryArena, Kilobytes(4));
    sp_BuildMeshMidphase(&mesh, &bvhNodeArena, &tempArena);

    // Then it generates a BVH tree with a single node
    bvh_Node *root = mesh.midphaseTree.root;
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_UINT32(0, root->leafIndex);
    AssertWithinVec3(EPSILON, Vec3(-0.5f, -0.5f, 0.0f), root->min);
    AssertWithinVec3(EPSILON, Vec3(0.5f, 0.5f, 0.0f), root->max);
}

void TestRayIntersectAabb()
{
    // Given an AABB
    vec3 min = Vec3(-0.5);
    vec3 max = Vec3(0.5);

    // When we intersect a ray against it
    vec3 rayOrigin = Vec3(0, 0, 10);
    vec3 rayDirection = Vec3(0, 0, -1);
    f32 t = simd_RayIntersectAabb(min, max, rayOrigin, rayDirection);

    // We get a t value for the intersection
    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 9.5f, t);
}

void TestRayIntersectAabb4()
{
    vec3 boxMins[] = {
        Vec3(-0.5, -0.5, -0.5f),
        Vec3(-0.5, -0.5, 1.0f),
        Vec3(-0.5, -0.5, 2.5f),
        Vec3(-0.5, -0.5, 4.0f),
    };

    vec3 boxMaxes[] = {
        Vec3(0.5f, 0.5f, 0.5f),
        Vec3(0.5f, 0.5f, 2.0f),
        Vec3(0.5f, 0.5f, 3.5f),
        Vec3(0.5f, 0.5f, 5.0f),
    };

    vec3 rayOrigin = Vec3(0, 0, 10);
    vec3 invRayDirection = Inverse(Vec3(0, 0, -1));

    // Bits 0, 1, 2, 3 should be set
    TEST_ASSERT_EQUAL_UINT32(0xF,
        simd_RayIntersectAabb4(boxMins, boxMaxes, rayOrigin, invRayDirection));

    TEST_ASSERT_EQUAL_UINT32(0x0, simd_RayIntersectAabb4(boxMins, boxMaxes,
                                      rayOrigin, Inverse(Vec3(1, 0, 0))));
    TEST_ASSERT_EQUAL_UINT32(0x0, simd_RayIntersectAabb4(boxMins, boxMaxes,
                                      rayOrigin, Inverse(Vec3(0, 0, 1))));
}

void TestRayIntersectAabb4Bug()
{
    vec3 boxMin[4] = {Vec3(-0.375038534, 0.843911469, 0.264082730), Vec3(0),
        Vec3(0), Vec3(0)};
    vec3 boxMax[4] = {Vec3(-0.238676921, 0.916244209, 0.386187375), Vec3(0),
        Vec3(0), Vec3(0)};
    vec3 rayOrigin = Vec3(-2.68516445, -1.71131170, -1.71610022);
    vec3 rayDirection = Vec3(0.576836348, 0.652352035, 0.491626590);

    // Why does this pass?
    // Looks like a floating point precision bug
    u32 mask = simd_RayIntersectAabb4(
        boxMin, boxMax, rayOrigin, Inverse(rayDirection));
    TEST_ASSERT_EQUAL_UINT32(0x1, mask);

    TEST_ASSERT_FLOAT_WITHIN(EPSILON, -1.0f,
        RayIntersectAabb(boxMin[0], boxMax[0], rayOrigin, rayDirection));
}

void TestRayIntersectAabbCompare()
{
    vec3 aabbMin[4] = {Vec3(-2.0f, -0.1f, -0.5f)};
    vec3 aabbMax[4] = {Vec3(-0.5f, 0.4f, 3.0f)};

    vec3 min = Vec3(-10.0f);
    vec3 max = Vec3(10.0f);

    RandomNumberGenerator rng = { 0x1A34C249 };

    u32 rayCount = 32;
    for (u32 i = 0; i < rayCount; ++i)
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
        vec3 invRayDirection = Inverse(rayDirection);

        f32 t =
            RayIntersectAabb(aabbMin[0], aabbMax[0], rayOrigin, rayDirection);
        u32 expected = t >= 0.0f ? 1 : 0;

        u32 mask =
            simd_RayIntersectAabb4(aabbMin, aabbMax, rayOrigin, invRayDirection);

        Assert(mask == expected);

        TEST_ASSERT_EQUAL_UINT32(expected, mask);
    }
}

void TestRandomDirectionOnHemisphere()
{
    RandomNumberGenerator rng = { 123456789 };

    vec3 normal = Vec3(0, -1, 0);

    for (u32 i = 0; i < 1000; i++)
    {
        vec3 v = RandomDirectionOnHemisphere(normal, &rng);
        TEST_ASSERT_FLOAT_WITHIN(EPSILON, 1.0f, Length(v));
        TEST_ASSERT_TRUE(Dot(v, normal) >= 0.0f);

        normal = v;
    }
}

int main()
{
    InitializeMemoryArena(
        &memoryArena, calloc(1, MEMORY_ARENA_SIZE), MEMORY_ARENA_SIZE);

    UNITY_BEGIN();
    RUN_TEST(TestPathTraceSingleColor);
    RUN_TEST(TestPathTraceTile);
    RUN_TEST(TestConfigureCamera);
    RUN_TEST(TestCalculateFilmP);
    RUN_TEST(TestTransformAabb);
    RUN_TEST(TestRayIntersectScene);

    RUN_TEST(TestEvaluateLightPath);
    RUN_TEST(TestMaterialAlbedoTexture);

    RUN_TEST(TestMetrics);

    RUN_TEST(TestRayIntersectMesh);
    RUN_TEST(TestCreateMeshBuildsBvhTreeSingleTriangle);

    RUN_TEST(TestRayIntersectAabb);
    RUN_TEST(TestRayIntersectAabb4);
    RUN_TEST(TestRayIntersectAabb4Bug);
    RUN_TEST(TestRayIntersectAabbCompare);

    RUN_TEST(TestRandomDirectionOnHemisphere);

    free(memoryArena.base);

    return UNITY_END();
}
