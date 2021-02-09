#pragma once

#include <cmath>
#include <cfloat>

#define PI 3.14159265359f
#define EPSILON FLT_EPSILON

inline f32 Min(f32 a, f32 b)
{
    f32 result = a < b ? a : b;
    return result;
}

inline f32 Max(f32 a, f32 b)
{
    f32 result = a > b ? a : b;
    return result;
}

inline i32 Min(i32 a, i32 b)
{
    i32 result = a < b ? a : b;
    return result;
}

inline i32 Max(i32 a, i32 b)
{
    i32 result = a > b ? a : b;
    return result;
}

inline u32 MinU32(u32 a, u32 b)
{
    u32 result = a < b ? a : b;
    return result;
}

inline u32 MaxU32(u32 a, u32 b)
{
    u32 result = a > b ? a : b;
    return result;
}

inline f32 Sqrt(f32 x)
{
    f32 result = sqrtf(x);
    return result;
}

inline f32 Radians(f32 degrees)
{
    f32 result = degrees * (PI / 180.0f);
    return result;
}

inline f32 Abs(f32 x)
{
    f32 result = fabs(x);
    return result;
}

inline f32 Pow(f32 x, f32 y)
{
    f32 result = powf(x, y);
    return result;
}

inline f32 Sin(f32 x)
{
    f32 result = sinf(x);
    return result;
}

inline f32 Cos(f32 x)
{
    f32 result = cosf(x);
    return result;
}

inline f32 Tan(f32 x)
{
    f32 result = tanf(x);
    return result;
}

inline f32 Acos(f32 x)
{
    f32 result = acosf(x);
    return result;
}

inline f32 Atan2(f32 y, f32 x)
{
    f32 result = atan2f(y, x);
    return result;
}

inline f32 Round(f32 x)
{
    f32 result = roundf(x);
    return result;
}

inline f32 Truncate(f32 x)
{
    f32 result = (f32)((i32)x);
    return result;
}

inline f32 Floor(f32 x)
{
    f32 result = floorf(x);
    return result;
}

inline f32 Ceil(f32 x)
{
    f32 result = ceilf(x);
    return result;
}

inline b32 SignBit(f32 x)
{
    b32 result = signbit(x);
    return result;
}

inline f32 Fmod(f32 num, f32 denom)
{
    f32 result = fmod(num, denom);
    return result;
}

inline f32 Clamp(f32 x, f32 min, f32 max)
{
    f32 result = Min(Max(x, min), max);
    return result;
}

inline f32 Clamp01(f32 x)
{
    f32 result = Clamp(x, 0.0f, 1.0f);
    return result;
}

inline i32 Clamp(i32 x, i32 min, i32 max)
{
    i32 result = Min(Max(x, min), max);
    return result;
}

inline f32 MapToUnitRange(f32 x, f32 min, f32 max)
{
    f32 range = max - min;
    f32 result = Clamp01((x - min) / range);
    return result;
}

inline f32 Lerp(f32 a, f32 b, f32 t)
{
    f32 result = a * (1.0f - t) + b * t;
    return result;
}

inline f32 EaseInQuad(f32 t)
{
    f32 result = t * t;
    return result;
}

inline f32 EaseOutQuad(f32 t)
{
    f32 result = t * (2.0f - t);
    return result;
}

inline f32 EaseInOutQuad(f32 t)
{
    f32 result = t < 0.0f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
    return result;
}

struct RandomNumberGenerator
{
    u32 state;
};

// Reference implementation from https://en.wikipedia.org/wiki/Xorshift
inline u32 XorShift32(RandomNumberGenerator *rng)
{
    u32 x = rng->state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
    rng->state = x;
	return x;
}

inline f32 RandomUnilateral(RandomNumberGenerator *rng)
{
    // Clear the sign bit as SSE interprets integers as signed when converting
    // to packed single
    f32 numerator = (f32)(XorShift32(rng) >> 1);
    f32 denom = (f32)(U32_MAX >> 1);
    f32 result = numerator / denom;
    return result;
}

inline f32 RandomBilateral(RandomNumberGenerator *rng)
{
    f32 result = -1.0f + 2.0f * RandomUnilateral(rng);
    return result;
}
