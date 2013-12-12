#pragma once

#ifdef _WINDOWS
#define GLEW_STATIC
#include <GL\glew.h>
#endif

extern GLuint g_SelectedProgram;

GLuint UniformLoc(char* szName);
GLuint AttribLoc(char* szName);

GLuint OGLU_LoadShader(const char* szFilename, unsigned int type);
GLuint OGLU_LoadProgram(const char* vertexFilename, const char* fragmentFilename);
