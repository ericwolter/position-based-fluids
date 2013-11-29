#ifdef _WINDOWS
#define GLEW_STATIC
#include <GL\glew.h>
#endif

#include "visual.hpp"
#include "../ParamUtils.hpp"
#include "SOIL.h"
#include "../ocl/TwOpenGLCore.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <fstream>
#include <vector>

using namespace std;

#ifdef _WINDOWS
#define isnan(x) _isnan(x)
#endif

static float displayscale = 1.0f;

CTwGraphOpenGLCore tw;

CVisual::CVisual (const int width, const int height)
    : UICmd_GenerateWaves(false),
      UICmd_ResetSimulation(false),
      UICmd_PauseSimulation(false),
	  UICmd_FriendsHistogarm(false),
      mWidth(width),
      mHeight(height),
      mProgramID(0),
      mWindow(NULL),
      mSystemBufferID(0),
      mPositionAttrib(0),
      //mCamTarget( glm::vec3(0.0f, 0.0f, 0.0f) ),
      //mCamSphere( glm::vec3(0.0f, 20.0f, -1.5f) ),
      //mCamTarget( glm::vec3(0.5f,     0.0f,  0.50f) ),
      //mCamSphere( glm::vec3(-500.0f, 17.0f, -1.35f) )
      mCamTarget( glm::vec3(1.0f,    0.42f,  0.21f) ),
      mCamSphere( glm::vec3(-540.0f, 31.0f, -1.67f) )
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

void MouseButtonCB( GLFWwindow *, int button , int action , int mods)
{
    (void)mods;
    TwEventMouseButtonGLFW( button , action );
}

void MousePosCB(GLFWwindow *, double x , double y)
{
    TwEventMousePosGLFW( (int)(x * displayscale), (int)(y * displayscale) );
}

void KeyFunCB( GLFWwindow *, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;
    TwEventKeyGLFW( key , action );
    TwEventCharGLFW( key  , action );
}

void MouseScrollCB(  GLFWwindow *, double x , double y )
{
    (void)x;
    TwEventMouseWheelGLFW( (int)y );
}

void TW_CALL PlayPause(void *clientData)
{
    ((CVisual *)clientData)->UICmd_PauseSimulation = !((CVisual *)clientData)->UICmd_PauseSimulation;
}
void TW_CALL Reset(void *clientData)
{
    ((CVisual *)clientData)->UICmd_ResetSimulation = true;
}
void TW_CALL GenerateWaves(void *clientData)
{
    ((CVisual *)clientData)->UICmd_GenerateWaves = !((CVisual *)clientData)->UICmd_GenerateWaves;
}
void TW_CALL ShowVelocity(void *clientData)
{
    ((CVisual *)clientData)->initParticlesVisual(VELOCITY);
}
void TW_CALL ShowSorting(void *clientData)
{
    ((CVisual *)clientData)->initParticlesVisual(SORTING);
}
void TW_CALL FriendsHistogram(void *clientData)
{
    ((CVisual *)clientData)->UICmd_FriendsHistogarm = !((CVisual *)clientData)->UICmd_FriendsHistogarm;
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

    /* Set GLFW event callbacks */
    // Currently AntTweakBar DOES NOT work out of the box with GLFW3.
    // For now custom event callbacks have to be implemented.
    // see: http://blog-imgs-37.fc2.com/s/c/h/schrlab/TwGLFW_cpp.txt
    // - Directly redirect GLFW mouse button events to AntTweakBar
    glfwSetMouseButtonCallback( mWindow , MouseButtonCB );
    // - Directly redirect GLFW mouse position events to AntTweakBar
    glfwSetCursorPosCallback( mWindow , MousePosCB);
    // - Directly redirect GLFW mouse wheel events to AntTweakBar
    glfwSetScrollCallback( mWindow , MouseScrollCB );
    // - Directly redirect GLFW key events to AntTweakBar
    glfwSetKeyCallback(mWindow , KeyFunCB);


#ifdef _WINDOWS
    glewInit();
#endif

    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE);
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);

    TwInit(TW_OPENGL, NULL);
    int fbwidth , fbheight;
    glfwGetFramebufferSize( mWindow , &fbwidth , &fbheight );
    TwWindowSize(fbwidth, fbheight);

    displayscale = (float)fbwidth / (float)mWidth;

    tweakBar = TwNewBar("PBFTweak");
    TwAddButton(tweakBar, "Play/Pause" , PlayPause , this , " group=Controls ");
    TwAddButton(tweakBar, "Reset" , Reset , this , " group=Controls ");
    TwAddButton(tweakBar, "Generate waves" , GenerateWaves , this , " group=Controls ");
    TwAddButton(tweakBar, "Show velocity" , ShowVelocity , this , " group=Controls ");
    TwAddButton(tweakBar, "Show sorting" , ShowSorting , this , " group=Controls ");
	TwAddButton(tweakBar, "Firends Histogram" , FriendsHistogram , this , " group=Controls ");

    // Create bar
    TwAddVarRO(tweakBar, "Total sim time", TW_TYPE_DOUBLE, &mTotalSimTime, "precision=2 group=Stats");

    // static unsigned int numParticles = 10000;
    // TwAddVarRW(tweakBar, "# of particles", TW_TYPE_UINT32, &numParticles, " group=Parameters max=100000 min=1000 step=100 ");

	tw.Init();
	TwGenerateDefaultFonts();
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

