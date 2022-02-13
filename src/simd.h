#pragma once

#define SIMD_LANE_WIDTH 1

// TODO: Software/reference implementation of SIMD for testing
#if SIMD_LANE_WIDTH == 1
typedef f32 simd_f32;
typedef u32 simd_u32;
typedef vec3 simd_vec3;

inline simd_f32 SIMD_F32(f32 v)
{
    simd_f32 result = v;
    return result;
}

inline simd_u32 SIMD_U32(u32 u)
{
    simd_u32 result = u;
    return result;
}

inline simd_f32 simd_Abs(simd_f32 a)
{
    // FIXME: Doesn't look like there is a Abs SSE instruction
    simd_f32 result = Abs(a);
    return result;
}

inline simd_f32 simd_Select(simd_u32 mask, simd_f32 a, simd_f32 b)
{
    simd_f32 result = mask ? a : b;
    return result;
}

inline simd_u32 simd_LessThan(simd_f32 a, simd_f32 b)
{
    simd_u32 result = (a < b) ? U32_MAX : 0;
    return result;
}

inline simd_u32 simd_GreaterThan(simd_f32 a, simd_f32 b)
{
    simd_u32 result = (a > b) ? U32_MAX : 0;
    return result;
}

inline simd_u32 simd_Or(simd_u32 a, simd_u32 b)
{
    simd_u32 result = a | b;
    return result;
}

inline simd_u32 simd_And(simd_u32 a, simd_u32 b)
{
    simd_u32 result = a & b;
    return result;
}

inline simd_f32 simd_Subtract(simd_f32 a, simd_f32 b)
{
    simd_f32 result = a - b;
    return result;
}

inline simd_f32 simd_Divide(simd_f32 a, simd_f32 b)
{
    simd_f32 result = a / b;
    return result;
}

inline simd_f32 simd_Min(simd_f32 a, simd_f32 b)
{
    f32 result = (a < b) ? a : b;
    return result;
}

inline simd_f32 simd_Max(simd_f32 a, simd_f32 b)
{
    f32 result = (a > b) ? a : b;
    return result;
}

#elif SIMD_LANE_WIDTH == 4
typedef __m128 simd_f32;
typedef __m128i simd_u32;
union simd_vec3
{
    struct
    {
        simd_f32 x;
        simd_f32 y;
        simd_f32 z;
    };

    simd_f32 data[3];
};

inline simd_f32 SIMD_F32(f32 v)
{
    simd_f32 result = _mm_set_ps1(v); // or _mm_set1_ps?
    return result;
}

inline simd_u32 SIMD_U32(u32 v)
{
    simd_u32 result = _mm_set1_epi32(v);
    return result;
}

inline simd_f32 simd_Abs(simd_f32 a)
{
}

inline simd_f32 simd_Select(simd_u32 mask, simd_f32 a, simd_f32 b)
{
}

inline simd_u32 simd_LessThan(simd_f32 a, simd_f32 b)
{
}

inline simd_u32 simd_GreaterThan(simd_f32 a, simd_f32 b)
{
}

inline simd_u32 simd_Or(simd_u32 a, simd_u32 b)
{
}

inline simd_u32 simd_And(simd_u32 a, simd_u32 b)
{
}

inline simd_f32 simd_Subtract(simd_f32 a, simd_f32 b)
{
}

inline simd_f32 simd_Divide(simd_f32 a, simd_f32 b)
{
}

inline simd_f32 simd_Min(simd_f32 a, simd_f32 b)
{
}

inline simd_f32 simd_Max(simd_f32 a, simd_f32 b)
{
}

#else
#error "Invalid SIMD_LANE_WIDTH"
#endif

// TODO: Use inverse ray direction
inline simd_f32 simd_RayIntersectAabb(simd_vec3 boxMin, simd_vec3 boxMax,
    simd_vec3 rayOrigin, simd_vec3 rayDirection)
{
    simd_f32 tmin = SIMD_F32(0.0f);
    simd_f32 tmax = SIMD_F32(F32_MAX);

    simd_u32 parallelMask = SIMD_U32(0);

    // TODO: Loop unroll manually
    for (u32 axis = 0; axis < 3; ++axis)
    {
        simd_f32 s = rayOrigin.data[axis];
        simd_f32 d = rayDirection.data[axis];
        simd_f32 min = boxMin.data[axis];
        simd_f32 max = boxMax.data[axis];

        // Check if ray direction is parallel to axis
        simd_u32 mask = simd_LessThan(simd_Abs(d), SIMD_F32(EPSILON));
        simd_u32 u = simd_LessThan(s, min);
        simd_u32 v = simd_GreaterThan(s, max);
        simd_u32 mask0 = simd_Or(u, v);
        simd_u32 mask1 = simd_And(mask, mask0); // Out of bounds, no collision if true
        parallelMask = simd_Or(parallelMask, mask1);

        simd_f32 a = simd_Divide(simd_Subtract(min, s), d);
        simd_f32 b = simd_Divide(simd_Subtract(max, s), d);

        simd_f32 t0 = simd_Min(a, b);
        simd_f32 t1 = simd_Max(a, b);

        tmin = simd_Max(tmin, t0);
        tmax = simd_Min(tmax, t1);
    }

    // Check if tmin > tmax, no collision if true
    simd_u32 mask2 = simd_GreaterThan(tmin, tmax);
    simd_u32 noCollisionMask = simd_Or(parallelMask, mask2);
    simd_f32 result = simd_Select(noCollisionMask, SIMD_F32(-1.0f), tmin);
    return result;
}
