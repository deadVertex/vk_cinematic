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

enum
{
    CubeMapFace_PositiveX,
    CubeMapFace_NegativeX,
    CubeMapFace_PositiveY,
    CubeMapFace_NegativeY,
    CubeMapFace_PositiveZ,
    CubeMapFace_NegativeZ,
};

internal vec3 MapCubeMapLayerIndexToVector(u32 layerIndex)
{
    vec3 result = {};

    switch (layerIndex)
    {
        case CubeMapFace_PositiveX:
            result = Vec3(1, 0, 0);
            break;
        case CubeMapFace_NegativeX:
            result = Vec3(-1, 0, 0);
            break;
        case CubeMapFace_PositiveY:
            result = Vec3(0, 1, 0);
            break;
        case CubeMapFace_NegativeY:
            result = Vec3(0, -1, 0);
            break;
        case CubeMapFace_PositiveZ:
            result = Vec3(0, 0, 1);
            break;
        case CubeMapFace_NegativeZ:
            result = Vec3(0, 0, -1);
            break;
        default:
            InvalidCodePath();
            break;
    }

    return result;
}

internal BasisVectors MapCubeMapLayerIndexToBasisVectors(u32 layerIndex)
{
    vec3 forward = {};
    vec3 up = {};
    vec3 right = {};

    // NOTE: Doesn't match the cross product
    // https://en.wikipedia.org/wiki/Cube_mapping#/media/File:Cube_map.svg
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
        forward = Vec3(0, 1, 0);
        up = Vec3(0, 0, -1);
        right = Vec3(1, 0, 0);
        break;
    case 3: // Y-
        forward = Vec3(0, -1, 0);
        up = Vec3(0, 0, 1);
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

                // Flip Y axis
                fy = 1.0f - fy;

                // Map to -1 to 1
                fx = fx * 2.0f - 1.0f;
                fy = fy * 2.0f - 1.0f;

                vec3 dir = basis.forward + basis.right * fx + basis.up * fy;
                dir = Normalize(dir);

                vec3 irradiance = {};

// From https://learnopengl.com/PBR/IBL/Diffuse-irradiance
#if IRRADIANCE_CUBEMAP_USE_UNIFORM_SAMPLING
                vec3 normal = dir;
                vec3 tangent = Normalize(Cross(basis.up, normal));
                vec3 bitangent = Normalize(Cross(normal, tangent));

                f32 sampleDelta = 0.1f; // TODO: Parameterize
                u32 sampleCount = 0;
                for (f32 phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta)
                {
                    for (f32 theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta)
                    {
                        // Get sample vector in tangent space
                        vec3 tangentDir = MapSphericalToCartesianCoordinates(
                            Vec2(phi, theta));

                        // Map vector from tangent space to world space so we
                        // can sample the environment map
                        vec3 worldDir = normal * tangentDir.y +
                                        tangent * tangentDir.x +
                                        bitangent * tangentDir.z;

                        // Sample environment map
                        vec2 sphereCoords = ToSphericalCoordinates(worldDir);
                        vec2 uv = MapToEquirectangular(sphereCoords);
                        uv.y = 1.0f - uv.y; // Flip Y axis as usual
                        vec3 sample =
                            SampleImageBilinear(equirectangularImage, uv).xyz;

                        // Perform radiance clamp
                        vec3 radiance =
                            Clamp(sample, Vec3(0), Vec3(RADIANCE_CLAMP));

                        // Add irradiance contribution for integral
                        irradiance += radiance * Cos(theta) * Sin(theta);
                        sampleCount++;
                    }
                }

                irradiance = PI * irradiance * (1.0f / (f32)sampleCount);

#else // Random sampling
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
                    uv.y = 1.0f - uv.y; // Flip Y axis as usual
                    vec3 sample = SampleImageBilinear(equirectangularImage, uv).xyz;

                    // Perform radiance clamp
                    vec3 radiance =
                        Clamp(sample * cosine, Vec3(0), Vec3(RADIANCE_CLAMP));

                    irradiance += radiance * sampleContribution;
                }
#endif

                // Write irradiance value to image
                u32 pixelIndex = (y * dstImage->width + x) * 4;
                *(vec4 *)(dstImage->pixels + pixelIndex) = Vec4(irradiance, 1);
            }
        }
    }

    return result;
}
