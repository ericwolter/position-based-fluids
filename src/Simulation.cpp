
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

cl::Memory Simulation::CreateCachedBuffer(cl::ImageFormat& format, int elements)
{
    if (format.image_channel_order != CL_RGBA)
        throw "Image type is not supported";

    // Choose what type should be created
    if (Params.EnableCachedBuffers)
        return cl::Image2D(mCLContext, CL_MEM_READ_WRITE, format, 2048, DivCeil(elements, 2048));
    else
        return cl::Buffer(mCLContext, CL_MEM_READ_WRITE, elements * sizeof(float) * 4);
}

void OCL_InitMemory(cl::CommandQueue& queue, cl::Memory& mem, void* pData = NULL, int nDataSize = 0)
{
    // Get buffer size
    int memSize = mem.getInfo<CL_MEM_SIZE>();

    // Create memory
    char* pBuf = new char[memSize];
    memset(pBuf, 0, memSize);

    // Fill with data
    if ((pData != NULL) && (nDataSize > 0))
        for (int i = 0; i < memSize; i++)
            pBuf[i] = ((char*)pData)[i % nDataSize];

    // Choose the way to transfer the data
    switch (mem.getInfo<CL_MEM_TYPE>())
    {
        case CL_MEM_OBJECT_BUFFER:
            queue.enqueueWriteBuffer(*((cl::Buffer*)&mem), CL_TRUE, 0, memSize, pBuf);
            break;
    }

    // Release memory
    delete[] pBuf;
}


Simulation::Simulation(const cl::Context &clContext, const cl::Device &clDevice)
    : mCLContext(clContext),
      mCLDevice(clDevice),
      bDumpParticlesData(false)
{
    // Create Queue
    mQueue = cl::CommandQueue(mCLContext, mCLDevice, CL_QUEUE_PROFILING_ENABLE);
}

Simulation::~Simulation()
{
    glFinish();
    mQueue.finish();
}

void Simulation::CreateParticles()
{
    // Create buffers
    cl_float4* positions   = new cl_float4[Params.particleCount];

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

        positions[i].s[0] = offsetX + (x /*+ (y % 2) * .5*/) * d;
        positions[i].s[1] = offsetY + (y) * d;
        positions[i].s[2] = offsetZ + (z /*+ (y % 2) * .5*/) * d;
        positions[i].s[3] = 0;
    }

    // Copy data from Host to GPU
    OCL_InitMemory(mQueue, mPositionsPingBuffer, positions , sizeof(positions[0])  * Params.particleCount);
    OCL_InitMemory(mQueue, mVelocitiesBuffer);

    delete[] positions;
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
        string source = getKernelSource(pKernels[iSrc]);

        // Patch kernel for logging
        if (pKernels[iSrc] != "logging.cl")
            source = oclLog.PatchKernel(source);

        // Load into compile list
        kernelSources.push_back(source);
    }

    // Setup kernel compiler flags
    std::ostringstream clflags;
    clflags << "-cl-mad-enable -cl-no-signed-zeros -cl-fast-relaxed-math ";

    // Vendor related flags
    string devVendor = mCLDevice.getInfo<CL_DEVICE_VENDOR>();
    if (devVendor.find("NVIDIA") != std::string::npos)
        clflags << "-cl-nv-verbose ";

    clflags << std::showpoint;

    clflags << "-DLOG_SIZE="                    << (int)1024 << " ";
    clflags << "-DEND_OF_CELL_LIST="            << (int)(-1)         << " ";

    clflags << "-DMAX_PARTICLES_COUNT="         << (int)(Params.particleCount)      << " ";  
    clflags << "-DMAX_FRIENDS_CIRCLES="         << (int)(Params.friendsCircles)     << " ";  
    clflags << "-DMAX_FRIENDS_IN_CIRCLE="       << (int)(Params.particlesPerCircle) << " ";  
    clflags << "-DFRIENDS_BLOCK_SIZE="          << (int)(Params.particleCount * Params.friendsCircles) << " ";  

    clflags << "-DGRID_BUF_SIZE="     << (int)(Params.gridBufSize) << " ";

    clflags << "-DPOLY6_FACTOR="      << 315.0f / (64.0f * M_PI * pow(Params.h, 9)) << "f ";
    clflags << "-DGRAD_SPIKY_FACTOR=" << 45.0f / (M_PI * pow(Params.h, 6)) << "f ";

    if (Params.EnableCachedBuffers)
        clflags << "-DENABLE_CACHED_BUFFERS ";

    // Compile kernels
    cl::Program program = clSetup.createProgram(kernelSources, mCLContext, mCLDevice, clflags.str());
    if (program() == 0)
        return false;

    // save BuildLog
    string buildLog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(mCLDevice);
    ofstream f("build.log", ios::out | ios::trunc);
    f << buildLog;
    f.close();

    // Build kernels table
    mKernels = clSetup.createKernelsMap(program);

    // Write kernel info
    cout << "CL_KERNEL_WORK_GROUP_SIZE=" << mKernels["computeDelta"].getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(mCLDevice) << endl;
    cout << "CL_KERNEL_LOCAL_MEM_SIZE =" << mKernels["computeDelta"].getWorkGroupInfo<CL_KERNEL_LOCAL_MEM_SIZE>(mCLDevice) << endl;

    return true;
}

