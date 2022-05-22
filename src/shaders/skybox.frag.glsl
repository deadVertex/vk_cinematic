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
layout(binding = 6) uniform textureCube cubeMap;
layout(binding = 7) uniform textureCube irradianceMap;

layout(location = 0) out vec4 outputColor;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) flat in uint fragMaterialIndex;
layout(location = 2) in vec3 fragLocalPosition;
layout(location = 3) in vec2 fragTexCoord;

void main()
{
    // Material processing code
    vec3 baseColor = vec3(materials[fragMaterialIndex].baseColorR,
                          materials[fragMaterialIndex].baseColorG,
                          materials[fragMaterialIndex].baseColorB);
    vec3 emissionColor = vec3(materials[fragMaterialIndex].emissionColorR,
                              materials[fragMaterialIndex].emissionColorG,
                              materials[fragMaterialIndex].emissionColorB);

    // FIXME: Properly integrate this
    vec3 lightColor = vec3(1);
    vec3 lightDirection = -normalize(vec3(-1, -0.4, -0.4));
    vec3 viewDir = normalize(fragLocalPosition);
    emissionColor +=
        lightColor * max(0.0f, dot(lightDirection, viewDir));

    //vec4 textureSample =
        //texture(samplerCube(cubeMap, defaultSampler), fragLocalPosition);

    outputColor = vec4(emissionColor, 1);
}
