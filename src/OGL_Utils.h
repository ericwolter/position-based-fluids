#pragma once

#include "Precomp_OpenGL.h"
#include <glm/glm.hpp>

#include <list>
#include <string>
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
GLuint OGLU_LoadShader(const string programName, const string shaderCode, unsigned int type);
GLuint OGLU_LoadProgram(const string programName, const string vertexSource, const string fragmentSource);
GLuint OGLU_LoadProgram(const string programName, const string shaderCode, GLuint type);

GLuint OGLU_GenerateTexture(int Width, int Height, GLint InternalFormat, GLenum Type = GL_UNSIGNED_BYTE, void *pPixels = 0);
void OGLU_BindTextureToUniform(const char *szUniform, GLuint nTextureUnit, GLuint nTextureID);
void OGLU_CheckCoreError(const char* szTitle);

//
// Timing related
//
#define TIMING_HISTORY_DEPTH 4

typedef struct
{
    string  sectionName;
    GLuint  queryObject[TIMING_HISTORY_DEPTH];
    GLuint  queryInprogress;
    int     currentQOIndex;
    GLint64 total_time_nano;   
    double  total_time_ms;
    int     tag;
} OGLU_PERFORMANCE_TRACKER;

extern list<OGLU_PERFORMANCE_TRACKER*> g_OGL_Timings;

void OGLU_StartTimingSection(const char* szSectionName, ...);
void OGLU_EndTimingSection();
void OGLU_CollectTimings();

//
// Help classes
//
class CopyTextureToHost
{
public:
    GLint Width;
    GLint Height;
    GLint ExternalFormat;
    GLint InternalFormat;
    GLint ComponentCount;
    GLint BitsPerChannel;
    GLint BytesPerPixel;
    char  DecoderType;

    GLint BufferSize;
    char* Buffer;

    CopyTextureToHost(GLuint textureID);
    ~CopyTextureToHost();

    void SaveToFile(string filename);
    glm::vec4 GetPixel(float x, float y);
};

class CopyBufferToHost
{
public:
    GLint size;

    char*  pBytes;
    float* pFloats;
    int*   pIntegers;

    CopyBufferToHost(GLenum target, GLuint bufferObject);
    ~CopyBufferToHost();
};

// FrameBufferObject class
class FBO
{
public:

    // Info
    int Width;
    int Height;
    int ColorFormat;
    int ColorTargets;

    // Handles
    GLuint ID;
    GLuint pColorTextureId[8];
    GLuint pDepthTextureId;

    // Constructor & Destructor
    FBO();
    FBO(int numOfTargets, bool createDepth, int width, int height, int internalFormat);
    FBO(int target, int textureID);

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
