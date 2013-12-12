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

    void initWindow(const string windowname = "GLFW Window");

    static GLuint loadShaders(const string &vertexFilename, const string &fragmentFilename);

    GLvoid initParticlesVisual();

    GLvoid initSystemVisual(Simulation &sim);

    GLvoid visualizeParticles();

	GLvoid presentToScreen();

	GLuint createSharingBuffer(const GLsizei size) const;

public:
    GLFWwindow* mWindow;

	bool UICmd_GenerateWaves;
    bool UICmd_ResetSimulation;
    bool UICmd_PauseSimulation;
	bool UICmd_FriendsHistogarm;
	bool UICmd_ColorMethod;

private:
    // Window stuff
    int mWidth;
    int mHeight;

    GLuint mParticleProgramID;

	// Projection related
	float mWidthOfNearPlane;
	glm::mat4 mProjectionMatrix;

    // System sizes
    GLuint mSystemBufferID;

    Simulation *mSimulation;

    GLuint mWallTexture;

    ParticleRenderType mRenderType;
};

#endif // _VISUAL_HPP
