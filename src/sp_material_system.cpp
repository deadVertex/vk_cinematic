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
