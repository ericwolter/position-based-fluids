#include "OGL_Utils.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
using namespace std;


#define FMT_TABLE_INDEX          0
#define FMT_TABLE_INTFMT         1
#define FMT_TABLE_EXTFMT         2
#define FMT_TABLE_HOSTTYPE       3
#define FMT_TABLE_COMPONENTBITS  4
#define FMT_TABLE_COMPONENTCOUNT 5
#define FMT_TABLE_DECODERTYPE    6
#define FMT_TABLE_ROW_ITEMS      7

GLint IntFmtToFmt[] = {//ID IntFmt                 ExtFmt              HostType                 ComponentSize, NumberOfComponents, DecoderType
                         1,  GL_R8,                 GL_RED,             GL_UNSIGNED_BYTE,        8,  1, 'u',
                         2,  GL_R8_SNORM,           GL_RED,             GL_BYTE,                 8,  1, 'i',
                         3,  GL_R16F,               GL_RED,             GL_HALF_FLOAT,           16, 1, 'f',
                         4,  GL_R32F,               GL_RED,             GL_FLOAT,                32, 1, 'f',
                         5,  GL_R8UI,               GL_RED_INTEGER,     GL_UNSIGNED_BYTE,        8,  1, 'u',
                         6,  GL_R8I,                GL_RED_INTEGER,     GL_BYTE,                 8,  1, 'i',
                         7,  GL_R16UI,              GL_RED_INTEGER,     GL_UNSIGNED_SHORT,       16, 1, 'u',
                         8,  GL_R16I,               GL_RED_INTEGER,     GL_SHORT,                16, 1, 'i',
                         9,  GL_R32UI,              GL_RED_INTEGER,     GL_UNSIGNED_INT,         32, 1, 'u',
                        10,  GL_R32I,               GL_RED_INTEGER,     GL_INT,                  32, 1, 'i',
                        11,  GL_RG8,                GL_RG,              GL_UNSIGNED_BYTE,        8,  2, 'u',
                        12,  GL_RG8_SNORM,          GL_RG,              GL_BYTE,                 8,  2, 'i',
                        13,  GL_RG16F,              GL_RG,              GL_HALF_FLOAT,           16, 2, 'f',
                        14,  GL_RG32F,              GL_RG,              GL_FLOAT,                32, 2, 'f',
                        15,  GL_RG8UI,              GL_RG_INTEGER,      GL_UNSIGNED_BYTE,        8,  2, 'u',
                        16,  GL_RG8I,               GL_RG_INTEGER,      GL_BYTE,                 8,  2, 'i',
                        17,  GL_RG16UI,             GL_RG_INTEGER,      GL_UNSIGNED_SHORT,       16, 2, 'u',
                        18,  GL_RG16I,              GL_RG_INTEGER,      GL_SHORT,                16, 2, 'i',
                        19,  GL_RG32UI,             GL_RG_INTEGER,      GL_UNSIGNED_INT,         32, 2, 'u',
                        20,  GL_RG32I,              GL_RG_INTEGER,      GL_INT,                  32, 2, 'i',
                        21,  GL_RGB8,               GL_RGB,             GL_UNSIGNED_BYTE,        8,  3, 'u',
                        22,  GL_SRGB8,              GL_RGB,             GL_UNSIGNED_BYTE,        8,  3, 'u',
                        23,  GL_RGB8_SNORM,         GL_RGB,             GL_BYTE,                 8,  3, 'i',
                        24,  GL_RGB16F,             GL_RGB,             GL_HALF_FLOAT,           16, 3, 'f',
                        25,  GL_RGB32F,             GL_RGB,             GL_FLOAT,                32, 3, 'f',
                        26,  GL_RGB8UI,             GL_RGB_INTEGER,     GL_UNSIGNED_BYTE,        8,  3, 'u',
                        27,  GL_RGB8I,              GL_RGB_INTEGER,     GL_BYTE,                 8,  3, 'i',
                        28,  GL_RGB16UI,            GL_RGB_INTEGER,     GL_UNSIGNED_SHORT,       16, 3, 'u',
                        29,  GL_RGB16I,             GL_RGB_INTEGER,     GL_SHORT,                16, 3, 'i',
                        30,  GL_RGB32UI,            GL_RGB_INTEGER,     GL_UNSIGNED_INT,         32, 3, 'u',
                        31,  GL_RGB32I,             GL_RGB_INTEGER,     GL_INT,                  32, 3, 'i',
                        32,  GL_RGBA8,              GL_RGBA,            GL_UNSIGNED_BYTE,        8,  4, 'u',
                        33,  GL_SRGB8_ALPHA8,       GL_RGBA,            GL_UNSIGNED_BYTE,        8,  4, 'u',
                        34,  GL_RGBA8_SNORM,        GL_RGBA,            GL_BYTE,                 8,  4, 'i',
                        35,  GL_RGBA16F,            GL_RGBA,            GL_HALF_FLOAT,           16, 4, 'f',
                        36,  GL_RGBA32F,            GL_RGBA,            GL_FLOAT,                32, 4, 'f',
                        37,  GL_RGBA8UI,            GL_RGBA_INTEGER,    GL_UNSIGNED_BYTE,        8,  4, 'u',
                        38,  GL_RGBA8I,             GL_RGBA_INTEGER,    GL_BYTE,                 8,  4, 'i',
                        39,  GL_RGBA16UI,           GL_RGBA_INTEGER,    GL_UNSIGNED_SHORT,       16, 4, 'u',
                        40,  GL_RGBA16I,            GL_RGBA_INTEGER,    GL_SHORT,                16, 4, 'i',
                        41,  GL_RGBA32I,            GL_RGBA_INTEGER,    GL_INT,                  32, 4, 'i',
                        42,  GL_RGBA32UI,           GL_RGBA_INTEGER,    GL_UNSIGNED_INT,         32, 4, 'u',
                        43,  GL_DEPTH_COMPONENT,    GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,         24, 1, 'u',
                        44,  GL_DEPTH_COMPONENT16,  GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,       16, 1, 'u',
                        45,  GL_DEPTH_COMPONENT24,  GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,         24, 1, 'u',
                        46,  GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT,                32, 1, 'f',
                        0,   0,                     0,                  0,                       0,  0, ' '
                      };

