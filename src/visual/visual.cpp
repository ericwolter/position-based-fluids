#ifdef _WINDOWS
#define GLEW_STATIC
#include <GL\glew.h>
#endif

#include "visual.hpp"
#include "../io/parameters.hpp"

#define _USE_MATH_DEFINES
#include <math.h>

#include <stdexcept>
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <cmath>
#include <vector>

#include "SOIL.h"

using std::runtime_error;
using std::ifstream;
using std::ios;
using std::cout;
using std::cerr;
using std::endl;
using std::cin;
using std::istreambuf_iterator;
using std::vector;
using std::ifstream;

#ifdef _WINDOWS
#define isnan(x) _isnan(x)
#else
using std::isnan;
#endif

CVisual::CVisual (const int width, const int height)
    : UICmd_GenerateWaves(false),
      UICmd_ResetSimulation(false),
      UICmd_PauseSimulation(false),
      mWidth(width),
      mHeight(height),
      mProgramID(0),
      mWindow(NULL),
      mSystemBufferID(0),
      mPositionAttrib(0),
      //mCamTarget( glm::vec3(0.0f, 0.0f, 0.0f) ),
      //mCamSphere( glm::vec3(0.0f, 20.0f, -1.5f) ),
      mCamTarget( glm::vec3(0.08f, -0.28f, 0.00f) ),
      mCamSphere( glm::vec3(-23.0f, 20.0f, -1.50f) )
{

}

CVisual::~CVisual ()
{
    // Destructor never reached if ESC is pressed in GLFW
    //cout << "Finish." << endl;
    glDeleteProgram(mProgramID);

    glFinish();
    glfwTerminate();
}

void CVisual::KeyEvent(GLFWwindow* window,int key,int scancode,int action, int mods) 
{
    // ignore unused parameters, maybe just remove them?
    (void)window;
    (void)scancode;
    (void)mods;

	// Ignore none-press events
	if (action != GLFW_PRESS)
		return;

	// Process key
	switch ((char)key)
	{
		case 'G':
		    UICmd_GenerateWaves = !UICmd_GenerateWaves;
			break;
		case 'P':
		    UICmd_PauseSimulation = !UICmd_PauseSimulation;
			break;
		case 'R':
		    UICmd_ResetSimulation = true;
			break;
	}
}

static void StaticKeyEvent(GLFWwindow* window,int key,int scancode,int action, int mods) 
{
	CVisual* pThis = (CVisual*)glfwGetWindowUserPointer(window);
	
	if (pThis != NULL)
		pThis->KeyEvent(window, key, scancode, action, mods);
}

void CVisual::initWindow(const string windowname)
{
    // Initialise GLFW
    if ( !glfwInit() )
    {
        throw runtime_error("Could not initialize GLFW!");
    }

    glfwWindowHint(GLFW_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_VERSION_MINOR, 2);

    // Create a windowed mode window and its OpenGL context
    mWindow = glfwCreateWindow(mWidth, mHeight,
                               windowname.c_str(), NULL, NULL);

    if ( !mWindow )
    {
        throw runtime_error("Could not open GLFW Window!");
        glfwTerminate();
    }

    // Make the window's context current
    glfwMakeContextCurrent(mWindow);
    glfwSetInputMode(mWindow, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetWindowUserPointer(mWindow, this);
	glfwSetKeyCallback(mWindow, StaticKeyEvent);

#ifdef _WINDOWS
    glewInit();
#endif

    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE);
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
}

glm::vec3 CVisual::resolveCamPosition(void) const
{
    GLfloat phi = (M_PI / 180.0f) * (mCamSphere.x - 90.0f);
    GLfloat theta = (M_PI / 180.0f) * (mCamSphere.y + 90.0f);

    GLfloat fSinTheta = sinf(theta);
    GLfloat fCosTheta = cosf(theta);
    GLfloat fCosPhi = cosf(phi);
    GLfloat fSinPhi = sinf(phi);

    glm::vec3 dirToCamera(fSinTheta * fCosPhi, fCosTheta, fSinTheta * fSinPhi);

    return (dirToCamera * mCamSphere.z) + mCamTarget;
}

