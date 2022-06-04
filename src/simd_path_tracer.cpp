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

vec3 FresnelSchlick(f32 cosineTheta, vec3 F0)
{
    vec3 result =
        F0 + (Vec3(1) - F0) * Pow(1.0f - cosineTheta, 5.0f);
    return result;
}

f32 DistributionGGX(vec3 N, vec3 H, f32 roughness)
{
    f32 a = roughness * roughness;
    f32 a2 = a * a;
    f32 NdotH = Max(Dot(N, H), 0.0f);
    f32 NdotH2 = NdotH * NdotH;

    f32 num = a2;
    f32 denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;

    return num / denom;
}

f32 GeometrySchlickGGX(f32 NdotV, f32 roughness)
{
    f32 r = (roughness + 1.0f);
    f32 k = (r * r) / 8.0f;

    f32 num = NdotV;
    f32 denom = NdotV * (1.0f - k) + k;

    return num / denom;
}

f32 GeometrySmith(vec3 N, vec3 V, vec3 L, f32 roughness)
{
    f32 NdotV = Max(Dot(N, V), 0.0f);
    f32 NdotL = Max(Dot(N, L), 0.0f);
    f32 ggx2 = GeometrySchlickGGX(NdotV, roughness);
    f32 ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

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
        sp_MaterialOutput materialOutput = {};
        sp_Material *material = sp_FindMaterialById(materialSystem, materialId);
        if (material != NULL)
        {
            materialOutput =
                sp_EvaluateMaterial(materialSystem, material, vertex);
        }
        else
        {
            // No material, set emission color to magenta for debuggging
            materialOutput.emission = Vec3(1, 0, 1);
        }

        f32 cosine = Max(0.0, Dot(normal, incomingDir));
        vec3 incomingRadiance = radiance;

#if RADIANCE_CLAMP
        incomingRadiance =
            Clamp(incomingRadiance, Vec3(0), Vec3(RADIANCE_CLAMP));
#endif

        vec3 L = incomingDir;
        vec3 N = normal;
        vec3 V = vertex->outgoingDir;
        vec3 H = Normalize(L + V);

        f32 exponent = 64.0f;
        f32 spec = Pow(Max(Dot(N, H), 0.0), exponent);
        vec3 specularColor = Vec3(1);

        vec3 F0 = Vec3(0.04f);
        vec3 F = FresnelSchlick(Max(Dot(H, V), 0.0f), F0);

        vec3 kS = F;
        vec3 kD = Vec3(1) - kS;

        f32 oneOverPI = 1.0f / PI;

        f32 roughness = 0.1f;
        f32 NDF = DistributionGGX(N, H, roughness);       
        f32 G   = GeometrySmith(N, V, L, roughness);

        vec3 numerator = NDF * G * F;
        f32 denominator =
            4.0f * Max(Dot(N, V), 0.0f) * Max(Dot(N, L), 0.0f) + 0.0001f;
        vec3 specular = numerator * (1.0f / denominator);

        vec3 albedo = materialOutput.albedo;

        radiance =
            materialOutput.emission +
            Hadamard(Hadamard(kD, albedo) * oneOverPI + specular, incomingRadiance) *
                cosine;
    }

    return radiance;
}

// TODO: Actual SIMD!
void sp_PathTraceTile(
    sp_Context *ctx, Tile tile, RandomNumberGenerator *rng, sp_Metrics *metrics)
{
    u64 start = __rdtsc();

    sp_Camera *camera = ctx->camera;
    ImagePlane *imagePlane = camera->imagePlane;
    vec4 *pixels = imagePlane->pixels;
    sp_MaterialSystem *materialSystem = ctx->materialSystem;

    u32 minX = tile.minX;
    u32 minY = tile.minY;
    u32 maxX = MinU32(tile.maxX, imagePlane->width);
    u32 maxY = MinU32(tile.maxY, imagePlane->height);

    // TODO: Expose these via parameter!
    u32 sampleCount = SAMPLES_PER_PIXEL;
    u32 bounceCount = 3; // TODO: Use MAX_BOUNCES constant

#if (SP_DEBUG_BROADPHASE_INTERSECTION_COUNT || SP_DEBUG_SURFACE_NORMAL ||      \
     SP_DEBUG_MIDPHASE_INTERSECTION_COUNT || SP_DEBUG_COSINE)
    bounceCount = 1;
#endif

#if (SP_DEBUG_BROADPHASE_INTERSECTION_COUNT || SP_DEBUG_SURFACE_NORMAL ||      \
     SP_DEBUG_MIDPHASE_INTERSECTION_COUNT || SP_DEBUG_PATH_LENGTH ||           \
     SP_DEBUG_COSINE)
    sampleCount = 1;