FBO  g_ScreenFBO;
Mesh g_quadMesh;

GLuint g_SelectedProgram;
GLuint g_CurrentViewPort[4];

GLuint UniformLoc(const char *szName)
{
    GLuint retVal = glGetUniformLocation(g_SelectedProgram, szName);
    return retVal;
}

GLuint AttribLoc(const char *szName)
{
    return glGetAttribLocation(g_SelectedProgram, szName);
}

void CreateQuad()
{
    // Vertices
    g_quadMesh.vertices.push_back(glm::vec4( 1.0,  1.0,  0.0,  0.0)); // tr
    g_quadMesh.vertices.push_back(glm::vec4(-0.0,  1.0,  0.0,  0.0)); // tl
    g_quadMesh.vertices.push_back(glm::vec4(-0.0, -0.0,  0.0,  0.0)); // bl
    g_quadMesh.vertices.push_back(glm::vec4( 1.0, -0.0,  0.0,  0.0)); // br

    // UVs
    g_quadMesh.uvs.push_back(glm::vec2(1.0, 1.0));  // tr
    g_quadMesh.uvs.push_back(glm::vec2(0.0, 1.0));  // tl
    g_quadMesh.uvs.push_back(glm::vec2(0.0, 0.0));  // bl
    g_quadMesh.uvs.push_back(glm::vec2(1.0, 0.0));  // br

    // indices
    g_quadMesh.elements.push_back( 0 );
    g_quadMesh.elements.push_back( 1 );
    g_quadMesh.elements.push_back( 2 );
    g_quadMesh.elements.push_back( 2 );
    g_quadMesh.elements.push_back( 3 );
    g_quadMesh.elements.push_back( 0 );

    // Create VBO
    g_quadMesh.CreateVBO();
}

