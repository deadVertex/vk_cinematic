#version 450 

layout(binding = 0, rgba32f) uniform writeonly image2D outputImage;

void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y);
    imageStore(outputImage, p, vec4(1, 0, 1, 1));
}
