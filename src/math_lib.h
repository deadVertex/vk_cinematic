#pragma once

#include <cmath>
#include <cfloat>

#define PI 3.14159265359f
#define EPSILON FLT_EPSILON
#define F32_MAX FLT_MAX

#include "math_utils.h"

union vec2
{
    struct
    {
        f32 x;
        f32 y;
    };

    struct
    {
        f32 u;
        f32 v;
    };

    f32 data[2];
};

union vec3
{
    struct
    {
        f32 x;
        f32 y;
        f32 z;
    };

    struct
    {
        f32 r;
        f32 g;
        f32 b;
    };

    struct
    {
        vec2 xy;
        f32 __unused0;
    };

    f32 data[3];
};

union vec4
{
    struct
    {
        f32 r;
        f32 g;
        f32 b;
        f32 a;
    };

    struct
    {
        f32 x;
        f32 y;
        f32 z;
        f32 w;
    };

    struct
    {
        vec3 xyz;
        f32 __unused0;
    };

    struct
    {
        vec3 rgb;
        f32 __unused1;
    };

    struct
    {
        vec3 v;
        f32 s;
    };

    f32 data[4];
};

union mat4
{
    vec4 columns[4];
    f32 data[16];
};

typedef vec4 quat;

inline vec4 Vec4(f32 r, f32 g, f32 b, f32 a)
{
    vec4 result = {r, g, b, a};
    return result;
}

inline vec4 Vec4(f32 x)
{
    vec4 result = {x, x, x, x};
    return result;
}

inline vec3 Vec3(f32 x, f32 y, f32 z)
{
    vec3 result = {x, y, z};
    return result;
}

inline vec3 Vec3(f32 x)
{
    vec3 result = {x, x, x};
    return result;
}

inline vec3 Vec3(vec2 v, f32 z)
{
    vec3 result = {v.x, v.y, z};
    return result;
}

inline vec2 Vec2(f32 x, f32 y)
{
    vec2 result = { x, y };
    return result;
}

inline vec2 Vec2(f32 x)
{
    vec2 result = { x, x };
    return result;
}

