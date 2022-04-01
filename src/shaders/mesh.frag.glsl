#version 450

// FIXME: Copied from src/shaders/skybox.frag.glsl
#define PI 3.14159265359f

#define Material_CheckerBoard 2

struct Material
{
    float baseColorR, baseColorG, baseColorB;
    float emissionColorR, emissionColorG, emissionColorB;
};

struct SphereLightData
{
    float px, py, pz;
    float radianceR, radianceG, radianceB;
    float radius;
};

layout(binding = 5) readonly buffer Materials
{
    Material materials[];
};

layout(binding = 10) readonly buffer LightData
{
    SphereLightData sphereLights[];
};

layout(binding = 2) uniform sampler defaultSampler;
layout(binding = 6) uniform textureCube cubeMap;
//layout(binding = 7) uniform textureCube irradianceMap;
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

    // For each light
    //float cosine = max(dot(fragNormal, lightDirection), 0);
    //float brdf = 1.0;
    //vec3 incomingRadiance = lightColor * brdf * cosine;

    //vec3 incomingRadiance =
        //texture(samplerCube(irradianceMap, defaultSampler), normal).rgb;

    //vec3 outgoingRadiance = baseColor * incomingRadiance;

    // Sphere light test
    uint sphereLightIndex = 0;

    vec3 sphereCenter = vec3(sphereLights[sphereLightIndex].px,
                             sphereLights[sphereLightIndex].py,
                             sphereLights[sphereLightIndex].pz);
    float radius = sphereLights[sphereLightIndex].radius;
    vec3 lightRadiance = vec3(sphereLights[sphereLightIndex].radianceR,
                              sphereLights[sphereLightIndex].radianceG,
                              sphereLights[sphereLightIndex].radianceB);

    vec3 L = sphereCenter - fragWorldPosition;
    float dist = length(L);
    L = normalize(L);
    float k = radius / dist;
    lightRadiance *= (k * k);

    vec3 outgoingRadiance = baseColor * lightRadiance * max(dot(normal, L), 0.0);
    outgoingRadiance += emissionColor;

    outputColor = vec4(outgoingRadiance, 0);
}
