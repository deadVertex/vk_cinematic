#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define ENABLE_VALIDATION_LAYERS
//#define ENABLE_PROFILING

#define PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "platform.h"
#include "input.h"

struct DebugReadFileResult
{
    void *contents;
    u32 length;
};

#define DebugReadEntireFile(NAME) DebugReadFileResult NAME(const char *path)
typedef DebugReadEntireFile(DebugReadEntireFileFunction);
#define DebugFreeFileMemory(NAME) void NAME(void *memory)
typedef DebugFreeFileMemory(DebugFreeFileMemoryFunction);

internal void* AllocateMemory(u64 size, u64 baseAddress = 0);
internal void FreeMemory(void *p);
internal DebugReadEntireFile(ReadEntireFile);

#include "vulkan_renderer.h"
#include "mesh.h"
#include "profiler.h"
#include "debug.h"
#include "world.h"

#include "debug.cpp"
#include "cpu_ray_tracer.cpp"
#include "vulkan_renderer.cpp"
#include "mesh.cpp"

global GLFWwindow *g_Window;
global u32 g_FramebufferWidth = 1024;
global u32 g_FramebufferHeight = 768;

internal DebugLogMessage(LogMessage_)
{
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    OutputDebugString(buffer);
    OutputDebugString("\n");
    puts(buffer);
}

internal void GlfwErrorCallback(int error, const char *description)
{
    LogMessage("GLFW Error (%d): %s.", error, description);
}

#ifdef PLATFORM_WINDOWS
internal void *AllocateMemory(u64 size, u64 baseAddress)
{
    void *result =
        VirtualAlloc((void*)baseAddress, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    return result;
}

internal void FreeMemory(void *p)
{
    VirtualFree(p, 0, MEM_RELEASE);
}

DebugReadEntireFile(ReadEntireFile)
{
    DebugReadFileResult result = {};
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0,
                              OPEN_EXISTING, 0, 0);
    if (file != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER tempSize;
        if (GetFileSizeEx(file, &tempSize))
        {
            result.length = SafeTruncateU64ToU32(tempSize.QuadPart);
            result.contents = AllocateMemory(result.length);

            if (result.contents)
            {
                DWORD bytesRead;
                if (!ReadFile(file, result.contents, result.length, &bytesRead,
                              0) ||
                    (result.length != bytesRead))
                {
                    LogMessage("Failed to read file %s", path);
                    FreeMemory(result.contents);
                    result.contents = NULL;
                    result.length = 0;
                }
            }
            else
            {
                LogMessage("Failed to allocate %d bytes for file %s",
                          result.length, path);
            }
        }
        else
        {
            LogMessage("Failed to read file length for file %s", path);
        }
        CloseHandle(file);
    }
    else
    {
        LogMessage("Failed to open file %s", path);
    }
    return result;
}

#endif

#define KEY_HELPER(NAME)                                                       \
    case GLFW_KEY_##NAME:                                                      \
        return KEY_##NAME;