void OGLU_Init()
{
    // Setup ScreenFBO
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    g_ScreenFBO.Width  = vp[2];
    g_ScreenFBO.Height = vp[3];

    // Setup quad gemoetry
    CreateQuad();
}

GLuint OGLU_LoadShader(const string programName, const string shaderCode, unsigned int type)
{
    // Create the shaders
    GLuint handle = glCreateShader(type);

    // Compile Vertex Shader
    const char *source = shaderCode.c_str();
    glShaderSource(handle, 1, &source , NULL);
    glCompileShader(handle);

    // Compilation checking.
    GLint result = 0;
    GLint errorLoglength = 0;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &result);
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &errorLoglength);

    // Display message error/warning
    bool ShowWarnings = true;
    if ((errorLoglength > 1) && (ShowWarnings || !result))
    {
        // Report message
        char *errorMsg = new char[errorLoglength + 1];
        glGetShaderInfoLog(handle, errorLoglength, NULL, errorMsg);
        cerr << "Shader compile error: " << programName.c_str() << endl;
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

GLuint OGLU_LoadProgram(const string programName, const string shaderCode, GLuint type)
{
    GLuint programID = 0;

    // Load shaders
    GLuint shaderID   = OGLU_LoadShader(programName, shaderCode, type);

    // Create Program
    programID = glCreateProgram();
    glAttachShader(programID, shaderID);
    glLinkProgram(programID);

    // Compilation checking.
    GLint result = 0;
    GLint errorLoglength = 0;
    glGetProgramiv(programID, GL_LINK_STATUS, &result);
    glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &errorLoglength);

    // Display message error/warning
    bool ShowWarnings = true;
    if ((errorLoglength > 1) && (ShowWarnings || !result))
    {
        // Report message
        char *errorMsg = new char[errorLoglength + 1];
        glGetProgramInfoLog(programID, errorLoglength, NULL, errorMsg);
        cerr << "Shader compile " << (result ? "warning(s)" : "error(s)") << ": " << programName.c_str() << endl;
        cerr << errorMsg << endl;
        delete[] errorMsg;
    }

    // Delete shaders
    glDeleteShader(shaderID);

    // Handle error
    if (!result)
    {
        glDeleteShader(programID);
        programID = 0;
    }

    return programID;
}
GLuint OGLU_LoadProgram(const string programName, const string vertexSource, const string fragmentSource)
{
    GLuint programID = 0;

    // Load shaders
    GLuint vertexShaderID   = OGLU_LoadShader(programName, vertexSource,   GL_VERTEX_SHADER);
    GLuint fragmentShaderID = OGLU_LoadShader(programName, fragmentSource, GL_FRAGMENT_SHADER);

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
    bool ShowWarnings = true;
    if ((errorLoglength > 1) && (ShowWarnings || !result))
    {
        // Report message
        char *errorMsg = new char[errorLoglength + 1];
        glGetProgramInfoLog(programID, errorLoglength, NULL, errorMsg);
        cerr << "Shader compile " << (result ? "warning(s)" : "error(s)") << ": " << programName.c_str() << endl;
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

void OGLU_RenderQuad(float left, float top, float width, float height)
{
    // Define transform matrix according to required position
    glm::mat4 transform = glm::scale(glm::translate(glm::mat4(), glm::vec3(left, 1.0 - height - top, 0.0)), glm::vec3(width, height, 0.0));
    glUniformMatrix4fv(UniformLoc("projectionMatrix"), 1, GL_FALSE, glm::value_ptr(glm::ortho<float>(0, 1, 0, 1)));
    glUniformMatrix4fv(UniformLoc("modelViewMatrix"),  1, GL_FALSE, glm::value_ptr(transform));

    // disable depth test
    glDisable(GL_DEPTH_TEST);

    // Actual quad drawing
    g_quadMesh.Draw(1);

    // disable depth test
    glEnable(GL_DEPTH_TEST);
}

GLuint OGLU_GenerateTexture(int Width, int Height, GLint InternalFormat, GLenum Type, void *pPixels)
{
    // Generate and bind texture
    GLuint ret;
    glGenTextures(1, &ret);

    // Get pixel format
    int iFmt = 0;
    while ((IntFmtToFmt[iFmt + FMT_TABLE_INTFMT] != 0) && (IntFmtToFmt[iFmt + FMT_TABLE_INTFMT] != InternalFormat)) iFmt += FMT_TABLE_ROW_ITEMS;
    int colorFormat = IntFmtToFmt[iFmt + FMT_TABLE_EXTFMT];

    // Setup image
    glBindTexture(GL_TEXTURE_2D, ret);
    glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, Width, Height, 0, colorFormat, Type, pPixels);

    // Change default parameters to WARP=CLAMP FILTER=NEAREST
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    return ret;
}

void OGLU_BindTextureToUniform(const char *szUniform, GLuint nTextureUnit, GLuint nTextureID)
{
    glUniform1i(UniformLoc(szUniform), nTextureUnit);
    glActiveTexture(GL_TEXTURE0 + nTextureUnit);
    glBindTexture(GL_TEXTURE_2D, nTextureID);
}

void OGLU_CheckCoreError(const char* szTitle)
{
    int err=0;
    char msg[256];
    while( (err=glGetError())!=0 )
    {
        sprintf(msg, "OpenGL error 0x%x @ %s\n", err, szTitle);
        fputs(msg, stderr);
    }
}

CopyTextureToHost::CopyTextureToHost(GLuint textureID) : 
    Width(0), 
    Height(0), 
    ExternalFormat(0), 
    InternalFormat(0), 
    ComponentCount(0),
    BitsPerChannel(0),
    DecoderType(' '),
    BufferSize(0), 
    Buffer(NULL)
{
    // Make sure we don't have errors to begin with
    OGLU_CheckCoreError("CopyTextureToHost(start)");

    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID); 

    // Read parameters
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &Width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &Height);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &InternalFormat);


    // Find pixel format
    int iFmt = 0;
    while ((IntFmtToFmt[iFmt + FMT_TABLE_INTFMT] != 0) && (IntFmtToFmt[iFmt + FMT_TABLE_INTFMT] != InternalFormat)) iFmt += FMT_TABLE_ROW_ITEMS;
    ExternalFormat = IntFmtToFmt[iFmt + FMT_TABLE_EXTFMT];

    // Make sure format was found
    if (ExternalFormat == 0)
    {
        cout << "Unrecognized internal texture format\n";
        return ;
    }

    // Compute size
    DecoderType    = IntFmtToFmt[iFmt + FMT_TABLE_DECODERTYPE];
    ComponentCount = IntFmtToFmt[iFmt + FMT_TABLE_COMPONENTCOUNT];
    BitsPerChannel = IntFmtToFmt[iFmt + FMT_TABLE_COMPONENTBITS];
    int HostType   = IntFmtToFmt[iFmt + FMT_TABLE_HOSTTYPE];

    // Workaround for depth reading
    if (InternalFormat == GL_DEPTH_COMPONENT)
    {
        HostType       = GL_FLOAT;
        BitsPerChannel = 32;
    }

    // Allocate buffer
    BytesPerPixel = (BitsPerChannel * ComponentCount) / 8;
    BufferSize = Width * Height * BytesPerPixel;
    Buffer = new char[BufferSize];

    // Read texture
    glGetTexImage(GL_TEXTURE_2D, 0, ExternalFormat, HostType, Buffer);

    // Make sure we didn't fuck up
    OGLU_CheckCoreError("CopyTextureToHost(end)");
}

