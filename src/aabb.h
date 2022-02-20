#pragma once

struct Aabb
{
    vec3 min;
    vec3 max;
};

inline Aabb ComputeAabb(vec3 *vertices, u32 vertexCount)
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

inline Aabb TransformAabb(
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

inline b32 AabbContainsPoint(vec3 min, vec3 max, vec3 p)
{
    b32 result = true;
    for (u32 axis = 0; axis < 3; axis++)
    {
        if (p.data[axis] < min.data[axis] &&
            p.data[axis] > max.data[axis])
        {
            result = false;
            break;
        }
    }

    return result;
}

// Check that A contains B
inline b32 AabbContainsAabb(vec3 minA, vec3 maxA, vec3 minB, vec3 maxB)
{
    b32 result = AabbContainsPoint(minA, maxA, minB) &&
                 AabbContainsPoint(minA, maxA, maxB);
    return result;
}
