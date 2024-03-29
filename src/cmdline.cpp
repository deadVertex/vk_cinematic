#include <cstring>

internal b32 ParseCommandLineArgs(
    int argc, const char **argv, const char **assetDir)
{
    if (argc >= 3)
    {
        if (strcmp(argv[1], "--asset-dir") == 0)
        {
            *assetDir = argv[2];
            return true;
        }
    }
    return false;
}
