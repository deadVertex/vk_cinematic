#include "lib.h"

#include "math_lib.h"
#include "mesh.h"
#include "aabb.h"

#include <cstdio>

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

    scene->backgroundMaterial = Material_Black;

    AddEntity(scene, Vec3(0, 0, 0), Quat(Vec3(1, 0, 0), PI * -0.5f), Vec3(50),
        Mesh_Plane, Material_CheckerBoard);

    // Disk light
    AddEntity(
        scene, Vec3(-4, 2, -5), Quat(Vec3(0, 1, 0), PI * 0.25f), Vec3(4), Mesh_Disk, Material_BlueLight);
    // TODO: Set material rather than radiance value directly
    AddDiskLight(scene, Vec3(-4, 2, -5), Quat(Vec3(0, 1, 0), PI * 0.25f), 2.0f, Vec3(0.4, 0.6, 1) * 4.0f);

    AddEntity(
        scene, Vec3(4, 2, 5), Quat(Vec3(0, 1, 0), PI * -0.75f), Vec3(4), Mesh_Disk, Material_OrangeLight);
    // TODO: Set material rather than radiance value directly
    AddDiskLight(scene, Vec3(4, 2, 5), Quat(Vec3(0, 1, 0), PI * -0.75f), 2.0f, Vec3(10, 8, 7.5));

    //AddEntity(
        //scene, Vec3(4, 2, 5), Quat(Vec3(0, 1, 0), PI * 0.25f), Vec3(4), Mesh_Disk, Material_OrangeLight);
#if 0
    AddEntity(scene, Vec3(0, 10, 0), Quat(Vec3(1, 0, 0), PI * 0.5f), Vec3(5),
        Mesh_Sphere, Material_WhiteLight);
    AddSphereLight(scene, Vec3(0, 10, 0), Vec3(1), 5.0f * 0.5f);
#endif
    // TODO: Set lights via material?
    //SetAmbientLight(scene, Vec3(0.4, 0.6, 1) * 0.2); // NOTE: This needs to match backgroundMaterial for ray tracer

#if 0
    for (u32 z = 0; z < 4; ++z)
    {
        for (u32 x = 0; x < 4; ++x)
        {
            vec3 origin = Vec3(-8, 1, -8);
            vec3 p = origin + Vec3((f32)x, 0, (f32)z) * 5.0f;
            AddEntity(
                scene, p, Quat(), Vec3(1), Mesh_Sphere, Material_Red);
            //AddSphereLight(scene, p, Vec3(0.4, 0.6, 1) * 10.0f, 0.5f);
        }
    }
#endif

    AddEntity(
            scene, Vec3(0,-0.7,0), Quat(), Vec3(20), Mesh_Bunny, Material_Red);
}