internal i32 ConvertKey(int key)
{
    if (key >= GLFW_KEY_SPACE && key <= GLFW_KEY_GRAVE_ACCENT)
    {
        return key;
    }
    switch (key)
    {
        KEY_HELPER(BACKSPACE);
        KEY_HELPER(TAB);
        KEY_HELPER(INSERT);
        KEY_HELPER(HOME);
        KEY_HELPER(PAGE_UP);
    // Can't use KEY_HELPER( DELETE ) as windows has a #define for DELETE
    case GLFW_KEY_DELETE:
        return KEY_DELETE;
        KEY_HELPER(END);
        KEY_HELPER(PAGE_DOWN);
        KEY_HELPER(ENTER);

        KEY_HELPER(LEFT_SHIFT);
    case GLFW_KEY_LEFT_CONTROL:
        return KEY_LEFT_CTRL;
        KEY_HELPER(LEFT_ALT);
        KEY_HELPER(RIGHT_SHIFT);
    case GLFW_KEY_RIGHT_CONTROL:
        return KEY_RIGHT_CTRL;
        KEY_HELPER(RIGHT_ALT);

        KEY_HELPER(LEFT);
        KEY_HELPER(RIGHT);
        KEY_HELPER(UP);
        KEY_HELPER(DOWN);

        KEY_HELPER(ESCAPE);

        KEY_HELPER(F1);
        KEY_HELPER(F2);
        KEY_HELPER(F3);
        KEY_HELPER(F4);
        KEY_HELPER(F5);
        KEY_HELPER(F6);
        KEY_HELPER(F7);
        KEY_HELPER(F8);
        KEY_HELPER(F9);
        KEY_HELPER(F10);
        KEY_HELPER(F11);
        KEY_HELPER(F12);
    case GLFW_KEY_KP_0:
        return KEY_NUM0;
    case GLFW_KEY_KP_1:
        return KEY_NUM1;
    case GLFW_KEY_KP_2:
        return KEY_NUM2;
    case GLFW_KEY_KP_3:
        return KEY_NUM3;
    case GLFW_KEY_KP_4:
        return KEY_NUM4;
    case GLFW_KEY_KP_5:
        return KEY_NUM5;
    case GLFW_KEY_KP_6:
        return KEY_NUM6;
    case GLFW_KEY_KP_7:
        return KEY_NUM7;
    case GLFW_KEY_KP_8:
        return KEY_NUM8;
    case GLFW_KEY_KP_9:
        return KEY_NUM9;
    case GLFW_KEY_KP_DECIMAL:
        return KEY_NUM_DECIMAL;
    case GLFW_KEY_KP_DIVIDE:
        return KEY_NUM_DIVIDE;
    case GLFW_KEY_KP_MULTIPLY:
        return KEY_NUM_MULTIPLY;
    case GLFW_KEY_KP_SUBTRACT:
        return KEY_NUM_MINUS;
    case GLFW_KEY_KP_ADD:
        return KEY_NUM_PLUS;
    case GLFW_KEY_KP_ENTER:
        return KEY_NUM_ENTER;
    }
    return KEY_UNKNOWN;
}


void KeyCallback(GLFWwindow *window, int glfwKey, int scancode, int action, int mods)
{
    GameInput *input =
            (GameInput *)glfwGetWindowUserPointer(window);

    i32 key = ConvertKey(glfwKey);
    if (key != KEY_UNKNOWN)
    {
        Assert(key < MAX_KEYS);
        if (action == GLFW_PRESS)
        {
            input->buttonStates[key].isDown = true;
        }
        else if (action == GLFW_RELEASE)
        {
            input->buttonStates[key].isDown = false;
        }
        // else: ignore key repeat messages
    }
}

internal void MouseButtonCallback(
    GLFWwindow *window, int button, int action, int mods)
{
    GameInput *input =
            (GameInput *)glfwGetWindowUserPointer(window);

    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        input->buttonStates[KEY_MOUSE_BUTTON_LEFT].isDown = (action == GLFW_PRESS);
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    {
        input->buttonStates[KEY_MOUSE_BUTTON_MIDDLE].isDown = (action == GLFW_PRESS);
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        input->buttonStates[KEY_MOUSE_BUTTON_RIGHT].isDown = (action == GLFW_PRESS);
    }
}

global f64 g_PrevMousePosX;
global f64 g_PrevMousePosY;

internal void CursorPositionCallback(GLFWwindow *window, f64 xPos, f64 yPos)
{
    GameInput *input = (GameInput *)glfwGetWindowUserPointer(window);

    // Unfortunately GLFW doesn't seem to provide a way to get mouse motion
    // rather than cursor position so we have to compute the relative motion
    // ourselves.
    f64 relX = xPos - g_PrevMousePosX;
    f64 relY = yPos - g_PrevMousePosY;
    g_PrevMousePosX = xPos;
    g_PrevMousePosY = yPos;

    input->mouseRelPosX = (i32)Floor((f32)relX);
    input->mouseRelPosY = (i32)Floor((f32)relY);
    input->mousePosX = (i32)Floor((f32)xPos);
    input->mousePosY = (i32)Floor((f32)yPos);
}

