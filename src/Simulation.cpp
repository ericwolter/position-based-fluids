
#include "Precomp_OpenGL.h"
#include "Simulation.hpp"
#include "Resources.hpp"
#include "ParamUtils.hpp"
#include "ocl/OCLUtils.hpp"

#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>
#include <sstream>
#include <algorithm>

using namespace std;

unsigned int _NKEYS = 0;

Simulation::Simulation(const cl::Context &clContext, const cl::Device &clDevice)
    : mCLContext(clContext),
      mCLDevice(clDevice),
      mCells(NULL),
      mParticlesList(NULL),
      bDumpParticlesData(false),
      mPositions(NULL),
      mVelocities(NULL),
      mPredictions(NULL),
      mDeltas(NULL),
      mFriendsList(NULL)
{
}

Simulation::~Simulation()
{
    glFinish();
    mQueue.finish();

    // Delete buffers
    delete[] mCells;
    delete[] mPositions;
    delete[] mVelocities;
}

void Simulation::CreateParticles()
{
    // Compute particle count per axis
    int ParticlesPerAxis = (int)ceil(pow(Params.particleCount, 1 / 3.0));

    // Build particles blcok
    float d = Params.h * Params.setupSpacing;
    float offsetX = (1.0f - ParticlesPerAxis * d) / 2.0f;
    float offsetY = 0.3f;
    float offsetZ = (1.0f - ParticlesPerAxis * d) / 2.0f;
    for (cl_uint i = 0; i < Params.particleCount; i++)
    {
        cl_uint x = ((cl_uint)(i / pow(ParticlesPerAxis, 1)) % ParticlesPerAxis);
        cl_uint y = ((cl_uint)(i / pow(ParticlesPerAxis, 0)) % ParticlesPerAxis);
        cl_uint z = ((cl_uint)(i / pow(ParticlesPerAxis, 2)) % ParticlesPerAxis);

        mPositions[i].s[0] = offsetX + (x /*+ (y % 2) * .5*/) * d;
        mPositions[i].s[1] = offsetY + (y) * d;
        mPositions[i].s[2] = offsetZ + (z /*+ (y % 2) * .5*/) * d;
        mPositions[i].s[3] = 0;
    }

    // random_shuffle(&mPositions[0],&mPositions[Params.particleCount-1]);
}

const std::string *Simulation::KernelFileList()
{
    static const std::string kernels[] =
    {
        "hesp.hpp",
        "parameters.hpp",
        "logging.cl",
        "utilities.cl",
        "predict_positions.cl",
        "update_cells.cl",
        "build_friends_list.cl",
        "reset_grid.cl",
        "compute_scaling.cl",
        "compute_delta.cl",
        "update_predicted.cl",
        "pack_data.cl",
        "update_velocities.cl",
        "apply_viscosity.cl",
        "apply_vorticity.cl",
        "update_positions.cl",
        "radixsort.cl",
        ""
    };

    return kernels;
}

