#include "math_lib.h"
#include "cpu_ray_tracer.h"

#include "ray_intersection.cpp"
#include "tree_utils.cpp"

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

    LogMessage("Ray Count: %llu", metrics->rayCount);
    LogMessage("Total Sample Count: %llu", metrics->totalSampleCount);
    LogMessage("Total Pixel Count: %llu", metrics->totalPixelCount);
}

internal RayTracerMesh CreateMesh(
    MeshData meshData, MemoryArena *arena, MemoryArena *tempArena)
{
    u32 triangleCount = meshData.indexCount / 3;
    u32 meshletCount = MaxU32(triangleCount / MESHLET_SIZE, 1);
    vec3 *boxMin = AllocateArray(tempArena, vec3, meshletCount);
    vec3 *boxMax = AllocateArray(tempArena, vec3, meshletCount);

    for (u32 meshletIndex = 0; meshletIndex < meshletCount; ++meshletIndex)
    {
        vec3 meshletMin = Vec3(F32_MAX);
        vec3 meshletMax = Vec3(-F32_MAX);

        u32 startIndex = meshletIndex * MESHLET_SIZE;
        u32 onePastEndIndex =
            MinU32(startIndex + MESHLET_SIZE, triangleCount);
        for (u32 triangleIndex = startIndex; triangleIndex < onePastEndIndex;
             ++triangleIndex)
        {
            u32 indices[3];
            indices[0] = meshData.indices[triangleIndex * 3 + 0];
            indices[1] = meshData.indices[triangleIndex * 3 + 1];
            indices[2] = meshData.indices[triangleIndex * 3 + 2];

            vec3 vertices[3];
            vertices[0] = meshData.vertices[indices[0]].position;
            vertices[1] = meshData.vertices[indices[1]].position;
            vertices[2] = meshData.vertices[indices[2]].position;

            meshletMin = Min(
                Min(vertices[0], Min(vertices[1], vertices[2])), meshletMin);
            meshletMax = Max(
                Max(vertices[0], Max(vertices[1], vertices[2])), meshletMax);
        }

        boxMin[meshletIndex] = meshletMin;
        boxMax[meshletIndex] = meshletMax;
    }

    RayTracerMesh result = {};
    result.meshData = meshData;
    result.aabbTree = BuildAabbTree(boxMin, boxMax, meshletCount,
        meshletCount * 3, arena, tempArena);

    return result;
}