glm::mat4 CVisual::calcLookAtMatrix(const glm::vec3 &cameraPt, const glm::vec3 &lookPt, const glm::vec3 &upPt) const
{
    glm::vec3 lookDir = glm::normalize(lookPt - cameraPt);
    glm::vec3 upDir = glm::normalize(upPt);

    glm::vec3 rightDir = glm::normalize(glm::cross(lookDir, upDir));
    glm::vec3 perpUpDir = glm::cross(rightDir, lookDir);

    glm::mat4 rotMat(1.0f);

    rotMat[0] = glm::vec4(rightDir, 0.0f);
    rotMat[1] = glm::vec4(perpUpDir, 0.0f);
    rotMat[2] = glm::vec4(-lookDir, 0.0f);

    rotMat = glm::transpose(rotMat);

    glm::mat4 transMat(1.0f);

    transMat[3] = glm::vec4(-cameraPt, 1.0f);

    return rotMat * transMat;
}

GLvoid CVisual::initSystemVisual(Simulation& sim, const cl_float4 sizesMin, const cl_float4 sizesMax)
{
	// Store simulation object
	mSimulation = &sim;

    // Set system sizes
    mSizeXmin = sizesMin.s[0];
    mSizeXmax = sizesMax.s[0];
    mSizeYmin = sizesMin.s[1];
    mSizeYmax = sizesMax.s[1];
    mSizeZmin = sizesMin.s[2];
    mSizeZmax = sizesMax.s[2];

    float sizeX = (mSizeXmax - mSizeXmin) * 10.0f;
    float sizeY = (mSizeYmax - mSizeYmin) * 10.0f;
    float sizeZ = (mSizeZmax - mSizeZmin) * 10.0f;

    const GLfloat systemVertices[] =
    {
        -sizeX, mSizeYmin, mSizeZmin, 1.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        sizeX, mSizeYmin, mSizeZmin, 1.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        sizeX, sizeY, mSizeZmin, 1.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        -sizeX, sizeY, mSizeZmin, 1.0f,
        0.0f, 0.0f, 1.0f, 0.0f,

        -sizeX, mSizeYmin, mSizeZmin, 1.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        sizeX, mSizeYmin, mSizeZmin, 1.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        sizeX, mSizeYmin, sizeZ, 1.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        -sizeX, mSizeYmin, sizeZ, 1.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
    };

	// Create Background VBO
    glGenBuffers(1, &mSystemBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, mSystemBufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(systemVertices), systemVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Setup Texture
    glGenTextures(1, &mWallTexture);
    glBindTexture(GL_TEXTURE_2D, mWallTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    /// Load Texture
    int texWidth = 0, texHeight = 0, channels = 0;
    unsigned char *image = SOIL_load_image(getPathForTexture("wall.tga").c_str(), &texWidth, &texHeight, &channels, SOIL_LOAD_RGB);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    SOIL_free_image_data(image);
    glBindTexture(GL_TEXTURE_2D, 0);

    mProgramID = this->loadShaders(getPathForShader("shadervertex.glsl"),
                                   getPathForShader("shaderfragment.glsl"));
}

GLvoid CVisual::initParticlesVisual()
{
    mPositionAttrib          = glGetAttribLocation(mProgramID, "position");
    mNormalAttrib            = glGetAttribLocation(mProgramID, "normal");
    mTexcoordAttrib          = glGetAttribLocation(mProgramID, "texcoord");
    mCameraToClipMatrixUnif  = glGetUniformLocation(mProgramID, "cameraToClipMatrix");
    mWorldToCameraMatrixUnif = glGetUniformLocation(mProgramID, "worldToCameraMatrix");
    mModelToWorldMatrixUnif  = glGetUniformLocation(mProgramID, "modelToWorldMatrix");
    mTextureUnif             = glGetUniformLocation(mProgramID, "texture");

    mParticleProgramID = this->loadShaders(getPathForShader("particlevertex.glsl"),
                                           getPathForShader("particlefragment.glsl"));

    mParticlePositionAttrib          = glGetAttribLocation(mParticleProgramID, "position");
    mParticleCameraToClipMatrixUnif  = glGetUniformLocation(mParticleProgramID, "cameraToClipMatrix");
    mParticleWorldToCameraMatrixUnif = glGetUniformLocation(mParticleProgramID, "worldToCameraMatrix");
    mParticleModelToWorldMatrixUnif  = glGetUniformLocation(mParticleProgramID, "modelToWorldMatrix");

    glm::mat4 cameraToClipMatrix = glm::perspective(45.0f, mWidth / (GLfloat) mHeight, 0.1f, 1000.0f);
    glm::mat4 modelToWorldMatrix = glm::translate(
                                       glm::mat4(1.0f),
                                       glm::vec3(
                                           -(mSizeXmax - mSizeXmin) / 2.0f,
                                           -(mSizeYmax - mSizeYmin) / 2.0f,
                                           -(mSizeZmax - mSizeZmin) / 2.0f));

    //mCamSphere.z = -(mSizeZmax - mSizeZmin) * 2.0f;

    glUseProgram(mProgramID);
    glUniformMatrix4fv(mCameraToClipMatrixUnif, 1, GL_FALSE, glm::value_ptr(cameraToClipMatrix));
    glUniformMatrix4fv(mModelToWorldMatrixUnif, 1, GL_FALSE, glm::value_ptr(modelToWorldMatrix));
    glUseProgram(0);

    glUseProgram(mParticleProgramID);
    glUniformMatrix4fv(mParticleCameraToClipMatrixUnif, 1, GL_FALSE, glm::value_ptr(cameraToClipMatrix));
    glUniformMatrix4fv(mParticleModelToWorldMatrixUnif, 1, GL_FALSE, glm::value_ptr(modelToWorldMatrix));
    glUseProgram(0);
}

GLuint CVisual::createSharingBuffer(const GLsizei size) const
{
    GLuint bufferID = 0;
    glGenBuffers(1, &bufferID);
    glBindBuffer(GL_ARRAY_BUFFER, bufferID);

    // GL_STATIC_DRAW is recommended by apple if cgl sharing is used
    // STATIC seems only be in respect to the host
    glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW);

    return bufferID;
}

GLvoid CVisual::visualizeParticles(void)
{
    glClearColor(0.05f, 0.05f, 0.05f, 0.0f); // Dark blue background
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const glm::vec3 &camPos = resolveCamPosition();
    const glm::mat4 lookAtMat = calcLookAtMatrix( camPos, mCamTarget, glm::vec3(0.0f, 1.0f, 0.0f) );

    glUseProgram(mProgramID);

    glUniformMatrix4fv(
        mWorldToCameraMatrixUnif,
        1, GL_FALSE,
        glm::value_ptr(lookAtMat)
    );

    // Texture stuff do not optimize for now
    // 1. only write uniform once
    // 2. use variable offset instead of hardcoded 0
    glUniform1i(mTextureUnif, 0);
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, mWallTexture);

    glBindBuffer(GL_ARRAY_BUFFER, mSystemBufferID);
    glEnableVertexAttribArray(mPositionAttrib);
    glEnableVertexAttribArray(mNormalAttrib);
    glVertexAttribPointer(mPositionAttrib, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), 0);
    glVertexAttribPointer(mNormalAttrib, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void *) ( 4 * sizeof(GLfloat) ) );

    //glDrawArrays(GL_QUADS, 0, 8);

    glDisableVertexAttribArray(mNormalAttrib);
    glDisableVertexAttribArray(mPositionAttrib);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glUseProgram(0);

    glUseProgram(mParticleProgramID);
    glUniformMatrix4fv(mParticleWorldToCameraMatrixUnif, 1, GL_FALSE, glm::value_ptr(lookAtMat));

	glBindBuffer(GL_ARRAY_BUFFER, mSimulation->mSharingBufferID);
    glEnableVertexAttribArray(mParticlePositionAttrib);
    glVertexAttribPointer(mParticlePositionAttrib, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_POINTS, 0, Params.particleCount);
    glDisableVertexAttribArray(mParticlePositionAttrib);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUnmapBuffer(GL_ARRAY_BUFFER);

    glUseProgram(0);

    glfwSwapBuffers(mWindow);
}