void Simulation::InitBuffers()
{
    // Create buffers
    mPositionsPingBuffer   = cl::BufferGL(mCLContext, CL_MEM_READ_WRITE, mSharedPingBufferID); // buffer could be changed to be CL_MEM_WRITE_ONLY but for debugging also reading it might be helpful
    mPositionsPongBuffer   = cl::BufferGL(mCLContext, CL_MEM_READ_WRITE, mSharedPongBufferID); // buffer could be changed to be CL_MEM_WRITE_ONLY but for debugging also reading it might be helpful
    mParticlePosImg        = cl::Image2DGL(mCLContext, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, mSharedParticlesPos);

    mPredictedPingBuffer   = CreateCachedBuffer(cl::ImageFormat(CL_RGBA, CL_FLOAT), Params.particleCount);
    mPredictedPongBuffer   = CreateCachedBuffer(cl::ImageFormat(CL_RGBA, CL_FLOAT), Params.particleCount);
    mVelocitiesBuffer      = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, Params.particleCount * sizeof(cl_float4));
    mDeltaBuffer           = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, Params.particleCount * sizeof(cl_float4));
    mOmegaBuffer           = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, Params.particleCount * sizeof(cl_float4));
    mDensityBuffer         = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, Params.particleCount * sizeof(cl_float));
    mLambdaBuffer          = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, Params.particleCount * sizeof(cl_float));
    mParameters            = cl::Buffer(mCLContext, CL_MEM_READ_ONLY,  sizeof(Params));

    // Radix buffers
    mKeysCount             = IntCeil(Params.particleCount, _ITEMS * _GROUPS);
    mInKeysBuffer          = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * mKeysCount);
    mInPermutationBuffer   = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * mKeysCount);
    mOutKeysBuffer         = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * mKeysCount);
    mOutPermutationBuffer  = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * mKeysCount);
    mHistogramBuffer       = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _RADIX * _GROUPS * _ITEMS);
    mGlobSumBuffer         = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _HISTOSPLIT);
    mHistoTempBuffer       = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _HISTOSPLIT);

    // Update OpenGL lock list
    mGLLockList.push_back(mPositionsPingBuffer);
    mGLLockList.push_back(mPositionsPongBuffer);
    mGLLockList.push_back(mParticlePosImg);

    // Update mPositionsPingBuffer and mVelocitiesBuffer
    LockGLObjects();
    CreateParticles();
    UnlockGLObjects();

    // Copy Params (Host) => mParams (GPU)
    mQueue.enqueueWriteBuffer(mParameters, CL_TRUE, 0, sizeof(Params), &Params);
}

void Simulation::InitCells()
{
    // Write buffer for cells
    mCellsBuffer = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, Params.gridBufSize * 2 * sizeof(cl_uint));
    OCL_InitMemory(mQueue, mCellsBuffer, (void*)&END_OF_CELL_LIST, sizeof(END_OF_CELL_LIST));

    // Init Friends list buffer
    mFriendsListBuffer = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, Params.particleCount * Params.friendsCircles * (1 + Params.particlesPerCircle) * sizeof(cl_uint));
    OCL_InitMemory(mQueue, mFriendsListBuffer);
}

int dumpSession = 0;
int dumpCounter = 0;
int cycleCounter = 0;

void SaveFile(cl::CommandQueue queue, cl::Buffer buffer, const char *szFilename)
{
    // Exit if dump session is disabled
    if (dumpSession == 0)
        return;

    // Get buffer size
    int bufSize = buffer.getInfo<CL_MEM_SIZE>();

    // Read data from GPU
    char *buf = new char[bufSize];
    queue.enqueueReadBuffer(buffer, CL_TRUE, 0, bufSize, buf);
    queue.finish();

    // Compose file name
    dumpCounter++;
    char szTarget[256];
    sprintf(szTarget, "%s/dump%d/%d_%d_%s.bin", getRootPath().c_str(), dumpSession, dumpCounter, cycleCounter, szFilename);

    // Save to disk
    ofstream f(szTarget, ios::out | ios::trunc | ios::binary);
    f.seekp(0);
    f.write((const char *)buf, bufSize);
    f.close();

    delete[] buf;
}

