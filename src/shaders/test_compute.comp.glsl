#version 450 

#define U32_MAX 0xffffffffu
#define F32_MAX 3.402823466e+38

layout(binding = 0, rgba32f) uniform writeonly image2D outputImage;
layout(binding = 1, std140) uniform ComputeShaderUniformBuffer {
    mat4 cameraTransform;
} ubo;

layout(binding = 2) uniform sampler defaultSampler;
// FIXME: Texture binding is a horrific mess at the moment!
layout(binding = 8) uniform texture2D checkerBoardTexture;

// NOTE: This needs to be correctly seeded in main()
uint rng_state;

// Reference implementation from https://en.wikipedia.org/wiki/Xorshift
uint XorShift32()
{
    uint x = rng_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
    rng_state = x;
	return x;
}

float RandomUnilateral()
{
    float numerator = float(XorShift32());
    float denom = float(U32_MAX);
    float result = numerator / denom;
    return result;
}

float RandomBilateral()
{
    float result = -1.0f + 2.0f * RandomUnilateral();
    return result;
} 

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

struct RayIntersectTriangleResult
{
    float t;
    vec2 uv;
    vec3 normal;
};

RayIntersectTriangleResult RayIntersectTriangleMT(vec3 rayOrigin,
    vec3 rayDirection, vec3 a, vec3 b, vec3 c)
{
    RayIntersectTriangleResult result;
    result.t = -1.0f;

    vec3 T = rayOrigin - a;
    vec3 e1 = b - a;
    vec3 e2 = c - a;

    vec3 p = cross(rayDirection, e2);
    vec3 q = cross(T, e1);

    vec3 m = vec3(dot(q, e2), dot(p, T), dot(q, rayDirection));

    float det = 1.0f / dot(p, e1);

    float t = det * m.x;
    float u = det * m.y;
    float v = det * m.z;
    float w = 1.0f - u - v;

    if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f && w >= 0.0f &&
        w <= 1.0f)
    {
        result.t = t;
        result.normal = normalize(cross(e1, e2));
        result.uv = vec2(u, v);
    }

    return result;
}

vec2 ComputeUV(RayIntersectTriangleResult triangleIntersect, vec2 a, vec2 b, vec2 c)
{
    // Compute UVs from barycentric coordinates
    float w =
        1.0f - triangleIntersect.uv.x - triangleIntersect.uv.y;
    vec2 uv =
        a * w +
        b * triangleIntersect.uv.x +
        c * triangleIntersect.uv.y;
    return uv;
}

RayIntersectTriangleResult RayIntersectQuad(vec3 rayOrigin, vec3 rayDirection, vec3 position, vec3 scale)
{
    vec3 h = scale * 0.5f;
    vec3 verts[4];
    verts[0] = position + vec3(-h.x, 0, -h.z);
    verts[1] = position + vec3(h.x, 0, -h.z);
    verts[2] = position + vec3(h.x, 0, h.z);
    verts[3] = position + vec3(-h.x, 0, h.z);

    vec2 uvs[4];
    uvs[0] = vec2(0, 0);
    uvs[1] = vec2(1, 0);
    uvs[2] = vec2(1, 1);
    uvs[3] = vec2(0, 1);

    RayIntersectTriangleResult a = RayIntersectTriangleMT(rayOrigin,
        rayDirection, verts[0], verts[3], verts[2]);
    RayIntersectTriangleResult b = RayIntersectTriangleMT(rayOrigin,
        rayDirection, verts[0], verts[2], verts[1]);

    RayIntersectTriangleResult result;
    result.t = -1.0;

    if (a.t >= 0.0f)
    {
        result = a;
        result.uv = ComputeUV(a, uvs[0], uvs[3], uvs[2]);
    }
    if (b.t >= 0.0f)
    {
        if (result.t < 0.0f || b.t < result.t)
        {
            result = b;
            result.uv = ComputeUV(b, uvs[0], uvs[2], uvs[1]);
        }
    }

    return result;
}

struct RayIntersectionResult
{
    float t;
    uint materialIndex;
    vec3 normal;
    vec2 uv;
};

RayIntersectionResult RayIntersectScene(vec3 rayOrigin, vec3 rayDirection)
{
    RayIntersectionResult result;
    result.t = F32_MAX;
    result.materialIndex = 0;

    RayIntersectTriangleResult quadResult =
        RayIntersectQuad(rayOrigin, rayDirection, vec3(0, 0, 0), vec3(50));
    if (quadResult.t >= 0.0 && quadResult.t < result.t)
    {
        result.t = quadResult.t;
        result.materialIndex = 2;
        result.normal = quadResult.normal;
        result.uv = quadResult.uv;
    }

    // Sphere light
    vec3 sphereLightCenter = vec3(0, 10, 0);
    float sphereLightRadius = 5.0;
    float sphereLightT = RayIntersectSphere(sphereLightCenter, sphereLightRadius, rayOrigin, rayDirection);
    if (sphereLightT >= 0.0 && sphereLightT < result.t)
    {
        result.t = sphereLightT;
        result.materialIndex = 3;
        result.normal = normalize((rayOrigin + rayDirection * result.t) - sphereLightCenter);
    }

    for (uint z = 0; z < 4; ++z)
    {
        for (uint x = 0; x < 4; ++x)
        {
            vec3 origin = vec3(-8, 1, -8);
            vec3 sphereCenter = origin + vec3(float(x), 0, float(z)) * 5.0;
            float sphereRadius = 1.0;
            float sphereT = RayIntersectSphere(sphereCenter, sphereRadius, rayOrigin, rayDirection);
            if (sphereT >= 0.0 && sphereT < result.t)
            {
                result.t = sphereT;
                result.materialIndex = 1;
                result.normal = normalize((rayOrigin + rayDirection * result.t) - sphereCenter);
            }
        }
    }

    return result;
}