void CVisual::checkInput()
{
    glfwPollEvents();

    if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glFinish();
        glfwTerminate();
        exit(-1);
    }

    if (glfwGetKey(mWindow, 'W') == GLFW_PRESS)
    {
        mCamTarget.z -= 0.01f;
    }

    if (glfwGetKey(mWindow, 'S') == GLFW_PRESS)
    {
        mCamTarget.z += 0.01f;
    }

    if (glfwGetKey(mWindow, 'Q') == GLFW_PRESS)
    {
        mCamTarget.y -= 0.01f;
    }

    if (glfwGetKey(mWindow, 'E') == GLFW_PRESS)
    {
        mCamTarget.y += 0.01f;
    }

    if (glfwGetKey(mWindow, 'D') == GLFW_PRESS)
    {
        mCamTarget.x -= 0.01f;
    }

    if (glfwGetKey(mWindow, 'A') == GLFW_PRESS)
    {
        mCamTarget.x += 0.01f;
    }

    if (glfwGetKey(mWindow, 'O') == GLFW_PRESS)
    {
        mCamSphere.z *= 0.9f;
    }

    if (glfwGetKey(mWindow, 'U') == GLFW_PRESS)
    {
        mCamSphere.z *= 1.0f / 0.9f;
    }

    if (glfwGetKey(mWindow, 'I') == GLFW_PRESS)
    {
        mCamSphere.y -= 1.0f;
    }

    if (glfwGetKey(mWindow, 'K') == GLFW_PRESS)
    {
        mCamSphere.y += 1.0f;
    }

    if (glfwGetKey(mWindow, 'J') == GLFW_PRESS)
    {
        mCamSphere.x -= 1.0f;
    }

    if (glfwGetKey(mWindow, 'L') == GLFW_PRESS)
    {
        mCamSphere.x += 1.0f;
    }

    // Write current settings to window title
    char szTitle[100];
    sprintf(szTitle, "eye=(%3.2f %3.2f %3.2f) target=(%3.2f %3.2f %3.2f)", mCamSphere.x, mCamSphere.y, mCamSphere.z, mCamTarget.x, mCamTarget.y, mCamTarget.z);
    glfwSetWindowTitle(mWindow, szTitle);
}

