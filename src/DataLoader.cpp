#include "DataLoader.hpp"

#include <limits.h>

#if defined(_WINDOWS)
#include <windows.h>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#else
#include <stdlib.h>
#include <mach-o/dyld.h>
#include <libgen.h>
#endif

string rootDirectory;

void FindRootDirectory()
{
#if defined(_WINDOWS)
    char path[MAX_PATH];
    GetModuleFileName( NULL, path, sizeof(path));
    PathRemoveFileSpec(path);
    rootDirectory = path;
    rootDirectory += "\\..";
#else
    char path[PATH_MAX + 1];
    char absolute_path[PATH_MAX + 1];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0)
        realpath(path, absolute_path);

    rootDirectory = dirname(absolute_path);
#endif
}

const string getPathForScenario(const string scenario)
{
    if (rootDirectory == "")
        FindRootDirectory();

    return rootDirectory + "/data/scenarios/" + scenario;
}

const string getPathForKernel(const string kernel)
{
    if (rootDirectory == "")
        FindRootDirectory();

    return rootDirectory + "/data/kernels/" + kernel;
}

const string getPathForShader(const string shader)
{
    if (rootDirectory == "")
        FindRootDirectory();

    return rootDirectory + "/data/shaders/" + shader;
}

const string getPathForTexture(const string texture)
{
    if (rootDirectory == "")
        FindRootDirectory();

    return rootDirectory + "/data/textures/" + texture;
}
