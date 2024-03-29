#version 450

// FIXME: Copied from src/shaders/skybox.frag.glsl
#define PI 3.14159265359f

#define Material_CheckerBoard 2

struct Material
{
    float baseColorR, baseColorG, baseColorB;
    float emissionColorR, emissionColorG, emissionColorB;
    float roughness;
};

struct SphereLightData
{
    float px, py, pz;
    float radianceR, radianceG, radianceB;
    float radius;
};

// TODO: Just replace this with a vec3 surely?
struct AmbientLightData
{
    float radianceR, radianceG, radianceB;
};

struct DiskLightData
{
    float nx, ny, nz;
    float px, py, pz;
    float rr, rg, rb;
    float radius;
};

layout(binding = 5) readonly buffer Materials
{
    Material materials[];
};

layout(binding = 10) readonly buffer LightData
{
    uint sphereLightCount;
    SphereLightData sphereLights[20]; // TODO: Use MAX_SPHERE_LIGHTS constant
    AmbientLightData ambientLight;
    uint diskLightCount;
    DiskLightData diskLights[4];
} lightData;

layout(binding = 2) uniform sampler defaultSampler;
layout(binding = 6) uniform textureCube cubeMap;
layout(binding = 7) uniform textureCube irradianceMap;
layout(binding = 8) uniform texture2D checkerBoardTexture;

layout(location = 0) out vec4 outputColor;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) flat in uint fragMaterialIndex;
layout(location = 2) in vec3 fragLocalPosition;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in vec3 fragWorldPosition;

void main()
{
    vec3 baseColor = vec3(materials[fragMaterialIndex].baseColorR,
                          materials[fragMaterialIndex].baseColorG,
                          materials[fragMaterialIndex].baseColorB);
    vec3 emissionColor = vec3(materials[fragMaterialIndex].emissionColorR,
                              materials[fragMaterialIndex].emissionColorG,
                              materials[fragMaterialIndex].emissionColorB);
    if (fragMaterialIndex == Material_CheckerBoard)
    {
        baseColor = texture(
            sampler2D(checkerBoardTexture, defaultSampler), fragTexCoord).rgb;
    }

    vec3 lightColor = vec3(1, 0.95, 0.8);
    vec3 lightDirection = normalize(vec3(1, 1, 0.5));
    vec3 normal = normalize(fragNormal);

    vec3 outgoingRadiance = vec3(0);
    // Ambient light
    vec3 ambientLightRadiance = vec3(lightData.ambientLight.radianceR,
        lightData.ambientLight.radianceG, lightData.ambientLight.radianceB);

    // TODO: Ambient occlusion
    // FIXME: Color for this doesn't look right
    outgoingRadiance += baseColor * ambientLightRadiance;

    // Environment map lighting
    vec3 environmentMapRadiance =
        texture(samplerCube(irradianceMap, defaultSampler), normal).rgb;

    outgoingRadiance += baseColor * environmentMapRadiance;

    // Disk light test. From "Moving Frostbite to PBR" presentation
    for (uint i = 0; i < lightData.diskLightCount; i++)
    {
        vec3 diskNormal = vec3(lightData.diskLights[i].nx,
                lightData.diskLights[i].ny,
                lightData.diskLights[i].nz);

        vec3 diskPosition = vec3(lightData.diskLights[i].px,
                lightData.diskLights[i].py,
                lightData.diskLights[i].pz);

        vec3 diskRadiance = vec3(lightData.diskLights[i].rr,
                lightData.diskLights[i].rg,
                lightData.diskLights[i].rb);

        float diskRadius = lightData.diskLights[i].radius;

        // TODO: Horizon clipping!
        vec3 L = diskPosition - fragWorldPosition;
        float sqrDist = dot(L, L);
        L = normalize(L);
        vec3 N = normal;
        float luminance = PI * max(0.0, dot(diskNormal, -L)) *
            max(0.0, dot(N, L)) / (sqrDist / (diskRadius * diskRadius) + 1);
        outgoingRadiance += baseColor * diskRadiance * luminance;
    }

    // Sphere light test
    for (uint i = 0; i < lightData.sphereLightCount; i++)
    {
        vec3 sphereCenter = vec3(lightData.sphereLights[i].px,
                                 lightData.sphereLights[i].py,
                                 lightData.sphereLights[i].pz);

        float radius = lightData.sphereLights[i].radius;
        vec3 lightRadiance = vec3(lightData.sphereLights[i].radianceR,
                                  lightData.sphereLights[i].radianceG,
                                  lightData.sphereLights[i].radianceB);

        vec3 L = sphereCenter - fragWorldPosition;
        float dist = length(L);
        L = normalize(L);
        float k = radius / dist;
        lightRadiance *= (k * k);

        outgoingRadiance +=
            baseColor * lightRadiance * max(dot(normal, L), 0.0);
    }
    outgoingRadiance += emissionColor;

    outputColor = vec4(outgoingRadiance, 0);
}
