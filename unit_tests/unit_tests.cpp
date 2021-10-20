#include "unity.h"

#include "cpu_ray_tracer.h"

#include "cmdline.cpp"

void setUp(void)
{
    // set stuff up here
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
        }
    }
    // Create cube map by sampling equirectangular image
}
#endif

int main()
{
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
    return UNITY_END();
}
