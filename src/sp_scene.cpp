void sp_InitializeScene(sp_Scene *scene, MemoryArena *arena)
{
    // FIXME: What do we set this to?
    scene->memoryArena = SubAllocateArena(arena, Kilobytes(512));
}

// FIXME: What do we do with the memory!?!??!
sp_Mesh sp_CreateMesh(VertexPNT *vertices, u32 vertexCount, u32 *indices,
    u32 indexCount, b32 useSmoothShading = false)
{
    sp_Mesh result = {};
    result.vertices = vertices;
    result.vertexCount = vertexCount;
    result.indices = indices;
    result.indexCount = indexCount;
    result.useSmoothShading = useSmoothShading;

    return result;
}

void sp_BuildMeshMidphase(
    sp_Mesh *mesh, MemoryArena *arena, MemoryArena *tempArena)
{
    // Compute number of triangles in the mesh
    Assert(mesh->indexCount % 3 == 0);
    u32 triangleCount = mesh->indexCount / 3;

    // Allocate temp buffers for aabbMin and aabbMax
    vec3 *aabbMin = AllocateArray(tempArena, vec3, triangleCount);
    vec3 *aabbMax = AllocateArray(tempArena, vec3, triangleCount);

    // Compute AABBs for each triangle
    u32 indices[3] = {};
    vec3 vertices[3] = {};
    for (u32 i = 0; i < triangleCount; ++i)
    {
        // Fetch triangle indices
        indices[0] = mesh->indices[i * 3 + 0];
        indices[1] = mesh->indices[i * 3 + 1];
        indices[2] = mesh->indices[i * 3 + 2];

        // Fetch triangle vertices
        vertices[0] = mesh->vertices[indices[0]].position;
        vertices[1] = mesh->vertices[indices[1]].position;
        vertices[2] = mesh->vertices[indices[2]].position;

        // Take min and max value of each vertex in the triangle
        aabbMin[i] = Min(vertices[0], Min(vertices[1], vertices[2]));
        aabbMax[i] = Max(vertices[0], Max(vertices[1], vertices[2]));
    }

    // Build BVH tree
    mesh->midphaseTree = bvh_CreateTree(arena, aabbMin, aabbMax, triangleCount);
}

// Copy of ComputeAabb from aabb.h but for VertexPNT type
inline Aabb ComputeAabb(VertexPNT *vertices, u32 vertexCount)
{
    Assert(vertices != NULL);
    Assert(vertexCount > 0);

    Aabb result = {};
    result.min = vertices[0].position;
    result.max = vertices[0].position;

    // Build AABB by taking the min and max value of each component of the
    // input vertices
    for (u32 i = 1; i < vertexCount; ++i)
    {
        result.min = Min(result.min, vertices[i].position);
        result.max = Max(result.max, vertices[i].position);
    }

    return result;
}

void sp_AddObjectToScene(sp_Scene *scene, sp_Mesh mesh, u32 material,
    vec3 position, quat orientation, vec3 scale)
{
    // TODO: Do we want to add padding to AABBs to handle 0 length vector
    // components
    // FIXME: Avoid computing this for every object, should take as input
    // Compute AABB for mesh
    Aabb aabb = ComputeAabb(mesh.vertices, mesh.vertexCount);

    // Transform AABB
    Aabb transformedAabb =
        TransformAabb(aabb.min, aabb.max, position, orientation, scale);

    // Compute model matrix
    mat4 modelMatrix= Translate(position) * Rotate(orientation) * Scale(scale);

    // Compute inverse model matrix
    mat4 invModelMatrix = Scale(Inverse(scale)) *
                          Rotate(Conjugate(orientation)) *
                          Translate(-position);

    // Compute object index
    Assert(scene->objectCount < SP_SCENE_MAX_OBJECTS);
    u32 index = scene->objectCount++;

    // Store AABB
    scene->aabbMin[index] = transformedAabb.min;
    scene->aabbMax[index] = transformedAabb.max;

    // Store inverse model matrix
    scene->invModelMatrices[index] = invModelMatrix;

    // Store model matrix
    scene->modelMatrices[index] = modelMatrix;

    // Store mesh
    scene->meshes[index] = mesh;

    // Store material
    scene->materials[index] = material;
}

void sp_BuildSceneBroadphase(sp_Scene *scene)
{
    // TODO: Not very memory efficient
    scene->broadphaseTree =
        bvh_CreateTree(&scene->memoryArena, scene->aabbMin,
            scene->aabbMax, scene->objectCount);
}

