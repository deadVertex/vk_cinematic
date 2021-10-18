#pragma once

#include <cstdint>

#ifdef _MSC_VER
#define EXPORT_FUNCTION __declspec(dllexport)
#else
#define EXPORT_FUNCTION
#endif

extern "C"
{
    typedef struct
    {
        float *pixels;
        uint32_t width;
        uint32_t height;
    } HdrImage;

    // NOTE: Need to call free() on image->pixels when done
    EXPORT_FUNCTION int LoadExrImage(HdrImage *image, const char *path);
}
