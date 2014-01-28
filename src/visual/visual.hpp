#ifndef _VISUAL_HPP
#define _VISUAL_HPP

#define GL_GLEXT_PROTOTYPES // Necessary for vertex buffer

#include "../Precomp_OpenGL.h"
#include "../OGL_Utils.h"
#include "../hesp.hpp"
#include "../Resources.hpp"
#include "../Simulation.hpp"

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <AntTweakBar.h>

#include <string>
using std::string;


// Macros
static const unsigned int DIM = 3;


enum ParticleRenderType
{
    VELOCITY,
    SORTING
};


/**
 *  \brief  CVisual
 */
class CVisual
{
public:
    void KeyEvent(GLFWwindow *window, int key, int scancode, int action, int mods);

private:
    GLvoid swapTargets();

    GLvoid renderFluidSmoothDepth();

    GLvoid renderFluidFinal(GLuint depthTexture);

public:
    // Default constructor
    CVisual (const int windowWidth = 800, const int windowHeight = 600);

    ~CVisual(); // Destructor

    void initWindow(const string windowname = "GLFW Window");

    static GLuint loadShaders(const string &vertexFilename, const string &fragmentFilename);

    const string *ShaderFileList();

    bool initShaders();

    GLvoid setupProjection();

    GLvoid initSystemVisual(Simulation &sim);

    GLvoid renderParticles();

    GLvoid presentToScreen();

    GLuint createSharingBuffer(const GLsizei size) const;

    GLuint createSharingTexture(const GLsizei width, const GLsizei height) const;

public:
    GLFWwindow *mWindow;

    bool UICmd_GenerateWaves;
    bool UICmd_ResetSimulation;
    bool UICmd_PauseSimulation;
    bool UICmd_FriendsHistogarm;
    int  UICmd_DrawMode;
    bool UICmd_SmoothDepth;

private:
    // Window stuff
    int mWindowWidth;
    int mWindowHeight;
    int mFrameWidth;
    int mFrameHeight;

    // Rendering
    FBO *pPrevTarget;
    FBO *pNextTarget;
    FBO *pFBO_Thickness;

    GLuint mParticleProgID;
    GLuint mFluidFinalRenderProgID;
    GLuint mFluidDepthSmoothProgID;
    GLuint mStandardCopyProgID;
    GLuint mStandardColorProgID;

    // Projection related
    float mWidthOfNearPlane;
    glm::vec2 mInvFocalLen;
    glm::mat4 mProjectionMatrix;
    glm::mat4 mInvProjectionMatrix;

    // System sizes
    GLuint mSystemBufferID;

    Simulation *mSimulation;

    GLuint mWallTexture;

    ParticleRenderType mRenderType;
};

#endif // _VISUAL_HPP