sp_RayIntersectMeshResult sp_RayIntersectMesh(
    sp_Mesh mesh, vec3 rayOrigin, vec3 rayDirection, sp_Metrics *metrics)
{
    RayIntersectTriangleResult nearestTriangleIntersection = {};
    nearestTriangleIntersection.t = -1.0f;

    // TODO: What should be the upper limit on the number of midphase
    // intersections? Should there even be a fixed limit?
    // TODO: If we getting back up to 64 leaf node intersections from our
    // midphase test then what is our BVH even doing?
    // FIXME: Changing to a 4-node BVH tee is causing us to have more than 64
    // valid leaf node intersections, not sure whats going on!
    bvh_Node *intersectedNodes[128] = {};

    // NOTE: Midphase intersection testing is currently our performance
    // bottleneck with about ~80% of our cycles being spent in it for testing
    // Ray Mesh intersection.
    u64 midphaseStart = __rdtsc();

    // Intersect the ray against our midphase tree first to quickly get a
    // list of triangles to perform intersection tests against
    bvh_IntersectRayResult midphaseResult =
        bvh_IntersectRay(&mesh.midphaseTree, rayOrigin,
            rayDirection, intersectedNodes, ArrayCount(intersectedNodes));

    metrics->values[sp_Metric_CyclesElapsed_RayIntersectMeshMidphase] +=
        __rdtsc() - midphaseStart;
    metrics->values[sp_Metric_RayIntersectMesh_MidphaseAabbTestCount] +=
        midphaseResult.aabbTestCount;

    // TODO: What do we do if the errorOccurred flag is set?
    Assert(!midphaseResult.errorOccurred);

    // Process each midphase intersection
    for (u32 i = 0; i < midphaseResult.count; ++i)
    {
        // Fetch triangle index
        u32 triangleIndex = intersectedNodes[i]->leafIndex;

        // Compute vertex indices
        u32 indices[3];
        indices[0] = mesh.indices[triangleIndex * 3 + 0];
        indices[1] = mesh.indices[triangleIndex * 3 + 1];
        indices[2] = mesh.indices[triangleIndex * 3 + 2];

        // Fetch vertices using the computed indices
        VertexPNT vertices[3];
        vertices[0] = mesh.vertices[indices[0]];
        vertices[1] = mesh.vertices[indices[1]];
        vertices[2] = mesh.vertices[indices[2]];

        u64 triangleIntersectStart = __rdtsc();

        // Perform ray intersect triangle test
        RayIntersectTriangleResult triangleIntersect =
            RayIntersectTriangle(rayOrigin, rayDirection, vertices[0].position,
                vertices[1].position, vertices[2].position);

        metrics->values[sp_Metric_CyclesElapsed_RayIntersectTriangle] +=
            __rdtsc() - triangleIntersectStart;

        // Process triangle intersection result if intersection found
        if (triangleIntersect.t > 0.0f)
        {
            // Take the closest result (smallest t value)
            if (triangleIntersect.t < nearestTriangleIntersection.t ||
                nearestTriangleIntersection.t < 0.0f)
            {
                nearestTriangleIntersection = triangleIntersect;

                // Compute UVs from barycentric coordinates
                f32 w =
                    1.0f - triangleIntersect.uv.x - triangleIntersect.uv.y;
                vec2 uv =
                    vertices[0].textureCoord * w +
                    vertices[1].textureCoord * triangleIntersect.uv.x +
                    vertices[2].textureCoord * triangleIntersect.uv.y;

                if (mesh.useSmoothShading)
                {
                    // Compute smooth normal by interpolating the 3 vertex
                    // normals using barycentric coordinates
                    nearestTriangleIntersection.normal = Normalize(
                        vertices[0].normal * w +
                        vertices[1].normal * triangleIntersect.uv.x +
                        vertices[2].normal * triangleIntersect.uv.y);
                }
                nearestTriangleIntersection.uv = uv;
            }
        }
    }

    sp_RayIntersectMeshResult result = {};
    result.triangleIntersection = nearestTriangleIntersection;

#if SP_DEBUG_MIDPHASE_INTERSECTION_COUNT
    result.midphaseIntersectionCount = midphaseResult.count;
#endif

    return result;
}

