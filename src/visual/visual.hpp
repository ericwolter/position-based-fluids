#ifndef _VISUAL_HPP
#define _VISUAL_HPP

#define GL_GLEXT_PROTOTYPES // Necessary for vertex buffer

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunneeded-internal-declaration"
#include <glm/gtx/string_cast.hpp>
#pragma GCC diagnostic pop

#include <AntTweakBar.h>

#include "../hesp.hpp"
#include "../DataLoader.hpp"
#include "../Simulation.hpp"

#include <string>


// Macros
static const unsigned int DIM = 3;


using std::string;

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

public:
    // Default constructor
    CVisual (const int width = 800, const int height = 600);

    ~CVisual(); // Destructor

    /**
     *  \brief  Initializes window.
     */
    void initWindow(const string windowname = "GLFW Window");

    /**
     *  \brief  Loads vertex and fragment shader.
     */
    static GLuint loadShaders(const string &vertexFilename, const string &fragmentFilename);

    GLvoid initParticlesVisual(ParticleRenderType renderType);

    /**
     *  \brief  Initializes system sizes, textures and buffer objects.
     */
    GLvoid initSystemVisual(Simulation &sim);

    GLvoid visualizeParticles(void);

    GLuint createSharingBuffer(const GLsizei size) const;

    /**
     *  \brief  Checks if we want to generate waves with 'G'.
     */
    void checkInput();

    void DrawTweekBar();

	void DrawPerformanceGraph();

    glm::vec3 resolveCamPosition(void) const;

    glm::mat4 calcLookAtMatrix(const glm::vec3 &cameraPt, const glm::vec3 &lookPt, const glm::vec3 &upPt) const;

public:
    bool UICmd_GenerateWaves;
    bool UICmd_ResetSimulation;
    bool UICmd_PauseSimulation;

private:
    // Window stuff
    int mWidth;
    int mHeight;

    GLuint mProgramID; /**< Program ID for OpenGL shaders */
    GLuint mParticleProgramID;

    GLFWwindow *mWindow;

    // Tweakbar
    TwBar *tweakBar;
    double mTotalSimTime;

    // System sizes
    GLuint mSystemBufferID;

    GLuint mPositionAttrib;
    GLuint mNormalAttrib;
    GLuint mTexcoordAttrib;

    // Uniforms...
    GLint mCameraToClipMatrixUnif;
    GLint mWorldToCameraMatrixUnif;
    GLint mModelToWorldMatrixUnif;
    GLint mTextureUnif;

    // Camera stuff
    glm::vec3 mCamTarget;
    glm::vec3 mCamSphere;

    Simulation *mSimulation;

    GLuint mParticlePositionAttrib;
    GLint mParticleCameraToClipMatrixUnif;
    GLint mParticleWorldToCameraMatrixUnif;
    GLint mParticleModelToWorldMatrixUnif;

    GLuint mWallTexture;

    ParticleRenderType mRenderType;

};

#endif // _VISUAL_HPP
