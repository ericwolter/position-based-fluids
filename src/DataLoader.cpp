#include "DataLoader.hpp"

#include <limits.h>

#if defined(_WINDOWS)
  #include <windows.h>
  #include <Shlwapi.h>
  #pragma comment(lib, "shlwapi.lib")
#else
  #include <mach-o/dyld.h>
  #include <libgen.h>
#endif

DataLoader::DataLoader () 
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

DataLoader::~DataLoader() 
{
}

const string DataLoader::getPathForScenario(const string scenario)
{
  return rootDirectory + "/data/scenarios/" + scenario;
}

const string DataLoader::getPathForKernel(const string kernel) 
{
  return rootDirectory + "/data/kernels/" + kernel;
}

const string DataLoader::getPathForShader(const string shader) 
{
  return rootDirectory + "/data/shaders/" + shader;
}

const string DataLoader::getPathForTexture(const string texture) 
{
  return rootDirectory + "/data/textures/" + texture;
}