CopyTextureToHost::~CopyTextureToHost()
{
    delete[] Buffer;
}

void CopyTextureToHost::SaveToFile(string filename)
{
    // Write to file
    ofstream f(filename.c_str(), ios::out | ios::trunc | ios::binary);
    f.seekp(0);
    f.write((char*)Buffer, BufferSize);
    f.close();
}

glm::vec4 CopyTextureToHost::GetPixel(float x, float y)
{
    // Read pixel
    int ix = (int)(x * Width);
    int iy = (int)((1-y) * Height);
    int offset = (Width * iy + ix) * BytesPerPixel;

    // Translate pixels
    glm::vec4 ret;
    for (int iComp = 0; iComp < ComponentCount; iComp++)
    {
        float value = FLT_MAX;
        switch (DecoderType)
        {
            case 'i': 
                switch (BitsPerChannel)
                {
                    case 8:  value = (float)((GLbyte*)  Buffer) [(offset + iComp * BitsPerChannel / 8) / 1]; break;
                    case 16: value = (float)((GLshort*) Buffer) [(offset + iComp * BitsPerChannel / 8) / 2]; break;
                    case 32: value = (float)((GLint*)   Buffer) [(offset + iComp * BitsPerChannel / 8) / 4]; break;
                }

            case 'u': 
                switch (BitsPerChannel)
                {
                    case 8:  value = (float)((GLubyte*)  Buffer) [(offset + iComp * BitsPerChannel / 8) / 1]; break;
                    case 16: value = (float)((GLushort*) Buffer) [(offset + iComp * BitsPerChannel / 8) / 2]; break;
                    case 32: value = (float)((GLuint*)   Buffer) [(offset + iComp * BitsPerChannel / 8) / 4]; break;
                }

            case 'f': 
                switch (BitsPerChannel)
                {
                    case 32: value = ((float*) Buffer) [(offset + iComp * BitsPerChannel / 8) / 4]; break;
                }
        }

        ret[iComp] = value;
    }

    // return pixel
    return ret;
}

