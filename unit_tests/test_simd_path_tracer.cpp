#include "unity.h"

#include "platform.h"
#include "math_lib.h"
#include "tile.h"
#include "simd_path_tracer.h"

#include "custom_assertions.h"

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
    sp_PathTraceTile(&ctx, tile);

    // Then all of the pixels are set to magenta
    for (u32 i = 0; i < 16; i++)
    {
        AssertWithinVec4(EPSILON, Vec4(1, 0, 1, 1), pixels[i]);
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(TestPathTraceTile);
    return UNITY_END();
}