GLvoid CVisual::initSystemVisual(Simulation &sim)
{
    // Store simulation object
    mSimulation = &sim;

    // Compute background size
    float sizeX = (Params.xMax - Params.xMin) * 10.0f;
    float sizeY = (Params.yMax - Params.yMin) * 10.0f;
    float sizeZ = (Params.zMax - Params.zMin) * 10.0f;

    // Build background VBO
    const GLfloat systemVertices[] =
    {
        //   X,           Y,           Z,           Nx,   Ny,   Nz,
        -sizeX, Params.yMin, Params.zMin, 1.0f,   0.0f, 0.0f, 1.0f, 0.0f,
        +sizeX, Params.yMin, Params.zMin, 1.0f,   0.0f, 0.0f, 1.0f, 0.0f,
        +sizeX,       sizeY, Params.zMin, 1.0f,   0.0f, 0.0f, 1.0f, 0.0f,
        -sizeX,       sizeY, Params.zMin, 1.0f,   0.0f, 0.0f, 1.0f, 0.0f,

        -sizeX, Params.yMin, Params.zMin, 1.0f,   0.0f, 1.0f, 0.0f, 0.0f,
        +sizeX, Params.yMin, Params.zMin, 1.0f,   0.0f, 1.0f, 0.0f, 0.0f,
        +sizeX, Params.yMin,       sizeZ, 1.0f,   0.0f, 1.0f, 0.0f, 0.0f,
        -sizeX, Params.yMin,       sizeZ, 1.0f,   0.0f, 1.0f, 0.0f, 0.0f,
    };

    // Submit Background VBO
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

    // Load background drawing shader
    mProgramID = this->loadShaders(getPathForShader("shadervertex.glsl"),
                                   getPathForShader("shaderfragment.glsl"));
}

GLvoid CVisual::initParticlesVisual(ParticleRenderType renderType)
{
    mPositionAttrib          = glGetAttribLocation(mProgramID, "position");
    mNormalAttrib            = glGetAttribLocation(mProgramID, "normal");
    mTexcoordAttrib          = glGetAttribLocation(mProgramID, "texcoord");
    mCameraToClipMatrixUnif  = glGetUniformLocation(mProgramID, "cameraToClipMatrix");
    mWorldToCameraMatrixUnif = glGetUniformLocation(mProgramID, "worldToCameraMatrix");
    mModelToWorldMatrixUnif  = glGetUniformLocation(mProgramID, "modelToWorldMatrix");
    mTextureUnif             = glGetUniformLocation(mProgramID, "texture");

    mRenderType              = renderType;
    switch (renderType)
    {
    case SORTING:
        mParticleProgramID = this->loadShaders(getPathForShader("sortingvertex.glsl"),
                                               getPathForShader("sortingfragment.glsl"));
        break;

    default:
        mParticleProgramID = this->loadShaders(getPathForShader("particlevertex.glsl"),
                                               getPathForShader("particlefragment.glsl"));
        break;
    }
    mParticlePositionAttrib          = glGetAttribLocation(mParticleProgramID, "position");
    mParticleCameraToClipMatrixUnif  = glGetUniformLocation(mParticleProgramID, "cameraToClipMatrix");
    mParticleWorldToCameraMatrixUnif = glGetUniformLocation(mParticleProgramID, "worldToCameraMatrix");
    mParticleModelToWorldMatrixUnif  = glGetUniformLocation(mParticleProgramID, "modelToWorldMatrix");

    glm::mat4 cameraToClipMatrix = glm::perspective(45.0f, mWidth / (GLfloat) mHeight, 0.1f, 1000.0f);
    glm::mat4 modelToWorldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0));

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

    if (mRenderType == SORTING)
    {
        glUniform1f(glGetUniformLocation(mParticleProgramID, "particleCount"), Params.particleCount);
    }

    glBindBuffer(GL_ARRAY_BUFFER, mSimulation->mSharingYinBufferID);
    glEnableVertexAttribArray(mParticlePositionAttrib);
    glVertexAttribPointer(mParticlePositionAttrib, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_POINTS, 0, Params.particleCount);
    glDisableVertexAttribArray(mParticlePositionAttrib);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUnmapBuffer(GL_ARRAY_BUFFER);

    glUseProgram(0);

    DrawTweekBar();

    glfwSwapBuffers(mWindow);
}

