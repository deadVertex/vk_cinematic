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

void GenerateScene(Scene *scene)
{
    scene->count = 0;
    scene->lightData->sphereLightCount = 0;

    AddEntity(scene, Vec3(0, 0, 0), Quat(Vec3(1, 0, 0), PI * -0.5f), Vec3(50),
        Mesh_Plane, Material_CheckerBoard);

    AddEntity(scene, Vec3(0, 10, 0), Quat(Vec3(1, 0, 0), PI * 0.5f), Vec3(5),
        Mesh_Sphere, Material_WhiteLight);
    AddSphereLight(scene, Vec3(0, 10, 0), Vec3(1), 5.0f * 0.5f);

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
}
