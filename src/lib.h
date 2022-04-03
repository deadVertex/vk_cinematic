#pragma once

#include "platform.h"
#include "math_lib.h"
#include "scene.h"

// TODO: Move to platform.h?
#ifdef _MSC_VER
#define EXPORT_FUNCTION __declspec(dllexport)
#else
#define EXPORT_FUNCTION
#endif

extern "C"
{
    EXPORT_FUNCTION void GenerateScene(Scene *scene);
}
