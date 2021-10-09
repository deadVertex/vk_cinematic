#include <cstdarg>

#include "unity.h"

// For integration tests
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "cpu_ray_tracer.h"

#include "mesh.cpp"

#include "cmdline.cpp"

global const char *g_AssetDir = "../assets";

void setUp(void)
{
    // set stuff up here
}

void tearDown(void)
{
    // clean stuff up here
}

// Integration tests
void TestLoadMesh()
{
    MemoryArena memoryArena = {};
    u64 memoryCapacity = Megabytes(1);
    void *memory = malloc(memoryCapacity);
    InitializeMemoryArena(&memoryArena, memory, memoryCapacity);

    // TODO: This probably needs to be exposed as a commandline arg so we can
    // run our integration tests regardless of working dir.
    const char *assetDir = "../assets";

    const char *meshName = "bunny.obj";
    MeshData meshData = LoadMesh(meshName, &memoryArena, assetDir);
    TEST_ASSERT_GREATER_THAN_UINT(0, meshData.vertexCount);

    free(memory);
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
    return UNITY_END();
}
