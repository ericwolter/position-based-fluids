
#include "UIManager.h"
#include "ParamUtils.hpp"
#include "ZPR.h"
#include "OGL_RenderStageInspector.h"

#include <AntTweakBar.h>
#include "../../lib/AntTweakBar/src/TwPrecomp.h"
#include "../../lib/AntTweakBar/src/TwMgr.h"
#include "../../lib/AntTweakBar/src/TwOpenGLCore.h"

CTwGraphOpenGLCore tw;
void *twFont;

int         mWindowWidth;
int         mWindowHeight;
int         mFrameWidth;
int         mFrameHeight;

CTexFont    *mDisplayFont = g_DefaultSmallFont;

GLFWwindow *mWindow;
CVisual    *mRenderer;
Simulation *mSim;
TwBar      *mTweakBar;
double      mTotalSimTime;
float       mMousePosX;
float       mMousePosY;

int  UIM_SelectedInspectionStage;
bool UIM_SaveInspectionStage = false;
int  UIM_RenderMode;

bool mIsFirstCycle = true;
int  Prev_OGSI_Stages_Count = 0;


void MouseButtonCB(GLFWwindow *window, int button, int action, int mods)
{
    TwEventMouseButtonGLFW(button, action);
    ZPR_EventMouseButtonGLFW(window, button, action, mods);
}

void MousePosCB(GLFWwindow *window, double x, double y)
{
    // Convert Window -> Scaled -> Frame
    mMousePosX = (float)x / mWindowWidth;
    mMousePosY = (float)y / mWindowHeight;
    int FrameX = (int)(mMousePosX * mFrameWidth);
    int FrameY = (int)(mMousePosY * mFrameHeight);

    // Notify Tweekbar
    bool bProccesed = TwEventMousePosGLFW(FrameX, FrameY) != 0;

    // Notify ZPR
    if (!bProccesed)
        ZPR_EventMousePosGLFW(window, FrameX, FrameY);
}

void KeyFunCB(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)window;
    (void)scancode;
    (void)mods;

    TwEventKeyGLFW(key, action);
    TwEventCharGLFW(key, action);
}

void MouseScrollCB(GLFWwindow *window, double x, double y)
{
    (void)x;

    TwEventMouseWheelGLFW((int)y);
    ZPR_EventMouseWheelGLFW(window, y);
}

void TW_CALL Reset(void *clientData)
{
    ((CVisual *)clientData)->UICmd_ResetSimulation = true;
}

void TW_CALL DumpParticlesData(void *clientData)
{
    ((Simulation*)clientData)->bDumpParticlesData = true;
}

void TW_CALL SaveInspection(void *clientData)
{
    UIM_SaveInspectionStage = true;
}

void TW_CALL View_Reset(void *clientData)
{
    (void)clientData;

    ZPR_Reset();
}

