struct HdrCubeMap
{
    HdrImage images[6];
};

struct BasisVectors
{
    vec3 forward;
    vec3 up;
    vec3 right;
};

internal BasisVectors MapCubeMapLayerIndexToBasisVectors(u32 layerIndex)
{
    vec3 forward = {};
    vec3 up = {};
    vec3 right = {};

    // Mapping doesn't make sense, it seems like the right vector is always
    // flipped but visually it matches the ray tracer which is directly
    // sampling the equirectangular image
    // TODO: Does this match the cross product?
    switch (layerIndex)
    {
    case 0: // X+
        forward = Vec3(1, 0, 0);
        up = Vec3(0, 1, 0);
        right = Vec3(0, 0, -1);
        break;
    case 1: // X-
        forward = Vec3(-1, 0, 0);
        up = Vec3(0, 1, 0);
        right = Vec3(0, 0, 1);
        break;
    case 2: // Y+
        forward = Vec3(0, -1, 0);
        up = Vec3(0, 0, 1);
        right = Vec3(1, 0, 0);
        break;
    case 3: // Y-
        forward = Vec3(0, 1, 0);
        up = Vec3(0, 0, -1);
        right = Vec3(1, 0, 0);
        break;
    case 4: // Z+
        forward = Vec3(0, 0, 1);
        up = Vec3(0, 1, 0);
        right = Vec3(1, 0, 0);
        break;
    case 5: // Z-
        forward = Vec3(0, 0, -1);
        up = Vec3(0, 1, 0);
        right = Vec3(-1, 0, 0);
        break;
    default:
        InvalidCodePath();
        break;
    }

    BasisVectors result = {forward, up, right};

    return result;
}


// TODO: Does it rather make sense to pass in an already initialize HdrCubeMap
// structure which has been allocated from the textureUploadArena
internal HdrCubeMap CreateIrradianceCubeMap(HdrImage equirectangularImage,
    MemoryArena *arena, u32 width, u32 height, u32 samplesPerPixel = 32)
{
    HdrCubeMap result = {};

    u32 bytesPerPixel = sizeof(f32) * 4;
    u32 layerCount = 6;
    f32 sampleContribution = 1.0f / (f32)samplesPerPixel;

    for (u32 i = 0; i < 6; ++i)
    {
        result.images[i] = AllocateImage(width, height, arena);
    }

    RandomNumberGenerator rng = {};
    rng.state = 0x45BA12F3;

    for (u32 layerIndex = 0; layerIndex < 6; ++layerIndex)
    {
        // Map layer index to basis vectors for cube map face
        BasisVectors basis =
            MapCubeMapLayerIndexToBasisVectors(layerIndex);

        HdrImage *dstImage = result.images + layerIndex;

        for (u32 y = 0; y < dstImage->height; ++y)
        {
            for (u32 x = 0; x < dstImage->width; ++x)
            {
                // Convert pixel to cartesian direction vector
                f32 fx = (f32)x / (f32)width;
                f32 fy = (f32)y / (f32)height;

                // Map to -1 to 1
                fx = fx * 2.0f - 1.0f;
                fy = fy * 2.0f - 1.0f;

                vec3 dir = basis.forward + basis.right * fx + basis.up * fy;
                dir = Normalize(dir);

                vec4 irradiance = {};
                for (u32 sampleIndex = 0; sampleIndex < samplesPerPixel;
                     ++sampleIndex)
                {
                    // Compute random direction on hemi-sphere around
                    // rayHit.normal
                    vec3 offset = Vec3(RandomBilateral(&rng),
                            RandomBilateral(&rng), RandomBilateral(&rng));
                    vec3 sampleDir = Normalize(dir + offset);
                    if (Dot(sampleDir, dir) < 0.0f)
                    {
                        sampleDir = -sampleDir;
                    }

                    f32 cosine = Max(Dot(dir, sampleDir), 0.0f);

                    vec2 sphereCoords = ToSphericalCoordinates(sampleDir);
                    vec2 uv = MapToEquirectangular(sphereCoords);
                    vec4 sample = SampleImageNearest(equirectangularImage, uv);
                    irradiance += sample * sampleContribution * cosine;
                }

                irradiance.a = 1.0;

                // Write irradiance value to image
                u32 pixelIndex = (y * dstImage->width + x) * 4;
                *(vec4 *)(dstImage->pixels + pixelIndex) = irradiance;
            }
        }
    }

    return result;
}