#endif

    for (u32 y = minY; y < maxY; y++)
    {
        for (u32 x = minX; x < maxX; x++)
        {
            vec4 color = Vec4(0, 0, 0, 1);

            // Multiple samples per pixel
            vec3 totalRadiance = {};
            for (u32 sample = 0; sample < sampleCount; sample++)
            {
                // Offset pixel position by 0.5 to sample from center
                vec2 pixelPosition = Vec2((f32)x, (f32)y) + Vec2(0.5);
                pixelPosition +=
                    Vec2(camera->halfPixelWidth * RandomBilateral(rng),
                        camera->halfPixelHeight * RandomBilateral(rng));

                // Calculate position on film plane that ray passes through
                vec3 filmP = {};
                sp_CalculateFilmPositions(camera, &filmP, &pixelPosition, 1);

                // Calculate ray origin and ray direction
                vec3 rayOrigin = camera->position;
                vec3 rayDirection = Normalize(filmP - camera->position);

                // TODO: Allocate from temp arena
                sp_PathVertex path[4] = {};
                u32 pathLength = 0;

                for (u32 bounce = 0; bounce < bounceCount; bounce++)
                {
                    // Trace ray through scene
                    sp_RayIntersectSceneResult result = sp_RayIntersectScene(
                        ctx->scene, rayOrigin, rayDirection, metrics);

                    metrics->values[sp_Metric_RaysTraced]++;

                    // Allocate path vertex
                    sp_PathVertex *pathVertex = path + pathLength++;

#if SP_DEBUG_BROADPHASE_INTERSECTION_COUNT
                    {
                        // TODO: Constant for max broadphase intersections?
                        f32 t = (f32)result.broadphaseIntersectionCount / 8.0f;
                        color = Lerp(Vec4(0, 1, 0, 1), Vec4(1, 0, 0, 1), t);
                        //color = (result.broadphaseIntersectionCount != 0)
                                    //? color
                                    //: Vec4(0, 0, 0, 1);
                    }
#endif
#if SP_DEBUG_MIDPHASE_INTERSECTION_COUNT
                    {
                        // TODO: Constant for max midphase intersections?
                        f32 t = (f32)result.midphaseIntersectionCount / 128.0f;
                        color = Lerp(Vec4(0, 1, 0, 1), Vec4(1, 0, 0, 1), t);
                        color = (result.midphaseIntersectionCount != 0)
                                    ? color
                                    : Vec4(0, 0, 0, 1);
                    }
#endif

                    if (result.t > 0.0f)
                    {
                        pathVertex->materialId = result.materialId;
                        pathVertex->worldPosition = rayOrigin + rayDirection * result.t;
                        pathVertex->outgoingDir = -rayDirection;
                        pathVertex->normal = result.normal;
                        pathVertex->uv = result.uv;

                        // FIXME: This is generating terrible results that are not uniform!
                        // Compute random direction on hemi-sphere around
                        // result.normal
                        vec3 dir = RandomDirectionOnHemisphere(result.normal, rng);
                        pathVertex->incomingDir = dir;

                        // Move new ray origin out of hit surface with a small
                        // offset in the direction of the surface normal to prevent
                        // self intersection
                        // TODO: Make this a configurable constant
                        f32 bias = 0.0001f;
                        rayOrigin = pathVertex->worldPosition + result.normal * bias;
                        rayDirection = dir;

                        // Count ray hit for metrics
                        metrics->values[sp_Metric_RayHitCount]++;

#if SP_DEBUG_COSINE
                        color = Vec4(Vec3(Max(0.0, Dot(result.normal, rayDirection))), 1);
                        //color = Dot(result.normal, dir) < 0.0f ? Vec4(1, 0, 0, 1) : Vec4(0, 0, 0, 1);
#endif

#if SP_DEBUG_SURFACE_NORMAL
                        color = Vec4(result.normal * 0.5f + Vec3(0.5f), 1);
#endif
                    }
                    else
                    {
                        pathVertex->materialId =
                            materialSystem->backgroundMaterialId;
                        pathVertex->outgoingDir = -rayDirection;

                        // Count ray miss for metrics
                        metrics->values[sp_Metric_RayMissCount]++;

                        // FIXME: Don't want to have a break in this loop, going to
                        // make it much harder to convert to SIMD
                        break;
                    }
                }

                // Compute lighting for single path by calculating incoming
                // radiance using the rendering equation
                vec3 radiance = ComputeRadianceForPath(
                        path, pathLength, materialSystem);
                totalRadiance += radiance * (1.0f / (f32)sampleCount);

                // Record number of paths traced for tile
                metrics->values[sp_Metric_PathsTraced]++;


#if SP_DEBUG_PATH_LENGTH
                color = Vec4((f32)pathLength / (f32)bounceCount, 0, 0, 1);
#endif
            }

#if !(SP_DEBUG_BROADPHASE_INTERSECTION_COUNT || SP_DEBUG_SURFACE_NORMAL ||     \
      SP_DEBUG_MIDPHASE_INTERSECTION_COUNT || SP_DEBUG_PATH_LENGTH ||          \
      SP_DEBUG_COSINE)
            color = Vec4(totalRadiance, 1);
#endif

            // Write final pixel value
            pixels[x + y * imagePlane->width] = color;
        }
    }

    // Record the total number of cycles spent in this function
    metrics->values[sp_Metric_CyclesElapsed] = __rdtsc() - start;
}

