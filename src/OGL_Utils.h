#pragma once

#include "Precomp_OpenGL.h"
#include <glm/glm.hpp>

#include <vector>
using namespace std;

#include <GLFW/glfw3.h>
class Mesh;
class FBO;

// Extern members
extern GLuint g_SelectedProgram;
extern Mesh   g_quadMesh;
extern FBO    g_ScreenFBO;

// Uniform/Attribute indexing
GLuint UniformLoc(const char *szName);
GLuint AttribLoc(const char *szName);

// General methods
void OGLU_Init();
void OGLU_RenderQuad(float left, float top, float width, float height);
GLuint OGLU_LoadShader(const string szFilename, unsigned int type);
GLuint OGLU_LoadProgram(const string vertexFilename, const string fragmentFilename);

GLuint OGLU_GenerateTexture(int Width, int Height, GLint InternalFormat, GLenum Format, GLenum Type, void *pPixels);
void OGLU_BindTextureToUniform(const char *szUniform, GLuint nTextureUnit, GLuint nTextureID);

// FrameBufferObject class
class FBO
{
public:

    // Info
    int Width;
    int Height;
    int ColorFormat;
    int ColorTargets;
    float DisplayScale;

    // Handles
    GLuint ID;
    GLuint pColorTextureId[8];
    GLuint pDepthTextureId;

    // Constructor & Destructor
    FBO();
    FBO(int numOfTargets, bool createDepth, int width, int height, float displayScale, int internalFormat);

    ~FBO();

    // Methods
    void SetAsDrawTarget();
};

class Mesh
{
public:
    // Data structures
    vector<glm::vec4> vertices;
    vector<glm::vec3> normals;
    vector<glm::vec2> uvs;
    vector<GLushort>  elements;

    // VBO related
    GLuint VBO_vertices_handle;
    GLuint VBO_elements_handle;
    GLuint VBO_vertex_size;
    GLint VBO_offset_vertex;
    GLint VBO_offset_normal;
    GLint VBO_offset_uv;

    void CreateVBO();
    void Draw(int nInstances);
};