void Simulation::updateVelocities()
{
    int param = 0; cl::Kernel kernel = mKernels["updateVelocities"];
    kernel.setArg(param++, mParameters);
    kernel.setArg(param++, mPositionsPingBuffer);
    kernel.setArg(param++, mPredictedPingBuffer);
    kernel.setArg(param++, mParticlePosImg);
    kernel.setArg(param++, mVelocitiesBuffer);
    kernel.setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(kernel, 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("updateVelocities"));
}

void Simulation::applyViscosity()
{
    int param = 0; cl::Kernel kernel = mKernels["applyViscosity"];
    kernel.setArg(param++, mParameters);
    kernel.setArg(param++, mPredictedPingBuffer);
    kernel.setArg(param++, mVelocitiesBuffer);
    kernel.setArg(param++, mOmegaBuffer);
    kernel.setArg(param++, mFriendsListBuffer);
    kernel.setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(kernel, 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("applyViscosity"));
}

void Simulation::applyVorticity()
{
    int param = 0; cl::Kernel kernel = mKernels["applyVorticity"];
    kernel.setArg(param++, mParameters);
    kernel.setArg(param++, mPredictedPingBuffer);
    kernel.setArg(param++, mVelocitiesBuffer);
    kernel.setArg(param++, mOmegaBuffer);
    kernel.setArg(param++, mFriendsListBuffer);
    kernel.setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(kernel, 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("applyVorticity"));
}

void Simulation::predictPositions()
{
    int param = 0; cl::Kernel kernel = mKernels["predictPositions"];
    kernel.setArg(param++, mParameters);
    kernel.setArg(param++, (cl_uint)bPauseSim);
    kernel.setArg(param++, mPositionsPingBuffer);
    kernel.setArg(param++, mPredictedPingBuffer);
    kernel.setArg(param++, mVelocitiesBuffer);
    kernel.setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(kernel, 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("predictPositions"));
}

void Simulation::buildFriendsList()
{
    int param = 0; cl::Kernel kernel = mKernels["buildFriendsList"];
    kernel.setArg(param++, mParameters);
    kernel.setArg(param++, mPredictedPingBuffer);
    kernel.setArg(param++, mCellsBuffer);
    kernel.setArg(param++, mFriendsListBuffer);
    kernel.setArg(param++, Params.particleCount);
    mQueue.enqueueNDRangeKernel(kernel, 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("buildFriendsList"));

    param = 0; kernel = mKernels["resetGrid"];
    kernel.setArg(param++, mParameters);
    kernel.setArg(param++, mInKeysBuffer);
    kernel.setArg(param++, mCellsBuffer);
    kernel.setArg(param++, Params.particleCount);
    mQueue.enqueueNDRangeKernel(kernel, 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("resetPartList"));
}

void Simulation::updatePredicted(int iterationIndex)
{
    int param = 0; cl::Kernel kernel = mKernels["updatePredicted"];
    kernel.setArg(param++, mPredictedPingBuffer);
    kernel.setArg(param++, mPredictedPongBuffer);
    kernel.setArg(param++, mDeltaBuffer);
    kernel.setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(kernel, 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("updatePredicted", iterationIndex));

    SWAP(cl::Memory, mPredictedPingBuffer, mPredictedPongBuffer);
}

void Simulation::packData(cl::Memory& sourceImg, cl::Memory& pongImg, cl::Buffer packSource,  int iterationIndex)
{
    int param = 0; cl::Kernel kernel = mKernels["packData"];
   kernel.setArg(param++, pongImg);
   kernel.setArg(param++, sourceImg);
   kernel.setArg(param++, packSource);
   kernel.setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(kernel, 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("packData", iterationIndex));

    // Swap between source and pong
    SWAP(cl::Memory, sourceImg, pongImg);
}

