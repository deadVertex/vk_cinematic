#include "lib.h"

#include "math_lib.h"
#include "mesh.h"
#include "aabb.h"

#include <cstdio>

// TODO: Proper API for scene
internal void AddEntity(Scene *scene, vec3 position, quat rotation, vec3 scale,
    u32 mesh, u32 material)
{
    // TODO: Support non-uniform scaling in the ray tracer
    Assert(scale.x == scale.y && scale.x == scale.z);
    if (scene->count < scene->max)
    {
        Entity *entity = scene->entities + scene->count++;
        entity->position = position;
        entity->rotation = rotation;
        entity->scale = scale;
        entity->mesh = mesh;
        entity->material = material;

        // TODO: Don't duplicate this with sp_AddObjectToScene
        Aabb transformedAabb = TransformAabb(scene->meshAabbs[mesh].min,
            scene->meshAabbs[mesh].max, position, rotation, scale);

        entity->aabbMin = transformedAabb.min;
        entity->aabbMax = transformedAabb.max;
    }
}

// TODO: Derive radiance from material?
internal void AddSphereLight(
    Scene *scene, vec3 center, vec3 radiance, f32 radius)
{
    LightData *lightData = scene->lightData;
    if (lightData->sphereLightCount < MAX_SPHERE_LIGHTS)
    {
        SphereLightData *data =
            lightData->sphereLights + lightData->sphereLightCount++;
        data->position = center;
        data->radiance = radiance;
        data->radius = radius;
    }
}

internal void SetAmbientLight(Scene *scene, vec3 radiance)
{
    LightData *lightData = scene->lightData;
    lightData->ambientLight.radiance = radiance;
}

internal void AddDiskLight(
    Scene *scene, vec3 position, quat rotation, float radius, vec3 radiance)
{
    LightData *lightData = scene->lightData;
    Assert(lightData->diskLightCount < ArrayCount(lightData->diskLights));

    DiskLightData *diskLight =
        lightData->diskLights + lightData->diskLightCount++;

    diskLight->normal = RotateVector(Vec3(0, 0, 1), rotation);
    diskLight->position = position;
    diskLight->radiance = radiance;
    diskLight->radius = radius;
}

void GenerateScene(Scene *scene)
{
    scene->count = 0;
    scene->lightData->sphereLightCount = 0;
    scene->lightData->diskLightCount = 0;

    scene->backgroundMaterial = Material_HDRI;

    u32 materials[] = {Material_WhiteR10, Material_WhiteR30, Material_WhiteR60,
        Material_WhiteR80};
    for (u32 y = 0; y < 4; y++)
    {
        for (u32 x = 0; x < 4; x++)
        {
            vec3 p = Vec3(-4, -4, 0) + Vec3((f32)x, (f32)y, 0) * 2.5f;

            Assert(x < ArrayCount(materials));
            AddEntity(scene, p, Quat(), Vec3(1), Mesh_Sphere, materials[x]);
        }
    }
}