inline vec2 operator+(vec2 a, vec2 b)
{
    vec2 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

inline vec2& operator+=(vec2 &a, vec2 b)
{
    a = a + b;
    return a;
}

inline vec2 operator-(vec2 a, vec2 b)
{
    vec2 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

inline vec2& operator-=(vec2 &a, vec2 b)
{
    a = a - b;
    return a;
}

inline vec3 operator+(vec3 a, vec3 b)
{
    vec3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

inline vec3& operator+=(vec3 &a, vec3 b)
{
    a = a + b;
    return a;
}

inline vec3 operator-(vec3 a, vec3 b)
{
    vec3 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

inline vec3& operator-=(vec3 &a, vec3 b)
{
    a = a - b;
    return a;
}

inline mat4 Identity()
{
    mat4 result = {};
    result.data[0] = 1.0f;
    result.data[5] = 1.0f;
    result.data[10] = 1.0f;
    result.data[15] = 1.0f;

    return result;
};

inline mat4 Scale(vec3 scale)
{
    mat4 result = {};
    result.data[0] = scale.x;
    result.data[5] = scale.y;
    result.data[10] = scale.z;
    result.data[15] = 1.0f;

    return result;
}

inline mat4 Scale(f32 x)
{
    return Scale(Vec3(x, x, x));
}

inline mat4 Translate(vec3 translation)
{
    mat4 result = Identity();
    result.data[12] = translation.x;
    result.data[13] = translation.y;
    result.data[14] = translation.z;

    return result;
}

inline f32 Dot(vec4 a, vec4 b)
{
    // clang-format off
    f32 result = a.data[0] * b.data[0] +
                 a.data[1] * b.data[1] +
                 a.data[2] * b.data[2] +
                 a.data[3] * b.data[3];
    // clang-format on

    return result;
}

inline mat4 operator*(mat4 a, mat4 b)
{
    mat4 result = {};

    for (u32 i = 0; i < 4; ++i)
    {
        // clang-format off
        vec4 row = Vec4(a.columns[0].data[i],
                        a.columns[1].data[i],
                        a.columns[2].data[i],
                        a.columns[3].data[i]);

        result.columns[0].data[i] = Dot(row, b.columns[0]);
        result.columns[1].data[i] = Dot(row, b.columns[1]);
        result.columns[2].data[i] = Dot(row, b.columns[2]);
        result.columns[3].data[i] = Dot(row, b.columns[3]);
        // clang-format on
    }

    return result;
}

inline vec3 operator*(vec3 a, f32 b)
{
    vec3 result;
    result.x = a.x * b;
    result.y = a.y * b;
    result.z = a.z * b;
    return result;
}

inline vec3 operator*(f32 a, vec3 b)
{
    vec3 result = b * a;
    return result;
}

inline vec3& operator*=(vec3 &a, f32 b)
{
    a = a * b;
    return a;
}

inline vec2 operator*(vec2 a, f32 b)
{
    vec2 result;
    result.x = a.x * b;
    result.y = a.y * b;
    return result;
}

inline vec2 operator*(f32 a, vec2 b)
{
    vec2 result = b * a;
    return result;
}

inline vec3 Lerp(vec3 a, vec3 b, f32 t)
{
    vec3 result = a * (1.0f - t) + b * t;
    return result;
}

inline mat4 Perspective(f32 fovy, f32 aspect, f32 zNear, f32 zFar)
{
    f32 radians = Radians(fovy);
    f32 scale = Tan(radians * 0.5f) * zNear;

    f32 right = aspect * scale;
    f32 left = -right;

    f32 top = scale;
    f32 bottom = -scale;

    mat4 result = {};
    result.columns[0].x = (2.0f * zNear) / (right - left);
    result.columns[1].y = (2.0f * zNear) / (top - bottom);
    result.columns[2].x = (right + left) / (right - left);
    result.columns[2].y = (top + bottom) / (top - bottom);
    result.columns[2].z = -(zFar + zNear) / (zFar - zNear);
    result.columns[2].w = -1.0f;
    result.columns[3].z = -(2.0f * zFar * zNear) / (zFar - zNear);

    return result;
}

inline mat4 RotateX(f32 radians)
{
    mat4 result = Identity();

    result.columns[1].y = Cos(radians);
    result.columns[1].z = Sin(radians);
    result.columns[2].y = -Sin(radians);
    result.columns[2].z = Cos(radians);

    return result;
}

inline mat4 RotateY(f32 radians)
{
    mat4 result = Identity();

    result.columns[0].x = Cos(radians);
    result.columns[0].z = -Sin(radians);
    result.columns[2].x = Sin(radians);
    result.columns[2].z = Cos(radians);

    return result;
}

inline mat4 RotateZ(f32 radians)
{
    mat4 result = Identity();

    result.columns[0].x = Cos(radians);
    result.columns[0].y = Sin(radians);
    result.columns[1].x = -Sin(radians);
    result.columns[1].y = Cos(radians);

    return result;
}

inline vec3 operator-(vec3 a)
{
    vec3 result;
    result.x = -a.x;
    result.y = -a.y;
    result.z = -a.z;
    return result;
}

inline vec4 operator*(mat4 a, vec4 b)
{
    vec4 result = {};

    for (u32 i = 0; i < 4; ++i)
    {
        // clang-format off
        vec4 row = Vec4(a.columns[0].data[i],
                        a.columns[1].data[i],
                        a.columns[2].data[i],
                        a.columns[3].data[i]);

        result.data[i] = Dot(row, b);
        // clang-format on
    }

    return result;
}

inline vec4 operator*(vec4 v, f32 s)
{
    vec4 result;
    result.x = v.x * s;
    result.y = v.y * s;
    result.z = v.z * s;
    result.w = v.w * s;
    return result;
}

inline vec4 operator+(vec4 a, vec4 b)
{
    vec4 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    result.w = a.w + b.w;
    return result;
}

inline vec4 Lerp(vec4 a, vec4 b, f32 t)
{
    vec4 result = a * (1.0f - t) + b * t;
    return result;
}

inline vec4 Normalize(vec4 v)
{
    f32 length = Sqrt(Dot(v, v));
    vec4 result = {};
    if (length > EPSILON)
    {
        result = v * (1.0f / length);
    }
    return result;
}

inline vec4 Vec4(vec3 v, f32 w)
{
    vec4 result = {v.x, v.y, v.z, w};
    return result;
}

inline vec3 Vec3(vec4 v)
{
    vec3 result = {v.x, v.y, v.z};
    return result;
}

inline f32 Dot(vec3 a, vec3 b)
{
    f32 result = a.x * b.x + a.y * b.y + a.z * b.z;
    return result;
}

inline vec3 Cross(vec3 a, vec3 b)
{
    vec3 result;

    result.x = (a.y * b.z) - (a.z * b.y);
    result.y = (a.z * b.x) - (a.x * b.z);
    result.z = (a.x * b.y) - (a.y * b.x);

    return result;

}

inline f32 Dot(vec2 a, vec2 b)
{
    f32 result = a.x * b.x + a.y * b.y;
    return result;
}

inline f32 LengthSq(vec2 a)
{
    f32 result = Dot(a, a);
    return result;
}

inline f32 LengthSq(vec3 a)
{
    f32 result = Dot(a, a);
    return result;
}

inline f32 Length(vec3 v)
{
    f32 result = Sqrt(Dot(v, v));
    return result;
}

inline vec3 Normalize(vec3 v)
{
    f32 length = Length(v);
    vec3 result = {};
    if (length > EPSILON)
    {
        result = v * (1.0f / length);
    }
    return result;
}

inline vec3 Hadamard(vec3 a, vec3 b)
{
    vec3 result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    result.z = a.z * b.z;
    return result;
}

inline u32 ToColor(vec4 c)
{
    u8 r = (u8)Floor(c.r * 255.0f);
    u8 g = (u8)Floor(c.g * 255.0f);
    u8 b = (u8)Floor(c.b * 255.0f);
    u8 a = (u8)Floor(c.a * 255.0f);

    u32 result = (a << 24) | (b << 16) | (g << 8) | r;
    return result;
}

inline mat4 Orthographic(
    f32 left, f32 right, f32 bottom, f32 top, f32 zNear = 0.0f, f32 zFar = 1.0f)
{
    mat4 result = {};

    result.data[0] = 2.0f / (right - left);
    result.data[5] = 2.0f / (top - bottom);
    result.data[10] = -2.0f / (zFar - zNear);
    result.data[12] = -(right + left) / (right - left);
    result.data[13] = -(top + bottom) / (top - bottom);
    result.data[14] = -(zFar + zNear) / (zFar - zNear);
    result.data[15] = 1.0f;

    return result;
}

inline quat Quat()
{
    quat result = {0, 0, 0, 1};
    return result;
}

inline quat Quat(vec3 axis, f32 angle)
{
    quat result = {};
    result.xyz = axis * Sin(angle * 0.5f);
    result.w = Cos(angle * 0.5f);

    return result;
}

inline quat operator*(quat p, quat q)
{
    quat result;
    result.v = p.s * q.v + q.s * p.v + Cross(p.v, q.v);
    result.s = p.s * q.s - Dot(p.v, q.v);
    return result;
}

inline quat& operator*=(quat &p, quat q)
{
    p = p * q;
    return p;
}

// Same as inverse for unit length quaternions
inline quat Conjugate(quat p)
{
    quat result;
    result.v = -p.v;
    result.s = p.s;
    return result;
}

inline vec3 RotateVector(vec3 v, quat p)
{
    quat q = {v.x, v.y, v.z, 0.0f};
    quat result = p * q * Conjugate(p);
    return result.xyz;
}

inline quat LerpQuat(quat a, quat b, f32 t)
{
    quat result = Lerp(a, b, t);
    result = Normalize(result);
    return result;
}

inline mat4 Rotate(quat rotation)
{
    mat4 result = Identity();
    float x = rotation.x;
    float y = rotation.y;
    float z = rotation.z;
    float w = rotation.w;

    result.columns[0].x = 1.0f - 2.0f * y * y - 2.0f * z * z;
    result.columns[0].y = 2.0f * x * y + 2.0f * z * w;
    result.columns[0].z = 2.0f * x * z - 2.0f * y * w;

    result.columns[1].x = 2.0f * x * y - 2.0f * z * w;
    result.columns[1].y = 1.0f - 2.0f * x * x - 2.0f * z * z;
    result.columns[1].z = 2.0f * y * z + 2.0f * x * w;

    result.columns[2].x = 2.0f * x * z + 2.0f * y * w;
    result.columns[2].y = 2.0f * y * z - 2.0f * x * w;
    result.columns[2].z = 1.0f - 2.0f * x * x - 2.0f * y * y;

    return result;
}

// NOTE: From wikipedia, no idea how it works, see
// https://math.stackexchange.com/a/2975462 for a good explanation, should use
// this implementation as a basis in the future
inline vec3 ToEulerAngles(quat q)
{
    vec3 result;
    f32 sinXCosY = 2.0f * (q.s * q.x + q.y * q.z);
    f32 cosXCosY = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    result.x = atan2f(sinXCosY, cosXCosY);

    f32 sinY = 2.0f * (q.s * q.y - q.z * q.x);
    result.y = (Abs(sinY) >= 1.0f) ? (f32)copysign(PI * 0.5f, sinY) : (f32)asin(sinY);

    f32 sinZCosY = 2.0f * (q.w * q.z + q.x * q.y);
    f32 cosZCosY = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    result.z = atan2f(sinZCosY, cosZCosY);

    return result;
}

// NOTE: Same as ToEulerAngles
inline quat FromEulerAngles(vec3 eulerAngles)
{
    f32 cz = Cos(eulerAngles.z * 0.5f);
    f32 sz = Sin(eulerAngles.z * 0.5f);
    f32 cy = Cos(eulerAngles.y * 0.5f);
    f32 sy = Sin(eulerAngles.y * 0.5f);
    f32 cx = Cos(eulerAngles.x * 0.5f);
    f32 sx = Sin(eulerAngles.x * 0.5f);

    quat result;
    result.s = cz * cy * cx + sz * sy * sx;
    result.v.x = cz * cy * sx - sz * sy * cx;
    result.v.y = sz * cy * sx + cz * sy * cx;
    result.v.z = sz * cy * cx - cz * sy * sx;
    return result;
}

inline void AxisAngle(quat q, vec3 *axis, f32 *angle)
{
    f32 halfAngle = acosf(q.s);
    *angle = halfAngle * 2.0f;
    f32 invSinHalfAngle = 1.0f / Sin(halfAngle);
    *axis = q.v * invSinHalfAngle;
}

inline vec3 SrgbToLinear(vec3 v)
{
    vec3 result;
    result.x = Pow(v.x, 2.2f);
    result.y = Pow(v.y, 2.2f);
    result.z = Pow(v.z, 2.2f);

    return result;
}

inline mat4 ChangeOfBasis(vec3 x)
{
    vec3 z = Cross(x, Vec3(0, 1, 0));
    if (LengthSq(z) < EPSILON)
    {
        z = Cross(Vec3(1, 0, 0), x);
        Assert(LengthSq(z) > EPSILON);
    }
    z = Normalize(z);

    vec3 y = Normalize(Cross(z, x));

    mat4 result;
    result.columns[0] = Vec4(x, 0.0f);
    result.columns[1] = Vec4(y, 0.0f);
    result.columns[2] = Vec4(z, 0.0f);
    result.columns[3] = Vec4(0, 0, 0, 1);

    return result;
}

inline mat4 Transpose(mat4 a)
{
    mat4 result;
    result.data[0] = a.data[0];
    result.data[1] = a.data[4];
    result.data[2] = a.data[8];
    result.data[3] = a.data[12];

    result.data[4] = a.data[1];
    result.data[5] = a.data[5];
    result.data[6] = a.data[9];
    result.data[7] = a.data[13];

    result.data[8] = a.data[2];
    result.data[9] = a.data[6];
    result.data[10] = a.data[10];
    result.data[11] = a.data[14];

    result.data[12] = a.data[3];
    result.data[13] = a.data[7];
    result.data[14] = a.data[11];
    result.data[15] = a.data[15];

    return result;
}

struct rect2
{
    vec2 min;
    vec2 max;
};

inline b32 RectContainsPoint(rect2 rect, vec2 p)
{
    b32 result = false;
    if (p.x >= rect.min.x && p.x < rect.max.x)
    {
        if (p.y >= rect.min.y && p.y < rect.max.y)
        {
            result = true;
        }
    }

    return result;
}

inline vec3 TransformVector(vec3 d, mat4 m)
{
    vec4 v = m * Vec4(d, 0.0);
    return v.xyz;
}

inline vec3 TransformPoint(vec3 p, mat4 m)
{
    vec4 v = m * Vec4(p, 1.0);
    return v.xyz;
}

inline vec3 Min(vec3 a, vec3 b)
{
    vec3 result;
    result.x = Min(a.x, b.x);
    result.y = Min(a.y, b.y);
    result.z = Min(a.z, b.z);

    return result;
}

inline vec3 Max(vec3 a, vec3 b)
{
    vec3 result;
    result.x = Max(a.x, b.x);
    result.y = Max(a.y, b.y);
    result.z = Max(a.z, b.z);

    return result;
}

inline vec3 Abs(vec3 v)
{
    vec3 result;
    result.x = Abs(v.x);
    result.y = Abs(v.y);
    result.z = Abs(v.z);

    return result;
}

inline u32 CalculateLargestAxis(vec3 v)
{
    v = Abs(v);

    u32 axis = 0;
    if (v.data[1] > v.data[axis])
    {
        axis = 1;
    }
    if (v.data[2] > v.data[axis])
    {
        axis = 2;
    }

    return axis;
}

inline vec3 Pow(vec3 v, vec3 e)
{
    vec3 result;
    result.x = Pow(v.x, e.x);
    result.y = Pow(v.y, e.y);
    result.z = Pow(v.z, e.z);

    return result;
}

inline vec3 Sqrt(vec3 v)
{
    vec3 result;
    result.x = Sqrt(v.x);
    result.y = Sqrt(v.y);
    result.z = Sqrt(v.z);
    return result;
}

inline vec3 Clamp(vec3 v, vec3 min, vec3 max)
{
    vec3 result;
    result.x = Clamp(v.x, min.x, max.x);
    result.y = Clamp(v.y, min.y, max.y);
    result.z = Clamp(v.z, min.z, max.z);

    return result;
}

// NOTE: v must be a unit vector
// RETURNS: (azimuth, inclination)
inline vec2 ToSphericalCoordinates(vec3 v)
{
    // TODO: Do we always want these assertions compiled in?
    //Assert(LengthSq(v) - 1.0f <= EPSILON);

    f32 r = 1.0f;
    f32 inc = Atan2(Sqrt(v.x * v.x + v.z * v.z), v.y);

    f32 azimuth = Atan2(v.z,v.x);

    return Vec2(azimuth, inc);
}

inline vec2 MapToEquirectangular(vec2 sphereCoords)
{
    vec2 uv = {};
    if (sphereCoords.x < 0.0f)
    {
        sphereCoords.x += 2.0f * PI;
    }
    uv.x = sphereCoords.x / (2.0f * PI);
    uv.y = Cos(sphereCoords.y) * 0.5f + 0.5f;

    return uv;
}

// NOTE: This function could probably be done more efficiently, I've just done
// a direct inverse of the MapToEquirectangular function verified with some
// basic unit testing.
inline vec2 MapEquirectangularToSphereCoordinates(vec2 uv)
{
    f32 a = uv.y * 2.0f - 1.0f;
    f32 y = Acos(a);

    f32 b = uv.x * 2.0f * PI;
    if (b > PI)
    {
        b -= 2.0f * PI;
    }

    vec2 sphereCoords = Vec2(b, y);
    return sphereCoords;
}
