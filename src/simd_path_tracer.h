/* SIMD Path Tracer

TODO:
- Writing single colour to output image
- Basic ray triangle intersection
 */
#pragma once

struct ImagePlane
{
    vec4 *pixels;
    u32 width;
    u32 height;
};

struct sp_Camera
{
    f32 halfPixelWidth;
    f32 halfPixelHeight;

    f32 halfFilmWidth;
    f32 halfFilmHeight;

    vec3 cameraRight;
    vec3 cameraUp;
    vec3 cameraPosition;
    vec3 filmCenter;

    ImagePlane *imagePlane;
};

struct sp_Context
{
    // Camera
    sp_Camera *camera;

    // Scene
    // Mesh data
    // Material data
    // Texture data
    // Broadphase structure
};

void sp_PathTraceTile(sp_Context *ctx, Tile tile);
