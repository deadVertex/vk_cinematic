#include "unity.h"

#include "cpu_ray_tracer.h"
#include "profiler.h"

#include "debug.cpp"
#include "cmdline.cpp"
#include "cubemap.cpp"
#include "mesh_generation.cpp"
#include "ray_intersection.cpp"
#include "tree_utils.cpp"
#include "cpu_ray_tracer.cpp"

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
    const char *assetDir = NULL;
    const char *argv[] = {
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
    const char *assetDir = NULL;
    TEST_ASSERT_FALSE(ParseCommandLineArgs(0, NULL, &assetDir));
}

void TestToSphericalCoordinates()
{
    // Given
    vec3 inputDirections[] = {
        Vec3(1, 0, 0),
        Vec3(0, 0, -1),
        Vec3(0, 1, 0),
        Normalize(Vec3(0, 1, 1)),
        Vec3(-1, 0, 0),
    };
    vec2 expectedSphereCoords[] = {
        Vec2(0.0f, PI * 0.5f),
        Vec2(-PI * 0.5f, PI * 0.5f),
        Vec2(0.0f, 0.0f),
        Vec2(PI * 0.5f, PI * 0.25f),
        Vec2(PI, PI * 0.5f),
    };

    for (u32 i = 0; i < ArrayCount(inputDirections); ++i)
    {
        vec3 direction = inputDirections[i];
        vec2 expected = expectedSphereCoords[i];

        // When
        vec2 sphereCoords = ToSphericalCoordinates(direction);

        // Then
        TEST_ASSERT_EQUAL_FLOAT(expected.x, sphereCoords.x);
        TEST_ASSERT_EQUAL_FLOAT(expected.y, sphereCoords.y);
    }
}

void TestMapToEquirectangular()
{
    // Given
    vec2 inputSphereCoords[] = {
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, PI),
        Vec2(0.0f, PI * 0.5f),
        Vec2(PI * 0.5f, PI * 0.5f),
        Vec2(PI, PI * 0.5f),
        Vec2(PI * -0.5f, PI * 0.5f),
    };

    vec2 expectedUVs[] = {
        Vec2(0.0f, 1.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.5f),
        Vec2(0.25f, 0.5f),
        Vec2(0.5f, 0.5f),
        Vec2(0.75f, 0.5f),
    };

    for (u32 i = 0; i < ArrayCount(inputSphereCoords); ++i)
    {
        vec2 sphereCoords = inputSphereCoords[i];
        vec2 expected = expectedUVs[i];

        // When
        vec2 uv = MapToEquirectangular(sphereCoords);

        // Then
        TEST_ASSERT_EQUAL_FLOAT(expected.x, uv.x);
        TEST_ASSERT_EQUAL_FLOAT(expected.y, uv.y);
    }
}

void TestMapEquirectangularToSphereCoordinates()
{
    // Given
    vec2 inputUVs[] = {
        Vec2(0.0f, 1.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.5f),
        Vec2(0.25f, 0.5f),
        Vec2(0.5f, 0.5f),
        Vec2(0.75f, 0.5f),
    };

    vec2 expectedSphereCoords[] = {
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, PI),
        Vec2(0.0f, PI * 0.5f),
        Vec2(PI * 0.5f, PI * 0.5f),
        Vec2(PI, PI * 0.5f),
        Vec2(PI * -0.5f, PI * 0.5f),
    };

    for (u32 i = 0; i < ArrayCount(inputUVs); ++i)
    {
        vec2 uv = inputUVs[i];
        vec2 expected = expectedSphereCoords[i];

        // When
        vec2 sphereCoords = MapEquirectangularToSphereCoordinates(uv);

        // Then
        TEST_ASSERT_EQUAL_FLOAT(expected.x, sphereCoords.x);
        TEST_ASSERT_EQUAL_FLOAT(expected.y, sphereCoords.y);
    }
}

