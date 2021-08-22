#include "math_lib.h"

#define RAY_TRACER_WIDTH (1024 / 4)
#define RAY_TRACER_HEIGHT (768 / 4)

struct RayHitResult
{
    b32 isValid;
    f32 t;
    vec3 normal;
    u32 depth;
};

struct Metrics
{
    u64 aabbTestCount;
    u64 triangleTestCount;
    u64 rayCount;
    u64 totalSampleCount;
    u64 totalPixelCount;
    u64 triangleHitCount;
};

global Metrics g_Metrics;

internal void DumpMetrics(Metrics *metrics)
{
    LogMessage("AABB Test Count: %llu", metrics->aabbTestCount);
    LogMessage("Triangle Test Count: %llu", metrics->triangleTestCount);
    LogMessage("Triangle Hit Count: %llu", metrics->triangleHitCount);
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
    g_Metrics.aabbTestCount++;

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

#define MAX_BVH_NODES 0x10000

// TODO: Probably want multiple triangles per leaf node
struct BvhNode
{
    vec3 min;
    vec3 max;
    BvhNode *children[2];
    u32 triangleIndex;
};

struct RayTracer
{
    mat4 viewMatrix;
    DebugDrawingBuffer *debugDrawBuffer;
    MeshData meshData;
    BvhNode *nodes;
    u32 nodeCount;
    BvhNode *root;
    b32 useAccelerationStructure;
    u32 maxDepth;
};

internal void BuildBvh(RayTracer *rayTracer, MeshData meshData)
{
    u32 unmergedNodeIndices[MAX_BVH_NODES];
    u32 unmergedNodeCount = 0;

    // Build BvhNode for each triangle
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

        Assert(rayTracer->nodeCount < MAX_BVH_NODES);
        BvhNode *node = rayTracer->nodes + rayTracer->nodeCount++;
        node->min = Min(vertices[0], Min(vertices[1], vertices[2]));
        node->max = Max(vertices[0], Max(vertices[1], vertices[2]));
        node->triangleIndex = triangleIndex;
        node->children[0] = NULL;
        node->children[1] = NULL;

        // NOTE: Triangle index is our leaf index
        unmergedNodeIndices[triangleIndex] = triangleIndex;
    }
    Assert(rayTracer->nodeCount == triangleCount);
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

    // Assume the last node created is the root node
    rayTracer->root = rayTracer->nodes + (rayTracer->nodeCount - 1);
}

struct RayIntersectTriangleResult
{
    f32 t;
    vec3 normal;
};

// TODO: Do we need to normalize cross products?
internal RayIntersectTriangleResult RayIntersectTriangle(
    vec3 rayOrigin, vec3 rayDirection, vec3 a, vec3 b, vec3 c)
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

    if (t > 0.0f)
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
    DebugDrawingBuffer *debugDrawBuffer, u32 maxDepth)
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
        f32 t = RayIntersectAabb(node->min, node->max, rayOrigin, rayDirection);

        if (top.depth == maxDepth)
        {
            DrawBox(debugDrawBuffer, node->min, node->max,
                t > 0.0f ? Vec3(0, 1, 0) : Vec3(0.6, 0.2, 0.6));
        }

        if (t > 0.0f)
        {
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
                        vertices[1], vertices[2]);
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
        }

        if (stackSize == 0)
        {
            stackSize = newStackSize;
            // TODO: Use ping pong buffers rather than copying memory!
            CopyMemory(stack, newStack, newStackSize * sizeof(StackNode));

            newStackSize = 0;
        }
    }

    return result;
}

internal u32 GetEntitiesToTest(World *world, vec3 rayOrigin, vec3 rayDirection,
    u32 *entityIndices, u32 maxEntities)
{
    u32 count = 0;
    for (u32 entityIndex = 0; entityIndex < world->count; ++entityIndex)
    {
        Entity *entity = world->entities + entityIndex;
        f32 t = RayIntersectAabb(
            entity->aabbMin, entity->aabbMax, rayOrigin, rayDirection);
        if (t >= 0.0f)
        {
            if (count < maxEntities)
            {
                entityIndices[count++] = entityIndex;
            }
        }
    }

    return count;
}

