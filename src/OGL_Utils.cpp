#include "OGL_Utils.h"

#include <fstream>
#include <iostream>
using namespace std;

GLuint g_SelectedProgram;

GLuint UniformLoc(char* szName)
{
	GLuint retVal = glGetUniformLocation(g_SelectedProgram, szName);
	return retVal;
}

GLuint AttribLoc(char* szName)
{
	return glGetAttribLocation(g_SelectedProgram, szName);
}

GLuint OGLU_LoadShader(const char* szFilename, unsigned int type)
{
	// Load shader source
    string shaderCode;
    ifstream ifs(szFilename, ios::binary);
    if ( !ifs )
        throw runtime_error("Could not open file for vertex shader!");
    shaderCode = string( istreambuf_iterator<char>(ifs), istreambuf_iterator<char>() );
    ifs.close();

	// Create the shaders
    GLuint handle = glCreateShader(type);

    // Compile Vertex Shader
    const char* source = shaderCode.c_str();
    glShaderSource(handle, 1, &source , NULL);
    glCompileShader(handle);
	
	// Compilation checking.
	GLint result = 0;
	GLint errorLoglength = 0;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &result);
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &errorLoglength);
	
	// Display message error/warning
	bool ShowWarnings = false;
	if ((errorLoglength > 1) && (ShowWarnings || !result))
    {
		// Report message
        char* errorMsg = new char[errorLoglength + 1];
        glGetShaderInfoLog(handle, errorLoglength, NULL, errorMsg);
        cerr << errorMsg << endl;
        delete[] errorMsg;
    }

	// Handle error
	if (!result)
	{
		glDeleteShader(handle);
		handle = 0;
	}
	
	return handle;
}

GLuint OGLU_LoadProgram(const char* vertexFilename, const char* fragmentFilename)
{
    GLuint programID = 0;

	// Load shaders
	GLuint vertexShaderID   = OGLU_LoadShader(vertexFilename,   GL_VERTEX_SHADER);
	GLuint fragmentShaderID = OGLU_LoadShader(fragmentFilename, GL_FRAGMENT_SHADER);

	// Create Program
    programID = glCreateProgram();
    glAttachShader(programID, vertexShaderID);
    glAttachShader(programID, fragmentShaderID);
    glLinkProgram(programID);

	// Compilation checking.
	GLint result = 0;
	GLint errorLoglength = 0;
    glGetProgramiv(programID, GL_LINK_STATUS, &result);
    glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &errorLoglength);
	
	// Display message error/warning
	bool ShowWarnings = false;
	if ((errorLoglength > 1) && (ShowWarnings || !result))
    {
		// Report message
        char* errorMsg = new char[errorLoglength + 1];
		glGetProgramInfoLog(programID, errorLoglength, NULL, errorMsg);
        cerr << errorMsg << endl;
        delete[] errorMsg;
    }
	
	// Delete shaders
    glDeleteShader(vertexShaderID);
    glDeleteShader(fragmentShaderID);

	// Handle error
	if (!result)
	{
		glDeleteShader(programID);
		programID = 0;
	}

    return programID;
}
