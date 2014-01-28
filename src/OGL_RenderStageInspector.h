//
// OpenGL (Render) Stage Inspection (GLSI)
//
#pragma once

#include "Precomp_OpenGL.h"

#include <glm/glm.hpp>
#include <string>
using namespace std;

// A list of last cycles inspection points
extern unsigned int    OGSI_Stages_Count;
extern string OGSI_Stages[255];
extern glm::vec4 OGSI_SamplePixelData;

// One time set-up
// (the parameter inspectionShader is an copy-to-screen inspection shader program ID)
void OGSI_Setup(GLuint inspectionShader);

// The function show be called to notfiy GLSI on new render cycle
void OGSI_StartCycle();

// Use this function to define the point to be inspected
void OGSI_SetVisualizeStage(int stageIndex, bool saveInspectionToFile, float sampleX, float sampleY);

// Use this function to add an "inspection point"
bool OGSI_InspectTexture(GLuint textureID, const char* szBufferTitle, float blitGain, float blitOffset);