void TestMapSphericalToCartesianCoordinates()
{
    // Given
    vec2 inputSphereCoords[] = {
        Vec2(0.0f, PI * 0.5f),
        Vec2(-PI * 0.5f, PI * 0.5f),
        Vec2(0.0f, 0.0f),
        Vec2(PI * 0.5f, PI * 0.25f),
        Vec2(PI, PI * 0.5f),
    };

    vec3 expectedDirections[] = {
        Vec3(1, 0, 0),
        Vec3(0, 0, -1),
        Vec3(0, 1, 0),
        Normalize(Vec3(0, 1, 1)),
        Vec3(-1, 0, 0),
    };

    for (u32 i = 0; i < ArrayCount(inputSphereCoords); ++i)
    {
        vec2 sphereCoords = inputSphereCoords[i];
        vec3 expected = expectedDirections[i];

        // When
        vec3 direction = MapSphericalToCartesianCoordinates(sphereCoords);

        // Then
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(EPSILON, expected.x, direction.x, "X axis");
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(EPSILON, expected.y, direction.y, "Y axis");
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(EPSILON, expected.z, direction.z, "Z axis");

        TEST_ASSERT_EQUAL_FLOAT(1.0f, Length(direction));
    }
}

#if 0
void TestCreateCubeMap()
{
    // Create Equirectangular image of direction vectors
    u32 width = 8;
    u32 height = 4;
    vec3 pixels[8 * 4] = {};
    for (u32 y = 0; y < height; ++y)
    {
        for (u32 x = 0; x < width; ++x)
        {
            // Convert pixel coordinates to UV coordinates
            vec2 uv = Vec2((f32)x / (f32)width, (f32)y / (f32)height);

            // Convert UV coordinates into sphere coordinates
            vec2 sphereCoords = MapEquirectangularToSphereCoordinates(uv);

            // Convert sphere coordinates to cartesian coordinates
            vec3 direction = MapSphericalToCartesianCoordinates(sphereCoords);

            u32 index = y * width + x;
            pixels[index] = direction;

            //printf("[%0.2g, %0.2g, %0.2g] ", direction.x, direction.y, direction.z);
        }

        //printf("\n");
    }

    HdrImage equirectangularImage = {};
    equirectangularImage.width = width;
    equirectangularImage.height = height;
    equirectangularImage.pixels = (f32 *)pixels;

    MemoryArena arena = {};
    InitializeMemoryArena(&arena, malloc(4096), 4096);

    // Create cube map by sampling equirectangular image
    // Create cube map face +Z
    HdrImage image = CreateCubeMap(equirectangularImage, &arena, 3);

    for (u32 y = 0; y < 3; ++y)
    {
        for (u32 x = 0; x < 3; ++x)
        {
            u32 index = (y * 3 + x) * 4;
            vec4 color = *(vec4 *)(image.pixels + index);

            // TOMORROW: Our terrible sampling is struggling at such a low res
            printf("[%0.2g, %0.2g, %0.2g, %0.2g] ", color.x, color.y,
                color.z, color.w);
        }

        printf("\n");
    }

    vec3 expected = Vec3(0, 0, 1);
    vec4 direction = SampleImage(image, Vec2(0.5, 0.5));
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(EPSILON, expected.x, direction.x, "X axis");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(EPSILON, expected.y, direction.y, "Y axis");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(EPSILON, expected.z, direction.z, "Z axis");

    free(arena.base);
}
#endif

void TestRayIntersectTriangleHit()
{
    vec3 rayOrigin = Vec3(0, 0, 1);
    vec3 rayDirection = Vec3(0, 0, -1);
    vec3 vertices[] = {
        Vec3(-0.5f, -0.5f, -5.0f),
        Vec3(0.5f, -0.5f, -5.0f),
        Vec3(0.0f, 0.5f, -5.0f),
    };

    RayIntersectTriangleResult result = RayIntersectTriangle(
        rayOrigin, rayDirection, vertices[0], vertices[1], vertices[2]);

    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 6.0f, result.t);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(EPSILON, 0.0f, result.normal.x, "X axis");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(EPSILON, 0.0f, result.normal.y, "Y axis");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(EPSILON, 1.0f, result.normal.z, "Z axis");
}

