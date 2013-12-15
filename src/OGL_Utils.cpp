#include "OGL_Utils.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
using namespace std;

GLint IntFmtToFmt[] = {GL_R8,           GL_RED,
                        GL_R8_SNORM,     GL_RED,
                        GL_R16,          GL_RED,
                        GL_R16_SNORM,    GL_RED,
                        GL_RG8,          GL_RG,
                        GL_RG8_SNORM,    GL_RG,
                        GL_RG16,         GL_RG,
                        GL_RG16_SNORM,   GL_RG,
                        GL_R3_G3_B2,     GL_RGB,
                        GL_RGB4,         GL_RGB,
                        GL_RGB5,         GL_RGB,
                        GL_RGB8,         GL_RGB,
                        GL_RGB8_SNORM,   GL_RGB,
                        GL_RGB10,        GL_RGB,
                        GL_RGB12,        GL_RGB,
                        GL_RGB16_SNORM,  GL_RGB,
                        GL_RGBA2,        GL_RGB,
                        GL_RGBA4,        GL_RGB,
                        GL_RGB5_A1,      GL_RGBA,
                        GL_RGBA8,        GL_RGBA,
                        GL_RGBA8_SNORM,  GL_RGBA,
                        GL_RGB10_A2,     GL_RGBA,
                        GL_RGB10_A2UI,   GL_RGBA,
                        GL_RGBA12,       GL_RGBA,
                        GL_RGBA16,       GL_RGBA,
                        GL_SRGB8,        GL_RGB,
                        GL_SRGB8_ALPHA8, GL_RGBA,
                        GL_R16F,         GL_RED,
                        GL_RG16F,        GL_RG,
                        GL_RGB16F,       GL_RGB,
                        GL_RGBA16F,      GL_RGBA,
                        GL_R32F,         GL_RED,
                        GL_RG32F,        GL_RG,
                        GL_RGB32F,       GL_RGB,
                        GL_RGBA32F,      GL_RGBA,
                        GL_RGB9_E5,      GL_RGB,
                        GL_R8I,          GL_RED,
                        GL_R8UI,         GL_RED,
                        GL_R16I,         GL_RED,
                        GL_R16UI,        GL_RED,
                        GL_R32I,         GL_RED,
                        GL_R32UI,        GL_RED,
                        GL_RG8I,         GL_RG,
                        GL_RG8UI,        GL_RG,
                        GL_RG16I,        GL_RG,
                        GL_RG16UI,       GL_RG,
                        GL_RG32I,        GL_RG,
                        GL_RG32UI,       GL_RG,
                        GL_RGB8I,        GL_RGB,
                        GL_RGB8UI,       GL_RGB,
                        GL_RGB16I,       GL_RGB,
                        GL_RGB16UI,      GL_RGB,
                        GL_RGB32I,       GL_RGB,
                        GL_RGB32UI,      GL_RGB,
                        GL_RGBA8I,       GL_RGBA,
                        GL_RGBA8UI,      GL_RGBA,
                        GL_RGBA16I,      GL_RGBA,
                        GL_RGBA16UI,     GL_RGBA,
                        GL_RGBA32I,      GL_RGBA,
                        GL_RGBA32UI,     GL_RGBA, 
                        0,               0};

FBO  g_ScreenFBO;
Mesh g_quadMesh;

GLuint g_SelectedProgram;
GLuint g_CurrentViewPort[4];

GLuint UniformLoc(const char* szName)
{
    GLuint retVal = glGetUniformLocation(g_SelectedProgram, szName);
    return retVal;
}

GLuint AttribLoc(const char* szName)
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

