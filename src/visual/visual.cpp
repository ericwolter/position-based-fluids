#include "visual.hpp"
#include "../OGL_Utils.h"
#include "../ParamUtils.hpp"
#include "../ZPR.h"
#include "../OGL_RenderStageInspector.h"

#include "SOIL.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <fstream>
#include <vector>

using namespace std;

#ifdef _WINDOWS
#define isnan(x) _isnan(x)
#endif

void _stdcall OpenGL_DebugLog(GLenum source,
                       GLenum type,
                       GLuint id,
                       GLenum severity,
                       GLsizei length,
                       const GLchar* message,
                       void* userParam) 
{
    cout << "OpenGL Error: " << message << endl;
}


CVisual::CVisual (const int windowWidth, const int windowHeight)
    : mWindow(NULL),
      UICmd_GenerateWaves(false),
      UICmd_ResetSimulation(false),
      UICmd_PauseSimulation(false),
      UICmd_FriendsHistogarm(false),
      UICmd_RenderMode(0),
      mWindowWidth(windowWidth),
      mWindowHeight(windowHeight),
      mCycleID(0),
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

#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#else
    #define ENABLE_OPENGL_LOG_REPORTING
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

    // Create a windowed mode window and its OpenGL context
    mWindow = glfwCreateWindow(mWindowWidth, mWindowHeight, windowname.c_str(), NULL, NULL);

    // Make the window's context current
    glfwMakeContextCurrent(mWindow);
    glfwSetInputMode(mWindow, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetWindowUserPointer(mWindow, this);

#ifdef __APPLE__
    glewExperimental = true;
#endif
    glewInit();

#ifdef ENABLE_OPENGL_LOG_REPORTING
    // Enable OpenGL debugging
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(OpenGL_DebugLog, NULL);
#endif

    // Get frame size (apple retina makes the frame size X2 the window size)
    glfwGetFramebufferSize(mWindow, &mFrameWidth, &mFrameHeight);

    pPrevTarget    = new FBO(1, true, mFrameWidth, mFrameHeight, GL_RGBA32F);
    pNextTarget    = new FBO(1, true, mFrameWidth, mFrameHeight, GL_RGBA32F);
    pFBO_Thickness = new FBO(1, true, mFrameWidth, mFrameHeight, GL_R16UI);

    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE);
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);

    // Check that things are fine
    OGLU_CheckCoreError("initWindow (end)");
}


void CVisual::setupProjection()
{
    // Compute projection matrix
    float aspect = mFrameWidth / (GLfloat) mFrameHeight;
    float fovy = 45.0f;
    mProjectionMatrix = glm::perspective(fovy, aspect, 0.1f, 10.0f);
    mInvProjectionMatrix = glm::inverse(mProjectionMatrix);

    // compute mWidthOfNearPlane
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    mWidthOfNearPlane = (float)(abs(viewport[2] - viewport[0]) / (2.0f * tan(0.5f * fovy * M_PI / 180.0f)));
    
    // compute mInvFocalLen
    float tan_half_fovy = (float)tan(fovy * M_PI / 180.0 / 2.0);
    mInvFocalLen = glm::vec2(tan_half_fovy * aspect, tan_half_fovy);

    // Update ZPR
    ZPR_SetupView(mProjectionMatrix, NULL);
    ZPR_Reset();
}

#define DivCeil(num, divider) ((num + divider - 1) / divider) 

void CVisual::initImageBuffers()
{
    // Generate texture
    mImgParticleVisible = OGLU_GenerateTexture(2048, DivCeil(Params.particleCount, 2048), GL_R32UI);
    mImgGridChain       = OGLU_GenerateTexture(2048, DivCeil(Params.particleCount, 2048), GL_R32I);
    mImgGrid            = OGLU_GenerateTexture(2048, 2048, GL_R32I);

    // Bind texture to an image units (so we can write to it)
    glBindImageTexture(0, mImgParticleVisible, 0, true, 0,  GL_READ_WRITE, GL_R32UI);
    glBindImageTexture(1, mImgGridChain,       0, true, 0,  GL_READ_WRITE, GL_R32I);
    glBindImageTexture(2, mImgGrid,            0, true, 0,  GL_READ_WRITE, GL_R32I);
}

