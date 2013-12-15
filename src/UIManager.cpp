
#include "UIManager.h"
#include "ParamUtils.hpp"
#include "ZPR.h"

#include <AntTweakBar.h>
#include "../../lib/AntTweakBar/src/TwPrecomp.h"
#include "../../lib/AntTweakBar/src/TwMgr.h"

#include "../../lib/AntTweakBar/src/TwOpenGLCore.h"

CTwGraphOpenGLCore tw;
void *twFont;

float       mDisplayscale = 1.0f;
CTexFont    *mDisplayFont = g_DefaultSmallFont;

GLFWwindow *mWindow;
CVisual    *mRenderer;
Simulation *mSim;
TwBar      *mTweakBar;
double      mTotalSimTime;


void MouseButtonCB(GLFWwindow *window, int button, int action, int mods)
{
    TwEventMouseButtonGLFW(button, action);
    ZPR_EventMouseButtonGLFW(window, button, action, mods);
}

void MousePosCB(GLFWwindow *window, double x, double y)
{
    TwEventMousePosGLFW((int)(x * mDisplayscale), (int)(y * mDisplayscale));
    ZPR_EventMousePosGLFW(window, (int)(x * mDisplayscale), (int)(y * mDisplayscale));
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
    ((CVisual *)clientData)->UICmd_ColorMethod = 0;
}

void TW_CALL ShowSorting(void *clientData)
{
    ((CVisual *)clientData)->UICmd_ColorMethod = 1;
}

void TW_CALL FriendsHistogram(void *clientData)
{
    ((CVisual *)clientData)->UICmd_FriendsHistogarm = !((CVisual *)clientData)->UICmd_FriendsHistogarm;
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

    // Get window size
    TwInit(TW_OPENGL_CORE, NULL);
    int fbwidth , fbheight;
    glfwGetFramebufferSize(mWindow, &fbwidth, &fbheight );
    int width, height;
    glfwGetWindowSize(mWindow, &width, &height);
    TwWindowSize(fbwidth, fbheight);

    // Compute ratio...
    mDisplayscale = (float)fbwidth / width;
    mDisplayFont = mDisplayscale >= 2.0f ? g_DefaultNormalFont : g_DefaultSmallFont;

    mTweakBar = TwNewBar("PBFTweak");
    TwAddButton(mTweakBar, "Play/Pause",        PlayPause,        mRenderer, " group=Controls ");
    TwAddButton(mTweakBar, "Reset",             Reset,            mRenderer, " group=Controls ");
    TwAddButton(mTweakBar, "Generate waves",    GenerateWaves,    mRenderer, " group=Controls ");
    TwAddButton(mTweakBar, "Firends Histogram", FriendsHistogram, mRenderer, " group=Controls ");

    TwAddButton(mTweakBar, "View Reset",        View_Reset,       mRenderer, " group=View ");
    TwAddButton(mTweakBar, "Show velocity",     ShowVelocity,     mRenderer, " group=View ");
    TwAddButton(mTweakBar, "Show sorting",      ShowSorting,      mRenderer, " group=View ");

    // Create bar
    TwAddVarRO(mTweakBar, "Total sim time", TW_TYPE_DOUBLE, &mTotalSimTime, "precision=2 group=Stats");


    g_TwMgr->m_GraphAPI = TW_OPENGL_CORE;
    tw.Init();
    TwGenerateDefaultFonts(1.0);
    twFont = tw.NewTextObj();

}

void DrawPerformanceGraph()
{
    // Compute sizes
    const int ViewWidth    = 1280 * mDisplayscale;
    const int ViewHeight   = 720 * mDisplayscale;
    const int BarHeight    = 20 * mDisplayscale;
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
    //const color32 StartClr[] = {0xDE6866, 0x9ADE66, 0x66AADE};
    //const color32 EndClr[]   = {0xDEA1A0, 0xBAE09D, 0x9ABFDB};
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
    const int ViewWidth    = 1280;
    const int ViewHeight   = 720;
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

    // Update AntTweekBar
    mTotalSimTime = 0;
    for (size_t i = 0; i < mSim->PerfData.Trackers.size(); i++)
    {
        PM_PERFORMANCE_TRACKER *pTracker = mSim->PerfData.Trackers[i];

        // Check if we need to create Bar
        if (pTracker->Tag == 0)
        {
            // Remember that bar was created
            pTracker->Tag = 1;

            // Create bar
            TwAddVarRO(mTweakBar, ("  " + pTracker->eventName).c_str(), TW_TYPE_DOUBLE, &pTracker->total_time, "precision=2 group=Stats");

            // Make sure Stats group is folded
            TwDefine(" PBFTweak/Stats opened=false ");
        }

        // Accumulate execution time
        mTotalSimTime += pTracker->total_time;
    }

    // Make sure bar refresh it's values
    TwRefreshBar(mTweakBar);

    // Actual draw
    glActiveTexture(GL_TEXTURE0);
    TwDraw();

    DrawPerformanceGraph();

    // Draw Friends histogram
    if (mRenderer->UICmd_FriendsHistogarm)
        DrawFriendsHistogram();
}