void CVisual::DrawTweekBar()
{
    // Update AntTweekBar
    mTotalSimTime = 0;
    for (size_t i = 0; i < mSimulation->PerfData.Trackers.size(); i++)
    {
        PM_PERFORMANCE_TRACKER *pTracker = mSimulation->PerfData.Trackers[i];

        // Check if we need to create Bar
        if (pTracker->Tag == 0)
        {
            // Remember that bar was created
            pTracker->Tag = 1;

            // Create bar
            TwAddVarRO(tweakBar, ("  " + pTracker->eventName).c_str(), TW_TYPE_DOUBLE, &pTracker->total_time, "precision=2 group=Stats");

            // Make sure Stats group is folded
            TwDefine(" PBFTweak/Stats opened=false ");
        }

        // Accumulate execution time
        mTotalSimTime += pTracker->total_time;
    }

    // Make sure bar refresh it's values
    TwRefreshBar(tweakBar);

    // Actual draw
    TwDraw();

	DrawPerformanceGraph();

	// Draw Friends histogram
	if (UICmd_FriendsHistogarm)
		DrawFriendsHistogram();
}

void CVisual::DrawPerformanceGraph()
{
	// Compute sizes
	const int ViewWidth    = 1280;
	const int ViewHeight   = 720;
	const int BarHeight    = 20;
	const int BarWidth     = ViewWidth * 0.9;
	const int BarTop       = ViewHeight - BarHeight * 2;
	const int BarBottom    = BarTop + BarHeight;
	const int BarLeft      = (ViewWidth - BarWidth) / 2;
	const int BarRight     = ViewWidth - BarLeft;

	static void* pTextObj = tw.NewTextObj();

	tw.BeginDraw(ViewWidth, ViewHeight);

	// Find total time
	float totalTime = 0;
    for (size_t i = 0; i < mSimulation->PerfData.Trackers.size(); i++)
        totalTime += mSimulation->PerfData.Trackers[i]->total_time;
	
	// Draw
	//const color32 StartClr[] = {0xDE6866, 0x9ADE66, 0x66AADE};
	//const color32 EndClr[]   = {0xDEA1A0, 0xBAE09D, 0x9ABFDB};
	const color32 StartClr[] = {0xFF0000, 0x00FF00, 0x0000FF};
	const color32 EndClr[]   = {0xA00000, 0x00A000, 0x0000A0};
	const color32 Alpha      = 0x80000000;
	int prevX = BarLeft;
	float accTime = 0;
    for (size_t i = 0; i < mSimulation->PerfData.Trackers.size(); i++)
    {
        PM_PERFORMANCE_TRACKER* pTracker = mSimulation->PerfData.Trackers[i];

		// Add segment time
		accTime += pTracker->total_time;

		// Compute screen position
		int newX = BarLeft + (0.5f + accTime * BarWidth / totalTime);

		// draw bar
		tw.DrawRect(prevX, BarTop, newX, BarBottom, Alpha | StartClr[i % 3], Alpha | EndClr[i % 3], Alpha | StartClr[i % 3], Alpha | EndClr[i % 3]);

		// Do we have space for text?
		if (newX - prevX > 4)
		{
			// Draw name text
			const int NameVertOffset = -2;
			tw.BuildText(pTextObj, &pTracker->eventName, NULL, NULL, 1, g_DefaultSmallFont, 0, 0);
			tw.SetScissor(prevX+2, BarTop + NameVertOffset, newX - prevX, BarHeight);
			tw._DrawText(pTextObj, prevX+2, BarTop + NameVertOffset, 0xffffffffu, 0);

			// Build ms text
			char tmp[128];
			sprintf(tmp, "%3.2f", pTracker->total_time);
			string str(tmp);

			// Draw time text
			const int MSVertOffset = 9;
			tw.BuildText(pTextObj, &str, NULL, NULL, 1, g_DefaultSmallFont, 0, 0);
			tw.SetScissor(prevX+2, BarTop + MSVertOffset, newX - prevX, BarHeight);
			tw._DrawText(pTextObj, prevX+2, BarTop + MSVertOffset, 0xffffffffu, 0);

			// Remove Scissor
			tw.SetScissor(0, 0, 0, 0);
		}

		// Update prev X
		prevX = newX;
	}

	tw.EndDraw();
}

