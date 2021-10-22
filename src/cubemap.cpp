internal HdrImage CreateCubeMap(HdrImage image, MemoryArena *arena, u32 width)
{
    u32 channelCount = 4;
    HdrImage result = {};
    result.width = width;
    result.height = width;
    // FIXME: Don't just randomly assume 4 channels
    result.pixels = AllocateArray(arena, f32, channelCount * width * width);

    // For +Z vector
    f32 start = PI / 4.0f;
    f32 end = 3.0f * PI / 4.0f;
    f32 inc = (end - start) / (f32)width;
    for (u32 y = 0; y < width; ++y)
    {
        for (u32 x = 0; x < width; ++x)
        {
            vec2 sphereCoords = Vec2((f32)x * inc, (f32)y * inc);
            vec2 uv = MapToEquirectangular(sphereCoords);

            u32 index = (y * width + x) * channelCount;
            vec4 *dst = (vec4 *)(result.pixels + index);
            uv.y = 1.0f - uv.y; // Flip Y axis?
            *dst = SampleImage(image, uv);
        }
    }

    return result;
}
