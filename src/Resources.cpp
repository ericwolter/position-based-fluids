#include "Resources.hpp"

#include <map>
#include <list>
#include <string>
#include <sstream>
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

//#define USE_INTERNAL_RESOURCES

string rootDirectory;

map<string, string> g_code_resources;

void FindRootDirectory()
{
#if defined(_WINDOWS)
    bool bFolderFound;
    char path[MAX_PATH];
    GetModuleFileName( NULL, path, sizeof(path));
    do
    {
        // Remove to part
        if (!PathRemoveFileSpec(path))
            throw runtime_error("Could not find assets folder");

        // Compose new path
        rootDirectory = path + string("\\assets");

        // Check if exists
        DWORD dwAttrib = GetFileAttributes(rootDirectory.c_str());
        bFolderFound = ((dwAttrib != INVALID_FILE_ATTRIBUTES) && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));

    } while (!bFolderFound);
#else
    char path[PATH_MAX + 1];
    char absolute_path[PATH_MAX + 1];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0)
        realpath(path, absolute_path);

    rootDirectory = dirname(absolute_path);
#endif
}

#ifdef USE_INTERNAL_RESOURCES
void loadCodeResources()
{
    // Done this once
    if (g_code_resources.size() != 0)
        return;

    #include "code_resource.inc"
}
#endif

const string getPathForScenario(const string scenario)
{
    return getRootPath() + "/scenarios/" + scenario;
}

const string getPathForKernel(const string kernel)
{
    return getRootPath() + "/kernels/" + kernel;
}

const string getPathForShader(const string shader)
{
    return getRootPath() + "/shaders/" + shader;
}

const string getPathForTexture(const string texture)
{
    return getRootPath() + "/textures/" + texture;
}

const string getPathForObjects(const string object)
{
    return getRootPath() + "/objects/" + object;
}

const string getRootPath()
{
    if (rootDirectory == "")
        FindRootDirectory();

    return rootDirectory;
}

const string getScenario(const string name)
{
    #ifdef USE_INTERNAL_RESOURCES
        // Make sure resources are loaded
        loadCodeResources();

        // Return resource
        return g_code_resources[name];
    #else
        // Open file
        ifstream ifs(getPathForScenario(name).c_str());
        if ( !ifs.is_open() )
            throw runtime_error("Could not open File!");

        // read content
        return string(istreambuf_iterator<char>(ifs), istreambuf_iterator<char>());
    #endif
}

const string getShaderSource(const string shader)
{
    #ifdef USE_INTERNAL_RESOURCES
        // Make sure resources are loaded
        loadCodeResources();

        // Return resource
        return g_code_resources[shader];
    #else
        // Open file
        ifstream ifs(getPathForShader(shader).c_str());
        if ( !ifs.is_open() )
            throw runtime_error("Could not open File!");

        // read content
        return string(istreambuf_iterator<char>(ifs), istreambuf_iterator<char>());
    #endif
}

const string getKernelSource(const string kernel)
{
    #ifdef USE_INTERNAL_RESOURCES
        // Make sure resources are loaded
        loadCodeResources();

        // Return resource
        return g_code_resources[kernel];
    #else
        // Open file
        ifstream ifs(getPathForKernel(kernel).c_str());
        if ( !ifs.is_open() )
            throw runtime_error("Could not open File!");

        // read content
        return string(istreambuf_iterator<char>(ifs), istreambuf_iterator<char>());
    #endif
}

bool DetectResourceChanges(list<pair<string, time_t> >& fileList)
{
    #ifdef USE_INTERNAL_RESOURCES
        // Return "true" only for the first time
        bool bRet = fileList.size() != 0;
        fileList.clear();
        return bRet;
    #else
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
            {
                cerr << "File is exclusivly open [" << iter->first.c_str() << "]" << endl;
                return false;
            }
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
    #endif
}

