#include "unity.h"

#include "platform.h"
#include "math_lib.h"
#include "tile.h"
#include "simd_path_tracer.h"
#include "memory_pool.h"
#include "bvh.h"

#include "custom_assertions.h"

#include "memory_pool.cpp"
#include "ray_intersection.cpp"

// Include cpp file for faster unity build
#include "simd_path_tracer.cpp"

#include "bvh.cpp"

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

    sp_Context ctx = {};
    ctx.camera = &camera;

    // When we path trace a tile
    Tile tile = {};
    tile.minX = 0;
    tile.minY = 0;
    tile.maxX = 4;
    tile.maxY = 4;
    sp_PathTraceTile(&ctx, tile);

    // Then all of the pixels are set to black
    for (u32 i = 0; i < 16; i++)
    {
        AssertWithinVec4(EPSILON, Vec4(0, 0, 0, 1), pixels[i]);
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

    sp_Context ctx = {};
    ctx.camera = &camera;

    // When we path trace a tile
    Tile tile = {};
    tile.minX = 1;
    tile.minY = 1;
    tile.maxX = 3;
    tile.maxY = 3;
    sp_PathTraceTile(&ctx, tile);

    // Then only the pixels within that tile are updated
    // clang-format off
    vec4 expectedPixels[16] = {
        Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 0),
        Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 1), Vec4(0, 0, 0, 1), Vec4(0, 0, 0, 0),
        Vec4(0, 0, 0, 0), Vec4(0, 0, 0, 1), Vec4(0, 0, 0, 1), Vec4(0, 0, 0, 0),
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
    AssertWithinVec3(EPSILON, Vec3(-0.375, 1.625, -0.1), filmPositions[0]);
    AssertWithinVec3(EPSILON, Vec3(0.375, 2.375, -0.1), filmPositions[3]);
}

void TestCreateBvh()
{
    vec3 aabbMin[] = {Vec3(-0.5)};
    vec3 aabbMax[] = {Vec3(0.5)};

    // Build Bvh
    bvh_Tree worldBvh =
        bvh_CreateTree(&memoryArena, aabbMin, aabbMax, ArrayCount(aabbMin));
    TEST_ASSERT_NOT_NULL(worldBvh.root);

    //  Check node min and max
    AssertWithinVec3(EPSILON, aabbMin[0], worldBvh.root->min);
    AssertWithinVec3(EPSILON, aabbMax[0], worldBvh.root->max);
}

void TestCreateBvhMultipleNodes()
{
    vec3 aabbMin[] = {Vec3(-0.5), Vec3(1)};
    vec3 aabbMax[] = {Vec3(0.5), Vec3(2)};

    // Build Bvh
    bvh_Tree worldBvh =
        bvh_CreateTree(&memoryArena, aabbMin, aabbMax, ArrayCount(aabbMin));

    TEST_ASSERT_NOT_NULL(worldBvh.root);
    TEST_ASSERT_NOT_NULL(worldBvh.root->children[0]);
    TEST_ASSERT_NOT_NULL(worldBvh.root->children[1]);

    // Check memory pool for world bvh
    TEST_ASSERT_NOT_NULL(worldBvh.memoryPool.storage);
    TEST_ASSERT_EQUAL_UINT32(sizeof(bvh_Node), worldBvh.memoryPool.objectSize);

    // Check bounding volume for root contains all children
    AssertWithinVec3(EPSILON, Vec3(-0.5f), worldBvh.root->min);
    AssertWithinVec3(EPSILON, Vec3(2), worldBvh.root->max);
}

void TestBvh()
{
    vec3 aabbMin[] = {Vec3(-0.5f, -0.5f, -0.5f), Vec3(-0.5f, -0.5f, -1.5f)};
    vec3 aabbMax[] = {Vec3(0.5f, 0.5f, 0.5f), Vec3(0.5f, 0.5f, -0.5f)};

    // Build Bvh
    bvh_Tree worldBvh =
        bvh_CreateTree(&memoryArena, aabbMin, aabbMax, ArrayCount(aabbMin));

    bvh_Node *intersectedNodes[4] = {};

    vec3 rayOrigin = Vec3(0, 0, 10);
    vec3 rayDirection = Vec3(0, 0, -1);

    // Test ray intersection against it
    u32 intersectionCount = bvh_IntersectRay(&worldBvh, rayOrigin, rayDirection,
        intersectedNodes, ArrayCount(intersectedNodes));

    // Check that it returns the expected objects
    TEST_ASSERT_EQUAL_UINT32(2, intersectionCount);
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
    RUN_TEST(TestCreateBvh);
    RUN_TEST(TestCreateBvhMultipleNodes);
    RUN_TEST(TestBvh);

    free(memoryArena.base);

    return UNITY_END();
}
