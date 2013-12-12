
#include "..\OGL_Utils.h"
#include "..\ParamUtils.hpp"
#include "visual.hpp"
#include "..\ZPR.h"

#include "SOIL.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <fstream>
#include <vector>

using namespace std;

#ifdef _WINDOWS
#define isnan(x) _isnan(x)
#endif

CVisual::CVisual (const int width, const int height)
    : UICmd_GenerateWaves(false),
      UICmd_ResetSimulation(false),
      UICmd_PauseSimulation(false),
	  UICmd_FriendsHistogarm(false),
      mWidth(width),
      mHeight(height),
      mWindow(NULL),
      mSystemBufferID(0)
{
}

CVisual::~CVisual ()
{
    glFinish();
    glfwTerminate();
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
    mWindow = glfwCreateWindow(mWidth, mHeight, windowname.c_str(), NULL, NULL);

    // Make the window's context current
    glfwMakeContextCurrent(mWindow);
    glfwSetInputMode(mWindow, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetWindowUserPointer(mWindow, this);

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

GLvoid CVisual::initSystemVisual(Simulation &sim)
{
    // Store simulation object
    mSimulation = &sim;

	ZPR_Reset();
}

GLvoid CVisual::initParticlesVisual()
{
	// Load Shaders
	mParticleProgramID =  OGLU_LoadProgram(getPathForShader("particlevertex.glsl").c_str(), getPathForShader("particlefragment.glsl").c_str());

	// Compute projection matrix
	float FOV = 45.0f;
	mProjectionMatrix = glm::perspective(FOV, mWidth / (GLfloat) mHeight, 0.1f, 10.0f);

	// compute mWidthOfNearPlane
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	mWidthOfNearPlane = (float)abs(viewport[2]-viewport[0]) / (2.0 * tan(0.5 * FOV * M_PI / 180.0));

	// Update ZPR
	ZPR_SetupView(mProjectionMatrix, NULL);
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

void CheckGLError()
{
	if (glGetError() != 0)
		_asm nop;

}

GLvoid CVisual::visualizeParticles()
{
	// Clear target
	glClearColor(0.05f, 0.05f, 0.05f, 0.0f); // Dark blue background
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(g_SelectedProgram = mParticleProgramID);

	// Setup uniforms
	glUniformMatrix4fv(UniformLoc("projectionMatrix"), 1, GL_FALSE, glm::value_ptr(mProjectionMatrix));
    glUniformMatrix4fv(UniformLoc("modelViewMatrix"),  1, GL_FALSE, glm::value_ptr(ZPR_ModelViewMatrix));
	glUniform1i(UniformLoc("particleCount"),    Params.particleCount);
	glUniform1i(UniformLoc("colorMethod"),      0);
	glUniform1f(UniformLoc("widthOfNearPlane"), mWidthOfNearPlane);
	glUniform1f(UniformLoc("pointSize"),        Params.particleRenderSize);

	// Bind positions buffer
    glBindBuffer(GL_ARRAY_BUFFER, mSimulation->mSharingYinBufferID);
	glEnableVertexAttribArray(AttribLoc("position"));
    glVertexAttribPointer(AttribLoc("position"), 4, GL_FLOAT, GL_FALSE, 0, 0);

	// Draw particles
    glDrawArrays(GL_POINTS, 0, Params.particleCount);
    
	// Unbind buffer
	glDisableVertexAttribArray(AttribLoc("position"));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

GLvoid CVisual::presentToScreen()
{
    glfwSwapBuffers(mWindow);
}

