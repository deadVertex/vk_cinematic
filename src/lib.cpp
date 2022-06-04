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

    AddEntity(scene, Vec3(0), Quat(Vec3(1, 0, 0), PI * -0.5f), Vec3(50),
        Mesh_Plane, Material_CheckerBoard);

    AddEntity(scene, Vec3(0, 1, 0), Quat(), Vec3(1), Mesh_Sphere, Material_Red);
}
