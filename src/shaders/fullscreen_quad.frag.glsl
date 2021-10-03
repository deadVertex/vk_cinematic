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
void main()
{
    if (ubo.showComparision != 0)
    {
        if (fragTexCoord.x < 0.5)
            discard;
    }

    vec3 color = texture(sampler2D(inputTexture, defaultSampler), fragTexCoord).rgb;
    outputColor = vec4(color, 1.0);
}
