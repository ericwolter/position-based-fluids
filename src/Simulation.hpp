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

#include "hesp.hpp"
#include "Parameters.hpp"
#include "Particle.hpp"
#include "OCLPerfMon.h"
#include "OCL_Logger.h"

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

    // Copy current positions and velocities
    void dumpData(vec4 * (&positions), vec4 * (&velocities) );

public:

    // OpenCL objects supplied by OpenCL setup
    const cl::Context &mCLContext;
    const cl::Device &mCLDevice;

    // holds all shader programs used by simulation
    map<string, GLuint> mPrograms;
    /*!*/map<string, cl::Kernel> mKernels;

    // command queue all OpenCL calls are run on
    cl::CommandQueue mQueue;

    // ranges used for executing the kernels
    cl::NDRange mGlobalRange;
    cl::NDRange mLocalRange;

    // OCL buffer sizes
    size_t mBufferSizeParticles;
    size_t mBufferSizeCells;
    size_t mBufferSizeParticlesList;

    // The device memory buffers holding the simulation data
    GLuint            mCellsSBO;
    GLuint            mCellsTBO;
    /*!*/cl::Buffer mCellsBuffer;
    cl::Buffer mParticlesListBuffer;
    cl::Buffer mFriendsListBuffer;
    GLuint            mPositionsPingSBO;
    GLuint            mPositionsPingTBO;
    /*!*/cl::BufferGL mPositionsPingBuffer;
    cl::BufferGL mPositionsPongBuffer;
    GLuint            mPredictedPingSBO;
    GLuint            mPredictedPingTBO;
    /*!*/cl::Buffer   mPredictedPingBuffer;
    cl::Buffer mPredictedPongBuffer;
    GLuint            mVelocitiesSBO;
    GLuint            mVelocitiesTBO;
    cl::Buffer        mVelocitiesBuffer;
    cl::Buffer mDensityBuffer;
    cl::Buffer mDeltaBuffer;
    cl::Buffer mOmegaBuffer;
    GLuint   mGLParametersUBO;
    cl::Buffer mParameters;

    cl::Image2DGL mParticlePosImg;

    // Radix buffers
    GLuint mInKeysSBO;
    GLuint mInKeysTBO;
    /*!*/cl::Buffer mInKeysBuffer;
    GLuint mInPermutationSBO;
    GLuint mInPermutationTBO;
    /*!*/cl::Buffer mInPermutationBuffer;
    GLuint mOutKeysSBO;
    GLuint mOutKeysTBO;
    /*!*/cl::Buffer mOutKeysBuffer;
    GLuint mOutPermutationSBO;
    GLuint mOutPermutationTBO;
    /*!*/cl::Buffer mOutPermutationBuffer;
    GLuint mHistogramSBO;
    GLuint mHistogramTBO;
    /*!*/cl::Buffer mHistogramBuffer;
    GLuint mGlobSumSBO;
    GLuint mGlobSumTBO;
    /*!*/cl::Buffer mGlobSumBuffer;
    GLuint mHistoTempSBO;
    GLuint mHistoTempTBO;
    /*!*/cl::Buffer mHistoTempBuffer;
    cl::Buffer mStatsBuffer;

    // Lengths of each cell in each direction
    cl_float4 mCellLength;

    // Array for the cells
    cl_uint *mCells;

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
    void packData(cl::Buffer packTarget, cl::Buffer packSource, int iterationIndex);

public:
    // Default constructor.
    explicit Simulation(const cl::Context &clContext, const cl::Device &clDevice);

    // Destructor.
    ~Simulation ();

    // Create all buffer and particles
    void InitBuffers();

    // Init Grid
    void InitCells();

    // Load and build shaders
    bool InitShaders();
    /*!*/bool CL_InitKernels();

    // Perform single simulation step
    void Step();

    // Get a list of kernel files
    const std::string *ShaderFileList();
    /*!*/const std::string *CL_KernelFileList();

public:

    // Open GL Sharing buffers
    GLuint mSharedPingBufferID;
    GLuint mSharedPongBufferID;

    // Open GL Sharing Texture buffer
    GLuint mSharedParticlesPos;
    GLuint mSharedFriendsList;

    // Performance measurement
    OCLPerfMon PerfData;

    // OCL Logging
    OCL_Logger oclLog;

    // Rendering state
    bool      bPauseSim;
    bool      bReadFriendsList;
    bool      bDumpParticlesData;
    cl_float  fWavePos;

    // debug buffers (placed in host memory)
    vec4 *mPositions;
    vec4 *mVelocities;
    vec4 *mPredictions;
    vec4 *mDeltas;
    uint *mFriendsList;
};

#endif // __SIMULATION_HPP
