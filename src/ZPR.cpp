
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <cmath>
#include "ZPR.h"

// Model view matrices
glm::mat4x4 ZPR_ModelViewMatrix;
glm::mat4x4 ZPR_InvModelViewMatrix;

// Camera setup
glm::vec3 camLookAt;
glm::vec3 camEye;
glm::vec3 camUp;

float prevMouseX;
float prevMouseY;

// View setups
glm::mat4x4 ProjMatrix;
glm::mat4x4 InvProjMatrix;
glm::ivec4  Viewport;

// UI State
bool DragActive = false;
bool CtrlPressed = false;

void ComputeMVMatrix()
{
    // Build and get matrix
    ZPR_ModelViewMatrix = glm::lookAt(camEye, camLookAt, camUp);
    ZPR_InvModelViewMatrix = glm::inverse(ZPR_ModelViewMatrix);
}

void ZPR_SetupView(glm::mat4x4 projectionMatrix, glm::ivec4 *viewport)
{
    ProjMatrix = projectionMatrix;
    InvProjMatrix = glm::inverse(projectionMatrix);

    // Setup
    camLookAt = glm::vec3(1.097321030, -0.293091655, 0.234185457);
    camEye    = glm::vec3(0.370147407,  0.882159948, 1.898292300);
    camUp     = glm::vec3(0.217547148,  0.839536190, 0.497847676);
    ZPR_RollReset();
    ComputeMVMatrix();

    // Store view port
    if (viewport != NULL)
        Viewport = *viewport;
    else
        glGetIntegerv(GL_VIEWPORT, &Viewport[0]);
}

bool ZPR_ViewportToWorld(glm::vec3 View, glm::vec3 &World)
{
    if (Viewport[2] == 0)
        return false;

    // Viewport -> NDC -> Clip
    glm::vec4 ClipSpacePoint;
    ClipSpacePoint.x =     2 * View[0] / Viewport[2] - 1;
    ClipSpacePoint.y = 1 - 2 * View[1] / Viewport[3];
    ClipSpacePoint.z = View[2] * 2 - 1;
    ClipSpacePoint.w = 1;

    glm::vec4 ViewPoint = InvProjMatrix * ClipSpacePoint;

    // Transfrom Homogeneous -> Affine
    glm::vec4 Affline;
    Affline.x = ViewPoint[0] / ViewPoint[3];
    Affline.y = ViewPoint[1] / ViewPoint[3];
    Affline.z = ViewPoint[2] / ViewPoint[3];
    Affline.w = 1;

    // Transfrom View -> World
    glm::vec4 Result = ZPR_InvModelViewMatrix * Affline;

    // Store result
    World[0] = Result[0];
    World[1] = Result[1];
    World[2] = Result[2];

    return true;
}

void ZPR_WorldToViewport(glm::vec3 World, glm::vec3 &View)
{
    glm::vec4 WorldPoint;
    WorldPoint.x = World[0];
    WorldPoint.y = World[1];
    WorldPoint.z = World[2];
    WorldPoint.w = 1;

    // Transfrom World -> View
    glm::vec4 ViewPoint = ZPR_ModelViewMatrix * WorldPoint;

    // View -> Clip (NDC)
    glm::vec4 ClipSpacePoint = ProjMatrix * ViewPoint;

    // Transfrom Homogeneous -> Affine
    ClipSpacePoint[0] = ClipSpacePoint[0] / ClipSpacePoint[3];
    ClipSpacePoint[1] = ClipSpacePoint[1] / ClipSpacePoint[3];
    ClipSpacePoint[2] = ClipSpacePoint[2] / ClipSpacePoint[3];
    ClipSpacePoint[3] = 1;

    // Viewport -> NDC -> Clip
    View[0] = (ClipSpacePoint[0] + 1) / 2 * Viewport[2];
    View[1] = (1 - ClipSpacePoint[1]) / 2 * Viewport[3];
    View[2] = (ClipSpacePoint[2] + 1) / 2;// *UInt32.MaxValue;
}

void ZPR_Reset()
{
    camLookAt = glm::vec3(0, 0, 0);
    camEye    = glm::vec3(0, 0, 7);
    camUp     = glm::vec3(0, 1, 0);
    ComputeMVMatrix();
}

