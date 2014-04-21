#ifndef __SIMULATION_HPP
#define __SIMULATION_HPP

#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <assert.h>
#include <algorithm>

#include "Parameters.hpp"

#include <GLFW/glfw3.h>

#include <glm\glm.hpp>
using namespace glm;

// Macro used for the end of cell list
static const int END_OF_CELL_LIST = -1;

using std::map;
using std::vector;
using std::string;

class Simulation
{
private:
    // Avoid copy
    Simulation &operator=(const Simulation &other);
    Simulation (const Simulation &other);

    // Init particles positions
    void CreateParticles();
    unsigned int mNumGroups;

public:
    // holds all shader programs used by simulation
    map<string, GLuint> mPrograms;

    // OCL buffer sizes
    size_t mBufferSizeParticles;
    size_t mBufferSizeCells;
    size_t mBufferSizeParticlesList;

    // The device memory buffers holding the simulation data
    GLuint mCellsSBO;
    GLuint mCellsTBO;
    GLuint mFriendsListSBO;
    GLuint mFriendsListTBO;
    GLuint mPositionsPingSBO;
    GLuint mPositionsPingTBO;
    GLuint mPositionsPongSBO;
    GLuint mPositionsPongTBO;
    GLuint mPredictedPingSBO;
    GLuint mPredictedPingTBO;
    GLuint mPredictedPongSBO;
    GLuint mPredictedPongTBO;
    GLuint mVelocitiesSBO;
    GLuint mVelocitiesTBO;
    GLuint mDensitySBO;
    GLuint mDensityTBO;
    GLuint mDeltaSBO;
    GLuint mDeltaTBO;
    GLuint mOmegasSBO;
    GLuint mOmegasTBO;
    GLuint mGLParametersUBO;

    // Radix buffers
    GLuint mInKeysSBO;
    GLuint mInKeysTBO;
    GLuint mInPermutationSBO;
    GLuint mInPermutationTBO;
    GLuint mOutKeysSBO;
    GLuint mOutKeysTBO;
    GLuint mOutPermutationSBO;
    GLuint mOutPermutationTBO;
    GLuint mHistogramSBO;
    GLuint mHistogramTBO;
    GLuint mGlobSumSBO;
    GLuint mGlobSumTBO;
    GLuint mHistoTempSBO;
    GLuint mHistoTempTBO;

    // Array for the cells
    int *mCells;

    // Private member functions
    void updateCells();
    void updateVelocities();
    void applyViscosity();
    void applyVorticity();
    void predictPositions();
    void buildFriendsList();
    void updatePredicted(int iterationIndex);
    void computeScaling(int iterationIndex);
    void computeDelta(int iterationIndex);
    void radixsort();
    void packData();

public:
    // Default constructor.
    explicit Simulation();

    // Destructor.
    ~Simulation ();

    // Create all buffer and particles
    void InitBuffers();

    // Init Grid
    void InitCells();

    // Load and build shaders
    bool InitShaders();

    // Perform single simulation step
    void Step();

    // Get a list of kernel files
    const std::string *ShaderFileList();

public:

    // Rendering state
    bool   bPauseSim;
    bool   bReadFriendsList;
    bool   bDumpParticlesData;
    float  fWavePos;

    // debug buffers (placed in host memory)
    vec4 *mPositions;
    vec4 *mVelocities;
    vec4 *mPredictions;
    vec4 *mDeltas;
    uint *mFriendsList;
};

#endif // __SIMULATION_HPP
