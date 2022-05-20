#version 450 

#define U32_MAX 0xffffffffu
#define F32_MAX 3.402823466e+38

#define Mesh_Plane 2
#define Mesh_Cube 3
#define Mesh_Sphere 5

#define Material_Red 0
#define Material_Blue 1
#define Material_CheckerBoard 2
#define Material_White 3
#define Material_BlueLight 4
#define Material_WhiteLight 5
#define Material_Black 6

vec3 TransformVector(vec3 d, mat4 m)
{
    vec4 v = m * vec4(d, 0.0);
    return v.xyz;
}

vec3 TransformPoint(vec3 p, mat4 m)
{
    vec4 v = m * vec4(p, 1.0);
    return v.xyz;
}

struct VertexPNT
{
    float px, py, pz;
    float nx, ny, nz;
    float tx, ty;
};

struct RayTracingVertex
{
    vec3 position;
    vec3 normal;
    vec2 textureCoord;
};

struct Mesh
{
    uint indexCount;
    uint indicesOffset; // In sizeof(uint) units
    uint vertexDataOffset; // In sizeof(VertexPNT) units
};

struct Entity
{
    uint meshIndex;
    uint materialId;
    uint modelMatrixIndex;
};

layout(binding = 0, rgba32f) uniform writeonly image2D outputImage;
layout(binding = 1, std140) uniform ComputeShaderUniformBuffer {
    mat4 cameraTransform;
} ubo;

layout(binding = 2) uniform sampler defaultSampler;
// FIXME: Texture binding is a horrific mess at the moment!
layout(binding = 8) uniform texture2D checkerBoardTexture;

layout(binding = 3) readonly buffer Vertices
{
    VertexPNT g_vertices[];
};

layout(binding = 4) readonly buffer Indices
{
    uint g_indices[];
};

layout(binding = 5) readonly buffer Meshes
{
    Mesh g_meshes[];
};

layout(binding = 6) readonly buffer ModelMatrices
{
    mat4 g_modelMatrices[];
};

// FIXME: Copied from scene.h
#define MAX_ENTITIES 1024
layout(binding = 7) readonly buffer Entities
{
    uint count;
    Entity entities[MAX_ENTITIES];
} sceneBuffer;

layout(binding = 9) readonly buffer InvModelMatrices
{
    mat4 g_invModelMatrices[];
};

//layout(binding = 6) readonly buffer TileIndices
//{
    //uint g_tileIndices[];
//};

layout(push_constant) uniform PushConstants
{
    uint g_tileIndices[8];
};

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

RayTracingVertex FetchVertex(uint vertexIndex, uint vertexDataOffset)
{
    RayTracingVertex vertex;

    vertex.position = vec3(
            g_vertices[vertexIndex + vertexDataOffset].px,
            g_vertices[vertexIndex + vertexDataOffset].py,
            g_vertices[vertexIndex + vertexDataOffset].pz);

    vertex.normal = vec3(
            g_vertices[vertexIndex + vertexDataOffset].nx,
            g_vertices[vertexIndex + vertexDataOffset].ny,
            g_vertices[vertexIndex + vertexDataOffset].nz);

    vertex.textureCoord = vec2(
            g_vertices[vertexIndex + vertexDataOffset].tx,
            g_vertices[vertexIndex + vertexDataOffset].ty);

    return vertex;
}

