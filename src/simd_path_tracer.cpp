void sp_PathTraceTile(sp_Context *ctx, Tile tile)
{
    sp_Camera *camera = ctx->camera;
    ImagePlane *imagePlane = camera->imagePlane;
    vec4 *pixels = imagePlane->pixels;

    for (u32 y = 0; y < imagePlane->height; y++)
    {
        for (u32 x = 0; x < imagePlane->width; x++)
        {
            pixels[x + y * imagePlane->width] = Vec4(1, 0, 1, 1);
        }
    }
}
