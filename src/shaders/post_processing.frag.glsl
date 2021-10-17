#version 450 

layout(binding = 0) uniform UniformBufferObject {
    mat4 viewMatrices[16];
    mat4 projectionMatrices[16];
    vec3 cameraPosition;
    uint showComparision;
} ubo;

layout(binding = 2) uniform sampler defaultSampler;
layout(binding = 3) uniform texture2D inputTexture;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outputColor;

vec3 PerformToneMapping(vec3 color)
{
    color *= 1.0; // exposure adjustment

    color = color / (vec3(1.0) + color);

    return pow(color, vec3(1.0 / 2.2));
}

void main()
{
    if (ubo.showComparision != 0)
    {
        if (fragTexCoord.x < 0.5)
            discard;
    }

    vec3 color = texture(sampler2D(inputTexture, defaultSampler), fragTexCoord).rgb;

    vec3 toneMappedColor = PerformToneMapping(color);

    outputColor = vec4(toneMappedColor, 1.0);
}
