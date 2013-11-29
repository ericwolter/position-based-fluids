#include "Simulation.hpp"
#include "DataLoader.hpp"
#include "ParamUtils.hpp"
#include "ocl/OCLUtils.hpp"

#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>
#include <sstream>
#include <algorithm>

#if defined(__APPLE__)
#include <OpenGL/OpenGL.h>
#elif defined(UNIX)
#include <GL/glx.h>
#else // _WINDOWS
#include <Windows.h>
#include <GL/gl.h>
#endif

using namespace std;

unsigned int _NKEYS = 0;

Simulation::Simulation(const cl::Context &clContext, const cl::Device &clDevice)
    : mCLContext(clContext),
      mCLDevice(clDevice),
      mPositions(NULL),
      mVelocities(NULL),
      mPredictions(NULL),
      mDeltas(NULL),
      mFriendsList(NULL),
      mCells(NULL),
      mParticlesList(NULL)
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
    float offsetY = 0.3;
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
        "parameters.hpp",
        "predict_positions.cl",
        "reset_grid.cl",
        "reset_part_list.cl",
        "update_cells.cl",
        "compute_scaling.cl",
		"pack_data.cl",
        "compute_delta.cl",
        "update_predicted.cl",
        "update_velocities.cl",
        "apply_viscosity.cl",
        "apply_vorticity.cl",
        "update_positions.cl",
        "build_friends_list.cl",
        "radixsort.cl",
        ""
    };

    return kernels;
}

bool Simulation::InitKernels()
{
    // Setup OpenCL Ranges
    const cl_uint globalSize = ceil(Params.particleCount / 32.0f) * 32;
    mGlobalRange = cl::NDRange(globalSize);
    mLocalRange = cl::NullRange;

    // setup kernel sources
    OCLUtils clSetup;
    vector<string> kernelSources;
    string header = clSetup.readSource(getPathForKernel("hesp.hpp"));

    // Load kernel sources
    const std::string *pKernels = KernelFileList();
    for (int iSrc = 0; pKernels[iSrc]  != ""; iSrc++)
    {
        string source = clSetup.readSource(getPathForKernel(pKernels[iSrc]));
        kernelSources.push_back(header + source);
    }

    // Setup kernel compiler flags
    std::ostringstream clflags;
    clflags << "-cl-mad-enable -cl-no-signed-zeros -cl-fast-relaxed-math ";

#ifdef USE_DEBUG
    clflags << "-DUSE_DEBUG ";
#endif // USE_DEBUG

    clflags << std::showpoint;
    //clflags << "-DSYSTEM_MIN_X="      << Params.xMin << "f ";
    //clflags << "-DSYSTEM_MAX_X="      << Params.xMax << "f ";
    //clflags << "-DSYSTEM_MIN_Y="      << Params.yMin << "f ";
    //clflags << "-DSYSTEM_MAX_Y="      << Params.yMax << "f ";
    //clflags << "-DSYSTEM_MIN_Z="      << Params.zMin << "f ";
    //clflags << "-DSYSTEM_MAX_Z="      << Params.zMax << "f ";
    //clflags << "-DGRID_RES="          << (int)Params.gridRes << " ";
    //clflags << "-DTIMESTEP="          << Params.timeStepLength << "f ";
    //clflags << "-DREST_DENSITY="      << Params.restDensity << "f ";
    //clflags << "-DPBF_H="             << Params.h << "f ";
    //clflags << "-DPBF_H_2="           << Params.h_2 << "f ";
    //clflags << "-DEPSILON="           << Params.epsilon << "f ";

    clflags << "-DEND_OF_CELL_LIST="            << (int)(-1)         << " ";

    clflags << "-DFRIENDS_CIRCLES="             << (int)(Params.friendsCircles)     << " ";  // Defines how many friends circle are we going to scan for
    clflags << "-DMAX_PARTICLES_IN_CIRCLE="     << (int)(Params.particlesPerCircle) << " ";  // Defines the max number of particles per cycle
    clflags << "-DPARTICLE_FRIENDS_BLOCK_SIZE=" << (int)(Params.friendsCircles + Params.friendsCircles * Params.particlesPerCircle) << " ";  // FRIENDS_CIRCLES + FRIENDS_CIRCLES * MAX_PARTICLES_IN_CIRCLE

	clflags << "-DGRID_BUG_SIZE="     << (int)(Params.gridBufSize) << " ";

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
    mBufferSizeScalingFactors = Params.particleCount * sizeof(cl_float);

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
    mPositionsYinBuffer    = cl::BufferGL(mCLContext, CL_MEM_READ_WRITE, mSharingYinBufferID); // buffer could be changed to be CL_MEM_WRITE_ONLY but for debugging also reading it might be helpful
    mPositionsYangBuffer   = cl::BufferGL(mCLContext, CL_MEM_READ_WRITE, mSharingYangBufferID); // buffer could be changed to be CL_MEM_WRITE_ONLY but for debugging also reading it might be helpful
    mPredictedBuffer       = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mVelocitiesYinBuffer   = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mVelocitiesYangBuffer  = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mDeltaBuffer           = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mDeltaVelocityBuffer   = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mOmegaBuffer           = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mScalingFactorsBuffer  = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeScalingFactors);
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

    // Copy mPositions (Host) => mPositionsYinBuffer (GPU) (we have to lock the shared buffer)
    vector<cl::Memory> sharedBuffers;
    sharedBuffers.push_back(mPositionsYinBuffer);
    mQueue = cl::CommandQueue(mCLContext, mCLDevice);
    mQueue.enqueueAcquireGLObjects(&sharedBuffers);
    mQueue.enqueueWriteBuffer(mPositionsYinBuffer, CL_TRUE, 0, mBufferSizeParticles, mPositions);
    mQueue.enqueueReleaseGLObjects(&sharedBuffers);
    mQueue.finish();

    // Copy mVelocities (Host) => mVelocitiesYinBuffer (GPU)
    mQueue.enqueueWriteBuffer(mVelocitiesYinBuffer, CL_TRUE, 0, mBufferSizeParticles, mVelocities);
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