void UIManager_Init(GLFWwindow *window, CVisual *pRenderer, Simulation *pSim)
{
    // Save parameters
    mWindow   = window;
    mRenderer = pRenderer;
    mSim      = pSim;

    /* Set GLFW event callbacks */
    // Currently AntTweakBar DOES NOT work out of the box with GLFW3.
    // For now custom event callbacks have to be implemented.
    // see: http://blog-imgs-37.fc2.com/s/c/h/schrlab/TwGLFW_cpp.txt
    glfwSetMouseButtonCallback(mWindow, MouseButtonCB );
    glfwSetCursorPosCallback(mWindow, MousePosCB);
    glfwSetScrollCallback(mWindow, MouseScrollCB );
    glfwSetKeyCallback(mWindow, KeyFunCB);

    // Get window and frame size
    glfwGetFramebufferSize(mWindow, &mFrameWidth, &mFrameHeight);
    glfwGetWindowSize(mWindow, &mWindowWidth, &mWindowHeight);

    // Init TweekBar
    TwInit(TW_OPENGL_CORE, NULL);
    TwWindowSize(mWindowWidth, mWindowHeight);
    mDisplayFont = mWindowWidth < mFrameWidth? g_DefaultNormalFont : g_DefaultSmallFont;

    mTweakBar = TwNewBar("PBFTweak");
    TwDefine(" PBFTweak size='240 400' "); 

    // Define enums
    TwType enumRenderViewMode = TwDefineEnumFromString("enumRenderViewMode", "Velocity,Sorting");
    TwType enumInstStage      = TwDefineEnum("enumInspectionStages", NULL, 0);

    // Sim related
    TwAddButton(mTweakBar, "Reset",             Reset,              mRenderer,                         "group='Sim Controls' key=R");
    TwAddVarRW (mTweakBar, "Pause Sim",         TW_TYPE_BOOLCPP,    &mRenderer->UICmd_PauseSimulation, "group='Sim Controls' key=P");
    TwAddVarRW (mTweakBar, "Generate waves",    TW_TYPE_BOOLCPP,    &mRenderer->UICmd_GenerateWaves,   "group='Sim Controls' key=W");

    // View related
    TwAddButton(mTweakBar, "View Reset",             View_Reset,         mRenderer,                     "group='View Controls' key=Z");
    TwAddVarRW (mTweakBar, "Render Mode",            enumRenderViewMode, &mRenderer->UICmd_ColorMethod, "group='View Controls'");

    // Sim debugging related
    TwAddVarRW (mTweakBar, "Friends Histogram",      TW_TYPE_BOOLCPP,   &mRenderer->UICmd_FriendsHistogarm, "group='Sim Debugging'");
    TwAddButton(mTweakBar, "Dump Particles Data",    DumpParticlesData, mSim,                               "group='Sim Debugging'");

    // View debugging related
    TwAddButton(mTweakBar, "Save Inspection",       SaveInspection,   mRenderer, "group='View Debugging'");
    TwAddVarRO(mTweakBar, "Mouse.X", TW_TYPE_FLOAT, &mMousePosX, "precision=4 group='View Debugging'");
    TwAddVarRO(mTweakBar, "Mouse.Y", TW_TYPE_FLOAT, &mMousePosY, "precision=4 group='View Debugging'");
    TwAddVarRO(mTweakBar, "Data.X", TW_TYPE_FLOAT, &OGSI_SamplePixelData.x, "precision=4 group='View Debugging'");
    TwAddVarRO(mTweakBar, "Data.Y", TW_TYPE_FLOAT, &OGSI_SamplePixelData.y, "precision=4 group='View Debugging'");
    TwAddVarRO(mTweakBar, "Data.Z", TW_TYPE_FLOAT, &OGSI_SamplePixelData.z, "precision=4 group='View Debugging'");
    TwAddVarRO(mTweakBar, "Data.W", TW_TYPE_FLOAT, &OGSI_SamplePixelData.w, "precision=4 group='View Debugging'");
    TwAddVarRW(mTweakBar, "InspectionStage", enumInstStage, &UIM_SelectedInspectionStage, "label='Inspection Stage' group='View Debugging'");

    // Timing
    TwAddVarRO(mTweakBar, "Total sim time", TW_TYPE_DOUBLE, &mTotalSimTime, "precision=2 group=Stats");

    // Init drawing ATB
    g_TwMgr->m_GraphAPI = TW_OPENGL_CORE;
    tw.Init();
    TwGenerateDefaultFonts(1.0);
    twFont = tw.NewTextObj();
}

