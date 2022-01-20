#include "unity.h"

#include "platform.h"
#include "math_lib.h"
#include "tile.h"
#include "simd_path_tracer.h"

#include "custom_assertions.h"

#include "ray_intersection.cpp"

// Include cpp file for faster unity build
#include "simd_path_tracer.cpp"

void setUp(void)
{
    // set stuff up here
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

int main()
{
    UNITY_BEGIN();
    RUN_TEST(TestPathTraceSingleColor);
    RUN_TEST(TestPathTraceTile);
    RUN_TEST(TestConfigureCamera);
    RUN_TEST(TestCalculateFilmP);
    return UNITY_END();
}