GLuint OGLU_LoadShader(const string szFilename, unsigned int type)
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
    bool ShowWarnings = true;
    if ((errorLoglength > 1) && (ShowWarnings || !result))
    {
        // Report message
        char* errorMsg = new char[errorLoglength + 1];
        glGetShaderInfoLog(handle, errorLoglength, NULL, errorMsg);
        cerr << "Shader compile error: " << szFilename.c_str() << endl;
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

GLuint OGLU_LoadProgram(const string vertexFilename, const string fragmentFilename)
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
    bool ShowWarnings = true;
    if ((errorLoglength > 1) && (ShowWarnings || !result))
    {
        // Report message
        char* errorMsg = new char[errorLoglength + 1];
        glGetProgramInfoLog(programID, errorLoglength, NULL, errorMsg);
        cerr << "Shader compile error: " << vertexFilename.c_str() << " and " << fragmentFilename.c_str() << endl;
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

GLuint OGLU_GenerateTexture(int Width, int Height, GLint InternalFormat, GLenum Format, GLenum Type, void* pPixels)
{
    // Generate and bind texture
    GLuint ret;
    glGenTextures(1, &ret);
    
    // Setup image
    glBindTexture(GL_TEXTURE_2D, ret);
    glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, Width, Height, 0, Format, Type, pPixels);

    // Change default parameters to WARP=CLAMP FILTER=NEAREST
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    
    return ret;
}

void OGLU_BindTextureToUniform(const char* szUniform, GLuint nTextureUnit, GLuint nTextureID)
{
    glUniform1i(UniformLoc(szUniform), nTextureUnit);
    glActiveTexture(GL_TEXTURE0 + nTextureUnit); 
    glBindTexture(GL_TEXTURE_2D, nTextureID);
}

FBO::FBO()
{
	Width           = 0;
	Height          = 0;
	DisplayScale    = 0;
	ColorFormat     = 0;
	ColorTargets    = 0;
	ID              = 0;
	pDepthTextureId = 0;
	memset(pColorTextureId, 0, sizeof(pColorTextureId));
}

FBO::FBO(int numOfTargets, bool createDepth, int width, int height, float displayScale, int internalFormat)
{
    // Setup FBOInfo
    Width        = width * displayScale;
    Height       = height * displayScale;
    DisplayScale = displayScale;
    ColorFormat  = internalFormat;
    ColorTargets = numOfTargets;

    // Get pixel format
    int iFmt = 0;
    while ((IntFmtToFmt[iFmt] != 0) && (IntFmtToFmt[iFmt] != internalFormat)) iFmt += 2;
    int ColorFormat = IntFmtToFmt[iFmt + 1];

    // create a framebuffer object
    glGenFramebuffersEXT(1, &ID);
    glBindFramebuffer(GL_FRAMEBUFFER, ID);

    // Create colorTextureId 
    for (int i = 0; i < numOfTargets; i++)
    {
        // Create colorTextureId 
        pColorTextureId[i] = OGLU_GenerateTexture(Width, Height, internalFormat, ColorFormat, GL_UNSIGNED_BYTE, 0);

        // attach the texture to FBO depth attachment point
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, pColorTextureId[i], 0);
    }

    if (createDepth)
    {
        // create depth texture
        pDepthTextureId = OGLU_GenerateTexture(Width, Height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);

        // attach the texture to FBO depth attachment point
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, pDepthTextureId, 0);
    }
    
    // check FBO status
    GLenum FBOstatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(FBOstatus != GL_FRAMEBUFFER_COMPLETE)
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
    if (vertices.size() != 0) { VBO_offset_vertex =  VBO_vertex_size; VBO_vertex_size += 4 * sizeof(GLfloat); } // vertex
    if (normals.size()  != 0) { VBO_offset_normal =  VBO_vertex_size; VBO_vertex_size += 3 * sizeof(GLfloat); } // normal
    if (uvs.size()      != 0) { VBO_offset_uv     =  VBO_vertex_size; VBO_vertex_size += 2 * sizeof(GLfloat); } // uv
    
    // Build VBO buffer
    int vertexCount = vertices.size();
    char* pBuf = new char[VBO_vertex_size * vertexCount];
    for (int i = 0; i < vertexCount; i++)
    {
        char* pVertex = pBuf + i * VBO_vertex_size;
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
        glVertexAttribPointer(vertex_attrib, 4, GL_FLOAT, GL_FALSE, VBO_vertex_size, (void*) (intptr_t)VBO_offset_vertex);
    }

    if ((VBO_offset_normal != -1) && (normal_attrib != -1))
    {
        glEnableVertexAttribArray(normal_attrib);
        glVertexAttribPointer(normal_attrib, 3, GL_FLOAT, GL_FALSE, VBO_vertex_size, (void*) (intptr_t)VBO_offset_normal);
    }

    if ((VBO_offset_uv != -1) && (uv_attrib != -1))
    {
        glEnableVertexAttribArray(uv_attrib);
        glVertexAttribPointer(uv_attrib, 2, GL_FLOAT, GL_FALSE, VBO_vertex_size, (void*) (intptr_t)VBO_offset_uv);
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

