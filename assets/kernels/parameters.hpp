// add "#pragma once" for PC compile only
#if !defined(__OPENCL_VERSION__) && !defined(GLSL_COMPILER)
    #pragma once

    #include <glm\glm.hpp>
    using namespace glm;
#endif

// Define class header
#ifdef GLSL_COMPILER
    #define CLASS_HEADER layout (std140, binding = 10) uniform Parameter
    #define CLASS_FOOTER Params
#else
    #define CLASS_HEADER struct Parameters
    #define CLASS_FOOTER
#endif

CLASS_HEADER 
{
    // Runner related
    int resetSimOnChange;

    // Scene related
    uint  particleCount;
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
    uint  simIterations;
    uint  subSteps;
    float h;
    float restDensity;
    float epsilon;
    float gravity;
    float vorticityFactor;
    float viscosityFactor;
    float surfaceTenstionK;
    float surfaceTenstionDist;

    // Grid and friends list
    uint  friendsCircles;
    uint  particlesPerCircle;
    uint  gridBufSize;

    // Setup related
    float setupSpacing;

    // Sorting
    uint  segmentSize;
    uint  sortIterations;

    // Rendering related
    float particleRenderSize;

    // Computed fields
    float h_2;
} CLASS_FOOTER;

// RADIX SORTING
// NOTE: Brackets can't be used to define local size in compute shaders
#define _ITEMS          128  // number of items in a group 
#define _GROUPS        (16)  // the number of virtual processors is _ITEMS * _GROUPS
#define _HISTOSPLIT   (512)  // number of splits of the histogram
#define _TOTALBITS     (30)  // number of bits for the integer in the list (max=32)
#define _BITS           (5)  // number of bits in the radix
#define _N        (1 << 23)  // maximal size of the list  

#define _RADIX                  (1 << _BITS) //  radix  = 2^_BITS
#define _PASS             (_TOTALBITS/_BITS) // number of needed passes to sort the list
#define _HISTOSIZE   (_ITEMS*_GROUPS*_RADIX) // size of the histogram
#define _MAXINT        (1 << (_TOTALBITS-1)) // maximal value of integers for the sort to be correct

#define _MEMCACHE (_HISTOSPLIT) // max(_HISTOSPLIT, _ITEMS * _GROUPS * _RADIX / _HISTOSPLIT)
