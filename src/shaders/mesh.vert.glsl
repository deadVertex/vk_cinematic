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

struct VertexPNT
{
    float px, py, pz;
    float nx, ny, nz;
    float tx, ty;
};

layout(binding = 1) readonly buffer Vertices
{
    VertexPNT vertices[];
};

layout(binding = 4) readonly buffer ModelMatrices
{
    mat4 modelMatrices[];
};

layout(push_constant) uniform PushConstants
{
    uint modelMatrixIndex;
    uint vertexDataOffset;
};

layout(location = 0) out vec3 fragNormal;

void main()
{
    uint uboIndex = 0;

    mat4 modelMatrix = modelMatrices[modelMatrixIndex];
    mat3 invModelMatrix = transpose(mat3(modelMatrix));

    vec3 inPosition = vec3(
            vertices[gl_VertexIndex + vertexDataOffset].px,
            vertices[gl_VertexIndex + vertexDataOffset].py,
            vertices[gl_VertexIndex + vertexDataOffset].pz);

    vec3 inNormal = vec3(
            vertices[gl_VertexIndex + vertexDataOffset].nx,
            vertices[gl_VertexIndex + vertexDataOffset].ny,
            vertices[gl_VertexIndex + vertexDataOffset].nz);

    vec2 inTextureCoord = vec2(
            vertices[gl_VertexIndex + vertexDataOffset].tx,
            vertices[gl_VertexIndex + vertexDataOffset].ty);

    gl_Position = ubo.projectionMatrices[uboIndex] *
                  ubo.viewMatrices[uboIndex] * 
                  modelMatrix *
                  vec4(inPosition, 1.0);

    // Transform normal into world space
    fragNormal = normalize(inNormal * invModelMatrix);
}