void Simulation::computeDelta(int iterationIndex)
{
    int param = 0; cl::Kernel kernel = mKernels["computeDelta"];
    kernel.setArg(param++, mParameters);
    kernel.setArg(param++, oclLog.GetDebugBuffer());
    kernel.setArg(param++, mDeltaBuffer);
    kernel.setArg(param++, mPredictedPingBuffer); // xyz=Predicted z=Scaling
    kernel.setArg(param++, mFriendsListBuffer);
    kernel.setArg(param++, fWavePos);
    kernel.setArg(param++, Params.particleCount);

#ifdef LOCALMEM
    mQueue.enqueueNDRangeKernel(kernel, 0, cl::NDRange(DivCeil(Params.particleCount, 256)*256), cl::NDRange(256), NULL, PerfData.GetTrackerEvent("computeDelta", iterationIndex));
#else
    mQueue.enqueueNDRangeKernel(kernel, 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("computeDelta", iterationIndex));
#endif
}

void Simulation::computeScaling(int iterationIndex)
{
    int param = 0; cl::Kernel kernel = mKernels["computeScaling"];
    kernel.setArg(param++, mParameters);
    kernel.setArg(param++, mPredictedPingBuffer);
    kernel.setArg(param++, mDensityBuffer);
    kernel.setArg(param++, mLambdaBuffer);
    kernel.setArg(param++, mFriendsListBuffer);
    kernel.setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(kernel, 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("computeScaling", iterationIndex));
    // mQueue.enqueueNDRangeKernel(kernel, 0, cl::NDRange(((Params.particleCount + 399) / 400) * 400), cl::NDRange(400), NULL, PerfData.GetTrackerEvent("computeScaling", iterationIndex));
}

void Simulation::updateCells()
{
    int param = 0; cl::Kernel kernel = mKernels["updateCells"];
    kernel.setArg(param++, mParameters);
    kernel.setArg(param++, mInKeysBuffer);
    kernel.setArg(param++, mCellsBuffer);
    kernel.setArg(param++, Params.particleCount);
    mQueue.enqueueNDRangeKernel(kernel, 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("updateCells"));
}

void Simulation::radixsort()
{
    int param = 0; cl::Kernel kernel = mKernels["computeKeys"];
    kernel.setArg(param++, mParameters);
    kernel.setArg(param++, mPredictedPingBuffer);
    kernel.setArg(param++, mInKeysBuffer);
    kernel.setArg(param++, mInPermutationBuffer);
    kernel.setArg(param++, Params.particleCount);
    mQueue.enqueueNDRangeKernel(kernel, 0, cl::NDRange(mKeysCount), mLocalRange, NULL, PerfData.GetTrackerEvent("computeKeys"));

    for (size_t pass = 0; pass < _PASS; pass++)
    {
        // Histogram(pass);
        const size_t h_nblocitems = _ITEMS;
        const size_t h_nbitems = _GROUPS * _ITEMS;
        param = 0; kernel = mKernels["histogram"];
        kernel.setArg(param++, mInKeysBuffer);
        kernel.setArg(param++, mHistogramBuffer);
        kernel.setArg(param++, pass);
        kernel.setArg(param++, sizeof(cl_uint) * _RADIX * _ITEMS, NULL);
        kernel.setArg(param++, mKeysCount);
        mQueue.enqueueNDRangeKernel(kernel, 0, cl::NDRange(h_nbitems), cl::NDRange(h_nblocitems), NULL, PerfData.GetTrackerEvent("histogram", pass));

        // ScanHistogram();
        param = 0; kernel = mKernels["scanhistograms"];
        const size_t sh1_nbitems = _RADIX * _GROUPS * _ITEMS / 2;
        const size_t sh1_nblocitems = sh1_nbitems / _HISTOSPLIT ;
        const int maxmemcache = max(_HISTOSPLIT, _ITEMS * _GROUPS * _RADIX / _HISTOSPLIT);
        kernel.setArg(param++, mHistogramBuffer);
        kernel.setArg(param++, sizeof(cl_uint)* maxmemcache, NULL);
        kernel.setArg(param++, mGlobSumBuffer);
        mQueue.enqueueNDRangeKernel(kernel, 0, cl::NDRange(sh1_nbitems), cl::NDRange(sh1_nblocitems), NULL, PerfData.GetTrackerEvent("scanhistograms1", pass));
        mQueue.finish();

        param = 0; kernel = mKernels["scanhistograms"];
        const size_t sh2_nbitems = _HISTOSPLIT / 2;
        const size_t sh2_nblocitems = sh2_nbitems;
        kernel.setArg(0, mGlobSumBuffer);
        kernel.setArg(2, mHistoTempBuffer);
        mQueue.enqueueNDRangeKernel(kernel, 0, cl::NDRange(sh2_nbitems), cl::NDRange(sh2_nblocitems), NULL, PerfData.GetTrackerEvent("scanhistograms2", pass));

        param = 0; kernel = mKernels["pastehistograms"];
        const size_t ph_nbitems = _RADIX * _GROUPS * _ITEMS / 2;
        const size_t ph_nblocitems = ph_nbitems / _HISTOSPLIT;
        kernel.setArg(param++, mHistogramBuffer);
        kernel.setArg(param++, mGlobSumBuffer);
        mQueue.enqueueNDRangeKernel(kernel, 0, cl::NDRange(ph_nbitems), cl::NDRange(ph_nblocitems), NULL, PerfData.GetTrackerEvent("pastehistograms", pass));

        // Reorder(pass);
        param = 0; kernel = mKernels["reorder"];
        const size_t r_nblocitems = _ITEMS;
        const size_t r_nbitems = _GROUPS * _ITEMS;
        kernel.setArg(param++, mInKeysBuffer);
        kernel.setArg(param++, mOutKeysBuffer);
        kernel.setArg(param++, mHistogramBuffer);
        kernel.setArg(param++, pass);
        kernel.setArg(param++, mInPermutationBuffer);
        kernel.setArg(param++, mOutPermutationBuffer);
        kernel.setArg(param++, sizeof(cl_uint)* _RADIX * _ITEMS, NULL);
        kernel.setArg(param++, mKeysCount);
        mQueue.enqueueNDRangeKernel(kernel, 0, cl::NDRange(r_nbitems), cl::NDRange(r_nblocitems), NULL, PerfData.GetTrackerEvent("reorder", pass));

        SWAP(cl::Buffer, mInKeysBuffer, mOutKeysBuffer);
        SWAP(cl::Buffer, mInPermutationBuffer, mOutPermutationBuffer);
    }

    // Execute particle reposition
    param = 0; kernel = mKernels["sortParticles"];
    kernel.setArg(param++, mInPermutationBuffer);
    kernel.setArg(param++, mPositionsPingBuffer);
    kernel.setArg(param++, mPositionsPongBuffer);
    kernel.setArg(param++, mPredictedPingBuffer);
    kernel.setArg(param++, mPredictedPongBuffer);
    kernel.setArg(param++, Params.particleCount);
    mQueue.enqueueNDRangeKernel(kernel, 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("sortParticles"));

    // Double buffering of positions and velocity buffers
    SWAP(cl::BufferGL, mPositionsPingBuffer, mPositionsPongBuffer);
    SWAP(cl::Memory,  mPredictedPingBuffer, mPredictedPongBuffer);
    SWAP(GLuint,       mSharedPingBufferID,  mSharedPongBufferID);
}

