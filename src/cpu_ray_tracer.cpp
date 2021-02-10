#include "math_lib.h"

#define RAY_TRACER_WIDTH (1024 / 2)
#define RAY_TRACER_HEIGHT (768 / 2)

inline f32 RayIntersectSphere(vec3 center, f32 radius, vec3 rayOrigin, vec3 rayDirection)
{
    vec3 m = rayOrigin - center; // Use sphere center as origin

    f32 a = Dot(rayDirection, rayDirection);
    f32 b = 2.0f * Dot(m, rayDirection);
    f32 c = Dot(m, m) - radius * radius;

    f32 d = b*b - 4.0f * a * c; // Discriminant

    f32 t = F32_MAX;
    if (d > 0.0f)
    {
        f32 denom = 2.0f * a;
        f32 t0 = (-b - Sqrt(d)) / denom;
        f32 t1 = (-b + Sqrt(d)) / denom;

        t = t0; // Pick the entry point
    }

    return t;
}

internal void DoRayTracing(u32 width, u32 height, u32 *pixels)
{
    // TODO: Replace with view matrix
    vec3 cameraPosition = Vec3(0, 0, 1);
    vec3 cameraForward = Vec3(0, 0, -1);
    vec3 cameraRight = Normalize(Cross(Vec3(0, 1, 0), cameraForward));
    vec3 cameraUp = Normalize(Cross(cameraForward, cameraRight));

    f32 filmWidth = 1.0f;
    f32 filmHeight = 1.0f;
    if (width > height)
    {
        filmHeight = (f32)height / (f32)width;
    }
    else if (width < height)
    {
        filmWidth = (f32)width / (f32)height;
    }

    f32 filmDistance = 1.0f;
    vec3 filmCenter = -cameraForward * filmDistance + cameraPosition;

    f32 pixelWidth = 1.0f / (f32)width;
    f32 pixelHeight = 1.0f / (f32)height;

    // Used for multi-sampling
    f32 halfPixelWidth = 0.5f * pixelWidth;
    f32 halfPixelHeight = 0.5f * pixelHeight;

    f32 halfFilmWidth = filmWidth * 0.5f;
    f32 halfFilmHeight = filmHeight * 0.5f;

    for (u32 y = 0; y < height; ++y)
    {
        for (u32 x = 0; x < width; ++x)
        {
            f32 filmX = -1.0f + 2.0f * ((f32)x / (f32)width);
            f32 filmY = -1.0f + 2.0f * (1.0f - ((f32)y / (f32)height));

            //f32 offsetX =
                //filmX + halfPixelWidth + halfPixelWidth * RandomBilateral(rng);
            f32 offsetX = filmX + halfPixelWidth;
            f32 offsetY = filmY + halfPixelHeight;

            //f32 offsetY = filmY + halfPixelHeight +
                          //halfPixelHeight * RandomBilateral(rng);

            vec3 filmP = halfFilmWidth * offsetX * cameraRight +
                         halfFilmHeight * offsetY * cameraUp + filmCenter;

            vec3 rayOrigin = cameraPosition;
            vec3 rayDirection = Normalize(filmP - cameraPosition);

            f32 t = RayIntersectSphere(
                Vec3(0, 0, -5), 0.5f, rayOrigin, rayDirection);
            vec3 outputColor = (t < F32_MAX) ? Vec3(1, 0, 0) : Vec3(0, 0, 0);

            outputColor *= 255.0f;
            u32 bgra = (0xFF000000 | ((u32)outputColor.z) << 16 |
                    ((u32)outputColor.y) << 8) | (u32)outputColor.x;

            pixels[y * width + x] = bgra;
        }
    }
}