void DrawPerformanceGraph()
{
    // Compute sizes
    const int ViewWidth    = mFrameWidth;
    const int ViewHeight   = mFrameHeight;
    const int BarHeight    = (int)(mFrameHeight * 0.03);
    const int BarWidth     = (int)(ViewWidth * 0.9);
    const int BarTop       = ViewHeight - BarHeight * 2;
    const int BarBottom    = BarTop + BarHeight;
    const int BarLeft      = (ViewWidth - BarWidth) / 2;
    // const int BarRight     = ViewWidth - BarLeft;

    tw.BeginDraw(ViewWidth, ViewHeight);

    // Find total time
    double totalTime = 0;
    for (size_t i = 0; i < mSim->PerfData.Trackers.size(); i++)
        totalTime += mSim->PerfData.Trackers[i]->total_time;

    // Draw
    const color32 StartClr[] = {0xFF0000, 0x00FF00, 0x0000FF};
    const color32 EndClr[]   = {0xA00000, 0x00A000, 0x0000A0};
    const color32 Alpha      = 0x80000000;
    int prevX = BarLeft;
    double accTime = 0;
    for (size_t i = 0; i < mSim->PerfData.Trackers.size(); i++)
    {
        PM_PERFORMANCE_TRACKER *pTracker = mSim->PerfData.Trackers[i];

        // Add segment time
        accTime += pTracker->total_time;

        // Compute screen position
        int newX = BarLeft + (int)(0.5f + accTime * BarWidth / totalTime);

        // draw bar
        tw.DrawRect(prevX, BarTop, newX, BarBottom, Alpha | StartClr[i % 3], Alpha | EndClr[i % 3], Alpha | StartClr[i % 3], Alpha | EndClr[i % 3]);

        // Do we have space for text?
        if (newX - prevX > 4)
        {
            // Draw name text
            const int NameVertOffset = -2;

            tw.BuildText(twFont, &pTracker->eventName, NULL, NULL, 1, mDisplayFont, 0, 0);
            tw.SetScissor(prevX + 2, BarTop + NameVertOffset, newX - prevX, BarHeight);
            tw.DrawText(twFont, prevX + 2, BarTop + NameVertOffset, 0xffffffffu, 0);

            // Build ms text
            char tmp[128];
            sprintf(tmp, "%3.2f", pTracker->total_time);
            string str(tmp);

            // Draw time text
            const int MSVertOffset = 9;
            tw.BuildText(twFont, &str, NULL, NULL, 1, mDisplayFont, 0, 0);
            tw.SetScissor(prevX + 2, BarTop + MSVertOffset, newX - prevX, BarHeight);
            tw.DrawText(twFont, prevX + 2, BarTop + MSVertOffset, 0xffffffffu, 0);

            // Remove Scissor
            tw.SetScissor(0, 0, 0, 0);
        }

        // Update prev X
        prevX = newX;
    }

    tw.EndDraw();
}

void DrawFriendsHistogram()
{
    // Histogram
    int Histogram[32];
    memset(Histogram, 0, sizeof(Histogram));

    // Compute histogram data
    for (unsigned int iPart = 0; iPart < Params.particleCount; iPart++)
        for (unsigned int iCircle = 0; iCircle < Params.friendsCircles; iCircle++)
            Histogram[iCircle] += mSim->mFriendsList[iPart * (Params.friendsCircles * (1 + Params.particlesPerCircle)) + iCircle];

    // Compute total friends
    int histTotal = 0;
    for (unsigned int iHist = 0; iHist < Params.friendsCircles; iHist++)
        histTotal += Histogram[iHist];

    // Define coordinates
    const int ViewWidth    = mFrameWidth;
    const int ViewHeight   = mFrameHeight;
    const int HistWidth    = 100;
    const int HistHeight   = 100;
    const int HistTop      = 10;
    const int HistBottom   = HistTop + HistHeight;
    const int HistLeft     = (int)(ViewWidth * 0.95 - HistWidth);
    const int HistRight    = HistLeft + HistWidth;
    const int BarsWidth    = HistWidth / Params.friendsCircles;

    tw.BeginDraw(ViewWidth, ViewHeight);

    // Draw
    tw.DrawRect(HistLeft - 2, HistTop - 2, HistRight + 2, HistBottom + 2, 0x50ffffff);

    for (unsigned int iHist = 0; iHist < Params.friendsCircles; iHist++)
    {
        // Compute bar height
        float barHeight = (float)Histogram[iHist] / histTotal;

        // Compute screen pos
        int   screenY1 = HistBottom - (int)(barHeight * HistHeight) - 1;
        int   screenX1 = 1 + HistLeft + BarsWidth * iHist;

        // Draw
        tw.DrawRect(screenX1, screenY1, screenX1 + BarsWidth - 2, HistBottom, 0xff00ffff);

        // Build ms text
        char tmp[128];
        sprintf(tmp, "%2.0f%%", barHeight * 100);
        string str(tmp);

        // Draw time text
        tw.BuildText(twFont, &str, NULL, NULL, 1, mDisplayFont, 0, 0);
        tw.DrawText(twFont, screenX1, screenY1 - 10, 0xffffffffu, 0);

        // friends text
        sprintf(tmp, "%2.0f", (float)Histogram[iHist] / Params.particleCount);
        str = tmp;

        // Draw time text
        tw.BuildText(twFont, &str, NULL, NULL, 1, mDisplayFont, 0, 0);
        tw.DrawText(twFont, screenX1 + 2, HistBottom - 12, 0xffff0000u, 0);
    }

    tw.EndDraw();
}

