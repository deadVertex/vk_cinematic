#include "unity.h"

#include "platform.h"
#include "math_lib.h"
#include "tile.h"
#include "memory_pool.h"
#include "bvh.h"
#include "sp_scene.h"
#include "sp_material_system.h"
#include "simd_path_tracer.h"

#include "custom_assertions.h"

#include "memory_pool.cpp"
#include "ray_intersection.cpp"

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

    sp_Context ctx = {};
    ctx.camera = &camera;
    ctx.scene = &scene;

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

    sp_Scene scene = {};

    sp_Context ctx = {};
    ctx.camera = &camera;
    ctx.scene = &scene;

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
    AssertWithinVec3(EPSILON, Vec3(-0.375, 2.375, -0.1), filmPositions[0]);
    AssertWithinVec3(EPSILON, Vec3(0.375, 1.625, -0.1), filmPositions[3]);
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

void TestIntersectEmptyBvh()
{
    // Given an empty bvh
    bvh_Tree tree = {};

    bvh_Node *intersectedNodes[4] = {};

    vec3 rayOrigin = Vec3(0, 0, 10);
    vec3 rayDirection = Vec3(0, 0, -1);

    // When we test ray intersection against it
    bvh_IntersectRayResult result = bvh_IntersectRay(&tree, rayOrigin,
        rayDirection, intersectedNodes, ArrayCount(intersectedNodes));

    // Check that it returns no intersections (and doesn't crash)
    TEST_ASSERT_EQUAL_UINT32(0, result.count);
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
    bvh_IntersectRayResult result = bvh_IntersectRay(&worldBvh, rayOrigin,
        rayDirection, intersectedNodes, ArrayCount(intersectedNodes));

    // Check that it returns the expected objects
    TEST_ASSERT_EQUAL_UINT32(2, result.count);
    TEST_ASSERT_FALSE(result.errorOccurred);

    // Test oversubscription
    bvh_IntersectRayResult result2 = bvh_IntersectRay(&worldBvh, rayOrigin,
        rayDirection, intersectedNodes, 1);
    TEST_ASSERT_EQUAL_UINT32(1, result2.count);
    TEST_ASSERT_TRUE(result2.errorOccurred);
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

    vec3 vertices[] = {
        Vec3(-0.5, -0.5, 0),
        Vec3(0.5, -0.5, 0),
        Vec3(0.0, 0.5, 0),
    };

    u32 indices[] = { 0, 1, 2 };

    u32 material = 53;

    // TODO: Do we copy the vertex data and store it in the collision world?
    meshes[0] = sp_CreateMesh(
        vertices, ArrayCount(vertices), indices, ArrayCount(indices));
    meshes[1] = sp_CreateMesh(
        vertices, ArrayCount(vertices), indices, ArrayCount(indices));

    sp_AddObjectToScene(
        &scene, meshes[0], material, positions[0], orientations[0], scale[0]);
    sp_AddObjectToScene(
        &scene, meshes[1], material, positions[1], orientations[1], scale[1]);
    sp_BuildSceneBroadphase(&scene);

    // When we test if a ray intersects with the collision world
    vec3 rayOrigin = Vec3(0, 2, 0);
    vec3 rayDirection = Vec3(0, 0, -1);
    sp_RayIntersectSceneResult result =
        sp_RayIntersectScene(&scene, rayOrigin, rayDirection);

    // Then we find an intersection
    TEST_ASSERT_TRUE(result.t >= 0.0f);
    TEST_ASSERT_EQUAL_UINT32(material, result.materialId);
    // TODO: Check other properties of the intersection
}

void TestBvhFindClosestPartnerNode()
{
    bvh_Node nodes[3];
    for (u32 x = 0; x < 3; ++x)
    {
        vec3 origin = Vec3(-5, -5, 0);
        vec3 p = origin + Vec3((f32)x, 0, 0) * 3.0f;

        Aabb aabb =
            TransformAabb(Vec3(-0.5), Vec3(0.5), p, Quat(), Vec3(1));

        nodes[x].min = aabb.min;
        nodes[x].max = aabb.max;
        nodes[x].leafIndex = x;
    }

    bvh_Node *unmergedNodes[3] = {nodes + 0, nodes + 1, nodes + 2};
    bvh_FindClosestPartnerNodeResult result =
        bvh_FindClosestPartnerNode(unmergedNodes[0], unmergedNodes, 3);
    TEST_ASSERT_EQUAL_UINT32(1, result.index);
    TEST_ASSERT_EQUAL_PTR(unmergedNodes[1], result.node);
}

void TestBvhDuplicateIntersectionsBug()
{
    vec3 aabbMin[3];
    vec3 aabbMax[3];

    for (u32 x = 0; x < 3; ++x)
    {
        vec3 origin = Vec3(-5, -5, 0);
        vec3 p = origin + Vec3((f32)x, 0, 0) * 3.0f;

        Aabb aabb =
            TransformAabb(Vec3(-0.5), Vec3(0.5), p, Quat(), Vec3(1));

        aabbMin[x] = aabb.min;
        aabbMax[x] = aabb.max;
    }

    // Build Bvh
    bvh_Tree worldBvh =
        bvh_CreateTree(&memoryArena, aabbMin, aabbMax, ArrayCount(aabbMin));
    /*    [root]
     *    /    \
     *   []    [2]
     *  /  \
     * [0] [1]
     */
    TEST_ASSERT_EQUAL_UINT32(U32_MAX, worldBvh.root->leafIndex);
    TEST_ASSERT_NOT_NULL(worldBvh.root->children[1]);
    bvh_Node *branchNode = worldBvh.root->children[1];
    TEST_ASSERT_EQUAL_UINT32(0, branchNode->children[0]->leafIndex);
    TEST_ASSERT_EQUAL_UINT32(1, branchNode->children[1]->leafIndex);
    TEST_ASSERT_EQUAL_UINT32(2, worldBvh.root->children[0]->leafIndex);

    bvh_Node *intersectedNodes[3] = {};

    vec3 rayOrigin = Vec3(-10, -5, 0);
    vec3 rayDirection = Vec3(1, 0, 0);

    // Test ray intersection against it
    bvh_IntersectRayResult result = bvh_IntersectRay(&worldBvh, rayOrigin,
        rayDirection, intersectedNodes, ArrayCount(intersectedNodes));

    // Check that it returns the expected objects
    TEST_ASSERT_EQUAL_UINT32(3, result.count);
    TEST_ASSERT_FALSE(result.errorOccurred);

    TEST_ASSERT_TRUE(
        intersectedNodes[0]->leafIndex != intersectedNodes[1]->leafIndex &&
        intersectedNodes[0]->leafIndex != intersectedNodes[2]->leafIndex);
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
    RUN_TEST(TestIntersectEmptyBvh);
    RUN_TEST(TestBvh);
    RUN_TEST(TestTransformAabb);
    RUN_TEST(TestRayIntersectScene);
    RUN_TEST(TestBvhFindClosestPartnerNode);
    RUN_TEST(TestBvhDuplicateIntersectionsBug);

    free(memoryArena.base);

    return UNITY_END();
}
