#include "Resources.hpp"

#include <list>
#include <string>
#include <fstream>
#include <iostream>
using namespace std;

#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#if defined(_WINDOWS)
#include <windows.h>
#include <Shlwapi.h>
#include <io.h>
#pragma comment(lib, "shlwapi.lib")
#else
#include <stdlib.h>
#include <mach-o/dyld.h>
#include <libgen.h>
#include <unistd.h>
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

bool DetectResourceChanges(list<pair<string, time_t> >& fileList)
{
    // Assume nothing change
    bool bChanged = false;

    // Scan all file for change
    list<pair<string, time_t> >::iterator iter;
    for (iter = fileList.begin(); iter != fileList.end(); iter++)
    {
        // Get file stat
        struct stat fileStat;
        if (stat(iter->first.c_str(), &fileStat) != 0)
            cerr << "Unable to locate " << iter->first.c_str() << endl;

        // Compare for change with stored stat
        if (iter->second != fileStat.st_mtime)
        {
            bChanged = true;
            break;
        }
    }

    // Exit if not change was detected
    if (!bChanged)
        return false;

    // Test exclusive lock code
    // ifstream ifs = ifstream(mFilesTrack.begin()->first.c_str());

    // change was detected: make sure that all files are openable (no one is holding the files open)
    for (iter = fileList.begin(); iter != fileList.end(); iter++)
    {
        // Check if file can be open exclusivly
        bool ExclusiveOpen;
#ifdef _WINDOWS
        int fd;
        int openResult = _sopen_s(&fd, iter->first.c_str(), O_RDWR, _SH_DENYRW, 0);
        if (openResult == 0) _close(fd);
        ExclusiveOpen = openResult == 0;
#else
        int openResult = open (iter->first.c_str(), O_RDWR);
        int lockResult = openResult < 0 ? -1 : flock (openResult, LOCK_EX | LOCK_NB);
        ExclusiveOpen = lockResult == 0;
        if (lockResult == 0) flock (openResult, LOCK_UN);
        if (openResult > 0) close(openResult);
#endif

        // If failed to open file exclusivly, exit (we will try again later)
        if (!ExclusiveOpen)
            return false;
    }

    // There was a change AND all files are openable (in exclusive mode, ie: no one else accessing them) we can report the change back
    for (iter = fileList.begin(); iter != fileList.end(); iter++)
    {
        // Get file stat
        struct stat fileStat;
        stat(iter->first.c_str(), &fileStat);
        iter->second = fileStat.st_mtime;
    }

    return true;
}

