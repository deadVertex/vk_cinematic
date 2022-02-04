void sp_InitializeScene(sp_Scene *scene, MemoryArena *arena)
{
    // FIXME: What do we set this to?
    scene->memoryArena = SubAllocateArena(arena, Kilobytes(512));
}

// FIXME: What do we do with the memory!?!??!
sp_Mesh sp_CreateMesh(
    vec3 *vertices, u32 vertexCount, u32 *indices, u32 indexCount)
{
    sp_Mesh result = {};
    result.vertices = vertices;
    result.vertexCount = vertexCount;
    result.indices = indices;
    result.indexCount = indexCount;

    return result;
}

struct Aabb
{
    vec3 min;
    vec3 max;
};

Aabb ComputeAabb(vec3 *vertices, u32 vertexCount)
{
    Assert(vertices != NULL);
    Assert(vertexCount > 0);

    Aabb result = {};
    result.min = vertices[0];
    result.max = vertices[0];

    // Build AABB by taking the min and max value of each component of the
    // input vertices
    for (u32 i = 1; i < vertexCount; ++i)
    {
        result.min = Min(result.min, vertices[i]);
        result.max = Max(result.max, vertices[i]);
    }

    return result;
}

Aabb TransformAabb(
    vec3 boxMin, vec3 boxMax, vec3 position, quat orientation, vec3 scale)
{
    // Compute model matrix to transform each vertex by
    mat4 modelMatrix = Translate(position) * Rotate(orientation) * Scale(scale);

    // Build list of vertices for AABB
    vec3 vertices[8];
    vertices[0] = Vec3(boxMin.x, boxMin.y, boxMin.z);
    vertices[1] = Vec3(boxMax.x, boxMin.y, boxMin.z);
    vertices[2] = Vec3(boxMax.x, boxMin.y, boxMax.z);
    vertices[3] = Vec3(boxMin.x, boxMin.y, boxMax.z);
    vertices[4] = Vec3(boxMin.x, boxMax.y, boxMin.z);
    vertices[5] = Vec3(boxMax.x, boxMax.y, boxMin.z);
    vertices[6] = Vec3(boxMax.x, boxMax.y, boxMax.z);
    vertices[7] = Vec3(boxMin.x, boxMax.y, boxMax.z);

    // Transform each vertex of the AABB by the model matrix
    vec3 transformedVertices[8];
    for (u32 i = 0; i < ArrayCount(vertices); ++i)
    {
        transformedVertices[i] = TransformPoint(vertices[i], modelMatrix);
    }

    // Compute a new AABB from the transformed vertices
    Aabb result =
        ComputeAabb(transformedVertices, ArrayCount(transformedVertices));

    return result;
}

void sp_AddObjectToScene(sp_Scene *scene, sp_Mesh mesh, u32 material,
    vec3 position, quat orientation, vec3 scale)
{
    // TODO: Do we want to add padding to AABBs to handle 0 length vector
    // components
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

// TODO: Use BVH to reduce number of ray triangle intersections
RayIntersectTriangleResult sp_RayIntersectMesh(
    sp_Mesh mesh, vec3 rayOrigin, vec3 rayDirection)
{
    RayIntersectTriangleResult result = {};
    result.t = -1.0f;

    // Compute number of triangles to test for this mesh
    Assert(mesh.indexCount % 3 == 0);
    u32 triangleCount = mesh.indexCount / 3;

    for (u32 triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
    {
        // Compute vertex indices
        u32 indices[3];
        indices[0] = mesh.indices[triangleIndex * 3 + 0];
        indices[1] = mesh.indices[triangleIndex * 3 + 1];
        indices[2] = mesh.indices[triangleIndex * 3 + 2];

        // Fetch vertices using the computed indices
        vec3 vertices[3];
        vertices[0] = mesh.vertices[indices[0]];
        vertices[1] = mesh.vertices[indices[1]];
        vertices[2] = mesh.vertices[indices[2]];

        // Perform ray intersect triangle test
        RayIntersectTriangleResult triangleIntersect = RayIntersectTriangle(
            rayOrigin, rayDirection, vertices[0], vertices[1], vertices[2]);

        // Process triangle intersection result if intersection found
        if (triangleIntersect.t > 0.0f)
        {
            // Take the closest result (smallest t value)
            if (triangleIntersect.t < result.t || result.t < 0.0f)
            {
                result = triangleIntersect;
            }
        }
    }

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
    bvh_Node *intersectedNodes[8] = {};

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
    //Assert(!broadphaseResult.errorOccurred);

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
        RayIntersectTriangleResult meshIntersectionResult = sp_RayIntersectMesh(
            mesh, localRayOrigin, localRayDirection);

        metrics->values[sp_Metric_CyclesElapsed_RayIntersectMesh] +=
            __rdtsc() - rayIntersectMeshStart;

        // Process result if intersection found
        if (meshIntersectionResult.t >= 0.0f)
        {
            // Transform mesh intersection result into world space by
            // transforming the local hit point into world space and then using
            // it to compute the t value in world space
            vec3 localHitPoint =
                localRayOrigin + localRayDirection * meshIntersectionResult.t;
            vec3 worldHitPoint = TransformPoint(localHitPoint, modelMatrix);

            // Project world hit point onto the world space ray to compute t
            // value in world space
            f32 t = Dot(worldHitPoint - rayOrigin, rayDirection);

            // Transform mesh space normal into world space
            vec3 localNormal = meshIntersectionResult.normal;
            vec3 worldNormal = Normalize(TransformVector(localNormal, modelMatrix));

            // Take closest intersection
            if (t < result.t || result.t < 0.0f)
            {
                result.t = t;
                result.materialId = material;
                result.normal = worldNormal;
                // TODO: Store other properties for the intersection
            }
        }
    }

#if SP_DEBUG_BROADPHASE_INTERSECTION_COUNT
    result.broadphaseIntersectionCount = broadphaseResult.count;
#endif

    // Calculate the number of cycles spent in this function and add to total
    metrics->values[sp_Metric_CyclesElapsed_RayIntersectScene] +=
        __rdtsc() - cycleCountStart;

    return result;
}

