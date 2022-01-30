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

        // Flip y axis so we move down the image plane as y increases
        fy = 1.0f - fy;

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

struct sp_PathVertex
{
    u32 materialId;
    vec3 worldPosition;
    vec3 outgoingDir;
    vec3 incomingDir;
    vec3 normal;
};

vec3 ComputeRadianceForPath(
    sp_PathVertex *path, u32 pathLength, sp_MaterialSystem *materialSystem)
{
    vec3 radiance = {};
    for (i32 i = pathLength - 1; i >= 0; i--)
    {
        sp_PathVertex *vertex = path + i;

        // Fetch values out of path vertex
        vec3 incomingDir = vertex->incomingDir;
        vec3 normal = vertex->normal;
        u32 materialId = vertex->materialId;

        // Set surface albedo and emission from material
        vec3 albedo = {};
        vec3 emission = {};
        sp_Material *material = sp_FindMaterialById(materialSystem, materialId);
        if (material != NULL)
        {
            albedo = material->albedo;
        }
        else
        {
            // Assume this is the background material
            emission = materialSystem->backgroundEmission;
        }

        f32 cosine = Max(0.0, Dot(normal, incomingDir));
        vec3 incomingRadiance = radiance;

        radiance = emission + Hadamard(albedo, incomingRadiance) * cosine;
    }

    return radiance;
}

// TODO: Actual SIMD!
void sp_PathTraceTile(sp_Context *ctx, Tile tile, RandomNumberGenerator *rng)
{
    sp_Camera *camera = ctx->camera;
    ImagePlane *imagePlane = camera->imagePlane;
    vec4 *pixels = imagePlane->pixels;
    sp_MaterialSystem *materialSystem = ctx->materialSystem;

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

            sp_PathVertex path[4] = {};
            u32 pathLength = 0;

            u32 bounceCount = 4; // TODO: Use MAX_BOUNCES constant
#if (SP_DEBUG_BROADPHASE_INTERSECTION_COUNT || SP_DEBUG_SURFACE_NORMAL)
            bounceCount = 1;
#endif
            vec4 color = Vec4(0, 0, 0, 1);
            for (u32 bounce = 0; bounce < bounceCount; bounce++)
            {
                // Trace ray through scene
                sp_RayIntersectSceneResult result =
                    sp_RayIntersectScene(ctx->scene, rayOrigin, rayDirection);

                // If ray intersection set output color to magenta otherwise leave black

                // Allocate path vertex
                sp_PathVertex *pathVertex = path + pathLength++;

                if (result.t > 0.0f)
                {
                    pathVertex->materialId = result.materialId;
                    pathVertex->worldPosition = rayOrigin + rayDirection * result.t;
                    pathVertex->outgoingDir = -rayDirection;
                    pathVertex->normal = result.normal;

                    // Compute random direction on hemi-sphere around
                    // result.normal
                    vec3 offset = Vec3(RandomBilateral(rng),
                            RandomBilateral(rng), RandomBilateral(rng));
                    vec3 dir = Normalize(result.normal + offset);
                    if (Dot(dir, result.normal) < 0.0f)
                    {
                        dir = -dir;
                    }

                    pathVertex->incomingDir = dir;

                    // Move new ray origin out of hit surface with a small
                    // offset in the direction of the surface normal to prevent
                    // self intersection
                    // TODO: Make this a configurable constant
                    f32 bias = 0.0001f;
                    rayOrigin = pathVertex->worldPosition + result.normal * bias;
                    rayDirection = dir;

#if SP_DEBUG_SURFACE_NORMAL
                    color = Vec4(result.normal * 0.5f + Vec3(0.5f), 1);
#endif
                }
                else
                {
                    // TODO: Constant for background material
                    pathVertex->materialId = U32_MAX;
                    pathVertex->outgoingDir = -rayDirection;

                    // FIXME: Don't want to have a break in this loop, going to
                    // make it much harder to convert to SIMD
                    break;
                }

#if SP_DEBUG_BROADPHASE_INTERSECTION_COUNT
                {
                    // TODO: Constant for max broadphase intersections?
                    f32 t = (f32)result.broadphaseIntersectionCount / 8.0f;
                    color = Lerp(Vec4(0, 1, 0, 1), Vec4(1, 0, 0, 1), t);
                }
#endif
            }

#if !(SP_DEBUG_BROADPHASE_INTERSECTION_COUNT || SP_DEBUG_SURFACE_NORMAL)
            // Compute lighting for single path by calculating incoming
            // radiance using the rendering equation
            vec3 radiance = ComputeRadianceForPath(
                path, pathLength, materialSystem);
            color = Vec4(radiance, 1);
#endif

            // Write final pixel value
            pixels[x + y * imagePlane->width] = color;
        }
    }
}