RayIntersectTriangleResult RayIntersectTriangleMesh(
    Mesh mesh, vec3 rayOrigin, vec3 rayDirection)
{
    RayIntersectTriangleResult closestIntersection;
    closestIntersection.t = F32_MAX;

    uint triangleCount = mesh.indexCount / 3;

    // RayIntersectTriangleMT each one and keep track of closest intersection
    for (uint triangleIndex = 0; triangleIndex < triangleCount; triangleIndex++)
    {
        // Compute vertex indices
        uint indices[3];
        indices[0] = g_indices[triangleIndex * 3 + 0 + mesh.indicesOffset];
        indices[1] = g_indices[triangleIndex * 3 + 1 + mesh.indicesOffset];
        indices[2] = g_indices[triangleIndex * 3 + 2 + mesh.indicesOffset];

        RayTracingVertex vertices[3];
        vertices[0] = FetchVertex(indices[0], mesh.vertexDataOffset);
        vertices[1] = FetchVertex(indices[1], mesh.vertexDataOffset);
        vertices[2] = FetchVertex(indices[2], mesh.vertexDataOffset);

        RayIntersectTriangleResult triangleIntersect = RayIntersectTriangleMT(
            rayOrigin, rayDirection, vertices[0].position, vertices[1].position,
            vertices[2].position);

        if (triangleIntersect.t >= 0.0f && triangleIntersect.t < F32_MAX)
        {
            if (triangleIntersect.t < closestIntersection.t)
            {
                closestIntersection = triangleIntersect;

                // Compute UVs from barycentric coordinates
                float w =
                    1.0f - triangleIntersect.uv.x - triangleIntersect.uv.y;
                vec2 uv =
                    vertices[0].textureCoord * w +
                    vertices[1].textureCoord * triangleIntersect.uv.x +
                    vertices[2].textureCoord * triangleIntersect.uv.y;

                closestIntersection.uv = uv;
            }
        }
    }

    return closestIntersection;
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
    result.materialIndex = Material_Black;

#if 0
    RayIntersectTriangleResult quadResult =
        RayIntersectQuad(rayOrigin, rayDirection, vec3(0, 0, 0), vec3(50));
    if (quadResult.t >= 0.0 && quadResult.t < result.t)
    {
        result.t = quadResult.t;
        result.materialIndex = Material_CheckerBoard;
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
        result.materialIndex = Material_WhiteLight;
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
                result.materialIndex = Material_Red;
                result.normal = normalize((rayOrigin + rayDirection * result.t) - sphereCenter);
            }
        }
    }
#endif

#if 0
    // Test mesh
    Mesh mesh = g_meshes[Mesh_Cube];
    RayIntersectTriangleResult meshResult =
        RayIntersectTriangleMesh(mesh, rayOrigin, rayDirection);
    if (meshResult.t >= 0.0 && meshResult.t < result.t)
    {
        result.t = meshResult.t;
        result.materialIndex = Material_Red;
        result.normal = meshResult.normal;
        result.uv = meshResult.uv;
    }
#endif

    for (uint entityIndex = 0; entityIndex < sceneBuffer.count; entityIndex++)
    {
        Entity entity = sceneBuffer.entities[entityIndex];

        Mesh mesh = g_meshes[entity.meshIndex];
        mat4 modelMatrix = g_modelMatrices[entity.modelMatrixIndex];
        mat4 invModelMatrix = g_invModelMatrices[entity.modelMatrixIndex];

        // Transform ray into mesh local space by multiplying it by the inverse
        // model matrix to test it for intersection
        vec3 localRayOrigin = TransformPoint(rayOrigin, invModelMatrix);
        vec3 localRayDirection =
            normalize(TransformVector(rayDirection, invModelMatrix));
        RayIntersectTriangleResult triangleIntersection =
            RayIntersectTriangleMesh(mesh, localRayOrigin, localRayDirection);

        // Transform triangle intersection result from mesh space into
        // world space by transforming the local hit point into world space
        // and then using it to compute the t value in world space
        vec3 localHitPoint =
            localRayOrigin + localRayDirection * triangleIntersection.t;
        vec3 worldHitPoint = TransformPoint(localHitPoint, modelMatrix);

        // Project world hit point onto the world space ray to compute t
        // value in world space
        float t = dot(worldHitPoint - rayOrigin, rayDirection);

        // Transform mesh space normal into world space
        vec3 localNormal = triangleIntersection.normal;
        vec3 worldNormal = normalize(TransformVector(localNormal, modelMatrix));

        if (t >= 0.0 && t < result.t)
        {
            result.t = t;
            result.materialIndex = entity.materialId;
            result.normal = worldNormal;
            result.uv = triangleIntersection.uv;
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

        // TODO: Material buffer!
        vec3 emission = vec3(0, 0, 0);
        vec3 albedo = vec3(0, 0, 0);
        if (materialId == Material_Red)
        {
            albedo = vec3(0.18, 0.1, 0.1);
        }
        else if (materialId == Material_CheckerBoard)
        {
            albedo =
                texture(sampler2D(checkerBoardTexture, defaultSampler), uv).rgb;
        }
        else if (materialId == Material_WhiteLight)
        {
            emission = vec3(1);
        }

        float cosine = max(0.0, dot(normal, incomingDir));
        vec3 incomingRadiance = radiance;

        radiance = emission + albedo * incomingRadiance * cosine;
    }

    return radiance;
}

struct Camera
{
    vec3 right;
    vec3 up;
    vec3 forward;
    vec3 position;
};

