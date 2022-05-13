#version 450 

#define F32_MAX 3.402823466e+38

layout(binding = 0, rgba32f) uniform writeonly image2D outputImage;
layout(binding = 1, std140) uniform ComputeShaderUniformBuffer {
    mat4 cameraTransform;
} ubo;

float RayIntersectSphere(vec3 center, float radius, vec3 rayOrigin, vec3 rayDirection)
{
    vec3 m = rayOrigin - center; // Use sphere center as origin

    float a = dot(rayDirection, rayDirection);
    float b = 2.0f * dot(m, rayDirection);
    float c = dot(m, m) - radius * radius;

    float d = b*b - 4.0f * a * c; // Discriminant

    float t = F32_MAX;
    if (d > 0.0f)
    {
        float denom = 2.0f * a;
        float t0 = (-b - sqrt(d)) / denom;
        float t1 = (-b + sqrt(d)) / denom;

        t = t0; // Pick the entry point
    }

    return t;
}

float RayIntersectPlane(vec3 normal, float dist, vec3 rayOrigin, vec3 rayDirection)
{
    float t = dot(normal * dist - rayOrigin, normal) / dot(rayDirection, normal);
    return t;
}

struct RayIntersectionResult
{
    float t;
    uint materialIndex;
};

RayIntersectionResult RayIntersectScene(vec3 rayOrigin, vec3 rayDirection)
{
    RayIntersectionResult result;
    result.t = F32_MAX;
    result.materialIndex = 0;

    vec3 sphereCenter = vec3(0, 0, 0);
    float sphereRadius = 1.5;
    float sphereT = RayIntersectSphere(sphereCenter, sphereRadius, rayOrigin, rayDirection);
    if (sphereT >= 0.0 && sphereT < result.t)
    {
        result.t = sphereT;
        result.materialIndex = 1;
    }

    vec3 planeNormal = vec3(0, 1, 0);
    float planeDist = -0.5f;
    float planeT = RayIntersectPlane(planeNormal, planeDist, rayOrigin, rayDirection);
    if (planeT >= 0.0 && planeT < result.t)
    {
        result.t = planeT;
        result.materialIndex = 2;
    }

    return result;
}

void main()
{
    vec3 p = vec3(gl_GlobalInvocationID) / vec3(gl_NumWorkGroups);
    p.y = 1.0 - p.y; // Flip image vertically

    // Map p from 0..1 range into -1..1 range
    p = p * 2.0 - vec3(1.0);

    vec3 forward = vec3(ubo.cameraTransform[2]);
    vec3 up = vec3(ubo.cameraTransform[1]);
    vec3 right = vec3(ubo.cameraTransform[0]);
    vec3 cameraP = vec3(ubo.cameraTransform[3]);
    float filmDistance = 1.0;
    vec3 filmCenter = cameraP + forward * filmDistance;

    // Calculate aspect ratio correction
    float filmWidth = 1.0;
    float filmHeight = 1.0;
    if (gl_NumWorkGroups.x > gl_NumWorkGroups.y)
    {
        filmHeight = float(gl_NumWorkGroups.y) / float(gl_NumWorkGroups.x);
    }
    else if (gl_NumWorkGroups.x < gl_NumWorkGroups.y)
    {
        filmWidth = float(gl_NumWorkGroups.x) / float(gl_NumWorkGroups.y);
    }

    float halfFilmWidth = filmWidth * 0.5;
    float halfFilmHeight = filmHeight * 0.5;

    // Calculate position on film plane
    vec3 filmP = filmCenter;
    filmP += (halfFilmWidth * p.x) * right;
    filmP += (halfFilmHeight * p.y) * up;

    vec3 rayOrigin = cameraP;
    vec3 rayDirection = normalize(filmP - rayOrigin);

    vec3 outputColor = vec3(0.08, 0.02, 0.05);

    RayIntersectionResult result = RayIntersectScene(rayOrigin, rayDirection); 

    if (result.materialIndex == 1)
    {
        outputColor = vec3(1, 0, 1);
    }
    else if (result.materialIndex == 2)
    {
        outputColor = vec3(0.08, 0.08, 0.08);
    }

    ivec2 uv = ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y);
    imageStore(outputImage, uv, vec4(outputColor, 1));
}