void DrawAntTweakBar()
{
    if (mIsFirstCycle)
    {
        // Create performance rows
        for (size_t i = 0; i < mSim->PerfData.Trackers.size(); i++)
        {
            // create row
            string title = "  " + mSim->PerfData.Trackers[i]->eventName;
            void* pValue = &mSim->PerfData.Trackers[i]->total_time;
            TwAddVarRO(mTweakBar, title.c_str(),  TW_TYPE_DOUBLE,  pValue, "precision=2 group=Stats");
        }

        // Make sure Stats group is folded
        TwDefine(" PBFTweak/Stats opened=false ");
    }

    // Check if we need to refresh OGSI stage list
    if (Prev_OGSI_Stages_Count != OGSI_Stages_Count)
    {
        // Save OGSI_Stages_Count
        Prev_OGSI_Stages_Count = OGSI_Stages_Count;

        // Create StageInspection combo options string
        std::ostringstream comboOptions; 
        comboOptions << "PBFTweak/InspectionStage enum='0 {Final},";
        for (int i = 1; i <= OGSI_Stages_Count; i++)
            comboOptions << i << " " << " {" << OGSI_Stages[i-1] << "}" << (i != OGSI_Stages_Count ? "," : "");
        
        // Replace last "," with "'"
        string comboOptionsStr = comboOptions.str();
        comboOptionsStr[comboOptionsStr.length()] = '\'';

        // Update define
        TwDefine(comboOptionsStr.c_str());
    }

    // Accumulate execution time
    mTotalSimTime = 0;
    for (size_t i = 0; i < mSim->PerfData.Trackers.size(); i++)
        mTotalSimTime += mSim->PerfData.Trackers[i]->total_time;

    // Make sure bar refresh it's values
    TwRefreshBar(mTweakBar);

    // Actual draw
    glActiveTexture(GL_TEXTURE0);
    TwDraw();

    // Update stage inspection index
    OGSI_SetVisualizeStage(UIM_SelectedInspectionStage == 0 ? MAXINT : UIM_SelectedInspectionStage - 1, UIM_SaveInspectionStage, mMousePosX, mMousePosY);
    UIM_SaveInspectionStage = false;
}

void processGLFWEvents()
{
    glfwPollEvents();

    if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glFinish();
        glfwTerminate();
        exit(-1);
    }
}

void UIManager_Draw()
{
    processGLFWEvents();

    DrawAntTweakBar();

    DrawPerformanceGraph();

    if (mRenderer->UICmd_FriendsHistogarm)
        DrawFriendsHistogram();

    mIsFirstCycle = false;
}

