#pragma once

inline vec4 SampleImageNearest(HdrImage image, vec2 v)
{
    f32 fx = v.x * image.width;
    f32 fy = v.y * image.height;

    u32 x = (u32)Floor(fx);
    u32 y = (u32)Floor(fy);

    Assert(x <= image.width);
    Assert(y <= image.height);

    vec4 sample = *(((vec4 *)image.pixels) + (y * image.width + x));

    vec4 result = sample;
    return result;
}

inline vec4 GetPixel(HdrImage image, u32 x, u32 y)
{
    // TODO: Wrapping modes?
    Assert(x < image.width);
    Assert(y < image.height);

    u32 index = y * image.width + x;

    vec4 *pixels = (vec4 *)image.pixels;
    vec4 result = pixels[index];
    return result;
}

// TODO: Would be nice not to do 2 stages of clamping
inline vec4 SampleImageBilinear(HdrImage image, vec2 uv)
{
    Assert(uv.x >= 0.0f && uv.x <= 1.0f);
    Assert(uv.y >= 0.0f && uv.y <= 1.0f);

    // Compute pixel coordinates to sample
    f32 px = (uv.x * image.width) - 0.5f;
    f32 py = (uv.y * image.height) - 0.5f;

    // Clamp sample coords to valid range i.e. ClampToEdge wrapping mode
    px = Max(px, 0.0f);
    py = Max(py, 0.0f);

    u32 x0 = (u32)Floor(px);
    u32 x1 = x0 + 1;

    u32 y0 = (u32)Floor(py);
    u32 y1 = y0 + 1;

    f32 fx = px - (f32)x0;
    f32 fy = py - (f32)y0;

    // Clamp sample coords to valid range i.e. ClampToEdge wrapping mode
    x1 = MinU32(x1, image.width - 1);
    y1 = MinU32(y1, image.height - 1);

    // Sample 4 pixels
    vec4 samples[4];
    samples[0] = GetPixel(image, x0, y0);
    samples[1] = GetPixel(image, x1, y0);
    samples[2] = GetPixel(image, x0, y1);
    samples[3] = GetPixel(image, x1, y1);

    // Blend them together
    vec4 t0 = Lerp(samples[0], samples[1], fx);
    vec4 t1 = Lerp(samples[2], samples[3], fx);

    vec4 result = Lerp(t0, t1, fy);
    return result;
}

// NOTE: dir must be normalized
// TODO: Nothing uses this because it hard-codes the sampler method to nearest
inline vec4 SampleEquirectangularImage(HdrImage image, vec3 sampleDir)
{
    vec2 sphereCoords = ToSphericalCoordinates(sampleDir);
    vec2 uv = MapToEquirectangular(sphereCoords);
    //uv.y = 1.0f - uv.y; // Flip Y axis as usual
    vec4 sample = SampleImageNearest(image, uv);
    return sample;
}

// TODO: Don't assume 4 color channels
inline HdrImage AllocateImage(u32 width, u32 height, MemoryArena *arena)
{
    HdrImage image = {};
    image.width = width;
    image.height = height;
    image.pixels = AllocateArray(arena, f32, image.width * image.height * 4);

    return image;
}

inline void SetPixel(HdrImage *image, u32 x, u32 y, vec4 color)
{
    Assert(x < image->width);
    Assert(y < image->height);

    vec4 *pixels = (vec4 *)image->pixels;
    u32 index = y * image->width + x;
    pixels[index] = color;
}
