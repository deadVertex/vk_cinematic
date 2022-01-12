// TODO: Actual SIMD!
void sp_PathTraceTile(sp_Context *ctx, Tile tile)
{
    sp_Camera *camera = ctx->camera;
    ImagePlane *imagePlane = camera->imagePlane;
    vec4 *pixels = imagePlane->pixels;

    u32 minX = tile.minX;
    u32 minY = tile.minY;
    u32 maxX = MinU32(tile.maxX, imagePlane->width);
    u32 maxY = MinU32(tile.maxY, imagePlane->height);

    for (u32 y = minY; y < maxY; y++)
    {
        for (u32 x = minX; x < maxX; x++)
        {
            pixels[x + y * imagePlane->width] = Vec4(1, 0, 1, 1);
        }
    }
}