sp_RayIntersectSceneResult sp_RayIntersectScene(
    sp_Scene *scene, vec3 rayOrigin, vec3 rayDirection, sp_Metrics *metrics)
{
    // Record the CPU timestamp at the start of the function
    u64 cycleCountStart = __rdtsc();

    sp_RayIntersectSceneResult result = {};
    result.t = -1.0f;

    // TODO: What should be the upper limit on the number of broadphase
    // intersections? Should there even be a fixed limit?
    bvh_Node *intersectedNodes[32] = {};

    // Record CPU timestamp before we perform broadphase intersection test
    u64 broadphaseStart = __rdtsc();

    // Intersect the ray against our broadphase tree first to quickly get a
    // list of potential mesh intersections
    bvh_IntersectRayResult broadphaseResult =
        bvh_IntersectRay(&scene->broadphaseTree, rayOrigin,
            rayDirection, intersectedNodes, ArrayCount(intersectedNodes));

    // Compute number of cycles spent in broadphase BVH test and add to total
    metrics->values[sp_Metric_CyclesElapsed_RayIntersectBroadphase] +=
        __rdtsc() - broadphaseStart;

    // TODO: What do we do if the errorOccurred flag is set?
    Assert(!broadphaseResult.errorOccurred);

#if SP_DEBUG_MIDPHASE_INTERSECTION_COUNT
    u32 midphaseIntersectionCount = 0;
#endif

    // Process each broadphase intersection
    for (u32 i = 0; i < broadphaseResult.count; ++i)
    {
        // Fetch data for scene object
        u32 objectIndex = intersectedNodes[i]->leafIndex;
        mat4 invModelMatrix = scene->invModelMatrices[objectIndex];
        mat4 modelMatrix = scene->modelMatrices[objectIndex];
        sp_Mesh mesh = scene->meshes[objectIndex];
        u32 material = scene->materials[objectIndex];

        // Transform ray into mesh local space by multiplying it by the inverse
        // model matrix to test it for intersection
        vec3 localRayOrigin = TransformPoint(rayOrigin, invModelMatrix);
        vec3 localRayDirection =
            Normalize(TransformVector(rayDirection, invModelMatrix));

        u64 rayIntersectMeshStart = __rdtsc();

        // Find closest triangle intersection for this collision mesh
        sp_RayIntersectMeshResult meshIntersectionResult = sp_RayIntersectMesh(
            mesh, localRayOrigin, localRayDirection, metrics);

        metrics->values[sp_Metric_CyclesElapsed_RayIntersectMesh] +=
            __rdtsc() - rayIntersectMeshStart;
        metrics->values[sp_Metric_RayIntersectMesh_TestsPerformed]++;

#if SP_DEBUG_MIDPHASE_INTERSECTION_COUNT
        midphaseIntersectionCount +=
            meshIntersectionResult.midphaseIntersectionCount;
#endif

        // Process result if intersection found
        if (meshIntersectionResult.triangleIntersection.t >= 0.0f)
        {
            RayIntersectTriangleResult triangleIntersection =
                meshIntersectionResult.triangleIntersection;

            // Transform triangle intersection result from mesh space into
            // world space by transforming the local hit point into world space
            // and then using it to compute the t value in world space
            vec3 localHitPoint =
                localRayOrigin + localRayDirection * triangleIntersection.t;
            vec3 worldHitPoint = TransformPoint(localHitPoint, modelMatrix);

            // Project world hit point onto the world space ray to compute t
            // value in world space
            f32 t = Dot(worldHitPoint - rayOrigin, rayDirection);

            // Transform mesh space normal into world space
            vec3 localNormal = triangleIntersection.normal;
            vec3 worldNormal = Normalize(TransformVector(localNormal, modelMatrix));

            // Take closest intersection
            if (t < result.t || result.t < 0.0f)
            {
                result.t = t;
                result.materialId = material;
                result.normal = worldNormal;
                result.uv = triangleIntersection.uv;
                // TODO: Store other properties for the intersection
            }
        }
    }

#if SP_DEBUG_BROADPHASE_INTERSECTION_COUNT
    result.broadphaseIntersectionCount = broadphaseResult.count;
#endif

#if SP_DEBUG_MIDPHASE_INTERSECTION_COUNT
    result.midphaseIntersectionCount = midphaseIntersectionCount;
#endif

    // Calculate the number of cycles spent in this function and add to total
    metrics->values[sp_Metric_CyclesElapsed_RayIntersectScene] +=
        __rdtsc() - cycleCountStart;

    return result;
}

