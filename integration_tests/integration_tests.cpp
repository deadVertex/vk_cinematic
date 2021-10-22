#include <cstdarg>

#include "unity.h"

// For integration tests
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(PLATFORM_LINUX)
#endif

// Move to vulkan specific module?
#include <vulkan/vulkan.h>
#ifdef PLATFORM_WINDOWS
#include <vulkan/vulkan_win32.h>
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "platform.h"

// FIXME: Copied from main.cpp
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

#include "cpu_ray_tracer.h"

#include "vulkan_renderer.cpp"

#include "mesh.cpp"

#include "cmdline.cpp"


DebugReadEntireFile(ReadEntireFile)
{
    DebugReadFileResult result = {};
    return result;
}

global const char *g_AssetDir = "../assets";

void setUp(void)
{
    // set stuff up here
}

void tearDown(void)
{
    // clean stuff up here
}

// FIXME: Copied from main.cpp
internal void GlfwErrorCallback(int error, const char *description)
{
    LogMessage("GLFW Error (%d): %s.", error, description);
}

// Integration tests
void TestLoadMesh()
{
    MemoryArena memoryArena = {};
    u64 memoryCapacity = Megabytes(1);
    void *memory = malloc(memoryCapacity);
    InitializeMemoryArena(&memoryArena, memory, memoryCapacity);

    const char *meshName = "bunny.obj";
    MeshData meshData = LoadMesh(meshName, &memoryArena, g_AssetDir);
    TEST_ASSERT_GREATER_THAN_UINT(0, meshData.vertexCount);

    free(memory);
}

void TestUploadCubemapToGPU()
{
    // FIXME: Copied from main.cpp
    glfwSetErrorCallback(GlfwErrorCallback);
    glfwInit();

    u32 windowWidth = 256;
    u32 windowHeight = 256;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(windowWidth,
        windowHeight, "integration tests", NULL, NULL);
    TEST_ASSERT_TRUE(window != NULL);

    // Create Vulkan Renderer
    VulkanRenderer renderer = {};
    VulkanInit(&renderer, window);

    glfwTerminate();
}

// FIXME: Copied from main.cpp
internal DebugLogMessage(LogMessage_)
{
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    puts(buffer);
}

int main(int argc, char **argv)
{
    LogMessage = LogMessage_;
    ParseCommandLineArgs(argc, (const char **)argv, &g_AssetDir);
    LogMessage("Asset directory set to \"%s\".", g_AssetDir);

    RUN_TEST(TestLoadMesh);
    RUN_TEST(TestUploadCubemapToGPU);
    return UNITY_END();
}