struct Film
{
    float halfWidth;
    float halfHeight;
    float halfPixelWidth;
    float halfPixelHeight;
    vec3 center;
    vec2 oneOverImagePlaneDims;
};

struct Tile
{
    uint minX;
    uint minY;
    uint maxX;
    uint maxY;
};

void PathTraceTile(Tile tile, Camera camera, Film film)
{
    for (uint y = tile.minY; y < tile.maxY; y++)
    {
        for (uint x = tile.minX; x < tile.maxX; x++)
        {
            uint sampleCount = 1;
            float sampleContribution = 1.0 / float(sampleCount);

            vec3 totalRadiance = vec3(0, 0, 0);
            for (uint sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
            {
                // Offset pixel position by 0.5 to sample from center
                vec2 pixelPosition = vec2(x, y) + vec2(0.5);

                pixelPosition += vec2(film.halfPixelWidth * RandomBilateral(),
                                      film.halfPixelHeight * RandomBilateral());

                // Calculate position on film plane as 0->1 range
                vec2 p = pixelPosition * film.oneOverImagePlaneDims;

                p.y = 1.0 - p.y; // Flip image vertically

                // Map p from 0..1 range into -1..1 range
                p = p * 2.0 - vec2(1.0);

                // Compute world position on film plane
                vec3 filmP = film.center;
                filmP += (film.halfWidth * p.x) * camera.right;
                filmP += (film.halfHeight * p.y) * camera.up;

                vec3 rayOrigin = camera.position;
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
                        vertex.materialId = Material_Black;
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

            ivec2 uv = ivec2(x, y);
            imageStore(outputImage, uv, vec4(outputColor, 1));
        }
    }
}

void main()
{
    // FIXME: Don't duplicate this info from config.h
#define IMAGE_WIDTH (1024 / 4)
#define IMAGE_HEIGHT (768 / 4)
#define TILE_WIDTH 16
#define TILE_HEIGHT 16

    float imagePlaneWidth = float(IMAGE_WIDTH);
    float imagePlaneHeight = float(IMAGE_HEIGHT);
    float filmDistance = 0.8; // FIXME: Magic number copied from CPU path tracer

    // Calculate aspect ratio correction
    float filmWidth = 1.0;
    float filmHeight = 1.0;
    if (imagePlaneWidth > imagePlaneHeight)
    {
        filmHeight = imagePlaneHeight / imagePlaneWidth;
    }
    else if (imagePlaneWidth < imagePlaneHeight)
    {
        filmWidth = imagePlaneWidth / imagePlaneHeight;
    }

    Tile tile;
#if 0
    tile.minX = gl_GlobalInvocationID.x;
    tile.maxX = gl_GlobalInvocationID.x + 1;

    tile.minY = gl_GlobalInvocationID.y;
    tile.maxY = gl_GlobalInvocationID.y + 1;
#endif

    uint queueIndex = gl_GlobalInvocationID.x;
    uint tileIndex = g_tileIndices[queueIndex];
    uint tileCountX = IMAGE_WIDTH / TILE_WIDTH;
    uint tileX = tileIndex % tileCountX;
    uint tileY = tileIndex / tileCountX;
    //uint tileIndex = gl_GlobalInvocationID.y * TILE_WIDTH + gl_GlobalInvocationID.x;

    tile.minX = tileX * TILE_WIDTH;
    tile.maxX = min(IMAGE_WIDTH, (tileX + 1) * TILE_WIDTH);
    tile.minY = tileY * TILE_HEIGHT;
    tile.maxY = min(IMAGE_HEIGHT, (tileY + 1) * TILE_HEIGHT);

    // Seed our random number generator
    rng_state = (tileIndex + 1) * 0xF51C0E49;

    Camera camera;
    camera.right = vec3(ubo.cameraTransform[0]);
    camera.up = vec3(ubo.cameraTransform[1]);
    camera.forward = vec3(ubo.cameraTransform[2]);
    camera.position = vec3(ubo.cameraTransform[3]);

    Film film;
    film.halfWidth = filmWidth * 0.5;
    film.halfHeight = filmHeight * 0.5;
    film.halfPixelWidth = 0.5 / imagePlaneWidth;
    film.halfPixelHeight = 0.5 / imagePlaneHeight;
    film.center = camera.position + camera.forward * filmDistance;
    film.oneOverImagePlaneDims = vec2(1.0) / vec2(imagePlaneWidth, imagePlaneHeight);

    PathTraceTile(tile, camera, film);
}
