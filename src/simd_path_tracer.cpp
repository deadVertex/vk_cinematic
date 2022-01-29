void sp_ConfigureCamera(
    sp_Camera *camera, ImagePlane *imagePlane, vec3 position, quat rotation,
    f32 filmDistance)
{
    camera->imagePlane = imagePlane;
    camera->position = position;

    // Compute basis vectors from rotation
    camera->basis.right = RotateVector(Vec3(1, 0, 0), rotation);
    camera->basis.up = RotateVector(Vec3(0, 1, 0), rotation);
    camera->basis.forward = RotateVector(Vec3(0, 0, -1), rotation);

    // Compute filmCenter in world space
    camera->filmCenter = position + camera->basis.forward * filmDistance;

    // Compute half dimensions for a pixel in 0 to 1 range
    camera->halfPixelWidth = 0.5f / (f32)imagePlane->width;
    camera->halfPixelHeight = 0.5f / (f32)imagePlane->height;

    // Map film dimensions to 0-1 range based on largest dimension to account
    // for aspect ratio
    f32 filmWidth = 1.0f;
    f32 filmHeight = 1.0f;
    if (imagePlane->width > imagePlane->height)
    {
        filmHeight = (f32)imagePlane->height / (f32)imagePlane->width;
    }
    else if (imagePlane->width < imagePlane->height)
    {
        filmWidth = (f32)imagePlane->width / (f32)imagePlane->height;
    }

    // Compute half dimensions for normalized film dimensions
    camera->halfFilmWidth = 0.5f * filmWidth;
    camera->halfFilmHeight = 0.5f * filmHeight;
}

u32 sp_CalculateFilmPositions(
    sp_Camera *camera, vec3 *filmPositions, vec2 *pixelPositions, u32 count)
{
    ImagePlane *imagePlane = camera->imagePlane;

    for (u32 i = 0; i < count; ++i)
    {
        // Compute 0 to 1 range
        f32 fx = pixelPositions[i].x / (f32)imagePlane->width;
        f32 fy = pixelPositions[i].y / (f32)imagePlane->height;

        // Map to -1 to 1 range
        fx = fx * 2.0f - 1.0f;
        fy = fy * 2.0f - 1.0f;

        // Compute positions on film in world space
        filmPositions[i] = (camera->halfFilmWidth * fx) * camera->basis.right;
        filmPositions[i] += (camera->halfFilmHeight * fy) * camera->basis.up;
        filmPositions[i] += camera->filmCenter;
    }

    return count;
}

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
            // TODO: Multiple samples per pixel
            // Offset pixel position by 0.5 to sample from center
            vec2 pixelPosition = Vec2((f32)x, (f32)y) + Vec2(0.5);

            // Calculate position on film plane that ray passes through
            vec3 filmP = {};
            sp_CalculateFilmPositions(camera, &filmP, &pixelPosition, 1);

            // Calculate ray origin and ray direction
            vec3 rayOrigin = camera->position;
            vec3 rayDirection = Normalize(filmP - camera->position);

            // TODO: Multiple bounces

            // Trace ray through scene
            RayIntersectCollisionWorldResult result =
                RayIntersectCollisionWorld(
                    ctx->collisionWorld, rayOrigin, rayDirection);

            // If ray intersection set output color to magenta otherwise leave black

            vec4 color = Vec4(0, 0, 0, 1);
            if (result.t > 0.0f)
            {
                color = Vec4(1, 0, 1, 1);
            }

            // Write final pixel value
            pixels[x + y * imagePlane->width] = color;
        }
    }
}

