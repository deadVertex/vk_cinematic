#version 450

layout(location = 0) out vec4 outputColor;

layout(location = 0) in vec3 vertexColor;

void main()
{
    outputColor = vec4(vertexColor, 1.0);
}
