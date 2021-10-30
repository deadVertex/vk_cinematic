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
    vec4 textureSample =
        texture(samplerCube(cubeMap, defaultSampler), fragLocalPosition);

    outputColor = vec4(textureSample.rgb, 1);
}
