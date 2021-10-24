internal HdrImage CreateCubeMap(HdrImage image, MemoryArena *arena, u32 width,
         u32 faceIndex)
{
    u32 channelCount = 4;
    HdrImage result = {};
    result.width = width;
    result.height = width;
    // FIXME: Don't just randomly assume 4 channels
    result.pixels = AllocateArray(arena, f32, channelCount * width * width);

    f32 xStart = 0.0f;
    f32 xEnd = 0.0f;

    f32 yStart = PI / 4.0f;
    f32 yEnd = 3.0f * PI / 4.0f;
    switch (faceIndex)
    {
        case 0: // X+
            xStart = -0.25f * PI;
            xEnd = 0.25f * PI;
            break;
        case 1: // X-
            xStart = 0.75f * PI;
            xEnd = 1.25f * PI;
            break;
        case 2: // Y+
            xStart = 0.0f;
            xEnd = PI;
            yStart = -0.25f * PI;
            yEnd = 0.25f * PI;
            break;
        case 3: // Y-
            xStart = 0.25f * PI;
            xEnd = 0.75f * PI;
            yStart = 0.75f * PI;
            yEnd = 1.25f * PI;
            break;
        case 5: // Z+
            xStart = 0.25f * PI;
            xEnd = 0.75f * PI;
            break;
        case 4: // Z-
            xStart = 1.25f * PI;
            xEnd = 1.75f * PI;
            break;
        default:
            InvalidCodePath();
            break;
    }

    Assert(xStart <= xEnd);
    Assert(yStart <= yEnd);

    f32 xInc = (xEnd - xStart) / (f32)width;
    f32 yInc = (yEnd - yStart) / (f32)width;
    for (u32 y = 0; y < width; ++y)
    {
        for (u32 x = 0; x < width; ++x)
        {
            vec2 sphereCoords = Vec2(xStart + x * xInc, yStart + y * yInc);
            vec2 uv = MapToEquirectangular(sphereCoords);

            u32 index = (y * width + x) * channelCount;
            vec4 *dst = (vec4 *)(result.pixels + index);
            uv.y = 1.0f - uv.y; // Flip Y axis?
            *dst = SampleImage(image, uv);
        }
    }

    return result;
}
