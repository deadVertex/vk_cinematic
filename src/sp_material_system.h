#pragma once

struct sp_Material
{
    vec3 albedo;
};

#define SP_MAX_MATERIALS 32

struct sp_MaterialSystem
{
    u32 keys[SP_MAX_MATERIALS];
    sp_Material materials[SP_MAX_MATERIALS];
    u32 count;
};