void CVisual::DrawFriendsHistogram()
{
	static void* pTextObj = tw.NewTextObj();

	// Histogram
	int Histogram[32];
	memset(Histogram, 0, sizeof(Histogram));

	// Compute histogram data
	for (int iPart = 0; iPart < Params.particleCount; iPart++)
		for (int iCircle = 0; iCircle < Params.friendsCircles; iCircle++)
			Histogram[iCircle] += mSimulation->mFriendsList[iPart * (Params.friendsCircles * (1 + Params.particlesPerCircle)) + iCircle];

	// Compute total friends
	int histTotal = 0;
	for (int iHist = 0; iHist < Params.friendsCircles; iHist++)
		histTotal += Histogram[iHist];

	// Define coordinates
	const int ViewWidth    = 1280;
	const int ViewHeight   = 720;
	const int HistWidth    = 100;
	const int HistHeight   = 100;
	const int HistTop      = 10;
	const int HistBottom   = HistTop + HistHeight;
	const int HistLeft     = ViewWidth*0.95 - HistWidth;
	const int HistRight    = HistLeft + HistWidth;
	const int BarsWidth   = HistWidth / Params.friendsCircles;

	tw.BeginDraw(ViewWidth, ViewHeight);

	// Draw
	tw.DrawRect(HistLeft - 2, HistTop - 2,HistRight + 2, HistBottom + 2, 0x50ffffff);
	
	for (int iHist = 0; iHist < Params.friendsCircles; iHist++)
	{
		// Compute bar height
		float barHeight = (float)Histogram[iHist] / histTotal; 

		// Compute screen pos
		int   screenY1 = HistBottom - barHeight * HistHeight - 1;
		int   screenX1 = 1 + HistLeft + BarsWidth * iHist;

		// Draw
		tw.DrawRect(screenX1, screenY1, screenX1 + BarsWidth - 2, HistBottom, 0xff00ff0ff);

		// Build ms text
		char tmp[128];
		sprintf(tmp, "%3.0f", barHeight * 100);
		string str(tmp);

		// Draw time text
		tw.BuildText(pTextObj, &str, NULL, NULL, 1, g_DefaultSmallFont, 0, 0);
		tw._DrawText(pTextObj, screenX1, HistBottom - 10, 0xff000000u, 0);
	}

	tw.EndDraw();
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
    GLuint programID = 0;

    string line; // Used for getline()

    GLint result = GL_FALSE;
    GLint logLength = 0;
    const char *source = NULL;
    char *errorMsg = NULL;

    // Create the shaders
    GLuint vertexShaderID   = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

    string shaderCode;
    ifstream ifs(vertexFilename.c_str(), ios::binary);
    if ( !ifs )
        throw runtime_error("Could not open file for vertex shader!");
    shaderCode = string( istreambuf_iterator<char>(ifs), istreambuf_iterator<char>() );
    ifs.close();

    // Compile Vertex Shader
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

    // Read the Fragment Shader code from the file
    ifs.open(fragmentFilename.c_str(), ios::binary);
    if ( !ifs )
        throw runtime_error("Could not open file for fragment shader!");
    shaderCode = string(istreambuf_iterator<char>(ifs), istreambuf_iterator<char>());
    ifs.close();

    // Compile shaders
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