#define MAX_BOUNCES 3
struct PathVertex
{
    uint materialId;
    vec3 outgoingDir;
    vec3 incomingDir;
    vec3 normal;
    vec2 uv;
};

vec3 ComputeRadianceForPath(PathVertex path[MAX_BOUNCES], int pathLength)
{
    vec3 radiance = vec3(0, 0, 0);

    for (int i = pathLength - 1; i >= 0; i--)
    {
        PathVertex vertex = path[i];

        // Fetch values out of path vertex
        vec3 incomingDir = vertex.incomingDir;
        vec3 normal = vertex.normal;
        vec2 uv = vertex.uv;
        uint materialId = vertex.materialId;

        vec3 emission = vec3(0, 0, 0);
        vec3 albedo = vec3(0, 0, 0);
        if (materialId == 1)
        {
            albedo = vec3(0.18, 0.1, 0.1);
        }
        else if (materialId == 2)
        {
            //albedo = vec3(0.08, 0.08, 0.08);
            albedo =
                texture(sampler2D(checkerBoardTexture, defaultSampler), uv).rgb;
        }
        else if (materialId == 0)
        {
            // Background (black)
            //emission = vec3(0.1, 0.05, 0.15);
        }
        else if (materialId == 3)
        {
            // White light
            emission = vec3(1);
        }

        float cosine = max(0.0, dot(normal, incomingDir));
        vec3 incomingRadiance = radiance;

        radiance = emission + albedo * incomingRadiance * cosine;
    }

    return radiance;
}

void main()
{
    // Seed our random number generator
    rng_state =
        gl_GlobalInvocationID.x * 0xF51C0E49 + gl_GlobalInvocationID.y * 0x13AC29D1;

    float imagePlaneWidth = float(gl_NumWorkGroups.x);
    float imagePlaneHeight = float(gl_NumWorkGroups.y);
    vec2 oneOverImagePlaneDims = vec2(1.0) / vec2(imagePlaneWidth, imagePlaneHeight);

    float halfPixelWidth = 0.5 / imagePlaneWidth;
    float halfPixelHeight = 0.5 / imagePlaneHeight;

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

    uint sampleCount = 64;
    float sampleContribution = 1.0 / float(sampleCount);

    vec3 totalRadiance = vec3(0, 0, 0);
    for (uint sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
    {
        // Offset pixel position by 0.5 to sample from center
        vec2 pixelPosition =
            vec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y) + vec2(0.5);

        pixelPosition += vec2(halfPixelWidth * RandomBilateral(),
            halfPixelHeight * RandomBilateral());

        // Calculate position on film plane as 0->1 range
        vec2 p = pixelPosition * oneOverImagePlaneDims;

        p.y = 1.0 - p.y; // Flip image vertically

        // Map p from 0..1 range into -1..1 range
        p = p * 2.0 - vec2(1.0);

        // Compute world position on film plane
        vec3 filmP = filmCenter;
        filmP += (halfFilmWidth * p.x) * right;
        filmP += (halfFilmHeight * p.y) * up;

        vec3 rayOrigin = cameraP;
        vec3 rayDirection = normalize(filmP - rayOrigin);

        vec3 outputColor = vec3(0.08, 0.02, 0.05);

        PathVertex path[MAX_BOUNCES];
        int pathLength = 0;

        for (uint bounceCount = 0; bounceCount < MAX_BOUNCES; bounceCount++)
        {
            RayIntersectionResult result = RayIntersectScene(rayOrigin, rayDirection); 

            if (result.t > 0.0f && result.t < F32_MAX)
            {
                // Ray intersection occurred
                PathVertex vertex;
                vertex.materialId = result.materialIndex;
                vertex.outgoingDir = -rayDirection;
                vertex.normal = result.normal;
                vertex.uv = result.uv;

                // TODO: Compute bounce direction
                vec3 dirOffset =
                    vec3(RandomBilateral(), RandomBilateral(), RandomBilateral());

                vec3 dir = normalize(result.normal + dirOffset);
                if (dot(dir, result.normal) < 0.0f)
                {
                    dir = -dir;
                }

                vertex.incomingDir = dir;

                path[pathLength++] = vertex;

                // Update ray origin and ray direction for next iteration
                float bias = 0.001f;
                vec3 worldPosition = rayOrigin + rayDirection * result.t;
                rayOrigin = worldPosition + result.normal * bias;
                rayDirection = dir;
            }
            else
            {
                // Ray miss
                PathVertex vertex;
                vertex.materialId = 0;
                vertex.outgoingDir = -rayDirection;
                path[pathLength++] = vertex;

                // FIXME: Don't use break
                break;
            }

        }

        // Compute lighting for our path
        vec3 radiance = ComputeRadianceForPath(path, pathLength);
        totalRadiance += radiance * sampleContribution;
    }

    vec3 outputColor = totalRadiance;

    ivec2 uv = ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y);
    imageStore(outputImage, uv, vec4(outputColor, 1));
}