bool Simulation::InitKernels()
{
    // Setup OpenCL Ranges
    const cl_uint globalSize = (cl_uint)ceil(Params.particleCount / 32.0f) * 32;
    mGlobalRange = cl::NDRange(globalSize);
    mLocalRange = cl::NullRange;

    // Notify OCL logging that we're about to start new kernel processing
    oclLog.StartKernelProcessing(mCLContext, mCLDevice, 4096);

    // setup kernel sources
    OCLUtils clSetup;
    vector<string> kernelSources;

    // Load kernel sources
    const std::string *pKernels = KernelFileList();
    for (int iSrc = 0; pKernels[iSrc]  != ""; iSrc++)
    {
        // Read source from disk
        string source = clSetup.readSource(getPathForKernel(pKernels[iSrc]));

        // Patch kernel for logging
        if (pKernels[iSrc] != "logging.cl")
            source = oclLog.PatchKernel(source);

        // Load into compile list
        kernelSources.push_back(source);
    }

    // Setup kernel compiler flags
    std::ostringstream clflags;
    clflags << "-cl-mad-enable -cl-no-signed-zeros -cl-fast-relaxed-math ";

#ifdef USE_DEBUG
    clflags << "-DUSE_DEBUG ";
#endif // USE_DEBUG

    clflags << std::showpoint;

    clflags << "-DCANCEL_RANDOMNESS ";

    clflags << "-DLOG_SIZE="                    << (int)1024 << " ";
    clflags << "-DEND_OF_CELL_LIST="            << (int)(-1)         << " ";

    clflags << "-DFRIENDS_CIRCLES="             << (int)(Params.friendsCircles)     << " ";  // Defines how many friends circle are we going to scan for
    clflags << "-DMAX_PARTICLES_IN_CIRCLE="     << (int)(Params.particlesPerCircle) << " ";  // Defines the max number of particles per cycle
    clflags << "-DPARTICLE_FRIENDS_BLOCK_SIZE=" << (int)(Params.friendsCircles + Params.friendsCircles * Params.particlesPerCircle) << " ";  // FRIENDS_CIRCLES + FRIENDS_CIRCLES * MAX_PARTICLES_IN_CIRCLE

    clflags << "-DGRID_BUF_SIZE="     << (int)(Params.gridBufSize) << " ";

    clflags << "-DPOLY6_FACTOR="      << 315.0f / (64.0f * M_PI * pow(Params.h, 9)) << "f ";
    clflags << "-DGRAD_SPIKY_FACTOR=" << 45.0f / (M_PI * pow(Params.h, 6)) << "f ";


    // Compile kernels
    cl::Program program = clSetup.createProgram(kernelSources, mCLContext, mCLDevice, clflags.str());
    if (program() == 0)
        return false;

    // Build kernels table
    mKernels = clSetup.createKernelsMap(program);

    // Copy Params (Host) => mParams (GPU)
    mQueue = cl::CommandQueue(mCLContext, mCLDevice, CL_QUEUE_PROFILING_ENABLE);
    mQueue.enqueueWriteBuffer(mParameters, CL_TRUE, 0, sizeof(Params), &Params);
    mQueue.finish();

    return true;
}

