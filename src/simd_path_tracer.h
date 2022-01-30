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

struct Basis
{
    vec3 right;
    vec3 up;
    vec3 forward;
};

struct sp_Camera
{
    Basis basis;
    vec3 position;
    vec3 filmCenter;


    ImagePlane *imagePlane;

    f32 halfPixelWidth;
    f32 halfPixelHeight;

    f32 halfFilmWidth;
    f32 halfFilmHeight;

};

struct sp_Context
{
    // Camera
    sp_Camera *camera;

    // Scene
    sp_Scene *scene;

    // Material data
    sp_MaterialSystem *materialSystem;

    // Texture data
};