FBO::FBO()
{
    Width           = 0;
    Height          = 0;
    ColorFormat     = 0;
    ColorTargets    = 0;
    ID              = 0;
    pDepthTextureId = 0;
    memset(pColorTextureId, 0, sizeof(pColorTextureId));
}

FBO::FBO(int numOfTargets, bool createDepth, int width, int height, int internalFormat)
{
    // Setup FBOInfo
    Width        = width;
    Height       = height;
    ColorFormat  = internalFormat;
    ColorTargets = numOfTargets;

    // create a framebuffer object
    glGenFramebuffers(1, &ID);
    glBindFramebuffer(GL_FRAMEBUFFER, ID);

    // Create colorTextureId
    for (int i = 0; i < numOfTargets; i++)
    {
        // Create colorTextureId
        pColorTextureId[i] = OGLU_GenerateTexture(Width, Height, internalFormat);

        // attach the texture to FBO depth attachment point
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, pColorTextureId[i], 0);
    }

    if (createDepth)
    {
        // create depth texture
        pDepthTextureId = OGLU_GenerateTexture(Width, Height, GL_DEPTH_COMPONENT);

        // attach the texture to FBO depth attachment point
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, pDepthTextureId, 0);
    }

    // check FBO status
    GLenum FBOstatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (FBOstatus != GL_FRAMEBUFFER_COMPLETE)
        cout << "GL_FRAMEBUFFER_COMPLETE failed, CANNOT use FBO\n";

    // switch back to window-system-provided framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

FBO::FBO(int target, int textureID)
{
    // Setup FBOInfo
    glBindTexture(target, textureID);
    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, &Width);
    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, &Height);
    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_INTERNAL_FORMAT, &ColorFormat);
    ColorTargets = 1;
    pColorTextureId[0] = textureID;

    // create a framebuffer object
    glGenFramebuffers(1, &ID);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ID);

    // attach the texture to FBO depth attachment point
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, textureID, 0);

    // check FBO status
    GLenum FBOstatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (FBOstatus != GL_FRAMEBUFFER_COMPLETE)
        cout << "GL_FRAMEBUFFER_COMPLETE failed, CANNOT use FBO\n";

    // switch back to window-system-provided framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

FBO::~FBO()
{
    // Release depth texture
    if (pDepthTextureId != 0)
        glDeleteTextures(1, &pDepthTextureId);

    // Release color textures
    for (int i = 0; i < ColorTargets; i++)
        glDeleteTextures(1, &pColorTextureId[i]);

    // Release FBO
    glDeleteFramebuffers(1, &ID);
}

