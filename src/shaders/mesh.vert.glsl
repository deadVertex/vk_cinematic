#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 viewMatrices[16];
    mat4 projectionMatrices[16];
    vec3 cameraPosition;
} ubo;

struct VertexPC
{
    float px, py, pz;
    float cr, cg, cb;
};

layout(binding = 1) readonly buffer Vertices
{
    VertexPC vertices[];
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uint vertexDataOffset = 0;
    uint uboIndex = 0;

    vec3 inPosition = vec3(
            vertices[gl_VertexIndex + vertexDataOffset].px,
            vertices[gl_VertexIndex + vertexDataOffset].py,
            vertices[gl_VertexIndex + vertexDataOffset].pz);

    vec3 inColor = vec3(
            vertices[gl_VertexIndex + vertexDataOffset].cr,
            vertices[gl_VertexIndex + vertexDataOffset].cg,
            vertices[gl_VertexIndex + vertexDataOffset].cb);

    gl_Position = ubo.projectionMatrices[uboIndex] *
                  ubo.viewMatrices[uboIndex] * 
                  vec4(inPosition, 1.0);

    fragColor = vec4(inColor, 1.0);
}