internal RayHitResult RayIntersectTriangleMesh(RayTracerMesh mesh,
    vec3 rayOrigin, vec3 rayDirection, DebugDrawingBuffer *debugDrawBuffer,
    u32 maxDepth, f32 tmin)
{
    RayHitResult result = {};
    result.t = F32_MAX;

    MeshData meshData = mesh.meshData;
    u32 triangleCount = meshData.indexCount / 3;

    // TODO: Parameterize
    u32 leafIndices[256];
    f32 tValues[256];

    u32 count = RayIntersectAabbTree(mesh.aabbTree, rayOrigin, rayDirection,
        leafIndices, tValues, ArrayCount(leafIndices));

    for (u32 index = 0; index < count; ++index)
    {
        u32 leafIndex = leafIndices[index];
        f32 t = tValues[index];


        // TODO: Check t value against tmin

        // Test triangle
        u32 meshletIndex = leafIndex;

        u32 startIndex = meshletIndex * MESHLET_SIZE;
        u32 onePastEndIndex =
            MinU32(startIndex + MESHLET_SIZE, triangleCount);
        for (u32 triangleIndex = startIndex; triangleIndex < onePastEndIndex;
             ++triangleIndex)
        {
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
                    tmin = triangleIntersect.t;
                    // TODO: hitNormal, hitPoint, material, etc
                }
            }
        }
    }

    return result;
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
    u32 entityCount = RayIntersectAabbTree(rayTracer->aabbTree, rayOrigin,
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
            //
            // FIXME: This is assuming we only support uniform scaling
            f32 transformedTmin = tmin * (1.0f / entity->scale.x);

            entityResult = RayIntersectTriangleMesh(mesh, transformRayOrigin,
                transformRayDirection, rayTracer->debugDrawBuffer,
                rayTracer->maxDepth, transformedTmin);

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

            entityResult.materialIndex = entity->material;

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

inline vec3 PerformToneMapping(vec3 input)
{
    input *= 1.0f; // expose adjustment

    // Reinhard operator
    input.x = input.x / (1.0f + input.x);
    input.y = input.y / (1.0f + input.y);
    input.z = input.z / (1.0f + input.z);

    vec3 retColor = Pow(input, Vec3(1.0f / 2.2f));
    return retColor;
}

internal void DoRayTracing(u32 width, u32 height, u32 *pixels,
    RayTracer *rayTracer, World *world, Tile tile, RandomNumberGenerator *rng)
{
    PROFILE_FUNCTION_SCOPE();

    // TODO: Don't recompute this for every tile
    CameraConstants camera =
        CalculateCameraConstants(rayTracer->viewMatrix, width, height);

    vec3 lightColor = Vec3(1, 0.95, 0.8);
    vec3 lightDirection = Normalize(Vec3(1, 1, 0.5));
    vec3 backgroundColor = Vec3(0, 0, 0);

    for (u32 y = tile.minY; y < tile.maxY; ++y)
    {
        for (u32 x = tile.minX; x < tile.maxX; ++x)
        {
            g_Metrics.totalPixelCount++;

            vec3 radiance = {};
            for (u32 sample = 0; sample < SAMPLES_PER_PIXEL; ++sample)
            {
                vec3 filmP = CalculateFilmP(&camera, width, height, x, y, rng);

                g_Metrics.totalSampleCount++;

                vec3 rayOrigin = camera.cameraPosition;
                vec3 rayDirection = Normalize(filmP - camera.cameraPosition);

                PathVertex pathVertices[MAX_BOUNCES] = {};
                u32 pathVertexCount = 0;

                for (u32 i = 0; i < MAX_BOUNCES; ++i)
                {
                    RayHitResult rayHit = TraceRayThroughScene(
                        rayTracer, world, rayOrigin, rayDirection);

                    if (rayHit.t > 0.0f)
                    {
                        Material material =
                            rayTracer->materials[rayHit.materialIndex];
                        vec3 baseColor = material.baseColor;

                        vec3 hitPoint = rayOrigin + rayDirection * rayHit.t;
                        rayOrigin = hitPoint + rayHit.normal * 0.0000001f;

                        // Compute random direction on hemi-sphere around
                        // rayHit.normal
                        vec3 offset = Vec3(RandomBilateral(rng),
                            RandomBilateral(rng), RandomBilateral(rng));
                        vec3 dir = Normalize(rayHit.normal + offset);
                        if (Dot(dir, rayHit.normal) < 0.0f)
                        {
                            dir = -dir;
                        }

                        pathVertices[pathVertexCount].incomingDirection = dir;
                        pathVertices[pathVertexCount].outgoingDirection =
                            rayDirection;
                        pathVertices[pathVertexCount].surfaceNormal =
                            rayHit.normal;
                        pathVertices[pathVertexCount].materialIndex =
                            rayHit.materialIndex;
                        pathVertexCount++;

                        rayDirection = dir;

                    }
                    else
                    {
                        // Add dummy path vertex for background intersection
                        pathVertices[pathVertexCount].incomingDirection =
                            Vec3(0, 0, 0);
                        pathVertices[pathVertexCount].outgoingDirection =
                            rayDirection;
                        pathVertices[pathVertexCount].surfaceNormal =
                            Vec3(0, 0, 0);
                        pathVertices[pathVertexCount].materialIndex =
                            Material_Background;
                        pathVertexCount++;

                        break;
                    }
                }

                // Compute total radiance that reaches camera
                vec3 outgoingRadiance = Vec3(0, 0, 0);
                for (i32 vertexIndex = pathVertexCount - 1; vertexIndex >= 0;
                     --vertexIndex)
                {
                    PathVertex vertex = pathVertices[vertexIndex];
                    u32 materialIndex = vertex.materialIndex;
                    Material material = rayTracer->materials[materialIndex];

                    vec3 baseColor = material.baseColor;
                    vec3 emission = material.emission;

                    f32 cosine =
                        Max(Dot(vertex.surfaceNormal, vertex.incomingDirection),
                            0.0f);

                    vec3 incomingRadiance = outgoingRadiance;
                    f32 brdf = 1.0f;
                    outgoingRadiance =
                        emission +
                        Hadamard(baseColor, brdf * incomingRadiance) * cosine;
                }

                radiance += outgoingRadiance * (1.0f / (f32)SAMPLES_PER_PIXEL);
            }

            // Tone map value with S-curve (need to calculate avg luminance for the whole image!)
            // Convert from linear space to display space (assuming sRGB gamma for now)
            vec3 toneMappedValue = PerformToneMapping(radiance);
            vec3 outputColor = Clamp(toneMappedValue, Vec3(0), Vec3(1));

            outputColor *= 255.0f;
            u32 bgra = (0xFF000000 | ((u32)outputColor.z) << 16 |
                    ((u32)outputColor.y) << 8) | (u32)outputColor.x;

            pixels[y * width + x] = bgra;
        }
    }
}

internal void ComputeEntityBoundingBoxes(World *world, RayTracer *rayTracer)
{
    for (u32 entityIndex = 0; entityIndex < world->count; ++entityIndex)
    {
        Entity *entity = world->entities + entityIndex;
        RayTracerMesh mesh = rayTracer->meshes[entity->mesh];
        vec3 boxMin = mesh.aabbTree.root->min;
        vec3 boxMax = mesh.aabbTree.root->max;
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

internal AabbTree BuildWorldBroadphase(
    World *world, MemoryArena *arena, MemoryArena *tempArena)
{
    vec3 *boxMin = AllocateArray(tempArena, vec3, world->count);
    vec3 *boxMax = AllocateArray(tempArena, vec3, world->count);

    for (u32 entityIndex = 0; entityIndex < world->count; ++entityIndex)
    {
        Entity *entity = world->entities + entityIndex;
        boxMin[entityIndex] = entity->aabbMin;
        boxMax[entityIndex] = entity->aabbMax;
    }

    AabbTree aabbTree = BuildAabbTree(
        boxMin, boxMax, world->count, MAX_AABB_TREE_NODES, arena, tempArena);

    return aabbTree;
}

internal void EvaluateTree(AabbTree tree)
{
    u32 maxDepth = 0;
    u32 minDepth = U32_MAX;
    StackNode stack[MAX_AABB_TREE_NODES];
    u32 stackSize = 1;
    stack[0].node = tree.root;
    stack[0].depth = 1;

    StackNode newStack[MAX_AABB_TREE_NODES];
    u32 newStackSize = 0;
    while (stackSize > 0)
    {
        StackNode entry = stack[--stackSize];
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
            minDepth = MinU32(entry.depth, minDepth);
        }

        if (stackSize == 0)
        {
            stackSize = newStackSize;
            // TODO: Use ping pong buffers rather than copying memory!
            CopyMemory(stack, newStack, newStackSize * sizeof(StackNode));

            newStackSize = 0;
        }
    }

    LogMessage("Tree depth: %u max %u min", maxDepth, minDepth);
}

internal void DrawTree(
    AabbTree tree, DebugDrawingBuffer *debugDrawBuffer, u32 level)
{
    StackNode stack[MAX_AABB_TREE_NODES];
    u32 stackSize = 1;
    stack[0].node = tree.root;
    stack[0].depth = 1;

    StackNode newStack[MAX_AABB_TREE_NODES];
    u32 newStackSize = 0;
    while (stackSize > 0)
    {
        StackNode entry = stack[--stackSize];
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

internal void DrawMesh(RayTracerMesh mesh, DebugDrawingBuffer *debugDrawBuffer)
{
    AabbTree tree = mesh.aabbTree;
    MeshData meshData = mesh.meshData;

    u32 triangleCount = meshData.indexCount / 3;

    StackNode stack[MAX_AABB_TREE_NODES];
    u32 stackSize = 1;
    stack[0].node = tree.root;
    stack[0].depth = 1;

    StackNode newStack[MAX_AABB_TREE_NODES];
    u32 newStackSize = 0;
    while (stackSize > 0)
    {
        StackNode entry = stack[--stackSize];
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

            // Test triangle
            u32 meshletIndex = node->leafIndex;

            RandomNumberGenerator rng;
            rng.state = 0xA5983C1 | meshletIndex;
            vec3 color = Vec3(RandomUnilateral(&rng), RandomUnilateral(&rng),
                RandomUnilateral(&rng));

            u32 startIndex = meshletIndex * MESHLET_SIZE;
            u32 onePastEndIndex =
                MinU32(startIndex + MESHLET_SIZE, triangleCount);
            for (u32 triangleIndex = startIndex; triangleIndex < onePastEndIndex;
                 ++triangleIndex)
            {
                u32 indices[3];
                indices[0] = meshData.indices[triangleIndex * 3 + 0];
                indices[1] = meshData.indices[triangleIndex * 3 + 1];
                indices[2] = meshData.indices[triangleIndex * 3 + 2];

                vec3 vertices[3];
                vertices[0] = meshData.vertices[indices[0]].position;
                vertices[1] = meshData.vertices[indices[1]].position;
                vertices[2] = meshData.vertices[indices[2]].position;

                DrawTriangle(debugDrawBuffer, vertices[0], vertices[1],
                    vertices[2], color);
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
}