void Simulation::InitBuffers()
{
    // Define CL buffer sizes
    mBufferSizeParticles      = Params.particleCount * sizeof(cl_float4);
    mBufferSizeParticlesList  = Params.particleCount * sizeof(cl_int);

    // Allocate CPU buffers
    delete[] mPositions;   mPositions   = new cl_float4[Params.particleCount];
    delete[] mVelocities;  mVelocities  = new cl_float4[Params.particleCount];
    delete[] mPredictions; mPredictions = new cl_float4[Params.particleCount]; // (used for debugging)
    delete[] mDeltas;      mDeltas      = new cl_float4[Params.particleCount]; // (used for debugging)
    delete[] mFriendsList; mFriendsList = new cl_uint  [Params.particleCount * Params.friendsCircles * (1 + Params.particlesPerCircle)]; // (used for debugging)

    // Position particles
    CreateParticles();

    // Initialize particle speed arrays
    for (cl_uint i = 0; i < Params.particleCount; ++i)
    {
        mVelocities[i].s[0] = 0;
        mVelocities[i].s[1] = 0;
        mVelocities[i].s[2] = 0;
        mVelocities[i].s[3] = 1; // <= "m" == 1?
    }

    // Create buffers
    mPositionsPingBuffer   = cl::BufferGL(mCLContext, CL_MEM_READ_WRITE, mSharedPingBufferID); // buffer could be changed to be CL_MEM_WRITE_ONLY but for debugging also reading it might be helpful
    mPositionsPongBuffer   = cl::BufferGL(mCLContext, CL_MEM_READ_WRITE, mSharedPongBufferID); // buffer could be changed to be CL_MEM_WRITE_ONLY but for debugging also reading it might be helpful
    mParticlePosImg        = cl::Image2DGL(mCLContext, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, mSharedParticlesPos);

    mPredictedPingBuffer   = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mPredictedPongBuffer   = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mVelocitiesBuffer      = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mDeltaBuffer           = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mDeltaVelocityBuffer   = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mOmegaBuffer           = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mDensityBuffer         = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, Params.particleCount * sizeof(cl_float));
    mParameters            = cl::Buffer(mCLContext, CL_MEM_READ_ONLY,  sizeof(Params));

    // Radix buffers
    if (Params.particleCount % (_ITEMS * _GROUPS) == 0)
    {
        _NKEYS = Params.particleCount;
    }
    else
    {
        _NKEYS = Params.particleCount + (_ITEMS * _GROUPS) - Params.particleCount % (_ITEMS * _GROUPS);
    }
    mInKeysBuffer          = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _NKEYS);
    mInPermutationBuffer   = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _NKEYS);
    mOutKeysBuffer         = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _NKEYS);
    mOutPermutationBuffer  = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _NKEYS);
    mHistogramBuffer       = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _RADIX * _GROUPS * _ITEMS);
    mGlobSumBuffer         = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _HISTOSPLIT);
    mHistoTempBuffer       = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _HISTOSPLIT);

    if (mQueue() != 0)
        mQueue.flush();

    // Copy mPositions (Host) => mPositionsPingBuffer (GPU) (we have to lock the shared buffer)
    vector<cl::Memory> sharedBuffers;
    sharedBuffers.push_back(mPositionsPingBuffer);
    mQueue = cl::CommandQueue(mCLContext, mCLDevice);
    mQueue.enqueueAcquireGLObjects(&sharedBuffers);
    mQueue.enqueueWriteBuffer(mPositionsPingBuffer, CL_TRUE, 0, mBufferSizeParticles, mPositions);
    mQueue.enqueueReleaseGLObjects(&sharedBuffers);
    mQueue.finish();

    // Copy mVelocities (Host) => mVelocitiesBuffer (GPU)
    mQueue.enqueueWriteBuffer(mVelocitiesBuffer, CL_TRUE, 0, mBufferSizeParticles, mVelocities);
    mQueue.finish();

    // Copy mVelocities (Host) => mVelocitiesBuffer (GPU)
    mQueue.enqueueWriteBuffer(mVelocitiesBuffer, CL_TRUE, 0, mBufferSizeParticles, mVelocities);
    mQueue.finish();

}

void Simulation::InitCells()
{
    // Allocate host buffers
    delete[] mCells;         mCells         = new cl_int[Params.gridBufSize];
    delete[] mParticlesList; mParticlesList = new cl_int[Params.particleCount];

    // Init cells
    for (cl_uint i = 0; i < Params.gridBufSize; ++i)
        mCells[i] = END_OF_CELL_LIST;

    // Init particles
    for (cl_uint i = 0; i < Params.particleCount; ++i)
        mParticlesList[i] = END_OF_CELL_LIST;

    // Write buffer for cells
    mBufferSizeCells = Params.gridBufSize * sizeof(cl_int);
    mCellsBuffer = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeCells);
    mQueue.enqueueWriteBuffer(mCellsBuffer, CL_TRUE, 0, mBufferSizeCells, mCells);

    // Write buffer for particles
    mParticlesListBuffer = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticlesList);
    mQueue.enqueueWriteBuffer(mParticlesListBuffer, CL_TRUE, 0, mBufferSizeParticlesList, mParticlesList);

    // Init Friends list buffer
    int BufSize = Params.particleCount * Params.friendsCircles * (1 + Params.particlesPerCircle) * sizeof(cl_uint);
    memset(mFriendsList, 0, BufSize);
    mFriendsListBuffer = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, BufSize);
    mQueue.enqueueWriteBuffer(mFriendsListBuffer, CL_TRUE, 0, BufSize, mFriendsList);
}

int dumpSession = 0;
int dumpCounter = 0;
int cycleCounter = 0;

