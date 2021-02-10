#version 450

layout(location = 0) out vec2 fragTexCoord;

// Position XY
// Texture Coordinates ZW
vec4 vertices[] =
{
    vec4(-1.0f, -1.0f, 0.0f, 0.0f),
    vec4(1.0f, -1.0f, 1.0f, 0.0f),
    vec4(1.0f, 1.0f, 1.0f, 1.0f),
    vec4(1.0f, 1.0f, 1.0f, 1.0f),
    vec4(-1.0f, 1.0f, 0.0f, 1.0f),
    vec4(-1.0f, -1.0f, 0.0f, 0.0f),
};

void main()
{
    gl_Position = vec4(vertices[gl_VertexIndex].xy, 0.0, 1.0);
    fragTexCoord = vertices[gl_VertexIndex].zw;
}
