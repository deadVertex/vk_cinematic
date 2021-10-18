#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_OPENMP 0
#pragma warning(push)
#pragma warning(disable : 4018)
#pragma warning(disable : 4389)
#pragma warning(disable : 4706)
#include "tinyexr.h"
#pragma warning(pop)

#include "asset_loader.h"

int LoadExrImage(HdrImage *image, const char *path)
{
    int result = 0;

    float *out;
    int width;
    int height;
    const char *err = NULL;

    int ret = LoadEXR(&out, &width, &height, path, &err);

    if (ret != TINYEXR_SUCCESS)
    {
        if (err)
        {
            // TODO: Return error message
            fprintf(stderr, "ERR : %s\n", err);
            FreeEXRErrorMessage(err); // release memory of error message.
        }
        result = 1;
    }
    else
    {
        image->pixels = out;
        image->width = width;
        image->height = height;
        result = 0;
    }

    return result;
}