void CVisual::parametersChanged()
{
    initImageBuffers();
}

void CVisual::initSystemVisual(Simulation &sim)
{
    mSimulation = &sim;

    OGLU_Init();

    setupProjection();
}

const string *CVisual::ShaderFileList()
{
    static const string shaders[] =
    {
        "particles.vs",
        "particles_color.fs",
        "standard.vs",
        "standard_copy.fs",
        "standard_color.fs",
        "fluid_depth_smoothing.fs",
        "fluid_final_render.fs",
        "grid_reset.cms",
        "grid_chain_reset.cms",
        "grid_build.cms",
        "vis_scan.cms",
        ""
    };

    return shaders;
}

bool CVisual::initShaders()
{
    // Load Shaders
    bool bLoadOK = true;
    bLoadOK = bLoadOK && (mParticleProgID         = OGLU_LoadProgram("Particles",   getShaderSource("particles.vs"), getShaderSource("particles_color.fs")));
    bLoadOK = bLoadOK && (mFluidDepthSmoothProgID = OGLU_LoadProgram("DepthSmooth", getShaderSource("standard.vs"),  getShaderSource("fluid_depth_smoothing.fs")));
    bLoadOK = bLoadOK && (mFluidFinalRenderProgID = OGLU_LoadProgram("FluidFinal",  getShaderSource("standard.vs"),  getShaderSource("fluid_final_render.fs")));
    bLoadOK = bLoadOK && (mStandardCopyProgID     = OGLU_LoadProgram("StdCopy",     getShaderSource("standard.vs"),  getShaderSource("standard_copy.fs")));
    bLoadOK = bLoadOK && (mStandardColorProgID    = OGLU_LoadProgram("StdColor",    getShaderSource("standard.vs"),  getShaderSource("standard_color.fs")));
    bLoadOK = bLoadOK && (mVisibleScanProgID      = OGLU_LoadProgram("VisScan",     getShaderSource("vis_scan.cms"), GL_COMPUTE_SHADER));
    bLoadOK = bLoadOK && (mResetGridProgID        = OGLU_LoadProgram("GridReset",   getShaderSource("grid_reset.cms"), GL_COMPUTE_SHADER));
    bLoadOK = bLoadOK && (mResetGridChainProgID   = OGLU_LoadProgram("GridChReset", getShaderSource("grid_chain_reset.cms"), GL_COMPUTE_SHADER));
    bLoadOK = bLoadOK && (mBuildGridProgID        = OGLU_LoadProgram("GridBuild",   getShaderSource("grid_build.cms"), GL_COMPUTE_SHADER));

    // Set-up Render-Stange-Inspector
    OGSI_Setup(mStandardCopyProgID);

    // Setup Unitforms
    if (bLoadOK)
    {
        glUseProgram(g_SelectedProgram = mVisibleScanProgID);
        glUniform1i(UniformLoc("destTex"),    0);

        glUseProgram(g_SelectedProgram = mResetGridChainProgID);
        glUniform1i(UniformLoc("grid_chain"), 1);

        glUseProgram(g_SelectedProgram = mResetGridProgID);
        glUniform1i(UniformLoc("grid"),       2);

        glUseProgram(g_SelectedProgram = mBuildGridProgID);
        glUniform1i(UniformLoc("grid_chain"), 1);
        glUniform1i(UniformLoc("grid"),       2);

        glUseProgram(g_SelectedProgram = mFluidDepthSmoothProgID);
        glUniform1i(UniformLoc("grid_chain"), 1);
        glUniform1i(UniformLoc("grid"),       2);
    }

    return bLoadOK;
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

GLuint CVisual::createSharingTexture(const GLsizei width, const GLsizei height) const
{
    GLuint texID = OGLU_GenerateTexture(width, height, GL_RGBA32F);
    return texID;
}

void CVisual::swapTargets()
{
    FBO* pTmp = pNextTarget;
    pNextTarget = pPrevTarget;
    pPrevTarget = pTmp;
}

void CVisual::renderFluidSmoothDepth()
{
    pNextTarget->SetAsDrawTarget();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //ZPR_ModelViewMatrix = glm::inverse(ZPR_InvModelViewMatrix);
    ZPR_InvModelViewMatrix = glm::inverse(ZPR_ModelViewMatrix);
    glUseProgram(g_SelectedProgram = mFluidDepthSmoothProgID);
    OGLU_BindTextureToUniform("depthTexture", 0, pPrevTarget->pColorTextureId[0]);
    OGLU_BindTextureToUniform("particlesPos", 1, mSimulation->mSharedParticlesPos);
    OGLU_BindTextureToUniform("visParticles", 2, mImgParticleVisible);
    glUniformMatrix4fv(UniformLoc("iMV_Matrix"), 1, GL_FALSE, glm::value_ptr(ZPR_InvModelViewMatrix));
    glUniformMatrix4fv(UniformLoc("MV_Matrix"),  1, GL_FALSE, glm::value_ptr(ZPR_ModelViewMatrix));
    glUniformMatrix4fv(UniformLoc("Proj_Matrix"), 1, GL_FALSE, glm::value_ptr(mProjectionMatrix));
    glUniform2f(UniformLoc("depthRange"), 0.1f, 10.0f);
    glUniform2fv(UniformLoc("invFocalLen"), 1, glm::value_ptr(mInvFocalLen));
    glUniform1f(UniformLoc("smoothLength"), Params.h);
    glUniform1i(UniformLoc("particlesCount"), Params.particleCount);
    glUniform1ui(UniformLoc("currentCycleID"), mCycleID);

    OGLU_RenderQuad(0, 0, 1.0, 1.0);
    swapTargets();
}

void CVisual::drawFullScreenTexture(GLuint textureID)
{
        // Select shader
        glUseProgram(g_SelectedProgram = mStandardCopyProgID);

        // Update uniforms
        OGLU_BindTextureToUniform("ImageSrc", 0, textureID);
        glUniform1f(UniformLoc("offset"), 0.0f);
        glUniform1f(UniformLoc("gain"),   1.0f);
    
        // render quad
        OGLU_RenderQuad(0, 0, 1.0, 1.0);

}

void CVisual::scanForVisible(GLuint inputTexture)
{
    // Setup program
    glUseProgram(g_SelectedProgram = mVisibleScanProgID);
    glUniform1ui(UniformLoc("cycleID"), mCycleID);
    OGLU_BindTextureToUniform("inputTexture", 0, inputTexture);
    glDispatchCompute(mFrameWidth, mFrameHeight, 1);
}

void CVisual::buildGrid()
{
    // Reset grid
    glUseProgram(g_SelectedProgram = mResetGridProgID);
    glDispatchCompute(2048, 2048, 1);

    // Reset grid chains
    glUseProgram(g_SelectedProgram = mResetGridChainProgID);
    glDispatchCompute(2048, DivCeil(Params.particleCount, 2048), 1);

    // Build Grid
    glUseProgram(g_SelectedProgram = mBuildGridProgID);
    glUniform1ui(UniformLoc("currentCycleID"), mCycleID);
    glUniform1ui(UniformLoc("particlesCount"), Params.particleCount);
    glUniform1f (UniformLoc("smoothLength"), Params.h);
    OGLU_BindTextureToUniform("visiblityMap", 0, mImgParticleVisible);
    OGLU_BindTextureToUniform("particlesPos", 1, mSimulation->mSharedParticlesPos);
    
    glDispatchCompute(2048, DivCeil(Params.particleCount, 2048), 1);

    if (OGSI_InspectTexture(mImgGrid, "Build Grid", 1, 0)) return;
}

void CVisual::renderFluidFinal(GLuint depthTexture)
{
    // Copy Result to screen buffer
    g_ScreenFBO.SetAsDrawTarget();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(g_SelectedProgram = mFluidFinalRenderProgID);
    OGLU_BindTextureToUniform("depthTexture", 0, depthTexture);
    glUniformMatrix4fv(UniformLoc("invProjectionMatrix"), 1, GL_FALSE, glm::value_ptr(mInvProjectionMatrix));
    glUniformMatrix4fv(UniformLoc("iMV_Matrix"), 1, GL_FALSE, glm::value_ptr(ZPR_InvModelViewMatrix));
    glUniformMatrix4fv(UniformLoc("MV_Matrix"),  1, GL_FALSE, glm::value_ptr(ZPR_ModelViewMatrix));
    glUniform2f(UniformLoc("depthRange"), 0.1f, 10.0f);
    glUniform2fv(UniformLoc("invFocalLen"), 1, glm::value_ptr(mInvFocalLen));

    OGLU_RenderQuad(0, 0, 1.0, 1.0);
}

void CVisual::renderParticles()
{
    // Increment cycleID
    mCycleID++;

    // start buffer inspection
    OGSI_StartCycle();

    // Clear target
    pNextTarget->SetAsDrawTarget();

    OGLU_StartTimingSection("Clear FBO");
    glClearColor(0, 0, 0, 0);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Setup Particle drawing
    OGLU_StartTimingSection("Draw Particles");
    glUseProgram(g_SelectedProgram = mParticleProgID);

    // Setup uniforms
    glUniformMatrix4fv(UniformLoc("projectionMatrix"), 1, GL_FALSE, glm::value_ptr(mProjectionMatrix));
    glUniformMatrix4fv(UniformLoc("modelViewMatrix"),  1, GL_FALSE, glm::value_ptr(ZPR_ModelViewMatrix));
    glUniform1i(UniformLoc("particleCount"),    Params.particleCount);
    glUniform1i(UniformLoc("renderMethod"),     UICmd_RenderMode);
    glUniform1f(UniformLoc("widthOfNearPlane"), mWidthOfNearPlane);
    glUniform1f(UniformLoc("pointSize"),        Params.particleRenderSize);

    // Bind positions buffer
    glEnableVertexAttribArray(AttribLoc("position"));
    glBindBuffer(GL_ARRAY_BUFFER, mSimulation->mPositionsPingSBO);
    glVertexAttribPointer(AttribLoc("position"), 4, GL_FLOAT, GL_FALSE, 0, 0);

    glBindFragDataLocation(g_SelectedProgram, 0, "colorOut");

    // Draw particles
    glDrawArrays(GL_POINTS, 0, Params.particleCount);
    swapTargets();

    // Unbind buffer
    glDisableVertexAttribArray(AttribLoc("position"));
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Inspect
    if (OGSI_InspectTexture(pPrevTarget->pColorTextureId[0],  "Draw Particles [texture]", 1, 0)) return;
    if (OGSI_InspectTexture(pPrevTarget->pDepthTextureId,     "Draw Particles [depth]",   4, -3)) return;
    if (OGSI_InspectTexture(mSimulation->mSharedParticlesPos, "ParticlePos",   1, 0)) return;

    // Smooth fluid depth
    GLuint depthTexture = pPrevTarget->pDepthTextureId;
    if (UICmd_RenderMode == 0/*Smooth*/)
    {
        // Scan for visible particles
        OGLU_StartTimingSection("Scan for visible");
        scanForVisible(pPrevTarget->pColorTextureId[0]);
        if (OGSI_InspectTexture(mImgParticleVisible,    "VisImg",   1, 0)) return;

        // Build grid
        OGLU_StartTimingSection("Build visual grid");
        buildGrid();

        // Render depth
        OGLU_StartTimingSection("Render Depth Smooth");
        renderFluidSmoothDepth();
        if (OGSI_InspectTexture(pPrevTarget->pColorTextureId[0], "DepthSmooth [texture]", 1, 0)) return;

        // Change next step input to smoothed depth
        depthTexture = pPrevTarget->pColorTextureId[0];

        // Final fluid render
        OGLU_StartTimingSection("Final Render");
        renderFluidFinal(depthTexture);
    }
    else
    {
        // Copy to screen
        OGLU_StartTimingSection("Copy to Screen");

        // Select output
        g_ScreenFBO.SetAsDrawTarget();

        // Copy texture
        drawFullScreenTexture(pPrevTarget->pColorTextureId[0]);
    }

    // Unselect shader
    glUseProgram(0);
}

void CVisual::presentToScreen()
{
    glfwSwapBuffers(mWindow);
}