void SaveFile(cl::CommandQueue queue, cl::Buffer buffer, const char* szFilename)
{
    // Exit if dump session is disabled
    if (dumpSession == 0)
        return;

     // Get buffer size
    int bufSize = buffer.getInfo<CL_MEM_SIZE>();

    // Read data from GPU
    char* buf = new char[bufSize];
    queue.enqueueReadBuffer(buffer, CL_TRUE, 0, bufSize, buf);
    queue.finish();

    // Compose file name
    dumpCounter++;
    char szTarget[256];
    sprintf(szTarget, "%s/dump%d/%d_%d_%s.bin", getRootPath().c_str(), dumpSession, dumpCounter, cycleCounter, szFilename);
    
    // Save to disk
    ofstream f(szTarget, ios::out | ios::trunc | ios::binary);
    f.seekp(0);
    f.write((const char*)buf, bufSize);
    f.close();

    delete[] buf;
}

void Simulation::updatePositions()
{
    int param = 0;
    mKernels["updatePositions"].setArg(param++, mPositionsPingBuffer);
    mKernels["updatePositions"].setArg(param++, mPredictedPingBuffer);
    mKernels["updatePositions"].setArg(param++, mParticlePosImg);
    mKernels["updatePositions"].setArg(param++, mVelocitiesBuffer);
    mKernels["updatePositions"].setArg(param++, mDeltaVelocityBuffer);
    mKernels["updatePositions"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["updatePositions"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("updatePositions"));

    //SaveFile(mQueue, mVelocitiesBuffer, "Velo3");
    //SaveFile(mQueue, mPositionsPingBuffer, "pos1");
}

void Simulation::updateVelocities()
{
    int param = 0;
    mKernels["updateVelocities"].setArg(param++, mParameters);
    mKernels["updateVelocities"].setArg(param++, mPositionsPingBuffer);
    mKernels["updateVelocities"].setArg(param++, mPredictedPingBuffer);
    mKernels["updateVelocities"].setArg(param++, mVelocitiesBuffer);
    mKernels["updateVelocities"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["updateVelocities"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("updateVelocities"));

    //SaveFile(mQueue, mVelocitiesBuffer, "Velo2");
}

void Simulation::applyViscosity()
{
    int param = 0;
    mKernels["applyViscosity"].setArg(param++, mParameters);
    mKernels["applyViscosity"].setArg(param++, mPredictedPingBuffer);
    mKernels["applyViscosity"].setArg(param++, mVelocitiesBuffer);
    mKernels["applyViscosity"].setArg(param++, mDeltaVelocityBuffer);
    mKernels["applyViscosity"].setArg(param++, mOmegaBuffer);
    mKernels["applyViscosity"].setArg(param++, mFriendsListBuffer);
    mKernels["applyViscosity"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["applyViscosity"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("applyViscosity"));

    //SaveFile(mQueue, mOmegaBuffer, "Omega");
    //SaveFile(mQueue, mDeltaVelocityBuffer, "DeltaVel");
}

void Simulation::applyVorticity()
{
    int param = 0;
    mKernels["applyVorticity"].setArg(param++, mParameters);
    mKernels["applyVorticity"].setArg(param++, mPredictedPingBuffer);
    mKernels["applyVorticity"].setArg(param++, mDeltaVelocityBuffer);
    mKernels["applyVorticity"].setArg(param++, mOmegaBuffer);
    mKernels["applyVorticity"].setArg(param++, mFriendsListBuffer);
    mKernels["applyVorticity"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["applyVorticity"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("applyVorticity"));

    //SaveFile(mQueue, mDeltaVelocityBuffer, "Omega");
}

void Simulation::predictPositions()
{
    int param = 0;
    mKernels["predictPositions"].setArg(param++, mParameters);
    mKernels["predictPositions"].setArg(param++, (cl_uint)bPauseSim);
    mKernels["predictPositions"].setArg(param++, mPositionsPingBuffer);
    mKernels["predictPositions"].setArg(param++, mPredictedPingBuffer);
    mKernels["predictPositions"].setArg(param++, mVelocitiesBuffer);
    mKernels["predictPositions"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["predictPositions"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("predictPositions"));

    //SaveFile(mQueue, mPositionsPingBuffer, "PosPing");
    //SaveFile(mQueue, mVelocitiesBuffer,    "Velocity");
    //SaveFile(mQueue, mPredictedPingBuffer, "PredPosPing");
}

void Simulation::buildFriendsList()
{
    int param = 0;
    mKernels["buildFriendsList"].setArg(param++, mParameters);
    mKernels["buildFriendsList"].setArg(param++, mPredictedPingBuffer);
    mKernels["buildFriendsList"].setArg(param++, mCellsBuffer);
    mKernels["buildFriendsList"].setArg(param++, mParticlesListBuffer);
    mKernels["buildFriendsList"].setArg(param++, mFriendsListBuffer);
    mKernels["buildFriendsList"].setArg(param++, Params.particleCount);
    mQueue.enqueueNDRangeKernel(mKernels["buildFriendsList"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("buildFriendsList"));

    //SaveFile(mQueue, mFriendsListBuffer, "FriendList");

    param = 0;
    mKernels["resetGrid"].setArg(param++, mParameters);
    mKernels["resetGrid"].setArg(param++, mPredictedPingBuffer);
    mKernels["resetGrid"].setArg(param++, mParticlesListBuffer);
    mKernels["resetGrid"].setArg(param++, mCellsBuffer);
    mKernels["resetGrid"].setArg(param++, Params.particleCount);
    mQueue.enqueueNDRangeKernel(mKernels["resetGrid"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("resetPartList"));
}

void Simulation::updatePredicted(int iterationIndex)
{
    int param = 0;
    mKernels["updatePredicted"].setArg(param++, mPredictedPingBuffer);
    mKernels["updatePredicted"].setArg(param++, mDeltaBuffer);
    mKernels["updatePredicted"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["updatePredicted"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("updatePredicted", iterationIndex));
}

void Simulation::packData(cl::Buffer packTarget, cl::Buffer packSource,  int iterationIndex)
{
    int param = 0;
    mKernels["packData"].setArg(param++, packTarget);
    mKernels["packData"].setArg(param++, packSource);
    mKernels["packData"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["packData"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("packData", iterationIndex));
}

void Simulation::computeDelta(int iterationIndex)
{
    int param = 0;
    mKernels["computeDelta"].setArg(param++, mParameters);
    mKernels["computeDelta"].setArg(param++, oclLog.GetDebugBuffer());
    mKernels["computeDelta"].setArg(param++, mDeltaBuffer);
    mKernels["computeDelta"].setArg(param++, mPredictedPingBuffer); // xyz=Predicted z=Scaling
    mKernels["computeDelta"].setArg(param++, mFriendsListBuffer);
    mKernels["computeDelta"].setArg(param++, fWavePos);
    mKernels["computeDelta"].setArg(param++, Params.particleCount);

    //mQueue.enqueueNDRangeKernel(mKernels["computeDelta"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("computeDelta", iterationIndex));
    mQueue.enqueueNDRangeKernel(mKernels["computeDelta"], 0, cl::NDRange(((Params.particleCount + 127) / 128) * 128), cl::NDRange(128), NULL, PerfData.GetTrackerEvent("computeDelta", iterationIndex));

    //SaveFile(mQueue, mDeltaBuffer, "delta2");
}

void Simulation::computeScaling(int iterationIndex)
{
    int param = 0;
    mKernels["computeScaling"].setArg(param++, mParameters);
    mKernels["computeScaling"].setArg(param++, mPredictedPingBuffer);
    mKernels["computeScaling"].setArg(param++, mDensityBuffer);
    mKernels["computeScaling"].setArg(param++, mFriendsListBuffer);
    mKernels["computeScaling"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["computeScaling"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("computeScaling", iterationIndex));

    //SaveFile(mQueue, mPredictedPingBuffer, "pred2");
    //SaveFile(mQueue, mDensityBuffer,       "dens2");
}

void Simulation::updateCells()
{
    //SaveFile(mQueue, mCellsBuffer, "cells_before");

    int param = 0;
    mKernels["updateCells"].setArg(param++, mParameters);
    mKernels["updateCells"].setArg(param++, mPredictedPingBuffer);
    mKernels["updateCells"].setArg(param++, mCellsBuffer);
    mKernels["updateCells"].setArg(param++, mParticlesListBuffer);
    mKernels["updateCells"].setArg(param++, Params.particleCount);
    mQueue.enqueueNDRangeKernel(mKernels["updateCells"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("updateCells"));

    //SaveFile(mQueue, mCellsBuffer, "cells");
    //SaveFile(mQueue, mParticlesListBuffer, "friendlist2");
}

void Simulation::radixsort()
{
    int param = 0;
    mKernels["computeKeys"].setArg(param++, mParameters);
    mKernels["computeKeys"].setArg(param++, mPredictedPingBuffer);
    mKernels["computeKeys"].setArg(param++, mInKeysBuffer);
    mKernels["computeKeys"].setArg(param++, mInPermutationBuffer);
    mKernels["computeKeys"].setArg(param++, Params.particleCount);
    mQueue.enqueueNDRangeKernel(mKernels["computeKeys"], 0, cl::NDRange(_NKEYS), mLocalRange, NULL, PerfData.GetTrackerEvent("computeKeys"));

    // // DEBUG
    // cl_uint *keys = new cl_uint[_NKEYS];
    // cl_uint *permutation = new cl_uint[_NKEYS];
    // mQueue.finish();
    // mQueue.enqueueReadBuffer(mInKeysBuffer, CL_TRUE, 0, sizeof(cl_uint) * _NKEYS, keys);
    // mQueue.enqueueReadBuffer(mInPermutationBuffer, CL_TRUE, 0, sizeof(cl_uint) * _NKEYS, permutation);
    // mQueue.finish();
    // cout << "before sort:" << endl;
    // cout << "keys: ";
    // for (unsigned int i = 0; i < _NKEYS; ++i)
    // {
    //     cout << i<<"="<<keys[i] << ",";
    // }
    // cout << endl;
    // cout << "permu: ";
    // for (unsigned int i = 0; i < _NKEYS; ++i)
    // {
    //     cout << i<<"="<<permutation[i] << ",";
    // }
    // cout << endl;

    for (size_t pass = 0; pass < _PASS; pass++)
    {
        // Histogram(pass);
        const size_t h_nblocitems = _ITEMS;
        const size_t h_nbitems = _GROUPS * _ITEMS;
        param = 0;
        mKernels["histogram"].setArg(param++, mInKeysBuffer);
        mKernels["histogram"].setArg(param++, mHistogramBuffer);
        mKernels["histogram"].setArg(param++, pass);
        mKernels["histogram"].setArg(param++, sizeof(cl_uint) * _RADIX * _ITEMS, NULL);
        mKernels["histogram"].setArg(param++, _NKEYS);
        mQueue.enqueueNDRangeKernel(mKernels["histogram"], 0, cl::NDRange(h_nbitems), cl::NDRange(h_nblocitems), NULL, PerfData.GetTrackerEvent("histogram", pass));

        // ScanHistogram();
        const size_t sh1_nbitems = _RADIX * _GROUPS * _ITEMS / 2;
        const size_t sh1_nblocitems = sh1_nbitems / _HISTOSPLIT ;
        const int maxmemcache = max(_HISTOSPLIT, _ITEMS * _GROUPS * _RADIX / _HISTOSPLIT);
        mKernels["scanhistograms"].setArg(0, mHistogramBuffer);
        mKernels["scanhistograms"].setArg(1, sizeof(cl_uint)* maxmemcache, NULL);
        mKernels["scanhistograms"].setArg(2, mGlobSumBuffer);
        mQueue.enqueueNDRangeKernel(mKernels["scanhistograms"], 0, cl::NDRange(sh1_nbitems), cl::NDRange(sh1_nblocitems), NULL, PerfData.GetTrackerEvent("scanhistograms1", pass));
        mQueue.finish();

        const size_t sh2_nbitems = _HISTOSPLIT / 2;
        const size_t sh2_nblocitems = sh2_nbitems;
        mKernels["scanhistograms"].setArg(0, mGlobSumBuffer);
        mKernels["scanhistograms"].setArg(2, mHistoTempBuffer);
        mQueue.enqueueNDRangeKernel(mKernels["scanhistograms"], 0, cl::NDRange(sh2_nbitems), cl::NDRange(sh2_nblocitems), NULL, PerfData.GetTrackerEvent("scanhistograms2", pass));

        const size_t ph_nbitems = _RADIX * _GROUPS * _ITEMS / 2;
        const size_t ph_nblocitems = ph_nbitems / _HISTOSPLIT;
        param = 0;
        mKernels["pastehistograms"].setArg(param++, mHistogramBuffer);
        mKernels["pastehistograms"].setArg(param++, mGlobSumBuffer);
        mQueue.enqueueNDRangeKernel(mKernels["pastehistograms"], 0, cl::NDRange(ph_nbitems), cl::NDRange(ph_nblocitems), NULL, PerfData.GetTrackerEvent("pastehistograms", pass));

        // Reorder(pass);
        const size_t r_nblocitems = _ITEMS;
        const size_t r_nbitems = _GROUPS * _ITEMS;
        param = 0;
        mKernels["reorder"].setArg(param++, mInKeysBuffer);
        mKernels["reorder"].setArg(param++, mOutKeysBuffer);
        mKernels["reorder"].setArg(param++, mHistogramBuffer);
        mKernels["reorder"].setArg(param++, pass);
        mKernels["reorder"].setArg(param++, mInPermutationBuffer);
        mKernels["reorder"].setArg(param++, mOutPermutationBuffer);
        mKernels["reorder"].setArg(param++, sizeof(cl_uint)* _RADIX * _ITEMS, NULL);
        mKernels["reorder"].setArg(param++, _NKEYS);
        mQueue.enqueueNDRangeKernel(mKernels["reorder"], 0, cl::NDRange(r_nbitems), cl::NDRange(r_nblocitems), NULL, PerfData.GetTrackerEvent("reorder", pass));

        cl::Buffer tmp = mInKeysBuffer;
        mInKeysBuffer = mOutKeysBuffer;
        mOutKeysBuffer = tmp;

        tmp = mInPermutationBuffer;
        mInPermutationBuffer = mOutPermutationBuffer;
        mOutPermutationBuffer = tmp;
    }

    // // DEBUG
    // mQueue.finish();
    // mQueue.enqueueReadBuffer(mInKeysBuffer, CL_TRUE, 0, sizeof(cl_uint) * _NKEYS, keys);
    // mQueue.enqueueReadBuffer(mInPermutationBuffer, CL_TRUE, 0, sizeof(cl_uint) * _NKEYS, permutation);
    // mQueue.finish();
    // cout << "before sort:" << endl;
    // cout << "keys: ";
    // for (unsigned int i = 0; i < _NKEYS; ++i)
    // {
    //     cout << i<<"="<<keys[i] << ",";
    // }
    // cout << endl;
    // cout << "permu: ";
    // for (unsigned int i = 0; i < _NKEYS; ++i)
    // {
    //     cout << i<<"="<<permutation[i] << ",";
    // }
    // cout << endl;
    // delete[] keys;
    // delete[] permutation;

    // Lock Yang buffer (Yin is already locked)
    //vector<cl::Memory> sharedBuffers;
    //sharedBuffers.push_back(mPositionsYangBuffer);
    //mQueue.enqueueAcquireGLObjects(&sharedBuffers);

    // Execute particle reposition
    param = 0;
    mKernels["sortParticles"].setArg(param++, mInPermutationBuffer);
    mKernels["sortParticles"].setArg(param++, mPositionsPingBuffer);
    mKernels["sortParticles"].setArg(param++, mPositionsPongBuffer);
    mKernels["sortParticles"].setArg(param++, mPredictedPingBuffer);
    mKernels["sortParticles"].setArg(param++, mPredictedPongBuffer);
    mKernels["sortParticles"].setArg(param++, Params.particleCount);
    mQueue.enqueueNDRangeKernel(mKernels["sortParticles"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("sortParticles"));

    // UnLock Yang buffer
    //mQueue.enqueueReleaseGLObjects(&sharedBuffers);

    // Double buffering of positions and velocity buffers
    cl::BufferGL tmp1 = mPositionsPingBuffer;
    mPositionsPingBuffer = mPositionsPongBuffer;
    mPositionsPongBuffer = tmp1;

    cl::Buffer tmp2 = mPredictedPingBuffer;
    mPredictedPingBuffer = mPredictedPongBuffer;
    mPredictedPongBuffer = tmp2;

    GLuint tmp4 = mSharedPingBufferID;
    mSharedPingBufferID = mSharedPongBufferID;
    mSharedPongBufferID = tmp4;
}

void Simulation::Step()
{
    cycleCounter++;

    // Why is this here?
    glFinish();

    // Enqueue GL buffer acquire
    vector<cl::Memory> sharedBuffers;
    sharedBuffers.push_back(mPositionsPingBuffer);
    sharedBuffers.push_back(mPositionsPongBuffer);
    sharedBuffers.push_back(mParticlePosImg);
    mQueue.enqueueAcquireGLObjects(&sharedBuffers);

    // Predicit positions
    this->predictPositions();

    // sort particles buffer
    this->radixsort();

    // Update cells
    this->updateCells();

    // Build friends list
    this->buildFriendsList();

    for (unsigned int i = 0; i < Params.simIterations; ++i)
    {
        // Compute scaling value
        this->computeScaling(i);

        // Compute position delta
        this->computeDelta(i);
        
        // Update predicted position
        this->updatePredicted(i);
    }

    // Place density in "mPredictedPingBuffer[x].w"
    this->packData(mPredictedPingBuffer, mDensityBuffer, -1);

    // Recompute velocities
    this->updateVelocities();

    // Update vorticity and Viscosity
    this->applyViscosity();
    this->applyVorticity();

    // Update particle buffers
    if (!bPauseSim)
        this->updatePositions();

    // [DEBUG] Read back friends information (if needed)
    if (bReadFriendsList || bDumpParticlesData)
        mQueue.enqueueReadBuffer(mFriendsListBuffer, CL_TRUE, 0, sizeof(cl_uint) * Params.particleCount * Params.friendsCircles * (1 + Params.particlesPerCircle), mFriendsList);

    // [DEBUG] Do we need to dump particle data
    if (bDumpParticlesData)
    {
        // Turn off flag
        bDumpParticlesData = false;

        // Read data
        mQueue.enqueueReadBuffer(mPositionsPingBuffer, CL_TRUE, 0, mBufferSizeParticles, mPositions);
        mQueue.finish();

        // Save to disk
        ofstream f("particles_pos.bin", ios::out | ios::trunc | ios::binary);
        f.seekp(0);
        f.write((const char*)mPositions, mBufferSizeParticles);
        f.close();
    }
    // Release OpenGL shared object, allowing openGL do to it's thing...
    mQueue.enqueueReleaseGLObjects(&sharedBuffers);
    mQueue.finish();

    // Collect performance data
    PerfData.UpdateTimings();

    // Allow OpenCL logger to process
    oclLog.CycleExecute(mQueue);
}

void Simulation::dumpData( cl_float4 * (&positions), cl_float4 * (&velocities) )
{
    mQueue.enqueueReadBuffer(mPositionsPingBuffer, CL_TRUE, 0, mBufferSizeParticles, mPositions);
    mQueue.enqueueReadBuffer(mVelocitiesBuffer, CL_TRUE, 0, mBufferSizeParticles, mVelocities);

    // just a safety measure to be absolutely sure everything is transferred
    // from device to host
    mQueue.finish();

    positions = mPositions;
    velocities = mVelocities;
}