void TestRayIntersectTriangleMiss()
{
    vec3 rayOrigin = Vec3(0, 0, 1);
    vec3 rayDirection = Vec3(0, 0, -1);

    vec3 offset = Vec3(5.0f, 0.0f, 0.0f);
    vec3 vertices[] = {
        Vec3(-0.5f, -0.5f, -5.0f) + offset,
        Vec3(0.5f, -0.5f, -5.0f) + offset,
        Vec3(0.0f, 0.5f, -5.0f) + offset,
    };

    RayIntersectTriangleResult result = RayIntersectTriangle(
        rayOrigin, rayDirection, vertices[0], vertices[1], vertices[2]);

    TEST_ASSERT_FLOAT_WITHIN(EPSILON, -1.0f, result.t);
}

void TestRayIntersectTriangleHitUV()
{
    vec3 rayOrigin = Vec3(0, 0, 1);
    vec3 rayDirection = Vec3(0, 0, -1);
    vec3 offset = Vec3(0.5f, 0.5f, 0.0f);
    vec3 vertices[] = {
        Vec3(-0.5f, -0.5f, -5.0f) + offset,
        Vec3(0.5f, -0.5f, -5.0f) + offset,
        Vec3(0.0f, 0.5f, -5.0f) + offset,
    };

    RayIntersectTriangleResult result = RayIntersectTriangle(
        rayOrigin, rayDirection, vertices[0], vertices[1], vertices[2]);

    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 6.0f, result.t);

    f32 w = 1.0f - result.uv.x - result.uv.y;
    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 1.0f, w);
    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 0.0f, result.uv.x);
    TEST_ASSERT_FLOAT_WITHIN(EPSILON, 0.0f, result.uv.y);
}

/* Test to reproduce issue caused by not checking that all 3 barycentric
 * coordinates are within the 0-1 range when performing the Moller-Trumbore
 * triangle intersection algorithm.
 */
void TestRayIntersectTriangleMTBarycentricCoordsIssue()
{
    vec3 rayOrigin = Vec3(0.5, 0.5, 1);
    vec3 rayDirection = Vec3(0, 0, -1);

    vec3 vertices[] = {
        Vec3(-0.5f, -0.5f, -5.0f),
        Vec3(0.5f, -0.5f, -5.0f),
        Vec3(0.0f, 0.5f, -5.0f),
    };

    RayIntersectTriangleResult result = RayIntersectTriangleMT(
        rayOrigin, rayDirection, vertices[0], vertices[1], vertices[2]);

    TEST_ASSERT_FLOAT_WITHIN(EPSILON, -1.0f, result.t);
}

void AssertWithinVec3(f32 delta, vec3 expected, vec3 actual)
{
    b32 x = Abs(expected.x - actual.x) <= delta;
    b32 y = Abs(expected.y - actual.y) <= delta;
    b32 z = Abs(expected.z - actual.z) <= delta;

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "Expected [%g, %g, %g] Was [%g, %g, %g].",
            expected.x, expected.y, expected.z, actual.x, actual.y, actual.z);
    TEST_ASSERT_MESSAGE(x && y && z, buffer);
}

void TestRayIntersectTriangleMeshFlatShading()
{
    vec3 rayDirection = Vec3(0, 0, -1);

    // Given a mesh with different vertex normals and flat shading enabled
    MeshData meshData = CreateTriangleMeshData(&memoryArena);
    meshData.vertices[0].normal = Vec3(1, 0, 0);
    meshData.vertices[1].normal = Vec3(0, 1, 0);
    meshData.vertices[2].normal = Vec3(0, 0, 1);

    RayTracerMesh mesh =
        CreateMesh(meshData, &memoryArena, &memoryArena, false);

    // When we intersect a two rays with that mesh
    RayHitResult resultA = RayIntersectTriangleMesh(
        mesh, Vec3(0, 0, 5), rayDirection, NULL, 0, 0.0f);

    RayHitResult resultB = RayIntersectTriangleMesh(
        mesh, Vec3(0, 0.5, 5), rayDirection, NULL, 0, 0.0f);

    // Then we get the same normal regards of where the ray intersects the
    // triangle
    TEST_ASSERT_TRUE(resultA.t >= 0);
    TEST_ASSERT_TRUE(resultB.t >= 0);
    AssertWithinVec3(EPSILON, resultA.normal, resultB.normal);
};

