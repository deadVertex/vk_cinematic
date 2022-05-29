/* TODO (NEW):
- QoL improvements:
    - Clean up material syncing (see FIXMEs)
    - Texture binding mess (see FIXMEs)
    - Clean up how we are passing data to shaders (i.e. radianceR, radianceG, radianceB)
    - FIXME: Registering meshes really annoying due to path tracer asserts, need better syncing
    - TODO: Setup lights in the scene via material instead of raw radiance values
    - Improve output comparison system? (Swtich between CPU and Rast without losing CPU image)
- Bugs:
    - FIXME: Random vector on hemi-sphere code is generating non-uniform terrible results
- FEAT: Proper support for multiple scenes to help with testing
- FEAT: Lights for rasterization
    - Sphere
    - Ambient term [x]
    - Disk [x]
    - Spot
    - Quad
    - Tube/Line?
- FEAT: Restore Environment/HDR lighting
- FEAT: Proper PBR material model
    - Smith GGX BRDF
    - Albedo maps
    - Normal maps
    - Metalness
    - Roughness
- FEAT: Tone mapping
    - Reinhard
    - Filmic
    - ACES
- FEAT: GPU Ray tracing (Parked for now, want to work on rast and CPU for now)
    - Basic compute shader implementation [x]
    - Camera [x]
    - Basic ray bouncing [x]
    - Basic diffuse material [x]
    - Multi sampling [x]
    - Don't run GPU ray tracing every frame! [x]
    - Texture mapping [x]
    - Triangle intersection [x]
    - Fix camera FOV to match CPU path tracer [ ]
    - Triangle mesh support [x]
    - Uploading meshes [x]
    - Split compute shader workload into smaller chunks for prevent lockup [x]
    - Update screen with partial updates from GPU ray tracing [x]
    - Include system for shaders [ ]
    - Unit test framework for compute shaders [ ]
    - Broadphase? (Top Level Acceleration Structure) [ ] (NEEDS INFRA WORK)
        - Building broadphase tree on GPU [ ]
        - Ray intersection broadphase tree [ ]
    - Midphase (Bottom Level Acceleration Structure) [ ]
    - Fix texture coordinates mismatch [ ]
    - Accumulate pixel samples over multiple frames [ ]
    - Skip drawing rasterization scene when GPU ray tracing [ ]
- FEAT: Rasterizer Screen space ambient occlusion
- FEAT: Rasterizer Screen space reflections
- FEAT: Bloom Post processing
- FEAT: Rasterizer Global Illumination
    - Light Propagation Volumes
    - Voxel cone tracing
    - SDFGI
    - Screen space GI
    - Light probes
- FEAT: Rasterizer Shadows
    - Directional 
    - Omni directional
    - Cascaded shadow maps
- FEAT: CPU Ray tracer optimizations
    - Importance sampling
    - Next event estimation
    - Surface Area heuristic acceleration structure
    - Improve acceleration structure construction performance
*/
/* TODO:
List:
 - Code duplication for creating and uploading images/cube maps to GPU [x]
- [CPU] Don't model skybox using geometry [x]

Bugs:
 - Race condition when submitting work to queue when queue is empty but worker
   threads are still working on the tasks they've pulled from the queue.
 - IBL looks too dim, not sure what is causing it

Tech Debt:
 - Hard-coded material id to texture mapping in shader
 - [CPU] Ray tracer image resolution is hard-coded

Features:
 - Linear space rendering [x]
 - Texture mapping [x]
 - Smooth shading [x]
 - Bilinear sampling [x]
 - Image based lighting [x]
 - ACES tone mapping
 - Bloom
 - gltf importing
 - Cube map texture loading

Performance testing infrastructure
- Performance test suite
- Runtime/startup scene configuration

Functionality testing infrastructure
- Unit tests!

Visualization infrastructure

Usability
- Realtime feedback of ray tracing
- Live code reloading? +1
- Scene definition from file [ ] (Nice to have)
- Resource definition from file

Optimizations
- [CPU] Multiple triangles per tree leaf node
- [CPU] SIMD
- [CPU] Multi-core [X]
- [RAS] Sample cube maps in shaders rather than equirectangular images [x]
- [ALL] Startup time is too long! (building AABB trees for meshes most likely)
- [CPU] Profiling! (What is our current cost per ray?) - Midphase is culprit (tree is too deep/unbalanced)
- [CPU] Don't ray trace whole screen when using comparison view

Analysis
- AABB trees
- Triangle meshes
- Intersection functions
- Assembly code

Observability
- More metrics
- More logs
- More profiling
*/

#include <cstdarg>

#include "config.h"
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(PLATFORM_LINUX)
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#endif

// Move to vulkan specific module?
#include <vulkan/vulkan.h>
#ifdef PLATFORM_WINDOWS
#include <vulkan/vulkan_win32.h>
#endif

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

#include "math_lib.h"
#include "mesh.h"
#include "scene.h"
#include "vulkan_renderer.h"
#include "profiler.h"
#include "debug.h"
#include "intrinsics.h"
#include "work_queue.h"
#include "tile.h"
#include "memory_pool.h"
#include "bvh.h"
#include "ray_intersection.h"
#include "asset_loader/asset_loader.h"
#include "sp_scene.h"
#include "sp_material_system.h"
#include "sp_metrics.h"
#include "simd_path_tracer.h"
#include "simd.h"
#include "aabb.h"
#include "image.h"

#include "debug.cpp"
#include "ray_intersection.cpp"
#include "vulkan_renderer.cpp"
#include "mesh.cpp"
#include "cmdline.cpp"
#include "cubemap.cpp"
#include "mesh_generation.cpp"

