/* TODO:
List:
 - [ GOAL is to reduce cycle time for working on materials models/shaders ]
 - CPU bilinear sampling [X]
 - Profiling! (What is our current cost per ray?)
 - Startup time is too long! (building AABB trees for meshes most likely)
 - SIMD
 - Scene definition from file [ ]
 - Live code reloading? +1

Bugs:
 - Resizing window crashes app
 - Race condition when submitting work to queue when queue is empty but worker
   threads are still working on the tasks they've pulled from the queue.
 - IBL looks too dim, not sure what is causing it

Tech Debt:
 - Hard-coded material id to texture mapping in shader
 - Code duplication for creating and uploading images/cube maps to GPU

Features:
 - Linear space rendering [x]
 - Texture mapping [x]
 - Smooth shading [ ]
 - Bilinear sampling
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
- Scene definition from file
- Resource definition from file

Optimizations - CPU ray tracer
- Multiple triangles per tree leaf node
- SIMD
- Multi-core [X]
- Sample cube maps in shaders rather than equirectangular images [x]
- Startup time is too long! (building AABB trees for meshes most likely)

Analysis
- AABB trees
- Triangle meshes
- Intersection functions
- Assembly code

Observability
- More metrics
- More logs
- More profiling

Optimizations
- Don't ray trace whole screen when using comparison view
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

#include "vulkan_renderer.h"
#include "mesh.h"
#include "profiler.h"
#include "debug.h"
#include "world.h"
#include "cpu_ray_tracer.h"

#include "debug.cpp"
#include "ray_intersection.cpp"
#include "tree_utils.cpp"
#include "cpu_ray_tracer.cpp"
#include "cpu_ray_tracer_manual_tests.cpp"
#include "vulkan_renderer.cpp"
#include "mesh.cpp"
#include "cmdline.cpp"
#include "cubemap.cpp"
#include "mesh_generation.cpp"

#include "image.cpp"

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

internal void Update(
    VulkanRenderer *renderer, RayTracer *rayTracer, GameInput *input, f32 dt)
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

internal void AddEntity(World *world, vec3 position, quat rotation, vec3 scale,
    u32 mesh, u32 material)
{
    // TODO: Support non-uniform scaling in the ray tracer
    Assert(scale.x == scale.y && scale.x == scale.z);
    if (world->count < world->max)
    {
        Entity *entity = world->entities + world->count++;
        entity->position = position;
        entity->rotation = rotation;
        entity->scale = scale;
        entity->mesh = mesh;
        entity->material = material;
    }
}

internal void DrawEntityAabbs(World world, DebugDrawingBuffer *debugDrawBuffer)
{
    for (u32 entityIndex = 0; entityIndex < world.count; ++entityIndex)
    {
        Entity *entity = world.entities + entityIndex;
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

internal void WorkerThread(WorkQueue *queue)
{
    while (1)
    {
        if (queue->head != queue->tail)
        {
            // Work to do
            Task task = WorkQueuePop(queue);

            ThreadData *threadData = task.threadData;

            RandomNumberGenerator rng = {};
            rng.state = 0xF51C0E49;

            DoRayTracing(threadData->width, threadData->height,
                threadData->imageBuffer, threadData->rayTracer,
                threadData->world, task.tile, &rng);
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

internal void AddRayTracingWorkQueue(
    WorkQueue *workQueue, ThreadData *threadData)
{
    Assert(workQueue->head == workQueue->tail);

    // TODO: Don't need to compute and store and array for this, could just
    // store a queue of tile indices that the worker threads read from. They
    // can then construct the tile data for each index from just the image
    // dimensions and tile dimensions.
    Tile tiles[64];
    u32 tileCount = ComputeTiles(threadData->width, threadData->height,
        TILE_WIDTH, TILE_HEIGHT, tiles, ArrayCount(tiles));

    Assert(tileCount < MAX_TASKS);
    for (u32 i = 0; i < tileCount; ++i)
    {
        workQueue->tasks[i].threadData = threadData;
        workQueue->tasks[i].tile = tiles[i];
    }

    // TODO: Should have a nicer method for clearing the image
    ClearToZero(threadData->imageBuffer,
        sizeof(vec4) * threadData->width * threadData->height);

    workQueue->tail = tileCount;
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

internal void UploadMeshDataToCpuRayTracer(RayTracer *rayTracer,
    SceneMeshData *sceneMeshData, MemoryArena *memoryArena,
    MemoryArena *tempArena)
{
    for (u32 meshId = 0; meshId < MAX_MESHES; ++meshId)
    {
        rayTracer->meshes[meshId] = CreateMesh(
            sceneMeshData->meshes[meshId], memoryArena, tempArena, true);
    }
}

internal void UploadMaterialDataToGpu(
    VulkanRenderer *renderer, Material *materialData)
{
    Material *materials = (Material *)renderer->materialBuffer.data;
    CopyMemory(materials, materialData, sizeof(Material) * MAX_MATERIALS);
}

internal void UploadMaterialDataToCpuRayTracer(
    RayTracer *rayTracer, Material *materialData)
{
    CopyMemory(
        rayTracer->materials, materialData, sizeof(Material) * MAX_MATERIALS);
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

#if LIVE_CODE_RELOADING_TEST_ENABLED
typedef void LibraryFunction(void);

struct LibraryCode
{
    LibraryFunction *doThing;
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
        result.doThing =
            (LibraryFunction *)GetProcAddress(result.handle, "DoThing");
        Assert(result.doThing); // FIXME: Should error not assert!
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

#endif

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

    // Create CPU Ray tracer
    RayTracer rayTracer = {};
    rayTracer.useAccelerationStructure = true;

    // Load mesh data
    SceneMeshData sceneMeshData = {};
    LoadMeshData(&sceneMeshData, &meshDataArena, assetDir);

    // Publish mesh data to vulkan renderer
    UploadMeshDataToGpu(&renderer, &sceneMeshData);

    // Publish mesh data to CPU ray tracer
    UploadMeshDataToCpuRayTracer(&rayTracer, &sceneMeshData,
        &accelerationStructureMemoryArena, &tempArena);

    // Load image data
    {
        char fullPath[256];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", assetDir,
            "studio_garden_4k.exr");
            //"kiara_4_mid-morning_4k.exr");

        HdrImage image = {};
        if (LoadExrImage(&image, fullPath) != 0)
        {
            LogMessage("Failed to EXR image - %s", fullPath);
            InvalidCodePath();
        }
        rayTracer.image = image;
        rayTracer.image.pixels =
            AllocateArray(&imageDataArena, f32, image.width * image.height * 4);
        CopyMemory(rayTracer.image.pixels, image.pixels,
            sizeof(f32) * image.width * image.height * 4);
        free(image.pixels);
    }

    // Upload equirectangular environment map to GPU
    UploadHdrImageToVulkan(&renderer, rayTracer.image, Image_EnvMapTest, 6);

    // Create checkerboard image
    HdrImage checkerBoardImage = CreateCheckerBoardImage(&imageDataArena);
    UploadHdrImageToVulkan(&renderer, checkerBoardImage, Image_CheckerBoard, 8);
    rayTracer.checkerBoardImage = checkerBoardImage;

    // Create and upload test cube map
    UploadTestCubeMapToGPU(
        &renderer, rayTracer.image, Image_CubeMapTest, 6, 1024, 1024);

    // Create and upload irradiance cube map
    UploadIrradianceCubeMapToGPU(
        &renderer, rayTracer.image, Image_IrradianceCubeMap, 7);

    // Define materials, in the future this will come from file
    Material materialData[MAX_MATERIALS] = {};
    materialData[Material_Red].baseColor = Vec3(0.18, 0.1, 0.1);
    materialData[Material_Blue].baseColor = Vec3(0.1, 0.1, 0.18);
    materialData[Material_Background].emission = Vec3(1, 0.95, 0.8);
    materialData[Material_CheckerBoard].baseColor = Vec3(0.18, 0.18, 0.18);
    materialData[Material_White].baseColor = Vec3(0.18, 0.18, 0.18);

    // Publish material data to vulkan renderer
    UploadMaterialDataToGpu(&renderer, materialData);

    // Publish material data to CPU ray tracer
    UploadMaterialDataToCpuRayTracer(&rayTracer, materialData);

    // Create world
    World world = {};
    world.entities = AllocateArray(&entityMemoryArena, Entity, MAX_ENTITIES);
    world.max = MAX_ENTITIES;

    AddEntity(&world, Vec3(0, 0, 0), Quat(), Vec3(100), Mesh_Cube,
        Material_Background);
    for (u32 y = 0; y < 5; ++y)
    {
        for (u32 x = 0; x < 5; ++x)
        {
            vec3 origin = Vec3(-5, -5, 0);
            vec3 p = origin + Vec3((f32)x, (f32)y, 0) * 3.0f;
            AddEntity(&world, p, Quat(), Vec3(1), Mesh_Sphere, Material_White);
        }
    }

    // Compute bounding box for each entity
    // TODO: Should not rely on CPU ray tracer for computing bounding boxes,
    // they should be calculated as part of mesh loading.
    ComputeEntityBoundingBoxes(&world, &rayTracer);

    // Upload world to CPU ray tracer
    rayTracer.aabbTree = BuildWorldBroadphase(
        &world, &accelerationStructureMemoryArena, &tempArena);

#if ANALYZE_BROAD_PHASE_TREE
    LogMessage("Evaluate Broadphase tree");
    EvaluateTree(rayTracer.aabbTree);
    LogMessage("Evaluate Mesh_Bunny tree");
    EvaluateTree(rayTracer.meshes[Mesh_Bunny].aabbTree);
    LogMessage("Evaluate Mesh_Monkey tree");
    EvaluateTree(rayTracer.meshes[Mesh_Monkey].aabbTree);
#endif

    g_Profiler.samples =
        (ProfilerSample *)AllocateMemory(PROFILER_SAMPLE_BUFFER_SIZE);
    ProfilerResults profilerResults = {};

    DebugDrawingBuffer debugDrawBuffer = {};
    debugDrawBuffer.vertices = (VertexPC *)renderer.debugVertexDataBuffer.data;
    debugDrawBuffer.max = DEBUG_VERTEX_BUFFER_SIZE / sizeof(VertexPC);
    rayTracer.debugDrawBuffer = &debugDrawBuffer;

    ThreadData threadData = {};
    threadData.width = RAY_TRACER_WIDTH;
    threadData.height = RAY_TRACER_HEIGHT;
    threadData.imageBuffer = (vec4 *)renderer.imageUploadBuffer.data;
    threadData.rayTracer = &rayTracer;
    threadData.world = &world;

    WorkQueue workQueue = {};
    ThreadPool threadPool = CreateThreadPool(&workQueue);

    LogMessage("Start up time: %gs", glfwGetTime());

    vec3 lastCameraPosition = g_camera.position;
    vec3 lastCameraRotation = g_camera.rotation;
    f32 prevFrameTime = 0.0f;
    u32 maxDepth = 1;
    b32 drawTests = false;
    b32 isRayTracing = false;
    b32 showComparision = false;
    b32 showDebugDrawing = true;
    f32 t = 0.0f;
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

#if LIVE_CODE_RELOADING_TEST_ENABLED
        // Check if library has been modified, if so reload it
        if (WasLibraryCodeModified(&libraryCode))
        {
            LogMessage("Reloading LibraryCode");
            UnloadLibraryCode(&libraryCode);
            libraryCode = LoadLibraryCode();
        }

        libraryCode.doThing();
#endif

        if (WasPressed(input.buttonStates[KEY_SPACE]))
        {
            if (!isRayTracing)
            {
                // Only allow submitting new work to the queue if it is empty
                if (workQueue.head == workQueue.tail)
                {
                    isRayTracing = true;
                    AddRayTracingWorkQueue(&workQueue, &threadData);
                }
            }
            else
            {
                isRayTracing = false;
            }
        }

        if (WasPressed(input.buttonStates[KEY_P]))
        {
            DumpMetrics(&g_Metrics);
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

        VulkanCopyImageFromCPU(&renderer);
        
#if DRAW_ENTITY_AABBS
        DrawEntityAabbs(world, &debugDrawBuffer);
#endif
        // Move camera around
        Update(&renderer, &rayTracer, &input, dt);

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

        renderer.debugDrawVertexCount = debugDrawBuffer.count;

        u32 outputFlags = Output_None;
        if (showDebugDrawing)
        {
            outputFlags |= Output_ShowDebugDrawing;
        }
        VulkanRender(&renderer, outputFlags, world);
        prevFrameTime = (f32)(glfwGetTime() - frameStart);
    }
    return 0;
}
