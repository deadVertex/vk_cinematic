#version 450

layout(location = 0) out vec4 outputColor;

layout(location = 0) in vec3 fragNormal;

void main()
{
    vec3 baseColor = vec3(0.8, 0.8, 0.8);
    vec3 lightColor = vec3(1, 0.95, 0.8);
    vec3 lightDirection = normalize(vec3(1, 1, 0.5));
    vec3 normal = normalize(fragNormal);
    vec3 radiance =
        baseColor * lightColor * max(dot(fragNormal, lightDirection), 0);
    outputColor = vec4(radiance, 0);
}
