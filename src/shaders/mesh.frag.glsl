#version 450

// FIXME: Copied from src/shaders/skybox.frag.glsl
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
layout(binding = 6) uniform textureCube cubeMap;
layout(binding = 7) uniform texture2D irradianceMap;
layout(binding = 8) uniform texture2D checkerBoardTexture;

layout(location = 0) out vec4 outputColor;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) flat in uint fragMaterialIndex;
layout(location = 2) in vec3 fragLocalPosition;
layout(location = 3) in vec2 fragTexCoord;

// FIXME: Copied from src/math_lib.h and src/shaders/skybox.frag.glsl
// NOTE: v must be a unit vector
// RETURNS: (azimuth, inclination)
vec2 ToSphericalCoordinates(vec3 v)
{
    float r = 1.0f;
    float inc = atan(sqrt(v.x * v.x + v.z * v.z), v.y);

    float azimuth = atan(v.z,v.x);

    return vec2(azimuth, inc);
}

// FIXME: Copied from src/math_lib.h and src/shaders/skybox.frag.glsl
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
    vec3 baseColor = vec3(materials[fragMaterialIndex].baseColorR,
                          materials[fragMaterialIndex].baseColorG,
                          materials[fragMaterialIndex].baseColorB);
    if (fragMaterialIndex == 3)
    {
        baseColor = texture(
            sampler2D(checkerBoardTexture, defaultSampler), fragTexCoord).rgb;
    }

    vec3 lightColor = vec3(1, 0.95, 0.8);
    vec3 lightDirection = normalize(vec3(1, 1, 0.5));
    vec3 normal = normalize(fragNormal);

    // For each light
    //float cosine = max(dot(fragNormal, lightDirection), 0);
    //float brdf = 1.0;
    //vec3 incomingRadiance = lightColor * brdf * cosine;

    // TODO: Optimize this by using a cubemap instead
    vec2 sphereCoords =
        ToSphericalCoordinates(normal);
    vec2 uv = MapToEquirectangular(sphereCoords);
    uv.y = 1.0f - uv.y; // Flip Y axis as usual
    vec3 incomingRadiance = texture(sampler2D(irradianceMap, defaultSampler), uv).rgb;

    vec3 outgoingRadiance = baseColor * incomingRadiance;
    outputColor = vec4(outgoingRadiance, 0);
}