void ZPR_RollReset()
{
    glm::vec3 newUp    = glm::vec3(0, 1, 0);
    glm::vec3 viewDir  = glm::normalize(camEye - camLookAt);
    glm::vec3 sideAxis = glm::normalize(glm::cross(newUp, viewDir));
    camUp              = glm::normalize(glm::cross(viewDir, sideAxis));
    ComputeMVMatrix();
}

template <typename T> int sgn(T val)
{
    return (T(0) < val) - (val < T(0));
}

void ZPR_EventMouseButtonGLFW(GLFWwindow *window, int button, int action, int mods)
{
    (void)button;
    (void)action;
    (void)mods;

    DragActive = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)   == GLFW_PRESS) ||
                 (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) ||
                 (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)  == GLFW_PRESS);
}

void ZPR_EventMousePosGLFW(GLFWwindow *window, double x, double y)
{
    // get mouse delta
    float dx = (float)x - prevMouseX;
    float dy = (float)y - prevMouseY;
    prevMouseX = (float)x;
    prevMouseY = (float)y;

    // Make sure there is an active drag
    if (!DragActive)
        return;

    // Compute the drag-delta vector (in Screen space)
    glm::vec3 ScreenDelta;
    ScreenDelta[0] = dx;
    ScreenDelta[1] = dy;
    ScreenDelta[2] = 0;

    // Handle Panning
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
    {
        // Get the depth of Pivot (in depth [0..1] range)
        glm::vec3 PtInView;
        ZPR_WorldToViewport(camLookAt, PtInView);

        // Transfom screen offset to world offset in Pivot depth
        PtInView[0] -= ScreenDelta[0];
        PtInView[1] -= ScreenDelta[1];

        glm::vec3 NewPivot;
        ZPR_ViewportToWorld(PtInView, NewPivot);

        // Compute offset
        glm::vec3 ViewOffset = NewPivot - camLookAt;

        // Update Eye pos
        camEye += ViewOffset;

        // Update Pivot
        camLookAt = NewPivot;
    }
    else
    {
        // Select Rotation center
        // RotationCenter can be either the eye point or ther pivot point
        float Direction = -1;
        glm::vec3 RotationCenter = camLookAt;
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        {
            RotationCenter = camEye;
            Direction = 1;
        }

        // Find rotation angle and axis
        glm::vec3 ViewDir = camEye - camLookAt;
        glm::vec3 RotAxis = glm::cross(camUp, ViewDir);

        // Roll rotation
        if (0/*Control Pressed*/)
        {
            RotAxis = ViewDir;
            ScreenDelta[0] = 0;
        }

        // Normalize Rotation axis
        RotAxis = glm::normalize(RotAxis);

        // Build rotation quaternions
        glm::quat q_rotation1 = glm::angleAxis<float>((float)(Direction * ScreenDelta[0] / 5.0f), camUp  );
        glm::quat q_rotation2 = glm::angleAxis<float>((float)(Direction * ScreenDelta[1] / 5.0f), RotAxis );
        q_rotation1 = q_rotation2 * q_rotation1;

        // rotate Eyelocation
        glm::vec3 TempPoint = camEye - RotationCenter;
        TempPoint = glm::rotate(q_rotation1, TempPoint);
        camEye = TempPoint + RotationCenter;

        // rotate Pivot
        TempPoint = camLookAt - RotationCenter;
        TempPoint = glm::rotate(q_rotation1, TempPoint);
        camLookAt = TempPoint + RotationCenter;

        // rotate up vector
        camUp = glm::rotate(q_rotation1, camUp);
    }

    ZPR_RollReset();
    ComputeMVMatrix();
}

void ZPR_EventMouseWheelGLFW(GLFWwindow *window, double delta)
{
    (void)window;

    // Compute factor
    if (delta == 0)
        return;

    int nSteps   = (int)abs(delta);
    float Factor = (powf(1.1f, (float)(int)nSteps) - 1.f) * sgn(delta);

    // Compute offset vector
    glm::vec3 Eye_to_Pivot = camEye - camLookAt;

    glm::vec3 OffsetVec = Eye_to_Pivot * Factor;

    // Offset Eye and Pivot
    if (false/*Shift pressed*/)
    {
        camLookAt -= OffsetVec;
    }
    else
    {
        camEye += OffsetVec;
    }

    ComputeMVMatrix();
}


