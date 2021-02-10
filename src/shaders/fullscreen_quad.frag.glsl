#version 450 

layout(binding = 2) uniform sampler defaultSampler;
layout(binding = 3) uniform texture2D inputTexture;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outputColor;
void main()
{
    vec3 color = texture(sampler2D(inputTexture, defaultSampler), fragTexCoord).rgb;
    outputColor = vec4(color, 1.0);
}
