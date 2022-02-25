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


// TODO: Do we need to normalize cross products?
internal RayIntersectTriangleResult RayIntersectTriangleSlow(vec3 rayOrigin,
    vec3 rayDirection, vec3 a, vec3 b, vec3 c, f32 tmin = F32_MAX)
{
    RayIntersectTriangleResult result = {};
    result.t = -1.0f;

    //g_Metrics.triangleTestCount++;
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
        // And we can then we can compute the UV coordinates

        if (inCA && inBA && inBC)
        {
            result.t = t;
            result.normal = normal;
        }
    }

    return result;
}

internal RayIntersectTriangleResult RayIntersectTriangleMT(vec3 rayOrigin,
    vec3 rayDirection, vec3 a, vec3 b, vec3 c, f32 tmin = F32_MAX)
{
    RayIntersectTriangleResult result = {};
    result.t = -1.0f;

    vec3 T = rayOrigin - a;
    vec3 e1 = b - a;
    vec3 e2 = c - a;

    vec3 p = Cross(rayDirection, e2);
    vec3 q = Cross(T, e1);

    vec3 m = Vec3(Dot(q, e2), Dot(p, T), Dot(q, rayDirection));

    f32 det = 1.0f / Dot(p, e1);

    f32 t = det * m.x;
    f32 u = det * m.y;
    f32 v = det * m.z;
    f32 w = 1.0f - u - v;

    if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f && w >= 0.0f &&
        w <= 1.0f)
    {
        result.t = t;
        result.normal = Cross(e1, e2);
        result.uv = Vec2(u, v);
    }

    return result;
}

inline RayIntersectTriangleResult RayIntersectTriangle(vec3 rayOrigin,
    vec3 rayDirection, vec3 a, vec3 b, vec3 c, f32 tmin = F32_MAX)
{
#if USE_MT_RAY_TRIANGLE_INTERSECT
    return RayIntersectTriangleMT(rayOrigin, rayDirection, a, b, c, tmin);
#else
    return RayIntersectTriangleSlow(rayOrigin, rayDirection, a, b, c, tmin);
#endif
}
