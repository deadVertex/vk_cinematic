internal void TestTransformRayVsAabb(DebugDrawingBuffer *debugDrawBuffer)
{
    f32 scale = 5.0f;
    vec3 position = Vec3(2, 2, 0);
    mat4 modelMatrix = Translate(position) * Scale(Vec3(scale));
    mat4 invModelMatrix = Scale(Vec3(1.0f / scale)) * Translate(-position);

    // Aabb is not transformed
    vec3 boxMin = Vec3(-0.5);
    vec3 boxMax = Vec3(0.5);

    vec3 rayOrigin = Vec3(2, 2, 5);
    vec3 rayDirection = Vec3(0, 0, -1);

    // Ray is transformed by the inverse of the model matrix
    vec3 transformRayOrigin = TransformPoint(rayOrigin, invModelMatrix);
    vec3 transformRayDirection =
        Normalize(TransformVector(rayDirection, invModelMatrix));

    // Perform ray intersection in model space
    f32 t = RayIntersectAabb(
            boxMin, boxMax, transformRayOrigin, transformRayDirection);
    Assert(t >= 0.0f);

    // Results are then transformed by model matrix back to world space
    vec3 localHitPoint = transformRayOrigin + transformRayDirection * t;
    // Need to transform the hit point and normal, we can't reconstruct
    // these results in world space with just the t value (we can if only
    // support uniform scaling but thats not much use).
    vec3 worldHitPoint = TransformPoint(localHitPoint, modelMatrix);

    // Draw local space
    DrawBox(debugDrawBuffer, boxMin, boxMax, Vec3(0, 0.8, 0.8));
    DrawLine(debugDrawBuffer, transformRayOrigin,
        transformRayOrigin + transformRayDirection * 10.0f, Vec3(1, 0, 0));
    DrawPoint(debugDrawBuffer, localHitPoint, 0.25f, Vec3(1, 0, 0));

    // Draw world space
    vec3 worldBoxMin = TransformPoint(boxMin, modelMatrix);
    vec3 worldBoxMax = TransformPoint(boxMax, modelMatrix);
    DrawBox(debugDrawBuffer, worldBoxMin, worldBoxMax, Vec3(0, 0.8, 0.8));
    DrawLine(debugDrawBuffer, rayOrigin,
        rayOrigin + rayDirection * 10.0f, Vec3(1, 0, 0));
    DrawPoint(debugDrawBuffer, worldHitPoint, 0.25f, Vec3(1, 0, 0));
}

internal void TestTransformRayVsTriangle(DebugDrawingBuffer *debugDrawBuffer)
{
    vec3 position = Vec3(2, 0, 0);
    f32 scale = 5.0f;
    quat rotation = Quat(Vec3(1, 0, 0), PI * -0.25f);
    mat4 modelMatrix = Translate(position) * Rotate(rotation) * Scale(Vec3(scale));
    mat4 invModelMatrix = Scale(Vec3(1.0f / scale)) *
                          Rotate(Conjugate(rotation)) * Translate(-position);

    // clang-format off
    // triangle vertices are not transformed
    vec3 vertices[3] = {
        {  0.5, -0.5, 0.0 },
        {  0.0,  0.5, 0.0 },
        { -0.5, -0.5, 0.0 }
    };
    // clang-format on

    vec3 rayOrigin = Vec3(2, 1, 5);
    vec3 rayDirection = Vec3(0, 0, -1);

    // Ray is transformed by the inverse of the model matrix
    vec3 transformRayOrigin = TransformPoint(rayOrigin, invModelMatrix);
    vec3 transformRayDirection =
        Normalize(TransformVector(rayDirection, invModelMatrix));

    // Perform ray intersection test in model space
    RayIntersectTriangleResult result = RayIntersectTriangle(transformRayOrigin,
        transformRayDirection, vertices[0], vertices[1], vertices[2]);
    //Assert(result.t >= 0.0f);

    // Results are then transformed by model matrix back to world space
    vec3 localHitPoint = transformRayOrigin + transformRayDirection * result.t;
    // Need to transform the hit point and normal, we can't reconstruct
    // these results in world space with just the t value (we can if only
    // support uniform scaling but thats not much use).
    vec3 worldHitPoint = TransformPoint(localHitPoint, modelMatrix);
    vec3 worldNormal = Normalize(TransformVector(result.normal, modelMatrix));

    // Draw local space
    DrawTriangle(debugDrawBuffer, vertices[0], vertices[1], vertices[2],
        Vec3(0, 0.8, 0.8));
    DrawLine(debugDrawBuffer, transformRayOrigin,
        transformRayOrigin + transformRayDirection * 10.0f, Vec3(1, 0, 0));
    DrawPoint(debugDrawBuffer, localHitPoint, 0.25f, Vec3(1, 0, 0));
    DrawLine(debugDrawBuffer, localHitPoint, localHitPoint + result.normal,
        Vec3(1, 0, 1));

    // Draw world space
    vec3 worldVertices[3];
    worldVertices[0] = TransformPoint(vertices[0], modelMatrix);
    worldVertices[1] = TransformPoint(vertices[1], modelMatrix);
    worldVertices[2] = TransformPoint(vertices[2], modelMatrix);
    DrawTriangle(debugDrawBuffer, worldVertices[0], worldVertices[1],
        worldVertices[2], Vec3(0, 0.8, 0.8));
    DrawLine(debugDrawBuffer, rayOrigin,
        rayOrigin + rayDirection * 10.0f, Vec3(1, 0, 0));
    DrawPoint(debugDrawBuffer, worldHitPoint, 0.25f, Vec3(1, 0, 0));
    DrawLine(debugDrawBuffer, worldHitPoint, worldHitPoint + worldNormal,
        Vec3(1, 0, 1));
}