internal RayHitResult TraceRayThroughScene(
    RayTracer *rayTracer, World *world, vec3 rayOrigin, vec3 rayDirection)
{
    g_Metrics.rayCount++;
    PROFILE_FUNCTION_SCOPE();

    RayHitResult worldResult = {};

    u32 entitiesToTest[MAX_ENTITIES];
    u32 entityCount = GetEntitiesToTest(world, rayOrigin, rayDirection,
        entitiesToTest, ArrayCount(entitiesToTest));

    for (u32 index = 0; index < entityCount; index++)
    {
        u32 entityIndex = entitiesToTest[index];
        Entity *entity = world->entities + entityIndex;

        PROFILE_BEGIN_SCOPE("model matrices");
        // TODO: Don't calculate this for every ray!
        mat4 modelMatrix = Translate(entity->position) *
                             Rotate(entity->rotation) * Scale(entity->scale);
        mat4 invModelMatrix =
            Scale(Vec3(1.0f / entity->scale.x, 1.0f / entity->scale.y,
                1.0f / entity->scale.z)) *
            Rotate(Conjugate(entity->rotation)) * Translate(-entity->position);
        PROFILE_END_SCOPE("model matrices");

        //PROFILE_BEGIN_SCOPE("transform ray");
        // Ray is transformed by the inverse of the model matrix
        vec3 transformRayOrigin = TransformPoint(rayOrigin, invModelMatrix);
        vec3 transformRayDirection =
            Normalize(TransformVector(rayDirection, invModelMatrix));
        //PROFILE_END_SCOPE("transform ray");

        RayHitResult entityResult;
        // Perform ray intersection test in model space
        if (rayTracer->useAccelerationStructure)
        {
            entityResult = RayIntersectTriangleMeshAabbTree(rayTracer->root,
                rayTracer->meshData, transformRayOrigin, transformRayDirection,
                rayTracer->debugDrawBuffer, rayTracer->maxDepth);
        }
        else
        {
            entityResult = RayIntersectTriangleMeshSlow(
                rayTracer->meshData, transformRayOrigin, transformRayDirection);
        }

        // Transform the hit point and normal back into world space
        // PROBLEM: t value needs a bit more thought, luckily its not used for
        // anything other than checking if we intersected anythinh yet.
        // result.point = TransformPoint(result->point, modelMatrix);
        entityResult.normal =
            Normalize(TransformVector(entityResult.normal, modelMatrix));

        if (entityResult.isValid)
        {
            if (worldResult.isValid)
            {
                // FIXME: Probably not correct to compare t values from
                // different spaces, just going with it for now.
                if (entityResult.t < worldResult.t)
                {
                    worldResult = entityResult;
                }
            }
            else
            {
                worldResult = entityResult;
            }
        }
    }

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

inline vec3 CalculateFilmP(
    CameraConstants *camera, u32 width, u32 height, u32 x, u32 y)
{
    // Map to -1 to 1 range
    f32 filmX = -1.0f + 2.0f * ((f32)x / (f32)width);
    f32 filmY = -1.0f + 2.0f * (1.0f - ((f32)y / (f32)height));

    // f32 offsetX =
    // filmX + halfPixelWidth + halfPixelWidth * RandomBilateral(rng);
    f32 offsetX = filmX + camera->halfPixelWidth;
    f32 offsetY = filmY + camera->halfPixelHeight;

    // f32 offsetY = filmY + halfPixelHeight +
    // halfPixelHeight * RandomBilateral(rng);

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

    for (u32 y = 0; y < height; ++y)
    {
        for (u32 x = 0; x < width; ++x)
        {
            g_Metrics.totalPixelCount++;

            vec3 filmP = CalculateFilmP(&camera, width, height, x, y);

            g_Metrics.totalSampleCount++;

            vec3 rayOrigin = camera.cameraPosition;
            vec3 rayDirection = Normalize(filmP - camera.cameraPosition);

            //f32 t = RayIntersectSphere(
                //Vec3(0, 0, 0), 0.5f, rayOrigin, rayDirection);
            //vec3 outputColor = (t < F32_MAX) ? Vec3(1, 0, 0) : Vec3(0, 0, 0);
            //f32 t = RayIntersectTriangle(rayOrigin, rayDirection,
                        //Vec3(-0.5f, -0.5f, 0.0f),
                        //Vec3(0.5f, -0.5f, 0.0f),
                        //Vec3(0.0, 0.5f, 0.0f));
            RayHitResult rayHit =
                TraceRayThroughScene(rayTracer, world, rayOrigin, rayDirection);
            vec3 outputColor =
                rayHit.isValid ? rayHit.normal : Vec3(0, 0, 0);
                //rayHit.isValid ? rayHit.normal * 0.5f + Vec3(0.5f) : Vec3(0, 0, 0);

            outputColor = Clamp(outputColor, Vec3(0), Vec3(1));

            //f32 maxDepth = 10;
            //outputColor = Vec3(1, 1, 1) * ((f32)rayHit.depth / (f32)maxDepth);

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
