#include "math_lib.h"

#define RAY_TRACER_WIDTH (1024 / 2)
#define RAY_TRACER_HEIGHT (768 / 2)

struct Metrics
{
    u64 aabbTestCount;
    u64 triangleTestCount;
    u64 rayCount;
    u64 totalSampleCount;
    u64 totalPixelCount;
    // TODO: Triangle test pass/fail
};

global Metrics g_Metrics;

internal void DumpMetrics(Metrics *metrics)
{
    LogMessage("AABB Test Count: %llu", metrics->aabbTestCount);
    LogMessage("Triangle Test Count: %llu", metrics->triangleTestCount);
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
    f32 tmax = 1.0f;

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
    MeshData meshData;
    BvhNode *nodes;
    u32 nodeCount;
    BvhNode *root;
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
            BvhNode *closestPartnerNode = NULL;
            u32 closestPartnerIndex = 0;

            // TODO: Spatial hashing
            // Find a node who's centroid is closest to ours
            for (u32 partnerIndex = index + 1; partnerIndex < unmergedNodeCount;
                 ++partnerIndex)
            {
                u32 partnerNodeIndex = unmergedNodeIndices[partnerIndex];
                BvhNode *partnerNode = rayTracer->nodes + partnerNodeIndex;

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

// TODO: Do we need to normalize cross products?
internal f32 RayIntersectTriangle(
    vec3 rayOrigin, vec3 rayDirection, vec3 a, vec3 b, vec3 c)
{
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
            return t;
        }
    }

    return -1.0f;
}

internal f32 RayIntersectTriangleMesh(
    BvhNode *root, MeshData meshData, vec3 rayOrigin, vec3 rayDirection)
{
    f32 tmin = F32_MAX;
    BvhNode *stack[256];
    u32 stackSize = 1;
    stack[0] = root;

    BvhNode *newStack[256];
    u32 newStackSize = 0;
    while (stackSize > 0)
    {
        BvhNode *node = stack[--stackSize];
        f32 t = RayIntersectAabb(node->min, node->max, rayOrigin, rayDirection);
        if (t > 0.0f)
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

                t = RayIntersectTriangle(rayOrigin, rayDirection, vertices[0],
                    vertices[1], vertices[2]);
                if (t > 0.0f && t < tmin)
                {
                    tmin = t;
                }
            }
        }

        if (stackSize == 0)
        {
            stackSize = newStackSize;
            // TODO: Use ping pong buffers rather than copying memory!
            CopyMemory(stack, newStack, newStackSize * sizeof(BvhNode*));

            newStackSize = 0;
        }
    }

    if (tmin == F32_MAX)
    {
        tmin = -1.0f;
    }

    return tmin;
}

internal f32 TraceRayThroughScene(
    RayTracer *rayTracer, vec3 rayOrigin, vec3 rayDirection)
{
    g_Metrics.rayCount++;
    PROFILE_FUNCTION_SCOPE();

    f32 tmin = RayIntersectTriangleMesh(
        rayTracer->root, rayTracer->meshData, rayOrigin, rayDirection);

    return tmin;
}

internal void DoRayTracing(u32 width, u32 height, u32 *pixels, RayTracer *rayTracer)
{
    PROFILE_FUNCTION_SCOPE();
    vec3 cameraPosition = TransformPoint(Vec3(0, 0, 0), rayTracer->viewMatrix);
    vec3 cameraForward = TransformVector(Vec3(0, 0, -1), rayTracer->viewMatrix);
    vec3 cameraRight = TransformVector(Vec3(1, 0, 0), rayTracer->viewMatrix);
    vec3 cameraUp = TransformVector(Vec3(0, 1, 0), rayTracer->viewMatrix);

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

    f32 fovy = 90.0f * ((f32)width / (f32)height);
    f32 filmDistance = 1.0f / Tan(Radians(fovy) * 0.5f);
    vec3 filmCenter = cameraForward * filmDistance + cameraPosition;

    f32 pixelWidth = 1.0f / (f32)width;
    f32 pixelHeight = 1.0f / (f32)height;

    // Used for multi-sampling
    f32 halfPixelWidth = 0.5f * pixelWidth;
    f32 halfPixelHeight = 0.5f * pixelHeight;

    f32 halfFilmWidth = filmWidth * 0.5f;
    f32 halfFilmHeight = filmHeight * 0.5f;

    for (u32 y = 0; y < height; ++y)
    {
        for (u32 x = 0; x < width; ++x)
        {
            g_Metrics.totalPixelCount++;

            // Map to -1 to 1 range
            f32 filmX = -1.0f + 2.0f * ((f32)x / (f32)width);
            f32 filmY = -1.0f + 2.0f * (1.0f - ((f32)y / (f32)height));

            //f32 offsetX =
                //filmX + halfPixelWidth + halfPixelWidth * RandomBilateral(rng);
            f32 offsetX = filmX + halfPixelWidth;
            f32 offsetY = filmY + halfPixelHeight;

            //f32 offsetY = filmY + halfPixelHeight +
                          //halfPixelHeight * RandomBilateral(rng);

            vec3 filmP = halfFilmWidth * offsetX * cameraRight +
                         halfFilmHeight * offsetY * cameraUp + filmCenter;

            g_Metrics.totalSampleCount++;

            vec3 rayOrigin = cameraPosition;
            vec3 rayDirection = Normalize(filmP - cameraPosition);

            //f32 t = RayIntersectSphere(
                //Vec3(0, 0, 0), 0.5f, rayOrigin, rayDirection);
            //vec3 outputColor = (t < F32_MAX) ? Vec3(1, 0, 0) : Vec3(0, 0, 0);
            //f32 t = RayIntersectTriangle(rayOrigin, rayDirection,
                        //Vec3(-0.5f, -0.5f, 0.0f),
                        //Vec3(0.5f, -0.5f, 0.0f),
                        //Vec3(0.0, 0.5f, 0.0f));
            f32 t = TraceRayThroughScene(rayTracer, rayOrigin, rayDirection);
            vec3 outputColor = (t != -1.0f) ? Vec3(1, 0, 1) : Vec3(0, 0, 0);

            outputColor *= 255.0f;
            u32 bgra = (0xFF000000 | ((u32)outputColor.z) << 16 |
                    ((u32)outputColor.y) << 8) | (u32)outputColor.x;

            pixels[y * width + x] = bgra;
        }
    }
}