void Simulation::updatePositions()
{
    int param = 0;
    mKernels["updatePositions"].setArg(param++, mPositionsYinBuffer);
    mKernels["updatePositions"].setArg(param++, mPredictedBuffer);
    mKernels["updatePositions"].setArg(param++, mVelocitiesYinBuffer);
    mKernels["updatePositions"].setArg(param++, mDeltaVelocityBuffer);
    mKernels["updatePositions"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["updatePositions"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("updatePositions"));
}

void Simulation::updateVelocities()
{
    int param = 0;
    mKernels["updateVelocities"].setArg(param++, mParameters);
    mKernels["updateVelocities"].setArg(param++, mPositionsYinBuffer);
    mKernels["updateVelocities"].setArg(param++, mPredictedBuffer);
    mKernels["updateVelocities"].setArg(param++, mVelocitiesYinBuffer);
    mKernels["updateVelocities"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["updateVelocities"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("updateVelocities"));
}

void Simulation::applyViscosity()
{
    int param = 0;
    mKernels["applyViscosity"].setArg(param++, mParameters);
    mKernels["applyViscosity"].setArg(param++, mPredictedBuffer);
    mKernels["applyViscosity"].setArg(param++, mVelocitiesYinBuffer);
    mKernels["applyViscosity"].setArg(param++, mDeltaVelocityBuffer);
    mKernels["applyViscosity"].setArg(param++, mOmegaBuffer);
    mKernels["applyViscosity"].setArg(param++, mFriendsListBuffer);
    mKernels["applyViscosity"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["applyViscosity"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("applyViscosity"));
}

void Simulation::applyVorticity()
{
    int param = 0;
    mKernels["applyVorticity"].setArg(param++, mParameters);
    mKernels["applyVorticity"].setArg(param++, mPredictedBuffer);
    mKernels["applyVorticity"].setArg(param++, mDeltaVelocityBuffer);
    mKernels["applyVorticity"].setArg(param++, mOmegaBuffer);
    mKernels["applyVorticity"].setArg(param++, mFriendsListBuffer);
    mKernels["applyVorticity"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["applyVorticity"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("applyVorticity"));
}

void Simulation::predictPositions()
{
    int param = 0;
    mKernels["predictPositions"].setArg(param++, mParameters);
    mKernels["predictPositions"].setArg(param++, mPositionsYinBuffer);
    mKernels["predictPositions"].setArg(param++, mPredictedBuffer);
    mKernels["predictPositions"].setArg(param++, mVelocitiesYinBuffer);
    mKernels["predictPositions"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["predictPositions"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("predictPositions"));
}

void Simulation::buildFriendsList()
{
    int param = 0;
    mKernels["buildFriendsList"].setArg(param++, mParameters);
    mKernels["buildFriendsList"].setArg(param++, mPredictedBuffer);
    mKernels["buildFriendsList"].setArg(param++, mCellsBuffer);
    mKernels["buildFriendsList"].setArg(param++, mParticlesListBuffer);
    mKernels["buildFriendsList"].setArg(param++, mFriendsListBuffer);
    mKernels["buildFriendsList"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["buildFriendsList"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("buildFriendsList"));
}

void Simulation::updatePredicted(int iterationIndex)
{
    int param = 0;
    mKernels["updatePredicted"].setArg(param++, mPredictedBuffer);
    mKernels["updatePredicted"].setArg(param++, mDeltaBuffer);
    mKernels["updatePredicted"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["updatePredicted"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("updatePredicted", iterationIndex));
}

void Simulation::packData(cl::Buffer packTarget, cl::Buffer packSource,  int iterationIndex)
{
    int param = 0;
    mKernels["packData"].setArg(param++, packTarget);
    mKernels["packData"].setArg(param++, packSource);

    mQueue.enqueueNDRangeKernel(mKernels["packData"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("packData", iterationIndex));
}

void Simulation::computeDelta(int iterationIndex)
{
    int param = 0;
    mKernels["computeDelta"].setArg(param++, mParameters);
    mKernels["computeDelta"].setArg(param++, mDeltaBuffer);
    mKernels["computeDelta"].setArg(param++, mPredictedBuffer); // xyz=Predicted z=Scaling
    mKernels["computeDelta"].setArg(param++, mFriendsListBuffer);
    mKernels["computeDelta"].setArg(param++, fWavePos);
    mKernels["computeDelta"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["computeDelta"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("computeDelta", iterationIndex));
}

void Simulation::computeScaling(int iterationIndex)
{
    int param = 0;
    mKernels["computeScaling"].setArg(param++, mParameters);
    mKernels["computeScaling"].setArg(param++, mPredictedBuffer);
    mKernels["computeScaling"].setArg(param++, mScalingFactorsBuffer);
    mKernels["computeScaling"].setArg(param++, mDensityBuffer);
    mKernels["computeScaling"].setArg(param++, mFriendsListBuffer);
    mKernels["computeScaling"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["computeScaling"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("computeScaling", iterationIndex));
}

void Simulation::updateCells()
{
    int param = 0;
    mKernels["resetGrid"].setArg(param++, mCellsBuffer);
	mQueue.enqueueNDRangeKernel(mKernels["resetGrid"], 0, cl::NDRange(Params.gridBufSize), mLocalRange, NULL, PerfData.GetTrackerEvent("resetGrid"));

	param = 0;
	mKernels["resetPartList"].setArg(param++, mCellsBuffer);
	mQueue.enqueueNDRangeKernel(mKernels["resetPartList"], 0, cl::NDRange(Params.particleCount), mLocalRange, NULL, PerfData.GetTrackerEvent("resetGrid"));

    param = 0;
    mKernels["updateCells"].setArg(param++, mParameters);
    mKernels["updateCells"].setArg(param++, mPredictedBuffer);
    mKernels["updateCells"].setArg(param++, mCellsBuffer);
    mKernels["updateCells"].setArg(param++, mParticlesListBuffer);
    mKernels["updateCells"].setArg(param++, Params.particleCount);
    mQueue.enqueueNDRangeKernel(mKernels["updateCells"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("updateCells"));
}

void Simulation::radixsort()
{
    int param = 0;
    mKernels["computeKeys"].setArg(param++, mParameters);
    mKernels["computeKeys"].setArg(param++, mPositionsYinBuffer);
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
    mKernels["sortParticles"].setArg(param++, mPositionsYinBuffer);
    mKernels["sortParticles"].setArg(param++, mPositionsYangBuffer);
    mKernels["sortParticles"].setArg(param++, mVelocitiesYinBuffer);
    mKernels["sortParticles"].setArg(param++, mVelocitiesYangBuffer);
    mKernels["sortParticles"].setArg(param++, Params.particleCount);
    mQueue.enqueueNDRangeKernel(mKernels["sortParticles"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("sortParticles"));

	// UnLock Yang buffer 
    //mQueue.enqueueReleaseGLObjects(&sharedBuffers);

	// Double buffering of positions and velocity buffers
    cl::BufferGL tmp1 = mPositionsYinBuffer;
    mPositionsYinBuffer = mPositionsYangBuffer;
    mPositionsYangBuffer = tmp1;

    cl::Buffer tmp2 = mVelocitiesYinBuffer;
    mVelocitiesYinBuffer = mVelocitiesYangBuffer;
    mVelocitiesYangBuffer = tmp2;

    GLuint tmp3 = mSharingYinBufferID;
    mSharingYinBufferID = mSharingYangBufferID;
    mSharingYangBufferID = tmp3;
}

void Simulation::Step()
{
    // Why is this here?
    glFinish();

    // Enqueue GL buffer acquire
    vector<cl::Memory> sharedBuffers;
    sharedBuffers.push_back(mPositionsYinBuffer);
	sharedBuffers.push_back(mPositionsYangBuffer);
    mQueue.enqueueAcquireGLObjects(&sharedBuffers);

	// Predicit positions
    this->predictPositions();

    // Update cells
    this->updateCells();

    // Build friends list
    this->buildFriendsList();
	if (bReadFriendsList)
		mQueue.enqueueReadBuffer(mFriendsListBuffer, CL_TRUE, 0, sizeof(cl_uint) * Params.particleCount * Params.friendsCircles * (1 + Params.particlesPerCircle), mFriendsList);

    for (unsigned int i = 0; i < Params.simIterations; ++i)
    {
        // Compute scaling value
        this->computeScaling(i);
        /*mQueue.enqueueReadBuffer(mPredictedBuffer, CL_TRUE, 0, mBufferSizeParticles, mPredictions);
        if (mQueue.finish() != CL_SUCCESS)
            _asm nop;*/

        // Compute position delta
        this->computeDelta(i);
        /*mQueue.enqueueReadBuffer(mDeltaBuffer, CL_TRUE, 0, mBufferSizeParticles, mDeltas);
        if (mQueue.finish() != CL_SUCCESS)
            _asm nop;*/
	
		// Update predicted position
        this->updatePredicted(i);
    }

	// Place density in "mPredictedBuffer[x].w"
	this->packData(mPredictedBuffer, mDensityBuffer, -1);

	// Recompute velocities
    this->updateVelocities();

    // Update vorticity and Viscosity
    this->applyViscosity();
    this->applyVorticity();

    // Update particle buffers
    if (!bPauseSim)
        this->updatePositions();

	// sort particles buffer
    this->radixsort();

    // Release OpenGL shared object, allowing openGL do to it's thing...
    mQueue.enqueueReleaseGLObjects(&sharedBuffers);
    mQueue.finish();

    // Collect performance data
    PerfData.UpdateTimings();
}

void Simulation::dumpData( cl_float4 * (&positions), cl_float4 * (&velocities) )
{
    mQueue.enqueueReadBuffer(mPositionsYinBuffer, CL_TRUE, 0, mBufferSizeParticles, mPositions);
    mQueue.enqueueReadBuffer(mVelocitiesYinBuffer, CL_TRUE, 0, mBufferSizeParticles, mVelocities);

    // just a safety measure to be absolutely sure everything is transferred
    // from device to host
    mQueue.finish();

    positions = mPositions;
    velocities = mVelocities;
}
