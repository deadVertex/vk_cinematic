#include "unity.h"

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "platform.h"
#include "math_lib.h"
#include "mesh.h"
#include "profiler.h"
#include "cpu_ray_tracer.cpp"
#include "mesh.cpp"

void setUp(void)
{
    // set stuff up here
}

void tearDown(void)
{
    // clean stuff up here
}

#if 0
void test_repro_bug()
{
    MeshData bunnyMesh = LoadMesh();
    RayTracer rayTracer = {};
    rayTracer.nodes = (BvhNode *)malloc(sizeof(BvhNode) * MAX_BVH_NODES);
    rayTracer.meshData = bunnyMesh;
    rayTracer.useAccelerationStructure = true;
    BuildBvh(&rayTracer, bunnyMesh);

    vec3 rotation = Vec3(-0.145001, 0.25, 0);
    quat cameraRotation =
        Quat(Vec3(0, 1, 0), rotation.y) * Quat(Vec3(1, 0, 0), rotation.x);
    rayTracer.viewMatrix =
        Translate(Vec3(0.0141059, 0.106304, 0.163337)) * Rotate(cameraRotation);

    u32 width = RAY_TRACER_WIDTH;
    u32 height = RAY_TRACER_HEIGHT;

    CameraConstants camera =
        CalculateCameraConstants(rayTracer.viewMatrix, width, height);

    u32 x = 423 / 16;
    u32 y = 541 / 16;

    vec3 filmP = CalculateFilmP(&camera, width, height, x, y);

    vec3 rayOrigin = camera.cameraPosition;
    vec3 rayDirection = Normalize(filmP - camera.cameraPosition);

    RayHitResult rayHit =
        TraceRayThroughScene(&rayTracer, rayOrigin, rayDirection);

    TEST_ASSERT_TRUE(rayHit.isValid);
}
#endif

int main()
{
    //RUN_TEST(test_repro_bug);
    RUN_TEST(Test_BuildAabbTree);
    return UNITY_END();
}