void TestRayIntersectTriangleMeshSmoothShading()
{
    vec3 rayDirection = Vec3(0, 0, -1);

    // Given a mesh with different vertex normals and smooth shading enabled
    MeshData meshData = CreateTriangleMeshData(&memoryArena);
    meshData.vertices[0].normal = Vec3(1, 0, 0);
    meshData.vertices[1].normal = Vec3(0, 1, 0);
    meshData.vertices[2].normal = Vec3(0, 0, 1);

    RayTracerMesh mesh = CreateMesh(meshData, &memoryArena, &memoryArena, true);

    // When we intersect a two rays with that mesh
    RayHitResult resultA = RayIntersectTriangleMesh(
        mesh, Vec3(0, 0, 5), rayDirection, NULL, 0, 0.0f);

    RayHitResult resultB = RayIntersectTriangleMesh(
        mesh, Vec3(0, 0.5, 5), rayDirection, NULL, 0, 0.0f);

    // Then we get different normals for the intersection point
    TEST_ASSERT_TRUE(resultA.t >= 0);
    TEST_ASSERT_TRUE(resultB.t >= 0);
    TEST_ASSERT_TRUE(Length(resultA.normal - resultB.normal) > EPSILON);
};

void TestNearestSampling()
{
    // Given an image   | (0, 0, 0, 0) | (1, 1, 1, 1) |
    HdrImage image = AllocateImage(2, 1, &memoryArena);
    SetPixel(&image, 0, 0, Vec4(0));
    SetPixel(&image, 1, 0, Vec4(1));

    // When we sample the center of the image
    vec4 sample = SampleImageNearest(image, Vec2(0.5, 0.5));

    // Then we get the color of the nearest pixel
    TEST_ASSERT_EQUAL_FLOAT(1, sample.r);
}

void TestBilinearSampling()
{
    // Given an image with a checkerboard pattern
    // | (0, 0, 0, 0) | (1, 1, 1, 1) |
    // | (1, 1, 1, 1) | (0, 0, 0, 0) |
    HdrImage image = AllocateImage(2, 2, &memoryArena);
    SetPixel(&image, 0, 0, Vec4(0));
    SetPixel(&image, 1, 0, Vec4(1));
    SetPixel(&image, 0, 1, Vec4(1));
    SetPixel(&image, 1, 1, Vec4(0));

    // When we sample the center of the image
    vec4 sample = SampleImageBilinear(image, Vec2(0.5, 0.5));

    // Then we get the color of the 4 nearest pixels blended together
    TEST_ASSERT_EQUAL_FLOAT(0.5, sample.r);
}

int main()
{
    InitializeMemoryArena(
        &memoryArena, calloc(1, MEMORY_ARENA_SIZE), MEMORY_ARENA_SIZE);

    RUN_TEST(TestComputeTiles);
    RUN_TEST(TestComputeTilesNonDivisible);
    RUN_TEST(TestComputeTilesInsufficientSpace);
    RUN_TEST(TestWorkQueuePop);
    RUN_TEST(TestParseCommandLineArgs);
    RUN_TEST(TestParseCommandLineArgsEmpty);

    RUN_TEST(TestToSphericalCoordinates);

    RUN_TEST(TestMapToEquirectangular);

    RUN_TEST(TestMapEquirectangularToSphereCoordinates);
    RUN_TEST(TestMapSphericalToCartesianCoordinates);
    //RUN_TEST(TestCreateCubeMap);
    RUN_TEST(TestRayIntersectTriangleHit);
    RUN_TEST(TestRayIntersectTriangleMiss);
    RUN_TEST(TestRayIntersectTriangleHitUV);
    RUN_TEST(TestRayIntersectTriangleMTBarycentricCoordsIssue);

    RUN_TEST(TestRayIntersectTriangleMeshFlatShading);
    RUN_TEST(TestRayIntersectTriangleMeshSmoothShading);

    RUN_TEST(TestNearestSampling);
    RUN_TEST(TestBilinearSampling);

    free(memoryArena.base);

    return UNITY_END();
}