internal void ShowMouseCursor(b32 isVisible)
{
    glfwSetInputMode(g_Window, GLFW_CURSOR,
        isVisible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
}

struct FreeRoamCamera
{
    vec3 position;
    vec3 velocity;
    vec3 rotation;
};

global FreeRoamCamera g_camera;

internal void UpdateFreeRoamCamera(
    FreeRoamCamera *camera, GameInput *input, f32 dt)
{
    vec3 position = camera->position;
    vec3 velocity = camera->velocity;
    vec3 rotation = camera->rotation;

    f32 speed = 20.0f; //200.0f;
    f32 friction = 18.0f;

    f32 sens = 0.005f;
    f32 mouseX = -input->mouseRelPosY * sens;
    f32 mouseY = -input->mouseRelPosX * sens;

    vec3 newRotation = Vec3(mouseX, mouseY, 0.0f);

    if (input->buttonStates[KEY_MOUSE_BUTTON_RIGHT].isDown)
    {
        rotation += newRotation;
        ShowMouseCursor(false);
    }
    else
    {
        ShowMouseCursor(true);
    }

    rotation.x = Clamp(rotation.x, -PI * 0.5f, PI * 0.5f);

    if (rotation.y > 2.0f * PI)
    {
        rotation.y -= 2.0f * PI;
    }
    if (rotation.y < 2.0f * -PI)
    {
        rotation.y += 2.0f * PI;
    }

    f32 forwardMove = 0.0f;
    f32 rightMove = 0.0f;
    if (input->buttonStates[KEY_W].isDown)
    {
        forwardMove = -1.0f;
    }

    if (input->buttonStates[KEY_S].isDown)
    {
        forwardMove = 1.0f;
    }

    if (input->buttonStates[KEY_A].isDown)
    {
        rightMove = -1.0f;
    }
    if (input->buttonStates[KEY_D].isDown)
    {
        rightMove = 1.0f;
    }

    if (input->buttonStates[KEY_LEFT_SHIFT].isDown)
    {
        speed *= 10.0f;
    }

    mat4 rotationMatrix = RotateY(rotation.y) * RotateX(rotation.x);

    vec3 forward = (rotationMatrix * Vec4(0, 0, 1, 0)).xyz;
    vec3 right = (rotationMatrix * Vec4(1, 0, 0, 0)).xyz;

    vec3 targetDir = Normalize(forward * forwardMove + right * rightMove);

    velocity -= velocity * friction * dt;
    velocity += targetDir * speed * dt;

    position = position + velocity * dt;

    camera->position = position;
    camera->velocity = velocity;
    camera->rotation = rotation;
}

internal void Update(
    VulkanRenderer *renderer, RayTracer *rayTracer, GameInput *input, f32 dt)
{
    UpdateFreeRoamCamera(&g_camera, input, dt);
    UniformBufferObject *ubo = (UniformBufferObject *)renderer->uniformBuffer.data;
    vec3 cameraPosition = g_camera.position;
    vec3 rotation = g_camera.rotation;
    quat cameraRotation =
        Quat(Vec3(0, 1, 0), rotation.y) * Quat(Vec3(1, 0, 0), rotation.x);

    ubo->viewMatrices[0] =
        Rotate(Conjugate(cameraRotation)) * Translate(-cameraPosition);

    f32 aspect =
        (f32)renderer->swapchain.width / (f32)renderer->swapchain.height;

    // Vulkan specific correction matrix
    mat4 correctionMatrix = {};
    correctionMatrix.columns[0] = Vec4(1, 0, 0, 0);
    correctionMatrix.columns[1] = Vec4(0, -1, 0, 0);
    correctionMatrix.columns[2] = Vec4(0, 0, 0.5f, 0);
    correctionMatrix.columns[3] = Vec4(0, 0, 0.5f, 1);
    ubo->projectionMatrices[0] =
        correctionMatrix * Perspective(50.0f, aspect, 0.01f, 100.0f);

    rayTracer->viewMatrix = Translate(cameraPosition) * Rotate(cameraRotation);
}

internal void AddEntity(
    World *world, vec3 position, quat rotation, vec3 scale, u32 mesh)
{
    if (world->count < world->max)
    {
        Entity *entity = world->entities + world->count++;
        entity->position = position;
        entity->rotation = rotation;
        entity->scale = scale;
        entity->mesh = mesh;
    }
}

// FIXME: This assumes every entity uses the same mesh!
internal void ComputeEntityBoundingBoxes(World *world, vec3 boxMin, vec3 boxMax)
{
    for (u32 entityIndex = 0; entityIndex < world->count; ++entityIndex)
    {
        Entity *entity = world->entities + entityIndex;
        mat4 modelMatrix = Translate(entity->position) *
                           Rotate(entity->rotation) * Scale(entity->scale);

        // FIXME: Computing the sphere is pretty bad way of transforming the
        // AABB, its probably worth investing in doing this properly by
        // transforming the 8 vertices of AABB and compute a new one from that.
        vec3 center = (boxMin + boxMax) * 0.5f;
        vec3 halfDim = (boxMax - boxMin) * 0.5f;
        f32 radius = Length(halfDim);

        vec3 transformedCenter = TransformPoint(center, modelMatrix);
        u32 largestAxis = CalculateLargestAxis(entity->scale);
        f32 transformedRadius = radius * entity->scale.data[largestAxis];

        entity->aabbMin = transformedCenter - Vec3(transformedRadius);
        entity->aabbMax = transformedCenter + Vec3(transformedRadius);
    }
}

int main(int argc, char **argv)
{
    LogMessage = &LogMessage_;

    LogMessage("Compiled agist GLFW %i.%i.%i", GLFW_VERSION_MAJOR,
           GLFW_VERSION_MINOR, GLFW_VERSION_REVISION);

    i32 major, minor, revision;
    glfwGetVersion(&major, &minor, &revision);
    LogMessage("Running against GLFW %i.%i.%i", major, minor, revision);
    LogMessage("%s", glfwGetVersionString());

    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit())
    {
        LogMessage("Failed to initialize GLFW!");
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    g_Window = glfwCreateWindow(g_FramebufferWidth,
        g_FramebufferHeight, "vk_cinematic", NULL, NULL);
    Assert(g_Window != NULL);
    GameInput input = {};
    glfwSetWindowUserPointer(g_Window, &input);
    glfwSetKeyCallback(g_Window, KeyCallback);
    glfwSetMouseButtonCallback(g_Window, MouseButtonCallback);
    glfwSetCursorPosCallback(g_Window, CursorPositionCallback);

    VulkanRenderer renderer = {};
    VulkanInit(&renderer, g_Window);

    u32 tempMemorySize = Megabytes(64);
    MemoryArena tempArena = {};
    InitializeMemoryArena(
        &tempArena, AllocateMemory(tempMemorySize), tempMemorySize);

    // FIXME: Can't clear this memory until we've built BVH for the ray tracer
    MeshData bunnyMesh = LoadMesh("bunny.obj", &tempArena);
    MeshData monkeyMesh = LoadMesh("monkey.obj", &tempArena);
    LogMessage("Meshes data memory usage: %uk / %uk",
        tempArena.size / 1024, tempArena.capacity / 1024);

    renderer.meshes[Mesh_Bunny].indexCount = bunnyMesh.indexCount;
    renderer.meshes[Mesh_Monkey].indexCount = monkeyMesh.indexCount;
    CopyMemory(renderer.vertexDataUploadBuffer.data, bunnyMesh.vertices,
        sizeof(VertexPNT) * bunnyMesh.vertexCount);
    renderer.meshes[Mesh_Bunny].vertexDataOffset =
        renderer.vertexDataUploadBufferSize / sizeof(VertexPNT);
    renderer.vertexDataUploadBufferSize +=
        sizeof(VertexPNT) * bunnyMesh.vertexCount;

    CopyMemory((u8 *)renderer.vertexDataUploadBuffer.data +
                   renderer.vertexDataUploadBufferSize,
        monkeyMesh.vertices, sizeof(VertexPNT) * monkeyMesh.vertexCount);
    renderer.meshes[Mesh_Monkey].vertexDataOffset =
        renderer.vertexDataUploadBufferSize / sizeof(VertexPNT);
    renderer.vertexDataUploadBufferSize +=
        sizeof(VertexPNT) * monkeyMesh.vertexCount;

    CopyMemory(renderer.indexUploadBuffer.data, bunnyMesh.indices,
        sizeof(u32) * bunnyMesh.indexCount);
    renderer.meshes[Mesh_Bunny].indexDataOffset =
        renderer.indexUploadBufferSize;
    renderer.indexUploadBufferSize += sizeof(u32) * bunnyMesh.indexCount;
    CopyMemory(
        (u8 *)renderer.indexUploadBuffer.data + renderer.indexUploadBufferSize,
        monkeyMesh.indices, sizeof(u32) * monkeyMesh.indexCount);
    renderer.meshes[Mesh_Monkey].indexDataOffset =
        renderer.indexUploadBufferSize;
    renderer.indexUploadBufferSize += sizeof(u32) * monkeyMesh.indexCount;

    VulkanCopyMeshDataToGpu(&renderer);

    u32 memorySize = Megabytes(64);
    MemoryArena memoryArena = {};
    InitializeMemoryArena(&memoryArena, AllocateMemory(memorySize), memorySize);

    World world = {};
    world.entities = AllocateArray(&memoryArena, Entity, MAX_ENTITIES);
    world.max = MAX_ENTITIES;

    AddEntity(&world, Vec3(0, 0, 0), Quat(), Vec3(4), 0);
    AddEntity(&world, Vec3(2, 0, 0), Quat(Vec3(0, 1, 0), PI * 0.5f), Vec3(1), 0);

    u32 gridDim = 10;
    for (u32 y = 0; y < gridDim; ++y)
    {
        for (u32 z = 0; z < gridDim; ++z)
        {
            for (u32 x = 0; x < gridDim; ++x)
            {
                vec3 origin = Vec3(-5.0f, 0.0f, -5.0f);
                vec3 p = origin + Vec3((f32)x, (f32)y, (f32)z);
                u32 mesh = x % 2 == 0 ? Mesh_Monkey : Mesh_Bunny;
                vec3 scale = mesh == Mesh_Monkey ? Vec3(0.1) : Vec3(1);
                AddEntity(&world, p, Quat(), scale, mesh);
            }
        }
    }

    RayTracer rayTracer = {};
    rayTracer.nodes = AllocateArray(&memoryArena, BvhNode, MAX_BVH_NODES);
    rayTracer.meshData = bunnyMesh;
    rayTracer.useAccelerationStructure = true;
    BuildBvh(&rayTracer, bunnyMesh);

    // Compute bounding box for each entity
    ComputeEntityBoundingBoxes(&world, rayTracer.root->min, rayTracer.root->max);

    g_Profiler.samples = (ProfilerSample *)AllocateMemory(PROFILER_SAMPLE_BUFFER_SIZE);
    ProfilerResults profilerResults = {};

    DebugDrawingBuffer debugDrawBuffer = {};
    debugDrawBuffer.vertices = (VertexPC *)renderer.debugVertexDataBuffer.data;
    debugDrawBuffer.max = DEBUG_VERTEX_BUFFER_SIZE / sizeof(VertexPC);
    rayTracer.debugDrawBuffer = &debugDrawBuffer;

    // Sync model matrices with world
    mat4 *modelMatrices = (mat4 *)renderer.modelMatricesBuffer.data;
    for (u32 i = 0; i < world.count; ++i)
    {
        Entity *entity = world.entities + i;
        modelMatrices[i] = Translate(entity->position) *
                           Rotate(entity->rotation) * Scale(entity->scale);
    }

    vec3 lastCameraPosition = g_camera.position;
    vec3 lastCameraRotation = g_camera.rotation;
    b32 drawScene = true;
    f32 prevFrameTime = 0.0f;
    u32 maxDepth = 1;
    b32 drawTests = false;
    while (!glfwWindowShouldClose(g_Window))
    {
        f32 dt = prevFrameTime;
        dt = Min(dt, 0.25f);
        if (drawScene)
        {
            dt = 0.016f;
        }

        debugDrawBuffer.count = 0;


        f64 frameStart = glfwGetTime();
        InputBeginFrame(&input);
        glfwPollEvents();

        b32 isDirty = false;
        if (WasPressed(input.buttonStates[KEY_SPACE]))
        {
            drawScene = !drawScene;
            isDirty = true;
        }

        if (WasPressed(input.buttonStates[KEY_TAB]))
        {
            rayTracer.useAccelerationStructure = !rayTracer.useAccelerationStructure;
            isDirty = true;
        }

        if (LengthSq(g_camera.position - lastCameraPosition) > 0.0001f ||
            LengthSq(g_camera.rotation - lastCameraRotation) > 0.0001f)
        {
            lastCameraPosition = g_camera.position;
            lastCameraRotation = g_camera.rotation;
            isDirty = true;
        }

        if (WasPressed(input.buttonStates[KEY_UP]))
        {
            maxDepth++;
        }
        if (WasPressed(input.buttonStates[KEY_DOWN]))
        {
            if (maxDepth > 0)
            {
                maxDepth--;
            }
        }

        if (drawTests)
        {
            // Force drawing through the vulkan renderer
            drawScene = true;

            // Run tests
            //TestTransformRayVsAabb(&debugDrawBuffer);
            TestTransformRayVsTriangle(&debugDrawBuffer);
        }
        
        if (!drawScene && isDirty)
        {
            LogMessage("Begin ray tracing");
            f64 rayTracingStartTime = glfwGetTime();
            DoRayTracing(RAY_TRACER_WIDTH, RAY_TRACER_HEIGHT,
                (u32 *)renderer.imageUploadBuffer.data, &rayTracer, &world);
            f64 rayTracingElapsedTime = glfwGetTime() - rayTracingStartTime;
            LogMessage("Camera Position: (%g, %g, %g)", g_camera.position.x,
                g_camera.position.y, g_camera.position.z);
            LogMessage("Camera Rotation: (%g, %g, %g)", g_camera.rotation.x,
                g_camera.rotation.y, g_camera.rotation.z);
            LogMessage("Ray tracing time spent: %gs", rayTracingElapsedTime);
            LogMessage("Triangle count: %u", rayTracer.meshData.indexCount / 3);
            LogMessage("Memory Usage: %ukb / %ukb", memoryArena.size / 1024,
                    memoryArena.capacity / 1024);
            LogMessage("Entities: %u / %u", world.count, world.max);
            LogMessage("BVH Nodes: %u / %u", rayTracer.nodeCount, MAX_BVH_NODES);
            LogMessage("Debug Vertices: %u / %u", debugDrawBuffer.count,
                debugDrawBuffer.max);
            LogMessage("Profiler Samples: %u / %u", g_Profiler.count,
                    PROFILER_SAMPLE_BUFFER_SIZE / sizeof(ProfilerSample));

            DumpMetrics(&g_Metrics);
            Profiler_ProcessResults(&g_Profiler, &profilerResults);
            Profiler_PrintResults(&profilerResults);
            ClearToZero(&profilerResults, sizeof(profilerResults));
            LogMessage("Rays per second: %g",
                    (f64)g_Metrics.rayCount / rayTracingElapsedTime);
            ClearToZero(&g_Metrics, sizeof(g_Metrics));
            VulkanCopyImageFromCPU(&renderer);
            isDirty = false;
        }

        Update(&renderer, &rayTracer, &input, dt);
        renderer.debugDrawVertexCount = debugDrawBuffer.count;
        DrawCommand drawCommands[MAX_ENTITIES];
        for (u32 i = 0; i < world.count; ++i)
        {
            drawCommands[i].mesh = world.entities[i].mesh;
        }

        VulkanRender(&renderer, drawScene, drawCommands, world.count);
        prevFrameTime = (f32)(glfwGetTime() - frameStart);
    }
    return 0;
}
