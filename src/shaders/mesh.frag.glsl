#version 450

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

layout(location = 0) out vec4 outputColor;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) flat in uint fragMaterialIndex;

void main()
{
    vec3 baseColor = vec3(materials[fragMaterialIndex].baseColorR,
                          materials[fragMaterialIndex].baseColorG,
                          materials[fragMaterialIndex].baseColorB);

    vec3 lightColor = vec3(1, 0.95, 0.8);
    vec3 lightDirection = normalize(vec3(1, 1, 0.5));
    vec3 normal = normalize(fragNormal);

    // For each light
    float cosine = max(dot(fragNormal, lightDirection), 0);
    float brdf = 1.0;
    vec3 incomingRadiance = lightColor * brdf * cosine;

    vec3 outgoingRadiance = baseColor * incomingRadiance;
    outputColor = vec4(outgoingRadiance, 0);
}
