#include "math_lib.h"
#include "cpu_ray_tracer.h"

internal void DumpMetrics(Metrics *metrics)
{
    LogMessage("Broad Phase Test Count: %llu / %llu",
        metrics->broadPhaseHitCount, metrics->broadPhaseTestCount);
    LogMessage("Mid Phase Test Count: %llu / %llu", metrics->midPhaseHitCount,
        metrics->midPhaseTestCount);
    LogMessage("Triangle Test Count: %llu / %llu", metrics->triangleHitCount,
        metrics->triangleTestCount);
    LogMessage("Mesh Test Count: %llu / %llu", metrics->meshHitCount,
        metrics->meshTestCount);

    LogMessage("Broadphase Cycles : %llu %llu %llu %g",
        metrics->broadphaseCycleCount, metrics->rayCount,
        metrics->broadphaseCycleCount / metrics->rayCount,
        (f64)metrics->broadphaseCycleCount /
            (f64)metrics->rayTraceSceneCycleCount);
    LogMessage("Build Model Matrix Cycles : %llu %llu %llu %llu %g",
        metrics->buildModelMatricesCycleCount, metrics->meshTestCount,
        metrics->buildModelMatricesCycleCount / metrics->meshTestCount,
        metrics->buildModelMatricesCycleCount / metrics->rayCount,
        (f64)metrics->buildModelMatricesCycleCount /
            (f64)metrics->rayTraceSceneCycleCount);
    LogMessage("Transform Ray Cycles : %llu %llu %llu %llu %g",
        metrics->transformRayCycleCount, metrics->meshTestCount,
        metrics->transformRayCycleCount / metrics->meshTestCount,
        metrics->transformRayCycleCount / metrics->rayCount,
        (f64)metrics->transformRayCycleCount /
            (f64)metrics->rayTraceSceneCycleCount);
    LogMessage("Ray Intersect Mesh Cycles : %llu %llu %llu %llu %g",
        metrics->rayIntersectMeshCycleCount, metrics->meshTestCount,
        metrics->rayIntersectMeshCycleCount / metrics->meshTestCount,
        metrics->rayIntersectMeshCycleCount / metrics->rayCount,
        (f64)metrics->rayIntersectMeshCycleCount /
            (f64)metrics->rayTraceSceneCycleCount);
    LogMessage("Ray Trace Scene Cycles : %llu %llu %llu",
        metrics->rayTraceSceneCycleCount, metrics->rayCount,
        metrics->rayTraceSceneCycleCount / metrics->rayCount);
    LogMessage("Ray Midphase Test Cycle Count : %llu %llu %g",
        metrics->midPhaseTestCycleCount, metrics->midPhaseTestCount,
        (f64)metrics->midPhaseTestCycleCount /
            (f64)metrics->rayIntersectMeshCycleCount);
    LogMessage("Ray Triangle Test Cycle Count : %llu %llu %g",
        metrics->triangleTestCycleCount, metrics->triangleTestCount,
        (f64)metrics->triangleTestCycleCount /
            (f64)metrics->rayIntersectMeshCycleCount);
    LogMessage("Copy Memory Cycle Count : %llu %llu %g",
        metrics->copyMemoryCycleCount, metrics->copyMemoryCallCount,
        (f64)metrics->copyMemoryCycleCount /
            (f64)metrics->rayIntersectMeshCycleCount);

    LogMessage("Ray Count: %llu", metrics->rayCount);
    LogMessage("Total Sample Count: %llu", metrics->totalSampleCount);
    LogMessage("Total Pixel Count: %llu", metrics->totalPixelCount);
}

inline f32 RayIntersectSphere(vec3 center, f32 radius, vec3 rayOrigin, vec3 rayDirection)
{
    vec3 m = rayOrigin - center; // Use sphere center as origin

    f32 a = Dot(rayDirection, rayDirection);
    f32 b = 2.0f * Dot(m, rayDirection);
    f32 c = Dot(m, m) - radius * radius;

    f32 d = b*b - 4.0f * a * c; // Discriminant

    f32 t = F32_MAX;
    if (d > 0.0f)
    {
        f32 denom = 2.0f * a;
        f32 t0 = (-b - Sqrt(d)) / denom;
        f32 t1 = (-b + Sqrt(d)) / denom;

        t = t0; // Pick the entry point
    }

    return t;
}

inline f32 RayIntersectAabb(
    vec3 boxMin, vec3 boxMax, vec3 rayOrigin, vec3 rayDirection)
{
    f32 tmin = 0.0f;
    f32 tmax = F32_MAX;

    b32 negateNormal = false;
    u32 normalIdx = 0;

    for (u32 axis = 0; axis < 3; ++axis)
    {
        f32 s = rayOrigin.data[axis];
        f32 d = rayDirection.data[axis];
        f32 min = boxMin.data[axis];
        f32 max = boxMax.data[axis];

        if (Abs(d) < EPSILON)
        {
            // Ray is parallel to axis plane
            if (s < min || s > max)
            {
                // No collision
                tmin = -1.0f;
                break;
            }
        }
        else
        {
            f32 a = (min - s) / d;
            f32 b = (max - s) / d;

            f32 t0 = Min(a, b);
            f32 t1 = Max(a, b);

            if (t0 > tmin)
            {
                negateNormal = (d >= 0.0f);
                normalIdx = axis;
                tmin = t0;
            }

            tmax = Min(tmax, t1);

            if (tmin > tmax)
            {
                // No collision
                tmin = -1.0f;
                break;
            }
        }
    }

    return tmin;
}

