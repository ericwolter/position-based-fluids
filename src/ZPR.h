#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

extern glm::mat4x4 ZPR_ModelViewMatrix;
extern glm::mat4x4 ZPR_InvModelViewMatrix;

void ZPR_SetupView(glm::mat4x4 projectionMatrix, glm::ivec4 *viewport);

void ZPR_EventMouseButtonGLFW(GLFWwindow *window, int button, int action, int mods);
void ZPR_EventMousePosGLFW(GLFWwindow *window, double x, double y);
void ZPR_EventMouseWheelGLFW(GLFWwindow *window, double delta);

void ZPR_Reset();
void ZPR_RollReset();
