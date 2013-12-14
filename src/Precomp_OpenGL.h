#pragma once

#if defined(__APPLE__)
	#include <OpenGL/OpenGL.h>
#elif defined(UNIX)
	#include <GL/glx.h>
#else // _WINDOWS
	#define GLEW_STATIC
	#include <GL\glew.h>

	#include <Windows.h>
	#include <GL/gl.h>
#endif
