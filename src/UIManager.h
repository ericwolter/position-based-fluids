#pragma once

#include "visual/visual.hpp"

extern int UIM_SelectedInspectionStage;

void UIManager_Init(GLFWwindow* window, CVisual* pRenderer, Simulation* pSim);
void UIManager_Draw();
bool UIManager_WindowShouldClose();
