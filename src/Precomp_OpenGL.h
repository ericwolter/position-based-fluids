#pragma once

#if defined(__APPLE__)
    #include <GL/glew.h>
    #include <GLFW/glfw3.h>
#elif defined(UNIX)
    #include <GL/glx.h>
#else // _WINDOWS
    #define GLEW_STATIC
    #include <GL\glew.h>
    #include <Windows.h>
    #include <GL/gl.h>
#endif
