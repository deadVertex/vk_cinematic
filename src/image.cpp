#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_OPENMP 0
#pragma warning(push)
#pragma warning(disable : 4018)
#pragma warning(disable : 4389)
#pragma warning(disable : 4706)
#include "tinyexr.h"
#pragma warning(pop)

internal HdrImage LoadExrImage(const char *path, MemoryArena *arena)
{
    HdrImage result = {};

    float *out; // width * height * RGBA
    int width;
    int height;
    const char *err = NULL; // or nullptr in C++11

    int ret = LoadEXR(&out, &width, &height, path, &err);

    if (ret != TINYEXR_SUCCESS)
    {
        if (err)
        {
            fprintf(stderr, "ERR : %s\n", err);
            FreeEXRErrorMessage(err); // release memory of error message.
        }
    }
    else
    {
        result.pixels = AllocateArray(arena, f32, 4 * width * height);
        CopyMemory(result.pixels, out, sizeof(f32) * 4 * width * height);
        result.width = width;
        result.height = height;
        free(out); // release memory of image data
    }

#if 0
    // 1. Read EXR version.
    EXRVersion exr_version = {};

    int ret = ParseEXRVersionFromFile(&exr_version, path);
    if (ret == 0)
    {
        if (!exr_version.multipart)
        {
            // 2. Read EXR header
            EXRHeader exr_header = {};
            InitEXRHeader(&exr_header);

            const char *err = NULL;
            ret = ParseEXRHeaderFromFile(&exr_header, &exr_version, path, &err);
            if (ret == 0)
            {
                EXRImage exr_image = {};
                InitEXRImage(&exr_image);

                ret = LoadEXRImageFromFile(&exr_image, &exr_header, path, &err);
                if (ret == 0)
                {
                    // FIXME: Only support RGB images for now
                    Assert(exr_image.num_channels == 3);

                    u32 width = exr_image.width;
                    u32 height = exr_image.height;
                    u32 bytesPerPixel = 3;

                    result.pixels = AllocateArray(
                        arena, f32, width * height * bytesPerPixel);
                    result.width = width;
                    result.height = height;

                    for (u32 y = 0; y < height; ++y)
                    {
                        for (u32 x = 0; x < width; ++x)
                        {
                            u32 offset =
                                y * width * bytesPerPixel + x * bytesPerPixel;

                            f32 *dst = result.pixels + offset;

                            f32 *srcR = ((f32*)exr_image.images[0]) + offset;
                            f32 *srcG = ((f32*)exr_image.images[1]) + offset;
                            f32 *srcB = ((f32*)exr_image.images[2]) + offset;

                            dst[0] = *srcR;
                            dst[1] = *srcG;
                            dst[2] = *srcB;
                        }
                    }

                    FreeEXRImage(&exr_image);
                }
                else
                {
                    LogMessage("Load EXR err: %s", err);
                    FreeEXRErrorMessage(err);
                }
                FreeEXRHeader(&exr_header);
            }
            else
            {
                LogMessage("Parse EXR err: %s", err);
                FreeEXRErrorMessage(err); // free's buffer for an error message
            }
        }
        else
        {
            LogMessage("Multipart images are not supported");
        }

    }
    else
    {
        LogMessage("Invalid EXR file: %s", path);
    }
#endif

    return result;
}
