b32 sp_RegisterMaterial(
    sp_MaterialSystem *materialSystem, sp_Material material, u32 id)
{
    b32 result = false;
    if (materialSystem->count < SP_MAX_MATERIALS)
    {
        u32 index = materialSystem->count++;
        materialSystem->keys[index] = id;
        materialSystem->materials[index] = material;
        result = true;
    }
    return result;
}

sp_Material *sp_FindMaterialById(sp_MaterialSystem *materialSystem, u32 id)
{
    sp_Material *result = NULL;
    for (u32 i = 0; i < materialSystem->count; ++i)
    {
        if (materialSystem->keys[i] == id)
        { 
            result = materialSystem->materials + i;
            break;
        }
    }

    return result;
}

HdrImage *sp_FindTexture(sp_MaterialSystem *materialSystem, u32 id)
{
    HdrImage *result = NULL;
    for (u32 i = 0; i < materialSystem->imageCount; i++)
    {
        if (materialSystem->imageKeys[i] == id)
        {
            result = materialSystem->images + i;
            break;
        }
    }

    return result;
}

b32 sp_RegisterTexture(
    sp_MaterialSystem *materialSystem, HdrImage image, u32 id)
{
    b32 result = false;
    if (materialSystem->imageCount < SP_MAX_IMAGES)
    {
        u32 index = materialSystem->imageCount++;
        materialSystem->imageKeys[index] = id;
        materialSystem->images[index] = image;
        result = true;
    }
    return result;
}

sp_MaterialOutput sp_EvaluateMaterial(sp_MaterialSystem *materialSystem,
    sp_Material *material, sp_PathVertex *vertex)
{
    // This is basically the shader
    sp_MaterialOutput output = {};

    // Find Texture
    HdrImage *albedoTexture =
        sp_FindTexture(materialSystem, material->albedoTexture);
    HdrImage *emissionTexture =
        sp_FindTexture(materialSystem, material->emissionTexture);
    if (albedoTexture != NULL)
    {
        output.albedo = SampleImageNearest(*albedoTexture, vertex->uv).xyz;
    }
    else
    {
        output.albedo = material->albedo;
    }

    if (emissionTexture != NULL)
    {
        // Assume this is the background material
        // emission = materialSystem->backgroundEmission;

        // Compute UVs
        // Reverse direction as outgoingDir is the direction the light is
        // coming from, however we want to sample the equirectangular map in the
        // direction of the surface towards the light source
        vec2 sphereCoords = ToSphericalCoordinates(-vertex->outgoingDir);
        vec2 uv = MapToEquirectangular(sphereCoords);

        // Sample image
        uv.y = 1.0f - uv.y; // Flip Y axis as usual
        output.emission = SampleImageNearest(*emissionTexture, uv).xyz;
    }
    else
    {
        output.emission = material->emission;
    }

    return output;
}
