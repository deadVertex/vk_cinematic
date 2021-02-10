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

// TODO: Do we need to normalize cross products?
internal f32 RayIntersectTriangle(
    vec3 rayOrigin, vec3 rayDirection, vec3 a, vec3 b, vec3 c)
{
    vec3 edgeCA = c - a;
    vec3 edgeBA = b - a;
    vec3 normal = Cross(edgeBA, edgeCA);
    f32 distance = Dot(normal, a);

#if 0
    {
        vec3 centroid = (a + b + c) * (1.0f / 3.0f);
        DrawLine(centroid, centroid + Normalize(normal), Vec3(1, 0, 1));
    }
#endif

    // Segment intersect plane
    f32 denom = Dot(normal, rayDirection);
    f32 t = Dot(a - rayOrigin, normal) / denom;
    //f32 t = (distance - Dot(normal, rayOrigin)) / denom;

    if (t > 0.0f)
    {
        vec3 p = rayOrigin + t * rayDirection;

        // FIXME: This is super dicey, converted function for triangle vs line
        // segment intersection not ray triangle.
        //
        //
        //
        // Compute plane equation for each edge
        vec3 normalCA = Cross(normal, edgeCA);
        f32 distCA = Dot(a, normalCA);
        vec3 normalBA = Cross(edgeBA, normal);
        f32 distBA = Dot(a, normalBA);

        vec3 edgeBC = b - c;
        vec3 normalBC = Cross(normal, edgeBC);
        f32 distBC = Dot(c, normalBC);

#if 0
        {
            vec3 centroidCA = (c + a) * 0.5f;
            vec3 centroidBA = (b + a) * 0.5f;
            vec3 centroidBC = (b + c) * 0.5f;
            vec3 color = Vec3(1, 0, 1);
            DrawLine(centroidCA, centroidCA + Normalize(normalCA), color);
            DrawLine(centroidBA, centroidBA + Normalize(normalBA), color);
            DrawLine(centroidBC, centroidBC + Normalize(normalBC), color);
        }
#endif

        b32 inCA = (Dot(normalCA, p) - distCA) < 0.0f;
        b32 inBA = (Dot(normalBA, p) - distBA) < 0.0f;
        b32 inBC = (Dot(normalBC, p) - distBC) < 0.0f;

        // TODO: Do this with Barycentric coordinates, then we only need to
        // compute 2 cross products

        if (inCA && inBA && inBC)
        {
            return t;
        }
    }

    return -1.0f;
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

    f32 filmDistance = 0.1f;
    vec3 filmCenter = cameraForward * filmDistance + cameraPosition;

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
            // Map to -1 to 1 range
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

            //f32 t = RayIntersectSphere(
                //Vec3(0, 0, 0), 0.5f, rayOrigin, rayDirection);
            //vec3 outputColor = (t < F32_MAX) ? Vec3(1, 0, 0) : Vec3(0, 0, 0);
            f32 t = RayIntersectTriangle(rayOrigin, rayDirection,
                        Vec3(-0.5f, -0.5f, 0.0f),
                        Vec3(0.5f, -0.5f, 0.0f),
                        Vec3(0.0, 0.5f, 0.0f));
            vec3 outputColor = (t > 0.0f) ? Vec3(1, 0, 1) : Vec3(0, 0, 0);

            outputColor *= 255.0f;
            u32 bgra = (0xFF000000 | ((u32)outputColor.z) << 16 |
                    ((u32)outputColor.y) << 8) | (u32)outputColor.x;

            pixels[y * width + x] = bgra;
        }
    }
}