internal RayTracerMesh BuildBvh(RayTracer *rayTracer, MeshData meshData)
{
    // FIXME: This should use a tempArena rather than the stack
    u32 unmergedNodeIndices[MAX_BVH_NODES];
    u32 unmergedNodeCount = 0;

    // Build BvhNode for each triangle
    u32 triangleCount = meshData.indexCount / 3;
    u32 nodesOffset = rayTracer->nodeCount;
    for (u32 triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
    {
        u32 indices[3];
        indices[0] = meshData.indices[triangleIndex * 3 + 0];
        indices[1] = meshData.indices[triangleIndex * 3 + 1];
        indices[2] = meshData.indices[triangleIndex * 3 + 2];

        vec3 vertices[3];
        vertices[0] = meshData.vertices[indices[0]].position;
        vertices[1] = meshData.vertices[indices[1]].position;
        vertices[2] = meshData.vertices[indices[2]].position;

        Assert(rayTracer->nodeCount < MAX_BVH_NODES);
        BvhNode *node = rayTracer->nodes + rayTracer->nodeCount++;
        node->min = Min(vertices[0], Min(vertices[1], vertices[2]));
        node->max = Max(vertices[0], Max(vertices[1], vertices[2]));
        node->triangleIndex = triangleIndex;
        node->children[0] = NULL;
        node->children[1] = NULL;

        // NOTE: Triangle index is our leaf index
        unmergedNodeIndices[triangleIndex] = nodesOffset + triangleIndex;
    }
    Assert(rayTracer->nodeCount - nodesOffset == triangleCount);
    unmergedNodeCount = triangleCount;

    while (unmergedNodeCount > 1)
    {
        u32 newUnmergedNodeCount = 0;

        // Join nodes with their nearest neighbors to build tree
        for (u32 index = 0; index < unmergedNodeCount; ++index)
        {
            u32 nodeIndex = unmergedNodeIndices[index];
            BvhNode *node = rayTracer->nodes + nodeIndex;
            vec3 centroid = (node->max + node->min) * 0.5f;

            f32 closestDistance = F32_MAX;
            f32 minVolume = F32_MAX;
            BvhNode *closestPartnerNode = NULL;
            u32 closestPartnerIndex = 0;

            // TODO: Spatial hashing
            // Find a node who's centroid is closest to ours
            for (u32 partnerIndex = 0; partnerIndex < unmergedNodeCount;
                 ++partnerIndex)
            {
                if (partnerIndex == index)
                {
                    continue;
                }

                u32 partnerNodeIndex = unmergedNodeIndices[partnerIndex];
                BvhNode *partnerNode = rayTracer->nodes + partnerNodeIndex;

#ifdef MIN_VOLUME
                vec3 min = Min(node->min, partnerNode->min);
                vec3 max = Max(node->max, partnerNode->max);
                f32 volume = (max.x - min.x) * (max.y - min.y) * (max.z - min.z);
                if (volume < minVolume)
                {
                    minVolume = volume;
                    closestPartnerNode = partnerNode;
                    closestPartnerIndex = partnerIndex;
                }
#else
                vec3 partnerCentroid =
                    (partnerNode->max + partnerNode->min) * 0.5f;

                // Find minimal distance via length squared to avoid sqrt
                f32 dist = LengthSq(partnerCentroid - centroid);
                if (dist < closestDistance)
                {
                    closestDistance = dist;
                    closestPartnerNode = partnerNode;
                    closestPartnerIndex = partnerIndex;
                }
#endif
            }

            if (closestPartnerNode != NULL)
            {
                // Create combined node
                Assert(rayTracer->nodeCount < MAX_BVH_NODES);
                u32 newNodeIndex = rayTracer->nodeCount++;
                BvhNode *newNode = rayTracer->nodes + newNodeIndex;
                newNode->min = Min(node->min, closestPartnerNode->min);
                newNode->max = Max(node->max, closestPartnerNode->max);
                newNode->children[0] = node;
                newNode->children[1] = closestPartnerNode;
                newNode->triangleIndex = 0;

                // Start overwriting the old entries with combined nodes, should
                // be roughly half the length as the original array
                Assert(newUnmergedNodeCount <= index);
                unmergedNodeIndices[newUnmergedNodeCount++] = newNodeIndex;

                // Remove partner from unmerged node indices array
                u32 last = unmergedNodeCount - 1;
                unmergedNodeIndices[closestPartnerIndex] =
                    unmergedNodeIndices[last];
                unmergedNodeCount--;
            }
        }

        unmergedNodeCount = newUnmergedNodeCount;
    }

    RayTracerMesh result = {};
    result.meshData = meshData;

    // Assume the last node created is the root node
    result.root = rayTracer->nodes + (rayTracer->nodeCount - 1);

    return result;
}

struct RayIntersectTriangleResult
{
    f32 t;
    vec3 normal;
};

// TODO: Do we need to normalize cross products?
internal RayIntersectTriangleResult RayIntersectTriangle(vec3 rayOrigin,
    vec3 rayDirection, vec3 a, vec3 b, vec3 c, f32 tmin = F32_MAX)
{
    RayIntersectTriangleResult result = {};
    result.t = -1.0f;

    g_Metrics.triangleTestCount++;
    //PROFILE_FUNCTION_SCOPE();

    vec3 edgeCA = c - a;
    vec3 edgeBA = b - a;
    vec3 normal = Normalize(Cross(edgeBA, edgeCA));
    f32 distance = Dot(normal, a);

#if 0
    {
        vec3 centroid = (a + b + c) * (1.0f / 3.0f);
        DrawLine(centroid, centroid + Normalize(normal), Vec3(1, 0, 1));
    }
#endif

    // Segment intersect plane
    f32 denom = Dot(normal, rayDirection);
    f32 t = Dot(a - rayOrigin, normal) / denom;
    //f32 t = (distance - Dot(normal, rayOrigin)) / denom;

    if (t > 0.0f && t < tmin)
    {
        vec3 p = rayOrigin + t * rayDirection;

        // FIXME: This is super dicey, converted function for triangle vs line
        // segment intersection not ray triangle.
        //
        //
        //
        // Compute plane equation for each edge
        vec3 normalCA = Cross(normal, edgeCA);
        f32 distCA = Dot(a, normalCA);
        vec3 normalBA = Cross(edgeBA, normal);
        f32 distBA = Dot(a, normalBA);

        vec3 edgeBC = b - c;
        vec3 normalBC = Cross(normal, edgeBC);
        f32 distBC = Dot(c, normalBC);

#if 0
        {
            vec3 centroidCA = (c + a) * 0.5f;
            vec3 centroidBA = (b + a) * 0.5f;
            vec3 centroidBC = (b + c) * 0.5f;
            vec3 color = Vec3(1, 0, 1);
            DrawLine(centroidCA, centroidCA + Normalize(normalCA), color);
            DrawLine(centroidBA, centroidBA + Normalize(normalBA), color);
            DrawLine(centroidBC, centroidBC + Normalize(normalBC), color);
        }
#endif

        b32 inCA = (Dot(normalCA, p) - distCA) < 0.0f;
        b32 inBA = (Dot(normalBA, p) - distBA) < 0.0f;
        b32 inBC = (Dot(normalBC, p) - distBC) < 0.0f;

        // TODO: Do this with Barycentric coordinates, then we only need to
        // compute 2 cross products

        if (inCA && inBA && inBC)
        {
            result.t = t;
            result.normal = normal;
        }
    }

    return result;
}
internal RayHitResult RayIntersectTriangleMeshSlow(
    MeshData meshData, vec3 rayOrigin, vec3 rayDirection)
{
    RayHitResult result = {};
    result.t = F32_MAX;

    u32 triangleCount = meshData.indexCount / 3;
    for (u32 triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
    {
        u32 indices[3];
        indices[0] = meshData.indices[triangleIndex * 3 + 0];
        indices[1] = meshData.indices[triangleIndex * 3 + 1];
        indices[2] = meshData.indices[triangleIndex * 3 + 2];

        vec3 vertices[3];
        vertices[0] = meshData.vertices[indices[0]].position;
        vertices[1] = meshData.vertices[indices[1]].position;
        vertices[2] = meshData.vertices[indices[2]].position;

        RayIntersectTriangleResult triangleIntersect = RayIntersectTriangle(
            rayOrigin, rayDirection, vertices[0], vertices[1], vertices[2]);
        if (triangleIntersect.t > 0.0f)
        {
            g_Metrics.triangleHitCount++;
            if (triangleIntersect.t < result.t)
            {
                result.t = triangleIntersect.t;
                result.isValid = true;
                result.normal = triangleIntersect.normal;
                // TODO: hitNormal, hitPoint, material, etc
            }
        }
    }

    return result;
}

struct StackNode
{
    BvhNode *node;
    u32 depth;
};

internal RayHitResult RayIntersectTriangleMeshAabbTree(BvhNode *root,
    MeshData meshData, vec3 rayOrigin, vec3 rayDirection,
    DebugDrawingBuffer *debugDrawBuffer, u32 maxDepth, f32 tmin)
{
    RayHitResult result = {};
    result.t = F32_MAX;

    StackNode stack[256];
    u32 stackSize = 1;
    stack[0].node = root;
    stack[0].depth = 0;

    StackNode newStack[256];
    u32 newStackSize = 0;
    while (stackSize > 0)
    {
        StackNode top = stack[--stackSize];
        BvhNode *node = top.node;
        g_Metrics.midPhaseTestCount++;
        u64 midPhaseTestStartCycles = __rdtsc();
        f32 t = RayIntersectAabb(node->min, node->max, rayOrigin, rayDirection);
        g_Metrics.midPhaseTestCycleCount += __rdtsc() - midPhaseTestStartCycles;

#if 0
        if (top.depth == maxDepth)
        {
            DrawBox(debugDrawBuffer, node->min, node->max,
                t > 0.0f ? Vec3(0, 1, 0) : Vec3(0.6, 0.2, 0.6));
        }
#endif

        if (t >= 0.0f && t < tmin)
        {
            // FIXME: Should we still count this a hit if its greater than our
            // tmin?
            g_Metrics.midPhaseHitCount++;
            result.depth = MaxU32(result.depth, top.depth);
            if (node->children[0] != NULL)
            {
                // Assuming that this is always true?
                Assert(node->children[1] != NULL);

                Assert(newStackSize + 2 <= ArrayCount(newStack));        
                newStack[newStackSize].node = node->children[0];
                newStack[newStackSize].depth = top.depth + 1;
                newStack[newStackSize + 1].node = node->children[1];
                newStack[newStackSize + 1].depth = top.depth + 1;
                newStackSize += 2;
            }
            else
            {
                u64 triangleTestStartCycles = __rdtsc();
                // Test triangle
                u32 triangleIndex = node->triangleIndex;
                u32 indices[3];
                indices[0] = meshData.indices[triangleIndex * 3 + 0];
                indices[1] = meshData.indices[triangleIndex * 3 + 1];
                indices[2] = meshData.indices[triangleIndex * 3 + 2];

                vec3 vertices[3];
                vertices[0] = meshData.vertices[indices[0]].position;
                vertices[1] = meshData.vertices[indices[1]].position;
                vertices[2] = meshData.vertices[indices[2]].position;

                RayIntersectTriangleResult triangleIntersect =
                    RayIntersectTriangle(rayOrigin, rayDirection, vertices[0],
                        vertices[1], vertices[2], tmin);
                if (triangleIntersect.t > 0.0f)
                {
                    g_Metrics.triangleHitCount++;
                    if (triangleIntersect.t < result.t)
                    {
                        result.t = triangleIntersect.t;
                        result.isValid = true;
                        result.normal = triangleIntersect.normal;
                        // TODO: hitNormal, hitPoint, material, etc
                    }
                }

                g_Metrics.triangleTestCycleCount +=
                    __rdtsc() - triangleTestStartCycles;
            }
        }

        u64 copyMemoryStartCycles = __rdtsc();
        if (stackSize == 0)
        {
            stackSize = newStackSize;
            // TODO: Use ping pong buffers rather than copying memory!
            CopyMemory(stack, newStack, newStackSize * sizeof(StackNode));

            newStackSize = 0;
        }
        g_Metrics.copyMemoryCycleCount += __rdtsc() - copyMemoryStartCycles;
        g_Metrics.copyMemoryCallCount++;
    }

    return result;
}

#if 0
internal u32 GetEntitiesToTest(World *world, vec3 rayOrigin, vec3 rayDirection,
    u32 *entityIndices, u32 maxEntities)
{
    u32 count = 0;
    for (u32 entityIndex = 0; entityIndex < world->count; ++entityIndex)
    {
        Entity *entity = world->entities + entityIndex;
        g_Metrics.broadPhaseTestCount++;
        f32 t = RayIntersectAabb(
            entity->aabbMin, entity->aabbMax, rayOrigin, rayDirection);
        if (t >= 0.0f)
        {
            g_Metrics.broadPhaseHitCount++;
            if (count < maxEntities)
            {
                entityIndices[count++] = entityIndex;
            }
        }
    }

    return count;
}
#endif

internal u32 GetEntitiesToTest(AabbTree tree, vec3 rayOrigin, vec3 rayDirection,
    u32 *entityIndices, f32 *tValues, u32 maxEntities)
{
    u32 count = 0;

    AabbTreeNode *stack[256];
    u32 stackSize = 1;
    stack[0] = tree.root;

    AabbTreeNode *newStack[256];
    u32 newStackSize = 0;
    while (stackSize > 0)
    {
        AabbTreeNode *node = stack[--stackSize];
        g_Metrics.broadPhaseTestCount++;
        f32 t = RayIntersectAabb(node->min, node->max, rayOrigin, rayDirection);
        if (t >= 0.0f)
        {
            if (node->children[0] != NULL)
            {
                // Assuming that this is always true?
                Assert(node->children[1] != NULL);

                Assert(newStackSize + 2 <= ArrayCount(newStack));        
                newStack[newStackSize] = node->children[0];
                newStack[newStackSize + 1] = node->children[1];
                newStackSize += 2;
            }
            else
            {
                Assert(node->children[1] == NULL);
                g_Metrics.broadPhaseHitCount++;
                if (count < maxEntities)
                {
                    u32 index = count++;
                    entityIndices[index] = node->entityIndex;
                    tValues[index] = t;
                }
            }
        }

        if (stackSize == 0)
        {
            stackSize = newStackSize;
            // TODO: Use ping pong buffers rather than copying memory!
            CopyMemory(stack, newStack, newStackSize * sizeof(StackNode));

            newStackSize = 0;
        }
    }

    return count;
}

internal RayHitResult TraceRayThroughScene(
    RayTracer *rayTracer, World *world, vec3 rayOrigin, vec3 rayDirection)
{
    u64 rayTraceSceneStartCycles = __rdtsc();
    g_Metrics.rayCount++;
    PROFILE_FUNCTION_SCOPE();

    RayHitResult worldResult = {};

    u64 broadphaseStartCycles = __rdtsc();
    u32 entitiesToTest[MAX_ENTITIES];
    f32 entityTmins[MAX_ENTITIES];
    u32 entityCount = GetEntitiesToTest(rayTracer->aabbTree, rayOrigin,
        rayDirection, entitiesToTest, entityTmins, ArrayCount(entitiesToTest));
    g_Metrics.broadphaseCycleCount += __rdtsc() - broadphaseStartCycles;

    f32 tmin = F32_MAX;
    for (u32 index = 0; index < entityCount; index++)
    {
        u32 entityIndex = entitiesToTest[index];
        if (entityTmins[index] < tmin)
        {
            g_Metrics.meshTestCount++;

            Entity *entity = world->entities + entityIndex;

            // TODO: Skip testing entity if our worldResult.tmin is closer than
            // the tmin for entity bounding box.

            u64 buildModelMatricesStartCycles = __rdtsc();
            PROFILE_BEGIN_SCOPE("model matrices");
            // TODO: Don't calculate this for every ray!
            mat4 modelMatrix = Translate(entity->position) *
                               Rotate(entity->rotation) * Scale(entity->scale);
            mat4 invModelMatrix =
                Scale(Vec3(1.0f / entity->scale.x, 1.0f / entity->scale.y,
                    1.0f / entity->scale.z)) *
                Rotate(Conjugate(entity->rotation)) *
                Translate(-entity->position);
            PROFILE_END_SCOPE("model matrices");
            g_Metrics.buildModelMatricesCycleCount +=
                __rdtsc() - buildModelMatricesStartCycles;

            RayTracerMesh mesh = rayTracer->meshes[entity->mesh];

            u64 transformRayStartCycles = __rdtsc();
            // Ray is transformed by the inverse of the model matrix
            vec3 transformRayOrigin = TransformPoint(rayOrigin, invModelMatrix);
            vec3 transformRayDirection =
                Normalize(TransformVector(rayDirection, invModelMatrix));
            g_Metrics.transformRayCycleCount +=
                __rdtsc() - transformRayStartCycles;

            u64 rayIntersectMeshStartCycles = __rdtsc();
            RayHitResult entityResult;
            // Perform ray intersection test in model space
            if (rayTracer->useAccelerationStructure)
            {
                // FIXME: This is assuming we only support uniform scaling
                f32 transformedTmin = tmin * (1.0f / entity->scale.x);

                entityResult = RayIntersectTriangleMeshAabbTree(mesh.root,
                    mesh.meshData, transformRayOrigin, transformRayDirection,
                    rayTracer->debugDrawBuffer, rayTracer->maxDepth,
                    transformedTmin);
            }
            else
            {
                entityResult = RayIntersectTriangleMeshSlow(
                    mesh.meshData, transformRayOrigin, transformRayDirection);
            }
            g_Metrics.rayIntersectMeshCycleCount +=
                __rdtsc() - rayIntersectMeshStartCycles;

            // Transform the hit point and normal back into world space
            // PROBLEM: t value needs a bit more thought, luckily its not used
            // for anything other than checking if we intersected anythinh yet.
            // result.point = TransformPoint(result->point, modelMatrix);
            entityResult.normal =
                Normalize(TransformVector(entityResult.normal, modelMatrix));

            // FIXME: This is assuming we only support uniform scaling
            entityResult.t *= entity->scale.x;

            if (entityResult.isValid)
            {
                g_Metrics.meshHitCount++;
                if (worldResult.isValid)
                {
                    // FIXME: Probably not correct to compare t values from
                    // different spaces, just going with it for now.
                    if (entityResult.t < worldResult.t)
                    {
                        worldResult = entityResult;
                        tmin = worldResult.t;
                    }
                }
                else
                {
                    worldResult = entityResult;
                    tmin = worldResult.t;
                }
            }
        }
    }

    g_Metrics.rayTraceSceneCycleCount += __rdtsc() - rayTraceSceneStartCycles;

    return worldResult;
}

struct CameraConstants
{
    f32 halfPixelWidth;
    f32 halfPixelHeight;

    f32 halfFilmWidth;
    f32 halfFilmHeight;

    vec3 cameraRight;
    vec3 cameraUp;
    vec3 cameraPosition;
    vec3 filmCenter;
};


internal CameraConstants CalculateCameraConstants(mat4 viewMatrix, u32 width, u32 height)
{
    vec3 cameraPosition = TransformPoint(Vec3(0, 0, 0), viewMatrix);
    vec3 cameraForward = TransformVector(Vec3(0, 0, -1), viewMatrix);
    vec3 cameraRight = TransformVector(Vec3(1, 0, 0), viewMatrix);
    vec3 cameraUp = TransformVector(Vec3(0, 1, 0), viewMatrix);

    f32 filmWidth = 1.0f;
    f32 filmHeight = 1.0f;
    if (width > height)
    {
        filmHeight = (f32)height / (f32)width;
    }
    else if (width < height)
    {
        filmWidth = (f32)width / (f32)height;
    }

    // FIXME: This is broken, just been manually tweaking FOV value to get the
    // same result as the rasterizer.
    f32 fovy = 77.0f * ((f32)width / (f32)height);
    f32 filmDistance = 1.0f / Tan(Radians(fovy) * 0.5f);
    vec3 filmCenter = cameraForward * filmDistance + cameraPosition;

    f32 pixelWidth = 1.0f / (f32)width;
    f32 pixelHeight = 1.0f / (f32)height;

    // Used for multi-sampling
    f32 halfPixelWidth = 0.5f * pixelWidth;
    f32 halfPixelHeight = 0.5f * pixelHeight;

    f32 halfFilmWidth = filmWidth * 0.5f;
    f32 halfFilmHeight = filmHeight * 0.5f;

    CameraConstants result = {};
    result.halfPixelWidth = halfPixelWidth;
    result.halfPixelHeight = halfPixelHeight;
    result.halfFilmWidth = halfFilmWidth;
    result.halfFilmHeight = halfFilmHeight;

    result.cameraRight = cameraRight;
    result.cameraUp = cameraUp;
    result.cameraPosition = cameraPosition;
    result.filmCenter = filmCenter;

    return result;
}

inline vec3 CalculateFilmP(CameraConstants *camera, u32 width, u32 height,
    u32 x, u32 y, RandomNumberGenerator *rng)
{
    // Map to -1 to 1 range
    f32 filmX = -1.0f + 2.0f * ((f32)x / (f32)width);
    f32 filmY = -1.0f + 2.0f * (1.0f - ((f32)y / (f32)height));

    f32 offsetX = filmX + camera->halfPixelWidth +
                  camera->halfPixelWidth * RandomBilateral(rng);

    f32 offsetY = filmY + camera->halfPixelHeight +
                  camera->halfPixelHeight * RandomBilateral(rng);

    vec3 filmP = camera->halfFilmWidth * offsetX * camera->cameraRight +
                 camera->halfFilmHeight * offsetY * camera->cameraUp +
                 camera->filmCenter;

    return filmP;
}

internal void DoRayTracing(
    u32 width, u32 height, u32 *pixels, RayTracer *rayTracer, World *world)
{
    PROFILE_FUNCTION_SCOPE();

    CameraConstants camera =
        CalculateCameraConstants(rayTracer->viewMatrix, width, height);

    vec3 baseColor = Vec3(0.8, 0.8, 0.8);
    vec3 lightColor = Vec3(1, 0.95, 0.8);
    vec3 lightDirection = Normalize(Vec3(1, 1, 0.5));
    vec3 backgroundColor = Vec3(0, 0, 0);

    u32 maxBounces = 2;
    u32 maxSamples = 128;

    for (u32 y = 0; y < height; ++y)
    {
        for (u32 x = 0; x < width; ++x)
        {
            g_Metrics.totalPixelCount++;

            vec3 radiance = {};
            for (u32 sample = 0; sample < maxSamples; ++sample)
            {
                vec3 filmP = CalculateFilmP(
                    &camera, width, height, x, y, &rayTracer->rng);

                g_Metrics.totalSampleCount++;

                vec3 rayOrigin = camera.cameraPosition;
                vec3 rayDirection = Normalize(filmP - camera.cameraPosition);

                vec3 contrib = Vec3(1);
                for (u32 i = 0; i < maxBounces; ++i)
                {
                    RayHitResult rayHit = TraceRayThroughScene(
                        rayTracer, world, rayOrigin, rayDirection);

                    if (rayHit.t > 0.0f)
                    {
                        vec3 hitPoint = rayOrigin + rayDirection * rayHit.t;
                        rayOrigin = hitPoint + rayHit.normal * 0.0000001f;

                        vec3 offset = Vec3(RandomBilateral(&rayTracer->rng),
                            RandomBilateral(&rayTracer->rng),
                            RandomBilateral(&rayTracer->rng));
                        vec3 dir = Normalize(rayHit.normal + offset);
                        if (Dot(dir, rayHit.normal) < 0.0f)
                        {
                            dir = -dir;
                        }

                        rayDirection = dir;

                        contrib = Hadamard(contrib,
                            baseColor *
                                Max(Dot(rayDirection, rayHit.normal), 0.0));
                    }
                    else
                    {
                        contrib = Hadamard(contrib,
                            lightColor *
                                Max(Dot(rayDirection, lightDirection), 0.0));
                        break;
                    }
                }

                radiance += contrib * (1.0f / (f32)maxSamples);
            }

            vec3 outputColor = Clamp(radiance, Vec3(0), Vec3(1));

            outputColor *= 255.0f;
            u32 bgra = (0xFF000000 | ((u32)outputColor.z) << 16 |
                    ((u32)outputColor.y) << 8) | (u32)outputColor.x;

            pixels[y * width + x] = bgra;
        }
    }
}

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

internal void ComputeEntityBoundingBoxes(World *world, RayTracer *rayTracer)
{
    for (u32 entityIndex = 0; entityIndex < world->count; ++entityIndex)
    {
        Entity *entity = world->entities + entityIndex;
        RayTracerMesh mesh = rayTracer->meshes[entity->mesh];
        vec3 boxMin = mesh.root->min;
        vec3 boxMax = mesh.root->max;
        mat4 modelMatrix = Translate(entity->position) *
                           Rotate(entity->rotation) * Scale(entity->scale);

#if COMPUTE_SLOW_ENTITY_AABBS
        vec3 vertices[8];
        vertices[0] = Vec3(boxMin.x, boxMin.y, boxMin.z);
        vertices[1] = Vec3(boxMax.x, boxMin.y, boxMin.z);
        vertices[2] = Vec3(boxMax.x, boxMin.y, boxMax.z);
        vertices[3] = Vec3(boxMin.x, boxMin.y, boxMax.z);
        vertices[4] = Vec3(boxMin.x, boxMax.y, boxMin.z);
        vertices[5] = Vec3(boxMax.x, boxMax.y, boxMin.z);
        vertices[6] = Vec3(boxMax.x, boxMax.y, boxMax.z);
        vertices[7] = Vec3(boxMin.x, boxMax.y, boxMax.z);

        vec3 transformedVertex = TransformPoint(vertices[0], modelMatrix);
        entity->aabbMin = transformedVertex;
        entity->aabbMax = transformedVertex;

        for (u32 i = 1; i < ArrayCount(vertices); ++i)
        {
            transformedVertex = TransformPoint(vertices[i], modelMatrix);
            entity->aabbMin = Min(entity->aabbMin, transformedVertex);
            entity->aabbMax = Max(entity->aabbMax, transformedVertex);
        }
#else
        // FIXME: Computing the sphere is pretty bad way of transforming the
        // AABB, its probably worth investing in doing this properly by
        // transforming the 8 vertices of AABB and compute a new one from that.
        vec3 center = (boxMin + boxMax) * 0.5f;
        vec3 halfDim = (boxMax - boxMin) * 0.5f;
        f32 radius = Length(halfDim);

        vec3 transformedCenter = TransformPoint(center, modelMatrix);
        u32 largestAxis = CalculateLargestAxis(entity->scale);
        f32 transformedRadius = radius * entity->scale.data[largestAxis];

        entity->aabbMin = transformedCenter - Vec3(transformedRadius);
        entity->aabbMax = transformedCenter + Vec3(transformedRadius);
#endif
    }
}

internal AabbTree BuildAabbTree(World *world, MemoryArena *arena)
{
    AabbTree tree = {};
    tree.nodes = AllocateArray(arena, AabbTreeNode, MAX_AABB_TREE_NODES);
    tree.max = MAX_AABB_TREE_NODES;

    // FIXME: This should use a tempArena rather than the stack
    u32 unmergedNodeIndices[MAX_AABB_TREE_NODES];
    u32 unmergedNodeCount = 0;

    for (u32 entityIndex = 0; entityIndex < world->count; ++entityIndex)
    {
        Entity *entity = world->entities + entityIndex;
        Assert(tree.count < tree.max);
        AabbTreeNode *node = tree.nodes + tree.count++;
        node->min = entity->aabbMin;
        node->max = entity->aabbMax;
        node->entityIndex = entityIndex;
        node->children[0] = NULL;
        node->children[1] = NULL;

        // NOTE: Entity index is our leaf index
        unmergedNodeIndices[entityIndex] = entityIndex;
    }
    unmergedNodeCount = world->count;

    while (unmergedNodeCount > 1)
    {
        u32 newUnmergedNodeCount = 0;

        // Join nodes with their nearest neighbors to build tree
        for (u32 index = 0; index < unmergedNodeCount; ++index)
        {
            u32 nodeIndex = unmergedNodeIndices[index];
            AabbTreeNode *node = tree.nodes + nodeIndex;
            vec3 centroid = (node->max + node->min) * 0.5f;

            f32 closestDistance = F32_MAX;
            f32 minVolume = F32_MAX;
            AabbTreeNode *closestPartnerNode = NULL;
            u32 closestPartnerIndex = 0;

            // TODO: Spatial hashing
            // Find a node who's centroid is closest to ours
            for (u32 partnerIndex = 0; partnerIndex < unmergedNodeCount;
                 ++partnerIndex)
            {
                if (partnerIndex == index)
                {
                    continue;
                }

                u32 partnerNodeIndex = unmergedNodeIndices[partnerIndex];
                AabbTreeNode *partnerNode = tree.nodes + partnerNodeIndex;

#ifdef MIN_VOLUME
                vec3 min = Min(node->min, partnerNode->min);
                vec3 max = Max(node->max, partnerNode->max);
                f32 volume = (max.x - min.x) * (max.y - min.y) * (max.z - min.z);
                if (volume < minVolume)
                {
                    minVolume = volume;
                    closestPartnerNode = partnerNode;
                    closestPartnerIndex = partnerIndex;
                }
#else
                vec3 partnerCentroid =
                    (partnerNode->max + partnerNode->min) * 0.5f;

                // Find minimal distance via length squared to avoid sqrt
                f32 dist = LengthSq(partnerCentroid - centroid);
                if (dist < closestDistance)
                {
                    closestDistance = dist;
                    closestPartnerNode = partnerNode;
                    closestPartnerIndex = partnerIndex;
                }
#endif
            }

            if (closestPartnerNode != NULL)
            {
                // Create combined node
                Assert(tree.count < tree.max);
                u32 newNodeIndex = tree.count++;
                AabbTreeNode *newNode = tree.nodes + newNodeIndex;
                newNode->min = Min(node->min, closestPartnerNode->min);
                newNode->max = Max(node->max, closestPartnerNode->max);
                newNode->children[0] = node;
                newNode->children[1] = closestPartnerNode;
                newNode->entityIndex = 0;

                // Start overwriting the old entries with combined nodes, should
                // be roughly half the length as the original array
                Assert(newUnmergedNodeCount <= index);
                unmergedNodeIndices[newUnmergedNodeCount++] = newNodeIndex;

                // Remove partner from unmerged node indices array
                u32 last = unmergedNodeCount - 1;
                unmergedNodeIndices[closestPartnerIndex] =
                    unmergedNodeIndices[last];
                unmergedNodeCount--;
            }
        }

        unmergedNodeCount = newUnmergedNodeCount;
    }

    // Assume the last node created is the root node
    tree.root = tree.nodes + (tree.count - 1);

    return tree;
}

struct AabbTreeStackNode
{
    AabbTreeNode *node;
    u32 depth;
};

internal void EvaluateTree(AabbTree tree)
{
    u32 maxDepth = 0;
    AabbTreeStackNode stack[MAX_AABB_TREE_NODES];
    u32 stackSize = 1;
    stack[0].node = tree.root;
    stack[0].depth = 1;

    AabbTreeStackNode newStack[MAX_AABB_TREE_NODES];
    u32 newStackSize = 0;
    while (stackSize > 0)
    {
        AabbTreeStackNode entry = stack[--stackSize];
        AabbTreeNode *node = entry.node;
        if (node->children[0] != NULL)
        {
            // Assuming that this is always true?
            Assert(node->children[1] != NULL);

            Assert(newStackSize + 2 <= ArrayCount(newStack));        
            newStack[newStackSize].node = node->children[0];
            newStack[newStackSize].depth = entry.depth + 1;
            newStack[newStackSize + 1].node = node->children[1];
            newStack[newStackSize + 1].depth = entry.depth + 1;
            newStackSize += 2;
        }
        else
        {
            // Leaf node
            Assert(node->children[1] == NULL);
            maxDepth = MaxU32(entry.depth, maxDepth);
        }

        if (stackSize == 0)
        {
            stackSize = newStackSize;
            // TODO: Use ping pong buffers rather than copying memory!
            CopyMemory(stack, newStack, newStackSize * sizeof(StackNode));

            newStackSize = 0;
        }
    }

    LogMessage("Tree depth: %u", maxDepth);
}

internal void DrawTree(
    AabbTree tree, DebugDrawingBuffer *debugDrawBuffer, u32 level)
{
    AabbTreeStackNode stack[MAX_AABB_TREE_NODES];
    u32 stackSize = 1;
    stack[0].node = tree.root;
    stack[0].depth = 1;

    AabbTreeStackNode newStack[MAX_AABB_TREE_NODES];
    u32 newStackSize = 0;
    while (stackSize > 0)
    {
        AabbTreeStackNode entry = stack[--stackSize];
        AabbTreeNode *node = entry.node;
        if (entry.depth == level)
        {
            DrawBox(debugDrawBuffer, node->min, node->max, Vec3(0, 1, 0));
        }

        if (node->children[0] != NULL)
        {
            // Assuming that this is always true?
            Assert(node->children[1] != NULL);

            Assert(newStackSize + 2 <= ArrayCount(newStack));        
            newStack[newStackSize].node = node->children[0];
            newStack[newStackSize].depth = entry.depth + 1;
            newStack[newStackSize + 1].node = node->children[1];
            newStack[newStackSize + 1].depth = entry.depth + 1;
            newStackSize += 2;
        }
        else
        {
            // Leaf node
            Assert(node->children[1] == NULL);
        }

        if (stackSize == 0)
        {
            stackSize = newStackSize;
            // TODO: Use ping pong buffers rather than copying memory!
            CopyMemory(stack, newStack, newStackSize * sizeof(StackNode));

            newStackSize = 0;
        }
    }
}
