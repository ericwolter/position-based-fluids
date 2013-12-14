
#include "visual.hpp"
#include "..\ParamUtils.hpp"
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
	  UICmd_ColorMethod(0),
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

	pTarget        = new FBO(1, true, mWidth, mHeight, GL_RGBA32F);
	pFBO_Thickness = new FBO(1, true, mWidth, mHeight, GL_R16);

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

	OGLU_Init();

	ZPR_Reset();

	/*
	// Generate test texture
	byte* map = new byte[1280*720*3];
	for (int x = 0; x < 1280; x++)
	{
		for (int y = 0; y < 720; y++)
		{
			map[(y * 1280 + x) * 3 + 0] = x ^ y;
			map[(y * 1280 + x) * 3 + 1] = x ^ y;
			map[(y * 1280 + x) * 3 + 2] = x ^ y;
		}
	}

	tex = OGLU_GenerateTexture(1280, 720, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, map);*/
}

GLvoid CVisual::initParticlesVisual()
{
	// Load Shaders
	mParticleProgID      =  OGLU_LoadProgram(getPathForShader("particles.vs").c_str(), getPathForShader("particles_color.fs").c_str());
	mStandardCopyProgID  =  OGLU_LoadProgram(getPathForShader("standard.vs").c_str(), getPathForShader("standard_copy.fs").c_str());
	mStandardColorProgID =  OGLU_LoadProgram(getPathForShader("standard.vs").c_str(), getPathForShader("standard_color.fs").c_str());

	// Compute projection matrix
	float FOV = 45.0f;
	mProjectionMatrix = glm::perspective(FOV, mWidth / (GLfloat) mHeight, 0.1f, 10.0f);

	// compute mWidthOfNearPlane
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	mWidthOfNearPlane = (float)abs(viewport[2]-viewport[0]) / (2.0f * tan(0.5f * FOV * M_PI / 180.0f));

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

GLvoid CVisual::renderParticles()
{
	// Clear target
	pTarget->SetAsDrawTarget();
	glClearColor(0, 0, 0, 0);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Setup Particle drawing
	glUseProgram(g_SelectedProgram = mParticleProgID);
	glUniformMatrix4fv(UniformLoc("projectionMatrix"), 1, GL_FALSE, glm::value_ptr(mProjectionMatrix));
    glUniformMatrix4fv(UniformLoc("modelViewMatrix"),  1, GL_FALSE, glm::value_ptr(ZPR_ModelViewMatrix));
	glUniform1i(UniformLoc("particleCount"),    Params.particleCount);
	glUniform1i(UniformLoc("colorMethod"),      UICmd_ColorMethod);
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

	// Copy to screen
	glUseProgram(g_SelectedProgram = mStandardCopyProgID);
	OGLU_BindTextureToUniform("SourceImg", 0, pTarget->pColorTextureId[0]);
	g_ScreenFBO.SetAsDrawTarget();
	OGLU_RenderQuad(0,0,1.0,1.0);
}

GLvoid CVisual::presentToScreen()
{
    glfwSwapBuffers(mWindow);
}