#include "image.cpp"
#include "memory_pool.cpp"
#include "bvh.cpp"
#include "sp_scene.cpp"
#include "sp_material_system.cpp"
#include "simd_path_tracer.cpp"

struct sp_Task
{
    sp_Context *context;
    Tile tile;
};

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
#ifdef PLATFORM_WINDOWS
    OutputDebugString(buffer);
    OutputDebugString("\n");
#endif
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

#elif defined(PLATFORM_LINUX)
internal void *AllocateMemory(u64 numBytes, u64 baseAddress)
{
    void *result = mmap((void *)baseAddress, numBytes, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Assert(result != MAP_FAILED);

    return result;
}

internal void FreeMemory(void *p)
{
    munmap(p, 0);
}

// NOTE: bytesToRead must equal the size of the file, if great we will enter an
// infinite loop.
internal bool ReadFile(int file, void *buf, int bytesToRead)
{
    while (bytesToRead)
    {
        int bytesRead = read(file, buf, bytesToRead);
        if (bytesRead == -1)
        {
            return false;
        }
        bytesToRead -= bytesRead;
        buf = (u8 *)buf + bytesRead;
    }
    return true;
}

internal DebugReadEntireFile(ReadEntireFile)
{
    DebugReadFileResult result = {};
    int file = open(path, O_RDONLY);
    if (file != -1)
    {
        struct stat fileStatus;
        if (fstat(file, &fileStatus) != -1)
        {
            result.length = SafeTruncateU64ToU32(fileStatus.st_size);
            result.contents = AllocateMemory(result.length);
            if (result.contents)
            {
                if (!ReadFile(file, result.contents, result.length))
                {
                    LogMessage("Failed to read file %s", path);
                    FreeMemory(result.contents);
                    result.contents = nullptr;
                    result.length = 0;
                }
            }
            else
            {
                LogMessage("Failed to allocate %d bytes for file %s",
                          result.length, path);
                result.length = 0;
            }
        }
        else
        {
            LogMessage("Failed to read file size for file %s", path);
        }
        close(file);
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

internal void Update(VulkanRenderer *renderer, GameInput *input, f32 dt)
{
    UpdateFreeRoamCamera(&g_camera, input, dt);
    UniformBufferObject *ubo = (UniformBufferObject *)renderer->uniformBuffer.data;
#if defined(OVERRIDE_CAMERA_POSITION) && defined(OVERRIDE_CAMERA_ROTATION)
    vec3 cameraPosition = OVERRIDE_CAMERA_POSITION;
    vec3 rotation = OVERRIDE_CAMERA_ROTATION;
#else
    vec3 cameraPosition = g_camera.position;
    vec3 rotation = g_camera.rotation;
#endif
    quat cameraRotation =
        Quat(Vec3(0, 1, 0), rotation.y) * Quat(Vec3(1, 0, 0), rotation.x);

    ubo->viewMatrices[0] =
        Rotate(Conjugate(cameraRotation)) * Translate(-cameraPosition);
    ubo->viewMatrices[1] =
        Rotate(Conjugate(cameraRotation));

    f32 aspect =
        (f32)renderer->swapchain.width / (f32)renderer->swapchain.height;

    // Vulkan specific correction matrix
    mat4 correctionMatrix = {};
    correctionMatrix.columns[0] = Vec4(1, 0, 0, 0);
    correctionMatrix.columns[1] = Vec4(0, -1, 0, 0);
    correctionMatrix.columns[2] = Vec4(0, 0, 0.5f, 0);
    correctionMatrix.columns[3] = Vec4(0, 0, 0.5f, 1);
    ubo->projectionMatrices[0] =
        correctionMatrix *
        Perspective(CAMERA_FOV, aspect, CAMERA_NEAR_CLIP, 100.0f);
    ubo->projectionMatrices[1] = ubo->projectionMatrices[0];

    // TODO: Provide proper API for this
    ComputeShaderUniformBuffer *computeUbo =
        (ComputeShaderUniformBuffer *)renderer->computeUniformBuffer.data;
    mat4 cameraTransform = Identity();
    cameraTransform.columns[0] = Vec4(RotateVector(Vec3(1, 0, 0), cameraRotation), 0);
    cameraTransform.columns[1] = Vec4(RotateVector(Vec3(0, 1, 0), cameraRotation), 0);
    cameraTransform.columns[2] = Vec4(RotateVector(Vec3(0, 0, -1), cameraRotation), 0);
    cameraTransform.columns[3] = Vec4(cameraPosition, 0);
    computeUbo->cameraTransform = cameraTransform;
}

internal void DrawEntityAabbs(Scene scene, DebugDrawingBuffer *debugDrawBuffer)
{
    for (u32 entityIndex = 0; entityIndex < scene.count; ++entityIndex)
    {
        Entity *entity = scene.entities + entityIndex;
        DrawBox(debugDrawBuffer, entity->aabbMin, entity->aabbMax,
            Vec3(0.8, 0.4, 0.2));
    }
}

internal void CopyMeshDataToUploadBuffer(
    VulkanRenderer *renderer, MeshData meshData, u32 mesh)
{
    // Vertex data
    CopyMemory((u8 *)renderer->vertexDataUploadBuffer.data +
                   renderer->vertexDataUploadBufferSize,
        meshData.vertices, sizeof(VertexPNT) * meshData.vertexCount);
    renderer->meshes[mesh].vertexDataOffset =
        renderer->vertexDataUploadBufferSize / sizeof(VertexPNT);
    renderer->vertexDataUploadBufferSize +=
        sizeof(VertexPNT) * meshData.vertexCount;

    // Index data
    CopyMemory((u8 *)renderer->indexUploadBuffer.data +
                   renderer->indexUploadBufferSize,
        meshData.indices, sizeof(u32) * meshData.indexCount);
    renderer->meshes[mesh].indexDataOffset = renderer->indexUploadBufferSize;
    renderer->indexUploadBufferSize += sizeof(u32) * meshData.indexCount;

    renderer->meshes[mesh].indexCount = meshData.indexCount;
}

global sp_Metrics g_metricsBuffer[MAX_TILES];
global volatile i32 g_metricsBufferLength;

internal void WorkerThread(WorkQueue *queue)
{
    while (1)
    {
        // If not empty
        if (queue->head != queue->tail)
        {
            RandomNumberGenerator rng = {};
            rng.state = 0xF51C0E49;

            // Work to do
            sp_Metrics metrics = {};
            sp_Task *task = (sp_Task *)WorkQueuePop(queue, sizeof(sp_Task));
            sp_PathTraceTile(task->context, task->tile, &rng, &metrics);

            u32 index = AtomicExchangeAdd(&g_metricsBufferLength, 1);
            g_metricsBuffer[index] = metrics;
        }
        else
        {
            // FIXME: Use a semaphore for signalling
#ifdef PLATFORM_WINDOWS
            Sleep(10);
#elif defined(PLATFORM_LINUX)
            usleep(1000);
#endif
        }
    }
}

#ifdef PLATFORM_WINDOWS
internal DWORD WinWorkerThreadProc(LPVOID lpParam) 
{
    WorkerThread((WorkQueue *)lpParam);
    return 0;
}
#elif defined(PLATFORM_LINUX)
internal void* LinuxWorkerThreadProc(void *arg)
{
    WorkerThread((WorkQueue *)arg);
    return NULL;
}
#endif

struct ThreadMetaData
{
#ifdef PLATFORM_WINDOWS
    HANDLE handle;
    DWORD id;
#elif defined(PLATFORM_LINUX)
    pthread_t handle;
#endif
};

struct ThreadPool
{
    ThreadMetaData threads[MAX_THREADS];
};

internal ThreadPool CreateThreadPool(WorkQueue *queue)
{
    ThreadPool pool = {};

#ifdef PLATFORM_WINDOWS
    for (u32 threadIndex = 0; threadIndex < MAX_THREADS; ++threadIndex)
    {
        ThreadMetaData metaData = {};
        metaData.handle =
            CreateThread(NULL, 0, WinWorkerThreadProc, queue, 0, &metaData.id);
        Assert(metaData.handle != INVALID_HANDLE_VALUE);
        LogMessage("ThreadId is %u", metaData.id);

        pool.threads[threadIndex] = metaData;
    }
#elif defined(PLATFORM_LINUX)
    for (u32 threadIndex = 0; threadIndex < MAX_THREADS; ++threadIndex)
    {
        ThreadMetaData metaData = {};
        int ret = pthread_create(&metaData.handle, NULL, LinuxWorkerThreadProc, queue);
        Assert(ret == 0);
        LogMessage("Thread handle is %u", metaData.handle);

        pool.threads[threadIndex] = metaData;
    }
#endif
    return pool;
}

internal void AddRayTracingWorkQueue(WorkQueue *workQueue, sp_Context *ctx)
{
    Assert(workQueue->head == workQueue->tail);

    ImagePlane *imagePlane = ctx->camera->imagePlane;

    // TODO: Don't need to compute and store and array for this, could just
    // store a queue of tile indices that the worker threads read from. They
    // can then construct the tile data for each index from just the image
    // dimensions and tile dimensions.
    Tile tiles[MAX_TILES];
    u32 tileCount = ComputeTiles(imagePlane->width, imagePlane->height,
        TILE_WIDTH, TILE_HEIGHT, tiles, ArrayCount(tiles));

    workQueue->tail = 0; // oof
    for (u32 i = 0; i < tileCount; ++i)
    {
        sp_Task task = {};
        task.context = ctx;
        task.tile = tiles[i];
        g_metricsBufferLength = 0;
        WorkQueuePush(workQueue, &task, sizeof(task));
    }

    workQueue->head = 0; // ooof
}

struct SceneMeshData
{
    MeshData meshes[MAX_MESHES];
};

internal void LoadMeshData(
    SceneMeshData *scene, MemoryArena *meshDataArena, const char *assetDir)
{
    scene->meshes[Mesh_Bunny] = LoadMesh("bunny.obj", meshDataArena, assetDir);
    scene->meshes[Mesh_Monkey] =
        LoadMesh("monkey.obj", meshDataArena, assetDir);
    scene->meshes[Mesh_Plane] = CreatePlaneMesh(meshDataArena);
    scene->meshes[Mesh_Cube] = CreateCubeMesh(meshDataArena);
    scene->meshes[Mesh_Triangle] = CreateTriangleMeshData(meshDataArena);
    scene->meshes[Mesh_Sphere] = CreateIcosahedronMesh(3, meshDataArena);
    scene->meshes[Mesh_Disk] = CreateDiskMesh(meshDataArena, 1.0f, 13);

    LogMessage("Meshes data memory usage: %uk / %uk", meshDataArena->size / 1024,
        meshDataArena->capacity / 1024);
}

internal void UploadMeshDataToGpu(
    VulkanRenderer *renderer, SceneMeshData *sceneMeshData)
{
    for (u32 meshId = 0; meshId < MAX_MESHES; ++meshId)
    {
        CopyMeshDataToUploadBuffer(
            renderer, sceneMeshData->meshes[meshId], meshId);
    }

    VulkanCopyMeshDataToGpu(renderer);
}

internal void UploadMaterialDataToGpu(
    VulkanRenderer *renderer, Material *materialData)
{
    Material *materials = (Material *)renderer->materialBuffer.data;
    CopyMemory(materials, materialData, sizeof(Material) * MAX_MATERIALS);
}

internal void DrawMeshDataNormals(
    DebugDrawingBuffer *debugDrawBuffer, MeshData meshData)
{
    f32 normalLength = 0.1f;
    u32 triangleCount = meshData.indexCount / 3;
    for (u32 triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
    {
        u32 i = triangleIndex * 3;
        u32 j = triangleIndex * 3 + 1;
        u32 k = triangleIndex * 3 + 2;

        VertexPNT vertices[3];
        vertices[0] = meshData.vertices[i];
        vertices[1] = meshData.vertices[j];
        vertices[2] = meshData.vertices[k];

        DrawTriangle(debugDrawBuffer, vertices[0].position,
            vertices[1].position, vertices[2].position, Vec3(0, 0.8, 1));

        // Draw normals
        DrawLine(debugDrawBuffer, vertices[0].position,
            vertices[0].position + vertices[0].normal * normalLength,
            Vec3(1, 0, 1));
        DrawLine(debugDrawBuffer, vertices[1].position,
            vertices[1].position + vertices[1].normal * normalLength,
            Vec3(1, 0, 1));
        DrawLine(debugDrawBuffer, vertices[2].position,
            vertices[2].position + vertices[2].normal * normalLength,
            Vec3(1, 0, 1));
    }
}

internal HdrImage LoadImage(
    const char *relativePath, MemoryArena *imageDataArena, const char *assetDir)
{
    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", assetDir, relativePath);

    HdrImage tempImage = {};
    if (LoadExrImage(&tempImage, fullPath) != 0)
    {
        LogMessage("Failed to EXR image - %s", fullPath);
        InvalidCodePath();
    }

    HdrImage result = tempImage;
    result.pixels =
        AllocateArray(imageDataArena, f32, result.width * result.height * 4);
    CopyMemory(result.pixels, tempImage.pixels,
        sizeof(f32) * result.width * result.height * 4);

    free(tempImage.pixels);

    return result;
}

#if LIVE_CODE_RELOADING_TEST_ENABLED
typedef void LibraryFunction(Scene *, Aabb *);

struct LibraryCode
{
    LibraryFunction *generateScene;
#ifdef PLATFORM_LINUX
    void *handle;
    time_t lastWriteTime;
#elif defined(PLATFORM_WINDOWS)
    HMODULE handle;
    FILETIME lastWriteTime;
#endif
};

#define LIBRARY_PATH "lib.dll"
#define LIBRARY_PATH_TEMP "lib_temp.dll"
#define LIBRARY_LOCK_PATH "lock.tmp"

#ifdef PLATFORM_WINDOWS
// TODO: 64-bit version?
inline FILETIME GetLastWriteTime(const char *fileName)
{
    FILETIME lastWriteTime = {};

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesEx(fileName, GetFileExInfoStandard, &data))
    {
        lastWriteTime = data.ftLastWriteTime;
    }
    return lastWriteTime;
}

internal LibraryCode LoadLibraryCode()
{
    LibraryCode result = {};
    result.lastWriteTime = GetLastWriteTime(LIBRARY_PATH);
    CopyFile(LIBRARY_PATH, LIBRARY_PATH_TEMP, false);

    result.handle = LoadLibraryA(LIBRARY_PATH_TEMP);
    if (result.handle != NULL)
    {
        result.generateScene =
            (LibraryFunction *)GetProcAddress(result.handle, "GenerateScene");
        Assert(result.generateScene); // FIXME: Should error not assert!
    }
    else
    {
        LogMessage("Failed to load library code!");
    }

    return result;
};

internal b32 IsLockFileActive(const char *lockPath)
{
    WIN32_FILE_ATTRIBUTE_DATA ignored;
    if (!GetFileAttributesEx(lockPath, GetFileExInfoStandard, &ignored))
        return false;

    return true;
}

internal b32 WasLibraryCodeModified(LibraryCode *lib)
{
    b32 result = false;
    FILETIME newWriteTime = GetLastWriteTime(LIBRARY_PATH);
    if (CompareFileTime(&newWriteTime, &lib->lastWriteTime) != 0)
    {
        if (!IsLockFileActive(LIBRARY_LOCK_PATH))
        {
            result = true;
        }
    }

    return result;
}

internal void UnloadLibraryCode(LibraryCode *lib)
{
    if (lib->handle != NULL)
    {
        FreeLibrary(lib->handle);
    }

    ClearToZero(lib, sizeof(*lib));
}
#else
internal LibraryCode LoadLibraryCode()
{
    LibraryCode result = {};
    return result;
}

internal b32 WasLibraryCodeModified(LibraryCode *lib)
{
    return false;
}

internal void UnloadLibraryCode(LibraryCode *lib)
{
}
#endif

#endif

internal sp_Mesh sp_CreateMeshFromMeshData(
    MeshData meshData, MemoryArena *arena, b32 useSmoothShading)
{
    sp_Mesh mesh = sp_CreateMesh(meshData.vertices, meshData.vertexCount,
        meshData.indices, meshData.indexCount, useSmoothShading);

    return mesh;
}

internal void BuildPathTracerScene(
    sp_Scene *scene, Scene *entityScene, sp_Mesh *meshes)
{
    for (u32 i = 0; i < entityScene->count; i++)
    {
        Entity *entity = entityScene->entities + i;

        sp_AddObjectToScene(scene, meshes[entity->mesh], entity->material,
            entity->position, entity->rotation, entity->scale);
    }
}

internal void DebugDrawBvh(bvh_Node *node, DebugDrawingBuffer *debugDrawBuffer)
{
    for (u32 i = 0; i < 4; i++)
    {
        bvh_Node *child = node->children[i];
        if (child != NULL)
        {
            DebugDrawBvh(child, debugDrawBuffer);
            //DrawBox(debugDrawBuffer, child->min, child->max, Vec3(1, 1, 0));
        }
    }

    DrawBox(debugDrawBuffer, node->min, node->max, Vec3(0, 1, 0));
}

internal void CreatePathTracerMeshData(SceneMeshData *sceneMeshData,
    sp_Mesh *meshes, MemoryArena *meshDataArena,
    MemoryArena *accelerationStructureMemoryArena, MemoryArena *tempArena)
{
    for (u32 i = 0; i < MAX_MESHES; ++i)
    {
        meshes[i] = sp_CreateMeshFromMeshData(
                sceneMeshData->meshes[i], meshDataArena, true);
        sp_BuildMeshMidphase(
            &meshes[i], accelerationStructureMemoryArena, tempArena);
    }
}

// Copied from old project, don't really remember how this works
internal vec3 GenerateSelectionRayDirection(f32 x, f32 y, f32 framebufferWidth,
    f32 framebufferHeight, f32 fov, f32 nearClip, vec3 cameraRotation)
{
    y = framebufferHeight - y;

    f32 aspect = framebufferWidth / framebufferHeight;
    f32 rad = Radians(fov);
    f32 vLen = Tan(rad / 2.0f) * nearClip;
    f32 hLen = vLen * aspect;

    mat4 rotation = RotateX(-cameraRotation.x) * RotateY(-cameraRotation.y);
    rotation = Transpose(rotation); // Why do this, just don't negate rotation

    vec3 right = Normalize(rotation.columns[0].xyz);
    vec3 up = Normalize(rotation.columns[1].xyz);
    vec3 forward = -Normalize(rotation.columns[2].xyz);

    vec3 v = up * vLen;
    vec3 h = right * hLen;

    x -= framebufferWidth * 0.5f;
    y -= framebufferHeight * 0.5f;

    x /= framebufferWidth * 0.5f;
    y /= framebufferHeight * 0.5f;

    vec3 dir = forward * nearClip + h * x + v * y;
    return dir;
}

internal void HandleSelection(
    Scene *scene, DebugDrawingBuffer *debugDrawBuffer, GameInput *input)
{
    // Create Ray
    vec3 rayOrigin = g_camera.position;
    vec3 rayDirection = GenerateSelectionRayDirection((f32)input->mousePosX,
        (f32)input->mousePosY, (f32)g_FramebufferWidth,
        (f32)g_FramebufferHeight, CAMERA_FOV, CAMERA_NEAR_CLIP,
        g_camera.rotation);

    DrawLine(debugDrawBuffer, rayOrigin, rayOrigin + rayDirection * 10.0f,
        Vec3(1, 0, 0));

    // Intersect ray against entity AABBs
    f32 tmin = F32_MAX;
    Entity *hitEntity = NULL;
    for (u32 i = 0; i < scene->count; i++)
    {
        Entity *entity = scene->entities + i;
        f32 t = RayIntersectAabb(
            entity->aabbMin, entity->aabbMax, rayOrigin, rayDirection);

        // NOTE: t > 0 to avoid selecting when inside the AABB
        if (t > 0.0f && t < tmin)
        {
            tmin = t;
            hitEntity = entity;
        }
    }

    if (hitEntity != NULL)
    {
        LogMessage("Selected entity with mesh %u", hitEntity->mesh);
        DrawBox(debugDrawBuffer, hitEntity->position - Vec3(0.25),
                hitEntity->position + Vec3(0.25), Vec3(1, 0, 0));
    }
}

int main(int argc, char **argv)
{
    LogMessage = &LogMessage_;

#if LIVE_CODE_RELOADING_TEST_ENABLED
    LibraryCode libraryCode = LoadLibraryCode();
#endif

    // Parse command line args
    const char *assetDir = "./";
    ParseCommandLineArgs(argc, (const char **)argv, &assetDir);
    LogMessage("Asset directoy set to %s", assetDir);

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

    // Create memory arenas
    u32 applicationMemorySize = APPLICATION_MEMORY_LIMIT;
    MemoryArena applicationMemoryArena = {};
    InitializeMemoryArena(&applicationMemoryArena,
        AllocateMemory(applicationMemorySize), applicationMemorySize);

    u32 workQueueArenaSize = Kilobytes(32);
    MemoryArena workQueueArena =
        SubAllocateArena(&applicationMemoryArena, workQueueArenaSize);

    u32 tempMemorySize = Megabytes(64); // TODO: Config option
    MemoryArena tempArena =
        SubAllocateArena(&applicationMemoryArena, tempMemorySize);

    u32 meshDataMemorySize = Megabytes(4); // TODO: Config option
    MemoryArena meshDataArena =
        SubAllocateArena(&applicationMemoryArena, meshDataMemorySize);

    u32 accelerationStructureMemorySize = Megabytes(64); // TODO: Config option
    MemoryArena accelerationStructureMemoryArena = SubAllocateArena(
        &applicationMemoryArena, accelerationStructureMemorySize);

    u32 entityMemorySize = Megabytes(4); // TODO: Config option
    MemoryArena entityMemoryArena =
        SubAllocateArena(&applicationMemoryArena, entityMemorySize);

    u32 imageDataMemorySize = Megabytes(256); // TODO: Config option
    MemoryArena imageDataArena =
        SubAllocateArena(&applicationMemoryArena, imageDataMemorySize);

    LogMessage("Application memory usage: %uk / %uk",
        applicationMemoryArena.size / 1024,
        applicationMemoryArena.capacity / 1024);

    // Create Vulkan Renderer
    VulkanRenderer renderer = {};
    VulkanInit(&renderer, g_Window);

    // Create SIMD Path tracer
    sp_Context context = {};

    // Load mesh data
    SceneMeshData sceneMeshData = {};
    LoadMeshData(&sceneMeshData, &meshDataArena, assetDir);

    // Publish mesh data to vulkan renderer
    UploadMeshDataToGpu(&renderer, &sceneMeshData);

    // Publish mesh data to GPU path tracer
    VulkanUploadComputeMeshData(&renderer);

    // Build mesh data for path tracer
    sp_Mesh meshes[MAX_MESHES];
    CreatePathTracerMeshData(&sceneMeshData, meshes, &meshDataArena,
        &accelerationStructureMemoryArena, &tempArena);

    // Load image data
    //HdrImage image =
        //LoadImage("studio_garden_4k.exr", &imageDataArena, assetDir);
    //HdrImage image =
        //LoadImage("kiara_4_mid-morning_4k.exr", &imageDataArena, assetDir);

    // Create checkerboard image
    HdrImage checkerBoardImage = CreateCheckerBoardImage(&imageDataArena);
    UploadHdrImageToGPU(&renderer, checkerBoardImage, Image_CheckerBoard, 8);

    // Create and upload test cube map
    //HdrCubeMap cubeMap = CreateCubeMap(image, &imageDataArena, 1024, 1024);
    //UploadCubeMapToGPU(&renderer, cubeMap, Image_CubeMapTest, 6, 1024, 1024);

    // Create and upload irradiance cube map
    //HdrCubeMap irradianceCubeMap =
        //CreateIrradianceCubeMap(image, &imageDataArena, 32, 32);
    //UploadCubeMapToGPU(
        //&renderer, irradianceCubeMap, Image_IrradianceCubeMap, 7, 32, 32);

    // Define materials, in the future this will come from file
    Material materialData[MAX_MATERIALS] = {};
    materialData[Material_Red].baseColor = Vec3(0.18, 0.1, 0.1);
    materialData[Material_Blue].baseColor = Vec3(0.1, 0.1, 0.18);
    materialData[Material_CheckerBoard].baseColor = Vec3(0.18, 0.18, 0.18);
    materialData[Material_White].baseColor = Vec3(0.18, 0.18, 0.18);
    materialData[Material_BlueLight].emission = Vec3(0.4, 0.6, 1) * 4.0f;
    materialData[Material_WhiteLight].emission = Vec3(10);
    materialData[Material_Black].baseColor = Vec3(0);
    materialData[Material_OrangeLight].emission = Vec3(10, 8, 7.5);

    // Publish material data to vulkan renderer
    UploadMaterialDataToGpu(&renderer, materialData);

    Aabb meshAabbs[MAX_MESHES] = {};
    meshAabbs[Mesh_Sphere].min = meshes[Mesh_Sphere].midphaseTree.root->min;
    meshAabbs[Mesh_Sphere].max = meshes[Mesh_Sphere].midphaseTree.root->max;
    meshAabbs[Mesh_Plane].min = meshes[Mesh_Plane].midphaseTree.root->min;
    meshAabbs[Mesh_Plane].max = meshes[Mesh_Plane].midphaseTree.root->max;

    // Create scene
    Scene scene = {};
    scene.meshAabbs = meshAabbs;
    scene.lightData =(LightData *)renderer.lightBuffer.data;
    Assert(sizeof(LightData) <= LIGHT_BUFFER_SIZE);

    scene.entities = AllocateArray(&entityMemoryArena, Entity, MAX_ENTITIES);
    scene.max = MAX_ENTITIES;
    libraryCode.generateScene(&scene, meshAabbs);
    VulkanUploadComputeSceneBuffer(&renderer, scene);

    g_Profiler.samples =
        (ProfilerSample *)AllocateMemory(PROFILER_SAMPLE_BUFFER_SIZE);
    ProfilerResults profilerResults = {};

    DebugDrawingBuffer debugDrawBuffer = {};
    debugDrawBuffer.vertices = (VertexPC *)renderer.debugVertexDataBuffer.data;
    debugDrawBuffer.max = DEBUG_VERTEX_BUFFER_SIZE / sizeof(VertexPC);

    ImagePlane imagePlane = {};
    imagePlane.pixels = (vec4 *)renderer.imageUploadBuffer.data;
    imagePlane.width = RAY_TRACER_WIDTH;
    imagePlane.height = RAY_TRACER_HEIGHT;

    sp_Camera camera = {};
    sp_ConfigureCamera(&camera, &imagePlane, Vec3(0, 0, 1), Quat(), 0.3f);
    context.camera = &camera;

    sp_Scene pathTracerScene = {};
    sp_InitializeScene(&pathTracerScene, &applicationMemoryArena);
    context.scene = &pathTracerScene;

    sp_MaterialSystem materialSystem = {};
    //sp_RegisterTexture(&materialSystem, image, 5);
    sp_RegisterTexture(&materialSystem, checkerBoardImage, 6);

#if 0
    sp_Material backgroundMaterial = {};
    backgroundMaterial.emissionTexture = 5;
    sp_RegisterMaterial(
        &materialSystem, backgroundMaterial, Material_Background);
#endif

    sp_Material checkerboardMaterial = {};
    checkerboardMaterial.albedoTexture = 6;
    sp_RegisterMaterial(
        &materialSystem, checkerboardMaterial, Material_CheckerBoard);

    sp_Material blackMaterial = {};
    sp_RegisterMaterial(
        &materialSystem, blackMaterial, Material_Black);


    sp_Material backgroundMaterial = {};
    backgroundMaterial.albedo = materialData[Material_BlueLight].baseColor;
    backgroundMaterial.emission = materialData[Material_BlueLight].emission;
    backgroundMaterial.flags = MaterialFlag_Background;
    sp_RegisterMaterial(&materialSystem, backgroundMaterial, Material_BlueLight);

    // Upload materials
    for (u32 i = 0; i < MAX_MATERIALS; ++i)
    {
        // TODO: Do texture uploading here too
        if (i != Material_CheckerBoard && i != Material_BlueLight)
        {
            sp_Material material = {};
            material.albedo = materialData[i].baseColor;
            material.emission = materialData[i].emission;
            // TODO: Don't ignore return value
            sp_RegisterMaterial(&materialSystem, material, i);
        }
    }
    materialSystem.backgroundMaterialId = scene.backgroundMaterial;

    context.materialSystem = &materialSystem;

    WorkQueue workQueue =
        CreateWorkQueue(&workQueueArena, sizeof(sp_Task), 1024);
    ThreadPool threadPool = CreateThreadPool(&workQueue);

    LogMessage("Start up time: %gs", glfwGetTime());

    // Debugging compute shader
    {
        Mesh mesh = renderer.meshes[Mesh_Plane];
        LogMessage("Mesh_Plane: indexCount: %u indexDataOffset: %u vertexDataOffset: %u",
                mesh.indexCount,
                mesh.indexDataOffset,
                mesh.vertexDataOffset);
    }

    vec3 lastCameraPosition = g_camera.position;
    vec3 lastCameraRotation = g_camera.rotation;
    f32 prevFrameTime = 0.0f;
    u32 maxDepth = 1;
    b32 drawTests = false;
    b32 isRayTracing = false;
    b32 showComparision = false;
    b32 showDebugDrawing = true;
    f32 t = 0.0f;
    f64 rayTracingStartTime = 0.0;
    f64 nextStatPrintTime = 0.0;
    b32 showComputeShaderOutput = false;
    b32 runPathTracingComputeShader = false;
    while (!glfwWindowShouldClose(g_Window))
    {
        f32 dt = prevFrameTime;
        dt = Min(dt, 0.25f);
        if (isRayTracing)
        {
            dt = 0.016f;
        }

        debugDrawBuffer.count = 0;


        f64 frameStart = glfwGetTime();
        InputBeginFrame(&input);
        glfwPollEvents();

        i32 framebufferWidth, framebufferHeight;
        glfwGetFramebufferSize(g_Window, &framebufferWidth, &framebufferHeight);
        if ((u32)framebufferWidth != g_FramebufferWidth ||
            (u32)framebufferHeight != g_FramebufferHeight)
        {
            LogMessage("Framebuffer resized to %d x %d", framebufferWidth,
                framebufferHeight);

            // Recreate vulkan swapchain
            VulkanFramebufferResize(
                &renderer, framebufferWidth, framebufferHeight);

            g_FramebufferWidth = framebufferWidth;
            g_FramebufferHeight = framebufferHeight;
        }

#if LIVE_CODE_RELOADING_TEST_ENABLED
        // Check if library has been modified, if so reload it
        if (WasLibraryCodeModified(&libraryCode))
        {
            LogMessage("Reloading LibraryCode");
            UnloadLibraryCode(&libraryCode);
            libraryCode = LoadLibraryCode();
        }

        // SURELY WE WOULD ONLY DO THIS IF THE LIBRARY HAS CHANGED!!!!!????
        //libraryCode.generateScene(&scene, meshAabbs);
#endif

        if (WasPressed(input.buttonStates[KEY_MOUSE_BUTTON_LEFT]))
        {
            HandleSelection(&scene, &debugDrawBuffer, &input);
        }

        if (isRayTracing)
        {
            // Only print metrics while we have path tracing work remaining
            if (workQueue.head != workQueue.tail)
            {
                if (glfwGetTime() > nextStatPrintTime)
                {
                    // Not sure how scalable this approach is of adding metrics
                    // for each tile to a queue. Decided to go with this
                    // approach rather than create a new global metric
                    // structure that stored atomic counters for each metric
                    // value which then each thread accumulates its local
                    // metric values onto.
                    sp_Metrics total = {};
                    for (i32 i = 0; i < g_metricsBufferLength; i++)
                    {
                        for (u32 j = 0; j < SP_MAX_METRICS; ++j)
                        {
                            total.values[j] += g_metricsBuffer[i].values[j];
                        }
                    }

                    u32 tilesProcessed = g_metricsBufferLength;
                    u32 totalNumberOfFiles = workQueue.tail;

                    LogMessage("Processed %u/%u tiles - %g%% complete",
                        tilesProcessed, totalNumberOfFiles,
                        ((f32)tilesProcessed / (f32)totalNumberOfFiles) *
                            100.0f);
                    LogMessage("Cycles elapsed: %llu",
                        total.values[sp_Metric_CyclesElapsed]);
                    LogMessage("Paths traced: %llu",
                        total.values[sp_Metric_PathsTraced]);
                    LogMessage("Rays traced: %llu",
                        total.values[sp_Metric_RaysTraced]);
                    LogMessage(
                        "Ray hits: %llu", total.values[sp_Metric_RayHitCount]);
                    LogMessage("Ray misses: %llu",
                        total.values[sp_Metric_RayMissCount]);
                    LogMessage("RayIntersectScene cycles elapsed: %llu",
                        total
                            .values[sp_Metric_CyclesElapsed_RayIntersectScene]);
                    LogMessage("RayIntersectBroadphase cycles elapsed: %llu",
                        total.values
                            [sp_Metric_CyclesElapsed_RayIntersectBroadphase]);
                    LogMessage("RayIntersectMesh cycles elapsed: %llu",
                        total.values[sp_Metric_CyclesElapsed_RayIntersectMesh]);
                    LogMessage("RayIntersectMesh Midphase cycles elapsed: %llu",
                        total.values
                            [sp_Metric_CyclesElapsed_RayIntersectMeshMidphase]);
                    LogMessage("RayIntersectTriangle cycles elapsed: %llu",
                        total.values
                            [sp_Metric_CyclesElapsed_RayIntersectTriangle]);
                    LogMessage(
                        "RayIntersectMesh Midphase AABB tests performed: %llu",
                        total.values
                            [sp_Metric_RayIntersectMesh_MidphaseAabbTestCount]);
                    LogMessage("RayIntersectMesh tests performed: %llu",
                        total
                            .values[sp_Metric_RayIntersectMesh_TestsPerformed]);

                    f64 secondsElapsed = glfwGetTime() - rayTracingStartTime;
                    LogMessage("Seconds elapsed: %g", secondsElapsed);
                    LogMessage("Paths traced per second: %g",
                        (f64)total.values[sp_Metric_PathsTraced] /
                            secondsElapsed);
                    LogMessage("Rays per second: %g",
                        (f64)total.values[sp_Metric_RaysTraced] /
                            secondsElapsed);
                    LogMessage("Average cycles per ray: %g",
                        (f64)total.values[sp_Metric_CyclesElapsed] /
                            (f64)total.values[sp_Metric_RaysTraced]);

                    // TODO: Expose constant, currently prints stats 1 per
                    // second
                    nextStatPrintTime = glfwGetTime() + 1.0;
                }
            }
        }

        if (WasPressed(input.buttonStates[KEY_SPACE]))
        {
            if (!isRayTracing)
            {
                // TODO: Build path tracer scene
                vec3 position = g_camera.position;
                quat rotation = Quat(Vec3(0, 1, 0), g_camera.rotation.y) *
                                Quat(Vec3(1, 0, 0), g_camera.rotation.x);

                sp_ConfigureCamera(
                    &camera, &imagePlane, position, rotation, 0.8f);

                // Only allow submitting new work to the queue if it is empty
                if (workQueue.head == workQueue.tail)
                {
                    isRayTracing = true;
                    ClearImagePlane(&imagePlane);

                    // TODO: Switch to immediate mode API style
                    pathTracerScene.objectCount = 0;
                    ResetMemoryArena(&pathTracerScene.memoryArena);

                    BuildPathTracerScene(&pathTracerScene, &scene, meshes);
                    sp_BuildSceneBroadphase(&pathTracerScene);

                    AddRayTracingWorkQueue(&workQueue, &context);
                    rayTracingStartTime = glfwGetTime();
                }
            }
            else
            {
                isRayTracing = false;
            }
        }

        if (LengthSq(g_camera.position - lastCameraPosition) > 0.0001f ||
            LengthSq(g_camera.rotation - lastCameraRotation) > 0.0001f)
        {
            lastCameraPosition = g_camera.position;
            lastCameraRotation = g_camera.rotation;
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

        if (WasPressed(input.buttonStates[KEY_F1]))
        {
            showComparision = !showComparision;
        }

        if (WasPressed(input.buttonStates[KEY_F2]))
        {
            showDebugDrawing = !showDebugDrawing;
        }

#if FEAT_ENABLE_GPU_PATH_TRACING
        if (WasPressed(input.buttonStates[KEY_F3]))
        {
            showComputeShaderOutput = !showComputeShaderOutput;
        }

        if (WasPressed(input.buttonStates[KEY_P]))
        {
            runPathTracingComputeShader = true;
            showComputeShaderOutput = true;
            VulkanUploadComputeTileQueue(&renderer);
        }
#endif

        VulkanCopyImageFromCPU(&renderer);
        
#if DRAW_ENTITY_AABBS
        DrawEntityAabbs(scene, &debugDrawBuffer);
#endif

        //DebugDrawBvh(
            //pathTracerScene.meshes[4].midphaseTree.root, &debugDrawBuffer);

        // Move camera around
        Update(&renderer, &input, dt);

        // TODO: Don't do this here
        UniformBufferObject *ubo =
            (UniformBufferObject *)renderer.uniformBuffer.data;

        if (isRayTracing)
        {
            ubo->showComparision = 1;
            if (showComparision)
                ubo->showComparision = 2;
        }
        else
        {
            ubo->showComparision = 0;
        }

        if (showComputeShaderOutput)
        {
            ubo->showComparision = 3;
        }

        renderer.debugDrawVertexCount = debugDrawBuffer.count;

        u32 outputFlags = Output_None;
        if (showDebugDrawing)
        {
            outputFlags |= Output_ShowDebugDrawing;
        }

#if FEAT_ENABLE_GPU_PATH_TRACING
        if (runPathTracingComputeShader)
        {
            outputFlags |= Output_RunPathTracingComputeShader;
        }
#endif
        VulkanRender(&renderer, outputFlags, scene);

        prevFrameTime = (f32)(glfwGetTime() - frameStart);
    }
    return 0;
}