void FBO::SetAsDrawTarget()
{
    // Bind to FBO
    glBindFramebuffer(GL_FRAMEBUFFER, ID);

    // Set the viewport for the OpenGL window
    glViewport(0, 0, Width, Height);

    // Setup MTR referances
    if (ColorTargets != 0)
    {
        GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,  GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5};
        glDrawBuffers(ColorTargets, buffers);
    }
}

void Mesh::CreateVBO()
{
    // Compute VBO object size and offsets
    VBO_vertex_size = 0;
    VBO_offset_vertex = VBO_offset_normal = VBO_offset_uv = -1;
    if (vertices.size() != 0)
    {
        VBO_offset_vertex =  VBO_vertex_size;    // vertex
        VBO_vertex_size += 4 * sizeof(GLfloat);
    }
    if (normals.size()  != 0)
    {
        VBO_offset_normal =  VBO_vertex_size;    // normal
        VBO_vertex_size += 3 * sizeof(GLfloat);
    }
    if (uvs.size()      != 0)
    {
        VBO_offset_uv     =  VBO_vertex_size;    // uv
        VBO_vertex_size += 2 * sizeof(GLfloat);
    }

    // Build VBO buffer
    int vertexCount = vertices.size();
    char *pBuf = new char[VBO_vertex_size * vertexCount];
    for (int i = 0; i < vertexCount; i++)
    {
        char *pVertex = pBuf + i * VBO_vertex_size;
        if (VBO_offset_vertex != -1) memcpy(pVertex + VBO_offset_vertex, glm::value_ptr(vertices[i]), 4 * sizeof(GLfloat));
        if (VBO_offset_normal != -1) memcpy(pVertex + VBO_offset_normal, glm::value_ptr(normals[i]),  3 * sizeof(GLfloat));
        if (VBO_offset_uv     != -1) memcpy(pVertex + VBO_offset_uv,     glm::value_ptr(uvs[i]),      2 * sizeof(GLfloat));
    }

    // Create and assign vertex VBO
    glGenBuffers(1, &VBO_vertices_handle);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_vertices_handle);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * VBO_vertex_size, pBuf, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Free VBO (after it was copied)
    delete[] pBuf;

    // Create and assign index VBO
    glGenBuffers(1, &VBO_elements_handle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBO_elements_handle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements.size() * sizeof(elements[0]), &elements[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Mesh::Draw(int nInstances)
{
    // Bind VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO_vertices_handle);

    // Setup buffer points
    GLint vertex_attrib = AttribLoc("position");
    GLint normal_attrib = AttribLoc("normal");
    GLint uv_attrib     = AttribLoc("uv");
    if ((VBO_offset_vertex != -1) && (vertex_attrib != -1))
    {
        glEnableVertexAttribArray(vertex_attrib);
        glVertexAttribPointer(vertex_attrib, 4, GL_FLOAT, GL_FALSE, VBO_vertex_size, (void *) (intptr_t)VBO_offset_vertex);
    }

    if ((VBO_offset_normal != -1) && (normal_attrib != -1))
    {
        glEnableVertexAttribArray(normal_attrib);
        glVertexAttribPointer(normal_attrib, 3, GL_FLOAT, GL_FALSE, VBO_vertex_size, (void *) (intptr_t)VBO_offset_normal);
    }

    if ((VBO_offset_uv != -1) && (uv_attrib != -1))
    {
        glEnableVertexAttribArray(uv_attrib);
        glVertexAttribPointer(uv_attrib, 2, GL_FLOAT, GL_FALSE, VBO_vertex_size, (void *) (intptr_t)VBO_offset_uv);
    }

    // Draw things
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBO_elements_handle);
    glDrawElementsInstanced(GL_TRIANGLES, elements.size(), GL_UNSIGNED_SHORT, NULL, nInstances);

    // Disable vertex attributes
    if (vertex_attrib != -1) glDisableVertexAttribArray(vertex_attrib);
    if (normal_attrib != -1) glDisableVertexAttribArray(normal_attrib);
    if (uv_attrib     != -1) glDisableVertexAttribArray(uv_attrib);
}

