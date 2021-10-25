#version 450

#define PI 3.14159265359f

struct Material
{
    float baseColorR, baseColorG, baseColorB;
    float emissionColorR, emissionColorG, emissionColorB;
};

layout(binding = 5) readonly buffer Materials
{
    Material materials[];
};

layout(binding = 2) uniform sampler defaultSampler;
//layout(binding = 6) uniform textureCube cubeMap;
layout(binding = 6) uniform texture2D envMap;
layout(binding = 7) uniform texture2D irradianceMap;

layout(location = 0) out vec4 outputColor;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) flat in uint fragMaterialIndex;
layout(location = 2) in vec3 fragLocalPosition;

// FIXME: Copied from src/math_lib.h
// NOTE: v must be a unit vector
// RETURNS: (azimuth, inclination)
vec2 ToSphericalCoordinates(vec3 v)
{
    float r = 1.0f;
    float inc = atan(sqrt(v.x * v.x + v.z * v.z), v.y);

    float azimuth = atan(v.z,v.x);

    return vec2(azimuth, inc);
}

// FIXME: Copied from src/math_lib.h
vec2 MapToEquirectangular(vec2 sphereCoords)
{
    vec2 uv = vec2(0, 0);
    if (sphereCoords.x < 0.0f)
    {
        sphereCoords.x += 2.0f * PI;
    }
    uv.x = sphereCoords.x / (2.0f * PI);
    uv.y = cos(sphereCoords.y) * 0.5f + 0.5f;

    return uv;
}

void main()
{
//    vec4 textureSample =
//        texture(samplerCube(cubeMap, defaultSampler), fragLocalPosition);

    vec2 sphereCoords =
        ToSphericalCoordinates(fragLocalPosition);
    vec2 uv = MapToEquirectangular(sphereCoords);
    uv.y = 1.0f - uv.y; // Flip Y axis as usual
    vec4 textureSample = texture(sampler2D(envMap, defaultSampler), uv);
    outputColor = vec4(textureSample.rgb, 1);
}
