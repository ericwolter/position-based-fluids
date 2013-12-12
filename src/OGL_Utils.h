#pragma once

#ifdef _WINDOWS
#define GLEW_STATIC
#include <GL\glew.h>
#endif

#include <GLFW/glfw3.h>

extern GLuint g_SelectedProgram;

GLuint UniformLoc(const char* szName);
GLuint AttribLoc(const char* szName);

GLuint OGLU_LoadShader(const char* szFilename, unsigned int type);
GLuint OGLU_LoadProgram(const char* vertexFilename, const char* fragmentFilename);