void Simulation::LockGLObjects()
{
    // Make sure OpenGL finish doing things (This is required according to OpenCL spec, see enqueueAcquireGLObjects)
    glFinish();

    // Request lock
    mQueue.enqueueAcquireGLObjects(&mGLLockList);
}

void Simulation::UnlockGLObjects()
{
    // Release lock
    mQueue.enqueueReleaseGLObjects(&mGLLockList);
    mQueue.finish();
}

void Simulation::Step()
{
    // Lock OpenGL objects
    LockGLObjects();

    // Inc sample counter
    cycleCounter++;

    // Predicit positions
    this->predictPositions();

    // sort particles buffer
    if (!bPauseSim)
        this->radixsort();

    // Update cells
    this->updateCells();

    // Build friends list
    this->buildFriendsList();

    for (unsigned int i = 0; i < Params.simIterations; ++i)
    {
        // Compute scaling value
        this->computeScaling(i);

        // Place lambda in "mPredictedPingBuffer[x].w"
        this->packData(mPredictedPingBuffer, mPredictedPongBuffer, mLambdaBuffer, i);

        // Compute position delta
        this->computeDelta(i);

        // Update predicted position
        this->updatePredicted(i);
    }

    // Place density in "mPredictedPingBuffer[x].w"
    this->packData(mPredictedPingBuffer, mPredictedPongBuffer, mDensityBuffer, -1);

    // Recompute velocities
    this->updateVelocities();

    // Update vorticity and Viscosity
    this->applyViscosity();
    //this->applyVorticity();

    // [DEBUG] Read back friends information (if needed)
    //if (bReadFriendsList || bDumpParticlesData)
        // TODO: Get frients list to host

    // [DEBUG] Do we need to dump particle data
    if (bDumpParticlesData)
    {
        // Turn off flag
        bDumpParticlesData = false;

        // TODO: Dump particles to disk
    }

    // Release OpenGL shared object, allowing openGL do to it's thing...
    UnlockGLObjects();

    // Collect performance data
    PerfData.UpdateTimings();

    // Allow OpenCL logger to process
    oclLog.CycleExecute(mQueue);
}
