// add "#pragma once" for PC compile only
#if !defined(__OPENCL_VERSION__) && !defined(GLSL_COMPILER)
    #pragma once
#endif

// define class header (GLSL is diffrent from C++/OpeCL)
#ifdef GLSL_COMPILER
layout (std140, binding = 10) uniform Parameters 
#else
struct Parameters
#endif
{
    // Runner related
    int  resetSimOnChange;

    // Scene related
    unsigned int  particleCount;
    float xMin;
    float xMax;
    float yMin;
    float yMax;
    float zMin;
    float zMax;

    float waveGenAmp;
    float waveGenFreq;
    float waveGenDuty;

    // Simulation consts
    float timeStep;
    unsigned int   simIterations;
    unsigned int   subSteps;
    float h;
    float restDensity;
    float epsilon;
    float gravity;
    float vorticityFactor;
    float viscosityFactor;
    float surfaceTenstionK;
    float surfaceTenstionDist;

    // Grid and friends list
    unsigned int  friendsCircles;
    unsigned int  particlesPerCircle;
    unsigned int  gridBufSize;

    // Setup related
    float setupSpacing;

    // Sorting
    unsigned int segmentSize;
    unsigned int sortIterations;

    // Rendering related
    float particleRenderSize;

    // Computed fields
    float h_2;
};
