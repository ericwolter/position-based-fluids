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

    // Create cached buffers
    cl::Memory CreateCachedBuffer(cl::ImageFormat& format, int elements);

    // Lock and unlock opengl objects
    void LockGLObjects();
    void UnlockGLObjects();

public:

    // OpenCL objects supplied by OpenCL setup
    const cl::Context &mCLContext;
    const cl::Device &mCLDevice;

    // holds all OpenCL kernels required for the simulation
    map<string, cl::Kernel> mKernels;

    // command queue all OpenCL calls are run on
    cl::CommandQueue mQueue;

    // ranges used for executing the kernels
    cl::NDRange mGlobalRange;
    cl::NDRange mLocalRange;

    // The device memory buffers holding the simulation data
    cl::Buffer   mCellsBuffer;
    cl::Buffer   mParticlesListBuffer;
    cl::Buffer   mFriendsListBuffer;
    cl::BufferGL mPositionsPingBuffer;
    cl::BufferGL mPositionsPongBuffer;
    cl::Memory   mPredictedPingBuffer;
    cl::Memory   mPredictedPongBuffer;
    cl::Buffer   mVelocitiesBuffer;
    cl::Buffer   mDensityBuffer;
    cl::Buffer   mLambdaBuffer;
    cl::Buffer   mDeltaBuffer;
    cl::Buffer   mOmegaBuffer;
    cl::Buffer   mParameters;
    cl::Image2D  mSurfacesMask;

    cl::Image2DGL mParticlePosImg;

    // Radix related
    cl_uint    mKeysCount;
    cl::Buffer mInKeysBuffer;
    cl::Buffer mInPermutationBuffer;
    cl::Buffer mOutKeysBuffer;
    cl::Buffer mOutPermutationBuffer;
    cl::Buffer mHistogramBuffer;
    cl::Buffer mGlobSumBuffer;
    cl::Buffer mHistoTempBuffer;

    // OpenGL locking related
    vector<cl::Memory> mGLLockList;

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
    void packData(cl::Memory& sourceImg, cl::Memory& pongImg, cl::Buffer packSource,  int iterationIndex);

public:
    // Default constructor.
    explicit Simulation(const cl::Context &clContext, const cl::Device &clDevice);

    // Destructor.
    ~Simulation ();

    // Create all buffer and particles
    void InitBuffers();

    // Init Grid
    void InitCells();

    // Load force masks
    void LoadForceMasks();

    // Load and build kernels
    bool InitKernels();

    // Perform single simulation step
    void Step();

    // Get a list of kernel files
    const std::string *KernelFileList();

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
};

#endif // __SIMULATION_HPP