/**
*  \brief  Loads all shaders.
*/
GLuint CVisual::loadShaders(const string &vertexFilename, const string &fragmentFilename)
{
    cout << vertexFilename << endl;
    cout << fragmentFilename << endl;

    GLuint programID = 0;

    string line; // Used for getline()

    GLint result = GL_FALSE;
    GLint logLength = 0;
    const char *source = NULL;
    char *errorMsg = NULL;

    // Create the shaders
    GLuint vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

    string shaderCode;
    ifstream ifs(vertexFilename.c_str(), ios::binary);

    // Read in vertex shader code
    if ( !ifs )
    {
        throw runtime_error("Could not open file for vertex shader!");
    }

    shaderCode = string( istreambuf_iterator<char>(ifs),
                         istreambuf_iterator<char>() );

    ifs.close();

    // Compile Vertex Shader
#if defined(USE_DEBUG)
    cout << "Compiling vertex shader " << vertexFilename << endl;
#endif // USE_DEBUG

    source = shaderCode.c_str();
    glShaderSource(vertexShaderID, 1, &source , NULL);
    glCompileShader(vertexShaderID);

    // Check Vertex Shader
    glGetShaderiv(vertexShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv(vertexShaderID, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength > 0)
    {
        errorMsg = new char[logLength + 1];

        glGetShaderInfoLog(vertexShaderID, logLength, NULL, errorMsg);
        cerr << errorMsg << endl;

        delete[] errorMsg;
    }

    shaderCode.clear();

    // Read the Fragment Shader code from the file
    ifs.open(fragmentFilename.c_str(), ios::binary);

    // Read in fragment shader code
    if ( !ifs )
    {
        throw runtime_error("Could not open file for fragment shader!");
    }

    shaderCode = string( istreambuf_iterator<char>(ifs),
                         istreambuf_iterator<char>() );

    ifs.close();

    // Compile Fragment Shader
#if defined(USE_DEBUG)
    cout << "Compiling fragment shader " << fragmentFilename << endl;
#endif // USE_DEBUG

    source = shaderCode.c_str();
    glShaderSource(fragmentShaderID, 1, &source, NULL);
    glCompileShader(fragmentShaderID);

    // Check Fragment Shader
    glGetShaderiv(fragmentShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv(fragmentShaderID, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength > 0)
    {
        errorMsg = new char[logLength + 1];

        glGetShaderInfoLog(fragmentShaderID, logLength, NULL, errorMsg);
        cerr << errorMsg << endl;

        delete[] errorMsg;
    }

    // Link the program
#if defined(USE_DEBUG)
    cout << "Linking program\n" << endl;
#endif // USE_DEBUG

    programID = glCreateProgram();

    glAttachShader(programID, vertexShaderID);
    glAttachShader(programID, fragmentShaderID);
    glLinkProgram(programID);

    // Check the program
    glGetProgramiv(programID, GL_LINK_STATUS, &result);
    glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength > 0)
    {
        errorMsg = new char[logLength + 1];

        glGetProgramInfoLog(programID, logLength, NULL, errorMsg);
        cerr << errorMsg << endl;

        delete[] errorMsg;
    }

    glDeleteShader(vertexShaderID);
    glDeleteShader(fragmentShaderID);

    return programID;
}
