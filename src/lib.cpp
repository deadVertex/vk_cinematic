#include "lib.h"

#include "math_lib.h"
#include "mesh.h"
#include "aabb.h"

#include <cstdio>

internal void AddEntity(Scene *scene, vec3 position, quat rotation, vec3 scale,
    u32 mesh, u32 material, Aabb *meshAabbs)
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
        Aabb transformedAabb = TransformAabb(meshAabbs[mesh].min,
            meshAabbs[mesh].max, position, rotation, scale);

        entity->aabbMin = transformedAabb.min;
        entity->aabbMax = transformedAabb.max;
    }
}

void GenerateScene(Scene *scene, Aabb *meshAabbs)
{
    scene->count = 0;

    AddEntity(scene, Vec3(0, 0, 0), Quat(Vec3(1, 0, 0), PI * -0.5f), Vec3(50),
        Mesh_Plane, Material_CheckerBoard, meshAabbs);

    //AddEntity(scene, Vec3(0, 10, 0), Quat(Vec3(1, 0, 0), PI * 0.5f), Vec3(5),
        //Mesh_Sphere, Material_WhiteLight, meshAabbs);

    for (u32 z = 0; z < 4; ++z)
    {
        for (u32 x = 0; x < 4; ++x)
        {
            vec3 origin = Vec3(-8, 1, -8);
            vec3 p = origin + Vec3((f32)x, 0, (f32)z) * 5.0f;
            AddEntity(scene, p, Quat(), Vec3(1), Mesh_Sphere, Material_WhiteLight,
                meshAabbs);
        }
    }
}
