
#include "Precomp_OpenGL.h"
#include "Simulation.hpp"
#include "Resources.hpp"
#include "ParamUtils.hpp"
#include "ocl/OCLUtils.hpp"
#include "OGL_Utils.h"

#include <sstream>
#include <algorithm>


using namespace std;

unsigned int _NKEYS = 0;

#define DivCeil(num, divider) ((num + divider - 1) / divider) 

#define  M_PI   3.14159265358979323846	/* pi */

Simulation::Simulation(const cl::Context &clContext, const cl::Device &clDevice)
    : mCLContext(clContext),
      mCLDevice(clDevice),
      mCells(NULL),
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
    // Reset velocities
    memset(mVelocities, 0, sizeof(mVelocities[0]) * Params.particleCount);

    // Choose between 2D and 3D
    float dim = 3.0;

    // Compute particle count per axis
    int ParticlesPerAxis = (int)ceil(pow(Params.particleCount, 1 / dim));

    // Build particles blcok
    float d = Params.h * Params.setupSpacing;
    float offsetX = (1.0f - ParticlesPerAxis * d) / 2.0f;
    float offsetY = 0.3f;
    float offsetZ = (1.0f - ParticlesPerAxis * d) / 2.0f;
    for (cl_uint i = 0; i < Params.particleCount; i++)
    {
        cl_uint x = ((cl_uint)(i / pow(ParticlesPerAxis, 1.0f)) % ParticlesPerAxis);
        cl_uint y = ((cl_uint)(i / pow(ParticlesPerAxis, 0.0f)) % ParticlesPerAxis);
        cl_uint z = ((cl_uint)(i / pow(ParticlesPerAxis, 2.0f)) % ParticlesPerAxis)  * (dim == 3);

        mPositions[i][0] = offsetX + x * d;
        mPositions[i][1] = offsetY + y * d;
        mPositions[i][2] = offsetZ + z * d;
        mPositions[i][3] = 0;
    }

    // random_shuffle(&mPositions[0],&mPositions[Params.particleCount-1]);
}

/*!*/const std::string *Simulation::CL_KernelFileList()
/*!*/{
/*!*/    static const std::string kernels[] =
/*!*/    {
/*!*/        "hesp.hpp",
/*!*/        "parameters.hpp",
/*!*/        "logging.cl",
/*!*/        "utilities.cl",
/*!*/        "predict_positions.cl",
/*!*/        "update_cells.cl",
/*!*/        "build_friends_list.cl",
/*!*/        "reset_grid.cl",
/*!*/        "compute_scaling.cl",
/*!*/        "compute_delta.cl",
/*!*/        "update_predicted.cl",
/*!*/        "pack_data.cl",
/*!*/        "update_velocities.cl",
/*!*/        "apply_viscosity.cl",
/*!*/        "apply_vorticity.cl",
/*!*/        "radixsort.cl",
/*!*/        ""
/*!*/    };
/*!*/
/*!*/    return kernels;
/*!*/}

/*!*/bool Simulation::CL_InitKernels()
/*!*/{
/*!*/    // Setup OpenCL Ranges
/*!*/    const cl_uint globalSize = (cl_uint)ceil(Params.particleCount / 32.0f) * 32;
/*!*/    mGlobalRange = cl::NDRange(globalSize);
/*!*/    mLocalRange = cl::NullRange;
/*!*/
/*!*/    // Notify OCL logging that we're about to start new kernel processing
/*!*/    oclLog.StartKernelProcessing(mCLContext, mCLDevice, 4096);
/*!*/
/*!*/    // setup kernel sources
/*!*/    OCLUtils clSetup;
/*!*/    vector<string> kernelSources;
/*!*/
/*!*/    // Load kernel sources
/*!*/    const std::string *pKernels = CL_KernelFileList();
/*!*/    for (int iSrc = 0; pKernels[iSrc]  != ""; iSrc++)
/*!*/    {
/*!*/        // Read source from disk
/*!*/        string source = getKernelSource(pKernels[iSrc]);
/*!*/
/*!*/        // Patch kernel for logging
/*!*/        if (pKernels[iSrc] != "logging.cl")
/*!*/            source = oclLog.PatchKernel(source);
/*!*/
/*!*/        // Load into compile list
/*!*/        kernelSources.push_back(source);
/*!*/    }
/*!*/
/*!*/    // Setup kernel compiler flags
/*!*/    std::ostringstream clflags;
/*!*/    clflags << "-cl-mad-enable -cl-no-signed-zeros -cl-fast-relaxed-math ";
/*!*/
/*!*/    // Vendor related flags
/*!*/    string devVendor = mCLDevice.getInfo<CL_DEVICE_VENDOR>();
/*!*/    if (devVendor.find("NVIDIA") != std::string::npos)
/*!*/        clflags << "-cl-nv-verbose ";
/*!*/
/*!*/#ifdef USE_DEBUG
/*!*/    clflags << "-DUSE_DEBUG ";
/*!*/#endif // USE_DEBUG
/*!*/
/*!*/    clflags << std::showpoint;
/*!*/
/*!*/    clflags << "-DCANCEL_RANDOMNESS ";
/*!*/
/*!*/    clflags << "-DLOG_SIZE="                    << (int)1024 << " ";
/*!*/    clflags << "-DEND_OF_CELL_LIST="            << (int)(-1)         << " ";
/*!*/
/*!*/    clflags << "-DMAX_PARTICLES_COUNT="         << (int)(Params.particleCount)      << " ";  
/*!*/    clflags << "-DMAX_FRIENDS_CIRCLES="         << (int)(Params.friendsCircles)     << " ";  
/*!*/    clflags << "-DMAX_FRIENDS_IN_CIRCLE="       << (int)(Params.particlesPerCircle) << " ";  
/*!*/    clflags << "-DFRIENDS_BLOCK_SIZE="          << (int)(Params.particleCount * Params.friendsCircles) << " ";  
/*!*/
/*!*/    clflags << "-DGRID_BUF_SIZE="     << (int)(Params.gridBufSize) << " ";
/*!*/
/*!*/    clflags << "-DPOLY6_FACTOR="      << 315.0f / (64.0f * M_PI * pow(Params.h, 9)) << "f ";
/*!*/    clflags << "-DGRAD_SPIKY_FACTOR=" << 45.0f / (M_PI * pow(Params.h, 6)) << "f ";
/*!*/
/*!*/    // Compile kernels
/*!*/    cl::Program program = clSetup.createProgram(kernelSources, mCLContext, mCLDevice, clflags.str());
/*!*/    if (program() == 0)
/*!*/        return false;
/*!*/
/*!*/    // save BuildLog
/*!*/    string buildLog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(mCLDevice);
/*!*/    ofstream f("build.log", ios::out | ios::trunc);
/*!*/    f << buildLog;
/*!*/    f.close();
/*!*/
/*!*/    // Build kernels table
/*!*/    mKernels = clSetup.createKernelsMap(program);
/*!*/
/*!*/    // Write kernel info
/*!*/    cout << "CL_KERNEL_WORK_GROUP_SIZE=" << mKernels["computeDelta"].getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(mCLDevice) << endl;
/*!*/    cout << "CL_KERNEL_LOCAL_MEM_SIZE =" << mKernels["computeDelta"].getWorkGroupInfo<CL_KERNEL_LOCAL_MEM_SIZE>(mCLDevice) << endl;
/*!*/
/*!*/    // Copy Params (Host) => mParams (GPU)
/*!*/    mQueue = cl::CommandQueue(mCLContext, mCLDevice, CL_QUEUE_PROFILING_ENABLE);
/*!*/    mQueue.enqueueWriteBuffer(mParameters, CL_TRUE, 0, sizeof(Params), &Params);
/*!*/    mQueue.finish();
/*!*/
/*!*/    return true;
/*!*/}

const string *Simulation::ShaderFileList()
{
    static const string shaders[] =
    {
        "predict_positions.cms",
        "compute_keys.cms",
        "histogram.cms",
        "scanhistograms1.cms",
        "scanhistograms2.cms",
        "pastehistograms.cms",
        "reorder.cms",
        "sort_particles.cms",
        "update_cells.cms",
        "build_friends_list.cms",
        "reset_grid.cms",
        "compute_scaling.cms",
        "compute_delta.cms",
        "update_predicted.cms",
        "pack_data.cms",
        ""
    };

    return shaders;
}

// Define bind points
#define BP_UBO_PARAMETERS 10

GLuint GenBuffer(GLenum target, int size, void* pData)
{
    GLuint ret;
    glGenBuffers(1, &ret);
    glBindBuffer(target, ret);
    glBufferData(target, size, pData, GL_DYNAMIC_COPY);
    glBindBuffer(target, 0);
    return ret;
}

GLuint GenTextureBuffer(GLenum format, GLuint bufferId)
{
    GLuint ret;
    glGenTextures(1, &ret);
    glBindTexture(GL_TEXTURE_BUFFER, ret);
    glTexBuffer(GL_TEXTURE_BUFFER, format, bufferId);
    glBindTexture(GL_TEXTURE_BUFFER, 0);
    return ret;
}

bool Simulation::InitShaders()
{
    // Build shader header
    string header = "#version 430\n#define GLSL_COMPILER\n";

    // Add Parameters 
    header += "#line 1\n" + getKernelSource("parameters.hpp") + "\n";

    // Add defines
    std::ostringstream defines;
    defines << "#define END_OF_CELL_LIST      (" << (int)(-1)                                  << ")"  << endl;
    defines << "#define MAX_PARTICLES_COUNT   (" << (int)(Params.particleCount)                << ")"  << endl;
    defines << "#define MAX_FRIENDS_CIRCLES   (" << (int)(Params.friendsCircles)               << ")"  << endl;
    defines << "#define MAX_FRIENDS_IN_CIRCLE (" << (int)(Params.particlesPerCircle)           << ")"  << endl;
    defines << "#define FRIENDS_BLOCK_SIZE    (" << (int)(Params.particleCount * Params.friendsCircles) << ")" << endl;
    defines << "#define GRID_BUF_SIZE         (" << (int)(Params.gridBufSize)                  << ")"  << endl;
    defines << "#define POLY6_FACTOR          (" << 315.0f / (64.0f * M_PI * pow(Params.h, 9)) << "f)" << endl;
    defines << "#define GRAD_SPIKY_FACTOR     (" << 45.0f / (M_PI * pow(Params.h, 6))          << "f)" << endl;
    header += "#line 1\n" + defines.str();

    // Add Utils
    header += "#line 1\n" + getKernelSource("utils.cms") + "\n";

    // Load and compile all shaders
    const string *pShaders = ShaderFileList();
    for (int iSrc = 0; pShaders[iSrc] != ""; iSrc++)
    {
        // Get shader name (no ext)
        size_t lastdot = pShaders[iSrc].find_last_of(".");
        string name = pShaders[iSrc].substr(0, lastdot);

        // Get source
        string src = header + "#line 1\n" + getKernelSource(pShaders[iSrc]);

        // compile shader
        if (!(mPrograms[name] = OGLU_LoadProgram(name, src, GL_COMPUTE_SHADER)))
            return false;
    }

    // Copy Parameters to GPU (parameters buffer object)
    mGLParametersUBO = GenBuffer(GL_UNIFORM_BUFFER, sizeof(Params), &Params);
    glBindBufferRange(GL_UNIFORM_BUFFER, BP_UBO_PARAMETERS, mGLParametersUBO, 0, sizeof(Params));

    return true;
}

void Simulation::InitBuffers()
{
    // Define CL buffer sizes
    mBufferSizeParticles      = Params.particleCount * sizeof(cl_float4);
    mBufferSizeParticlesList  = Params.particleCount * sizeof(cl_int);

    // Allocate CPU buffers
    delete[] mPositions;   mPositions   = new vec4[Params.particleCount];
    delete[] mVelocities;  mVelocities  = new vec4[Params.particleCount];
    delete[] mPredictions; mPredictions = new vec4[Params.particleCount]; // (used for debugging)
    delete[] mDeltas;      mDeltas      = new vec4[Params.particleCount]; // (used for debugging)
    delete[] mFriendsList; mFriendsList = new uint[Params.particleCount * Params.friendsCircles * (1 + Params.particlesPerCircle)]; // (used for debugging)

    // Position particles
    CreateParticles();

    // Create buffers
    /*!*/mPositionsPingBuffer   = cl::BufferGL(mCLContext, CL_MEM_READ_WRITE, mSharedPingBufferID); // buffer could be changed to be CL_MEM_WRITE_ONLY but for debugging also reading it might be helpful
    /*!*/mPositionsPongBuffer   = cl::BufferGL(mCLContext, CL_MEM_READ_WRITE, mSharedPongBufferID); // buffer could be changed to be CL_MEM_WRITE_ONLY but for debugging also reading it might be helpful
    mParticlePosImg        = cl::Image2DGL(mCLContext, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, mSharedParticlesPos);

    mPredictedPingSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * Params.particleCount, &mPositions[0]);
    mPredictedPingTBO = GenTextureBuffer(GL_RGBA32F, mPredictedPingSBO);
    /**/mPredictedPingBuffer   = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mPredictedPongSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * Params.particleCount, NULL);
    mPredictedPongTBO = GenTextureBuffer(GL_RGBA32F, mPredictedPongSBO);
    mPredictedPongBuffer   = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    /*!*/mVelocitiesBuffer      = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mDeltaSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * Params.particleCount, NULL);
    mDeltaTBO = GenTextureBuffer(GL_RGBA32F, mDeltaSBO);
    /*!*/mDeltaBuffer           = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mOmegaBuffer           = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mDensitySBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(float) * Params.particleCount, NULL);
    mDensityTBO = GenTextureBuffer(GL_R32F, mDensitySBO);
    /*!*/mDensityBuffer         = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, Params.particleCount * sizeof(cl_float));
    /*!*/mParameters            = cl::Buffer(mCLContext, CL_MEM_READ_ONLY,  sizeof(Params));
    /*!*/mStatsBuffer           = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * 2);

    // Radix buffers
    if (Params.particleCount % (_ITEMS * _GROUPS) == 0)
    {
        _NKEYS = Params.particleCount;
    }
    else
    {
        _NKEYS = Params.particleCount + (_ITEMS * _GROUPS) - Params.particleCount % (_ITEMS * _GROUPS);
    }
    /*!*/mInKeysBuffer          = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _NKEYS);
    mInKeysSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(uint) * _NKEYS, NULL);
    mInKeysTBO = GenTextureBuffer(GL_R32UI, mInKeysSBO);
    /*!*/mInPermutationBuffer   = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _NKEYS);
    mInPermutationSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(uint) * _NKEYS, NULL);
    mInPermutationTBO = GenTextureBuffer(GL_R32UI, mInPermutationSBO);
    /*!*/mOutKeysBuffer         = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _NKEYS);
    mOutKeysSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(uint) * _NKEYS, NULL);
    mOutKeysTBO = GenTextureBuffer(GL_R32UI, mOutKeysSBO);
    /*!*/mOutPermutationBuffer  = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _NKEYS);
    mOutPermutationSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(uint) * _NKEYS, NULL);
    mOutPermutationTBO = GenTextureBuffer(GL_R32UI, mOutPermutationSBO);    
    /*!*/mHistogramBuffer       = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _RADIX * _GROUPS * _ITEMS);
    mHistogramSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(uint) * _RADIX * _GROUPS * _ITEMS, NULL);
    mHistogramTBO = GenTextureBuffer(GL_R32UI, mHistogramSBO);
    /*!*/mGlobSumBuffer         = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _HISTOSPLIT);
    mGlobSumSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(uint) * _HISTOSPLIT, NULL);
    mGlobSumTBO = GenTextureBuffer(GL_R32UI, mGlobSumSBO);
    /*!*/mHistoTempBuffer       = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _HISTOSPLIT);
    mHistoTempSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(uint) * _HISTOSPLIT, NULL);
    mHistoTempTBO = GenTextureBuffer(GL_R32UI, mHistoTempSBO);

    /*!*/if (mQueue() != 0)
    /*!*/    mQueue.flush();

    // Copy mPositions (Host) => mPositionsPingBuffer (GPU) 
    mPositionsPingSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * Params.particleCount, &mPositions[0]);
    mPositionsPingTBO = GenTextureBuffer(GL_RGBA32F, mPositionsPingSBO);
    mPositionsPongSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * Params.particleCount, NULL);
    mPositionsPongTBO = GenTextureBuffer(GL_RGBA32F, mPositionsPongSBO);
    /*!*/vector<cl::Memory> sharedBuffers;
    /*!*/sharedBuffers.push_back(mPositionsPingBuffer);
    /*!*/mQueue = cl::CommandQueue(mCLContext, mCLDevice);
    /*!*/mQueue.enqueueAcquireGLObjects(&sharedBuffers);
    /*!*/mQueue.enqueueWriteBuffer(mPositionsPingBuffer, CL_TRUE, 0, mBufferSizeParticles, mPositions);
    /*!*/mQueue.enqueueReleaseGLObjects(&sharedBuffers);
    /*!*/mQueue.finish();

    // Copy mVelocities (Host) => mVelocitiesBuffer (GPU)
    mVelocitiesSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * Params.particleCount, &mVelocities[0]);
    mVelocitiesTBO = GenTextureBuffer(GL_RGBA32F, mVelocitiesSBO);
    /*!*/mQueue.enqueueWriteBuffer(mVelocitiesBuffer, CL_TRUE, 0, mBufferSizeParticles, mVelocities);
    /*!*/mQueue.finish();

    /*!*/cl_uint *stats = new cl_uint[2];
    /*!*/stats[0] = 0;
    /*!*/stats[1] = 0;
    /*!*/mQueue.enqueueWriteBuffer(mStatsBuffer, CL_TRUE, 0, sizeof(cl_uint) * 2, stats);
    /*!*/mQueue.finish();
}

void Simulation::InitCells()
{
    // Allocate host buffers
    delete[] mCells;
    mCells = new cl_int[Params.gridBufSize * 2];
    for (cl_int i = 0; i < Params.gridBufSize * 2; ++i)
        mCells[i] = END_OF_CELL_LIST;

    // Write buffer for cells
    mCellsSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, Params.gridBufSize * 2 * sizeof(cl_int), mCells);
    mCellsTBO = GenTextureBuffer(GL_R32I, mCellsSBO);
    /*!*/mCellsBuffer = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, Params.gridBufSize * 2 * sizeof(cl_int));
    /*!*/mQueue.enqueueWriteBuffer(mCellsBuffer, CL_TRUE, 0, Params.gridBufSize * 2 * sizeof(cl_int), mCells);

    // Init Friends list buffer
    int BufSize = Params.particleCount * Params.friendsCircles * (1 + Params.particlesPerCircle) * sizeof(cl_int);
    memset(mFriendsList, 0, BufSize);

    mFriendsListSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, BufSize, mFriendsList);
    mFriendsListTBO = GenTextureBuffer(GL_R32UI, mFriendsListSBO);

    mFriendsListBuffer = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, BufSize);
    mQueue.enqueueWriteBuffer(mFriendsListBuffer, CL_TRUE, 0, BufSize, mFriendsList);
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
    int param = 0;
    mKernels["updateVelocities"].setArg(param++, mParameters);
    mKernels["updateVelocities"].setArg(param++, mPositionsPingBuffer);
    mKernels["updateVelocities"].setArg(param++, mPredictedPingBuffer);
    mKernels["updateVelocities"].setArg(param++, mParticlePosImg);
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
    mKernels["applyVorticity"].setArg(param++, mVelocitiesBuffer);
    mKernels["applyVorticity"].setArg(param++, mOmegaBuffer);
    mKernels["applyVorticity"].setArg(param++, mFriendsListBuffer);
    mKernels["applyVorticity"].setArg(param++, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["applyVorticity"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("applyVorticity"));

    //SaveFile(mQueue, mDeltaVelocityBuffer, "Omega");
}

void CompareIntBuffers(cl::CommandQueue queue, cl::Buffer clBuf, GLuint glBuf)
{
    CopyBufferToHost gl(GL_SHADER_STORAGE_BUFFER, glBuf);
    OCL_CopyBufferToHost cl(queue, clBuf);

    if (gl.size != cl.size)
        throw "Size does not match";

    for (int i = 0; i < cl.size / 4; i++) {
        int clv = cl.pIntegers[i];
        int glv = gl.pIntegers[i];

        if (clv - glv != 0)
        {
            _asm nop;
            break;
        };
    }
}

void CompareFloatBuffers(cl::CommandQueue queue, cl::Buffer clBuf, GLuint glBuf)
{
    CopyBufferToHost gl(GL_SHADER_STORAGE_BUFFER, glBuf);
    OCL_CopyBufferToHost cl(queue, clBuf);

    if (gl.size != cl.size)
        throw "Size does not match";

    for (int i = 0; i < cl.size / 4; i++) {
        float clv = cl.pFloats[i];
        float glv = gl.pFloats[i];
        float diff = abs(clv - glv);

        if (diff > 0.01)
        {
            _asm nop;
            break;
        };
    }
}

void CL2GL_BufferCopy(cl::CommandQueue queue, cl::Buffer clBuf, GLuint glBuf)
{
    OCL_CopyBufferToHost cl(queue, clBuf);

    // Send data to OpenGL
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, glBuf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, cl.size, cl.pBytes, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void Simulation::predictPositions()
{
    // Setup shader
    glUseProgram(g_SelectedProgram = mPrograms["predict_positions"]);
    glBindImageTexture(0, mPositionsPingTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_RGBA32F);
    glBindImageTexture(1, mPredictedPingTBO, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(2, mVelocitiesTBO,    0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    glUniform1i(0/*N*/,        Params.particleCount);
    glUniform1i(1/*pauseSim*/, bPauseSim);

    // Execute shader
    glDispatchCompute(Params.particleCount, 1, 1);

    /*!*/int param = 0;
    /*!*/mKernels["predictPositions"].setArg(param++, mParameters);
    /*!*/mKernels["predictPositions"].setArg(param++, (cl_uint)bPauseSim);
    /*!*/mKernels["predictPositions"].setArg(param++, mPositionsPingBuffer);
    /*!*/mKernels["predictPositions"].setArg(param++, mPredictedPingBuffer);
    /*!*/mKernels["predictPositions"].setArg(param++, mVelocitiesBuffer);
    /*!*/mKernels["predictPositions"].setArg(param++, Params.particleCount);
    /*!*/
    /*!*/mQueue.enqueueNDRangeKernel(mKernels["predictPositions"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("predictPositions"));
    /*!*/
    /*!*/CompareFloatBuffers(mQueue, mPredictedPingBuffer, mPredictedPingSBO);
    /*!*/CompareFloatBuffers(mQueue, mVelocitiesBuffer, mVelocitiesSBO);
}

void DumpFriendsList(uint* friendList, float* position, char* szFilename)
{
    ofstream fdmp(szFilename);

    // Compute histogram data
    int FriendsCountBlockSize = Params.particleCount * Params.friendsCircles;
    for (unsigned int iPart = 0; iPart < Params.particleCount; iPart++)
    {
        fdmp << "Particle #" << iPart << ":" << endl;
        for (unsigned int iCircle = 0; iCircle < Params.friendsCircles; iCircle++)
        {
            int circleFriends = friendList[iCircle * Params.particleCount + iPart];
            float px = position[iPart * 4 + 0];
            float py = position[iPart * 4 + 1];
            float pz = position[iPart * 4 + 2];

            // Write circle header
            fdmp << "  C" << iCircle << " [" << circleFriends << "]: ";

            // Make sure circleFriends is not bigger than Params.particlesPerCircle
            if (circleFriends > Params.particlesPerCircle)
                circleFriends = Params.particlesPerCircle;

            // Print friends IDs
            for (int iFriend = 0; iFriend < circleFriends; iFriend++)
            {
                int friendIndex = friendList[FriendsCountBlockSize + 
                                             iCircle * (Params.particleCount * Params.particlesPerCircle) +  
                                             iFriend * (Params.particleCount) + 
                                             iPart];
                float dx = position[friendIndex * 4 + 0] - px;
                float dy = position[friendIndex * 4 + 1] - py;
                float dz = position[friendIndex * 4 + 2] - pz;
                fdmp << friendIndex << "(" << sqrt(dx*dx+dy*dy+dz*dz) / Params.h << ") ";
            }

            fdmp << endl;
        }
        fdmp << endl;
    }
}

void Simulation::buildFriendsList()
{
    // Setup shader
    glUseProgram(g_SelectedProgram = mPrograms["build_friends_list"]);
    glBindImageTexture(0, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_RGBA32F);
    glBindImageTexture(1, mCellsTBO,         0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32I);
    glBindImageTexture(2, mFriendsListTBO,   0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32I);
    glUniform1i(0/*N*/,        Params.particleCount);

    // Execute shader
    glDispatchCompute(Params.particleCount, 1, 1);

    /*!*/int param = 0;
    /*!*/mKernels["buildFriendsList"].setArg(param++, mParameters);
    /*!*/mKernels["buildFriendsList"].setArg(param++, mPredictedPingBuffer);
    /*!*/mKernels["buildFriendsList"].setArg(param++, mCellsBuffer);
    /*!*/mKernels["buildFriendsList"].setArg(param++, mFriendsListBuffer);
    /*!*/mKernels["buildFriendsList"].setArg(param++, Params.particleCount);
    /*!*/mQueue.enqueueNDRangeKernel(mKernels["buildFriendsList"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("buildFriendsList"));
    /*!*/
    /*!*///CopyBufferToHost pos(GL_SHADER_STORAGE_BUFFER, mPredictedPingSBO);
    /*!*/
    /*!*///OCL_CopyBufferToHost cl_flist(mQueue, mFriendsListBuffer);
    /*!*///DumpFriendsList((uint*)cl_flist.pIntegers, pos.pFloats, "CL_FriendsList.txt");
    /*!*/
    /*!*///CopyBufferToHost gl_flist(GL_SHADER_STORAGE_BUFFER, mFriendsListSBO);
    /*!*///DumpFriendsList((uint*)gl_flist.pIntegers, pos.pFloats, "GL_FriendsList.txt");
    /*!*/
    /*!*/CompareIntBuffers(mQueue, mFriendsListBuffer, mFriendsListSBO);

    // Setup shader
    glUseProgram(g_SelectedProgram = mPrograms["reset_grid"]);
    glBindImageTexture(0, mInKeysTBO, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32I);
    glBindImageTexture(1, mCellsTBO,  0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32I);
    glUniform1i(0/*N*/,        Params.particleCount);

    // Execute shader
    glDispatchCompute(Params.particleCount, 1, 1);

    /*!*/param = 0;
    /*!*/mKernels["resetGrid"].setArg(param++, mParameters);
    /*!*/mKernels["resetGrid"].setArg(param++, mInKeysBuffer);
    /*!*/mKernels["resetGrid"].setArg(param++, mCellsBuffer);
    /*!*/mKernels["resetGrid"].setArg(param++, Params.particleCount);
    /*!*/mQueue.enqueueNDRangeKernel(mKernels["resetGrid"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("resetPartList"));

    /*!*/CompareIntBuffers(mQueue, mInKeysBuffer, mInKeysSBO);
    /*!*/CompareIntBuffers(mQueue, mCellsBuffer, mCellsSBO);
}

void Simulation::updatePredicted(int iterationIndex)
{
    glUseProgram(g_SelectedProgram = mPrograms["update_predicted"]);
    glBindImageTexture(0, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindImageTexture(1, mDeltaTBO,         0, GL_FALSE, 0, GL_READ_ONLY,  GL_RGBA32F);
    glUniform1i(0/*N*/, Params.particleCount);
    
    // Execute shader
    glDispatchCompute(Params.particleCount, 1, 1);

    /*!*/int param = 0;
    /*!*/mKernels["updatePredicted"].setArg(param++, mPredictedPingBuffer);
    /*!*/mKernels["updatePredicted"].setArg(param++, mDeltaBuffer);
    /*!*/mKernels["updatePredicted"].setArg(param++, Params.particleCount);

    /*!*/mQueue.enqueueNDRangeKernel(mKernels["updatePredicted"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("updatePredicted", iterationIndex));

    /*!*/CompareFloatBuffers(mQueue, mPredictedPingBuffer, mPredictedPingSBO);
}

void Simulation::packData()
{
    glUseProgram(g_SelectedProgram = mPrograms["pack_data"]);
    glBindImageTexture(0, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindImageTexture(1, mDensityTBO,       0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32F);
    glUniform1i(0/*N*/, Params.particleCount);
    
    // Execute shader
    glDispatchCompute(Params.particleCount, 1, 1);

    /*!*/int param = 0;
    /*!*/mKernels["packData"].setArg(param++, mPredictedPingBuffer);
    /*!*/mKernels["packData"].setArg(param++, mDensityBuffer);
    /*!*/mKernels["packData"].setArg(param++, Params.particleCount);

    /*!*/mQueue.enqueueNDRangeKernel(mKernels["packData"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("packData"));

    /*!*/CompareFloatBuffers(mQueue, mPredictedPingBuffer, mPredictedPingSBO);
}

void Simulation::computeDelta(int iterationIndex)
{
    glUseProgram(g_SelectedProgram = mPrograms["compute_delta"]);
    glBindImageTexture(0, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_RGBA32F);
    glBindImageTexture(1, mDeltaTBO,         0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(2, mFriendsListTBO,   0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32I);
    glUniform1i(0/*N*/, Params.particleCount);
    glUniform1f(1/*wave_generator*/, fWavePos);
    
    // Execute shader
    glDispatchCompute(Params.particleCount, 1, 1);

    /*!*/int param = 0;
    /*!*/mKernels["computeDelta"].setArg(param++, mParameters);
    /*!*/mKernels["computeDelta"].setArg(param++, oclLog.GetDebugBuffer());
    /*!*/mKernels["computeDelta"].setArg(param++, mDeltaBuffer);
    /*!*/mKernels["computeDelta"].setArg(param++, mPredictedPingBuffer); // xyz=Predicted z=Scaling
    /*!*/mKernels["computeDelta"].setArg(param++, mFriendsListBuffer);
    /*!*/mKernels["computeDelta"].setArg(param++, fWavePos);
    /*!*/mKernels["computeDelta"].setArg(param++, Params.particleCount);

    // std::cout << "CL_KERNEL_LOCAL_MEM_SIZE = " << mKernels["computeDelta"].getWorkGroupInfo<CL_KERNEL_LOCAL_MEM_SIZE>(NULL) << std::endl;
    // std::cout << "CL_KERNEL_PRIVATE_MEM_SIZE = " << mKernels["computeDelta"].getWorkGroupInfo<CL_KERNEL_PRIVATE_MEM_SIZE>(NULL) << std::endl;

/*!*/#ifdef LOCALMEM
/*!*/    mQueue.enqueueNDRangeKernel(mKernels["computeDelta"], 0, cl::NDRange(DivCeil(Params.particleCount, 256)*256), cl::NDRange(256), NULL, PerfData.GetTrackerEvent("computeDelta", iterationIndex));
/*!*/#else
/*!*/    mQueue.enqueueNDRangeKernel(mKernels["computeDelta"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("computeDelta", iterationIndex));
/*!*/#endif

    /*!*/CompareFloatBuffers(mQueue, mDeltaBuffer, mDeltaSBO);

    //SaveFile(mQueue, mDeltaBuffer, "delta2");
}

void Simulation::computeScaling(int iterationIndex)
{
    glUseProgram(g_SelectedProgram = mPrograms["compute_scaling"]);
    glBindImageTexture(0, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_WRITE,  GL_RGBA32F);
    glBindImageTexture(1, mDensityTBO,       0, GL_FALSE, 0, GL_WRITE_ONLY,  GL_R32F);
    glBindImageTexture(2, mFriendsListTBO,   0, GL_FALSE, 0, GL_READ_ONLY,   GL_R32I);
    glUniform1i(0/*N*/, Params.particleCount);
    
    // Execute shader
    glDispatchCompute(Params.particleCount, 1, 1);

    /*!*/int param = 0;
    /*!*/mKernels["computeScaling"].setArg(param++, mParameters);
    /*!*/mKernels["computeScaling"].setArg(param++, mPredictedPingBuffer);
    /*!*/mKernels["computeScaling"].setArg(param++, mDensityBuffer);
    /*!*/mKernels["computeScaling"].setArg(param++, mFriendsListBuffer);
    /*!*/mKernels["computeScaling"].setArg(param++, Params.particleCount);

    // std::cout << "CL_KERNEL_LOCAL_MEM_SIZE = " << mKernels["computeScaling"].getWorkGroupInfo<CL_KERNEL_LOCAL_MEM_SIZE>(NULL) << std::endl;
    // std::cout << "CL_KERNEL_PRIVATE_MEM_SIZE = " << mKernels["computeScaling"].getWorkGroupInfo<CL_KERNEL_PRIVATE_MEM_SIZE>(NULL) << std::endl;

    /*!*/mQueue.enqueueNDRangeKernel(mKernels["computeScaling"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("computeScaling", iterationIndex));
    // mQueue.enqueueNDRangeKernel(mKernels["computeScaling"], 0, cl::NDRange(((Params.particleCount + 399) / 400) * 400), cl::NDRange(400), NULL, PerfData.GetTrackerEvent("computeScaling", iterationIndex));

    //SaveFile(mQueue, mPredictedPingBuffer, "pred2");
    //SaveFile(mQueue, mDensityBuffer,       "dens2");

    /*!*/CompareFloatBuffers(mQueue, mDensityBuffer, mDensitySBO);
    /*!*/CompareFloatBuffers(mQueue, mPredictedPingBuffer, mPredictedPingSBO);

}

void Simulation::updateCells()
{
    /*!*/CL2GL_BufferCopy(mQueue, mInKeysBuffer, mInKeysSBO);
    /*!*/CL2GL_BufferCopy(mQueue, mCellsBuffer, mCellsSBO);

    glUseProgram(g_SelectedProgram = mPrograms["update_cells"]);
    glBindImageTexture(0, mInKeysTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32I);
    glBindImageTexture(1, mCellsTBO,  0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32I);
    glUniform1i(0/*N*/, Params.particleCount);
    
    // Execute shader
    glDispatchCompute(Params.particleCount, 1, 1);

    /*!*/int param = 0;
    /*!*/mKernels["updateCells"].setArg(param++, mParameters);
    /*!*/mKernels["updateCells"].setArg(param++, mInKeysBuffer);
    /*!*/mKernels["updateCells"].setArg(param++, mCellsBuffer);
    /*!*/mKernels["updateCells"].setArg(param++, Params.particleCount);
    /*!*/mQueue.enqueueNDRangeKernel(mKernels["updateCells"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("updateCells"));
    /*!*/
    /*!*/CompareIntBuffers(mQueue, mCellsBuffer, mCellsSBO);
}

void Simulation::radixsort()
{
    // Setup
    glUseProgram(g_SelectedProgram = mPrograms["compute_keys"]);
    glBindImageTexture(0, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_RGBA32F);
    glBindImageTexture(1, mInKeysTBO,        0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);
    glBindImageTexture(2, mInPermutationTBO, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);

    glUniform1i(0/*N*/, Params.particleCount);

    // Execute shader
    glDispatchCompute(_NKEYS, 1, 1);

    /*!*/int param = 0;
    /*!*/mKernels["computeKeys"].setArg(param++, mParameters);
    /*!*/mKernels["computeKeys"].setArg(param++, mPredictedPingBuffer);
    /*!*/mKernels["computeKeys"].setArg(param++, mInKeysBuffer);
    /*!*/mKernels["computeKeys"].setArg(param++, mInPermutationBuffer);
    /*!*/mKernels["computeKeys"].setArg(param++, Params.particleCount);
    /*!*/mQueue.enqueueNDRangeKernel(mKernels["computeKeys"], 0, cl::NDRange(_NKEYS), mLocalRange, NULL, PerfData.GetTrackerEvent("computeKeys"));

    /*!*/CompareIntBuffers(mQueue, mInKeysBuffer, mInKeysSBO);
    /*!*/CompareIntBuffers(mQueue, mInPermutationBuffer, mInPermutationSBO);

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

        // Setup
        glUseProgram(g_SelectedProgram = mPrograms["histogram"]);
        glBindImageTexture(0, mInKeysTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32UI);
        glBindImageTexture(1, mHistogramTBO, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);
        glUniform1i(0, pass);
        glUniform1i(1, _NKEYS);

        // Execute shader
        glDispatchCompute(h_nbitems / h_nblocitems, 1, 1);

        /*!*/param = 0;
        /*!*/mKernels["histogram"].setArg(param++, mInKeysBuffer);
        /*!*/mKernels["histogram"].setArg(param++, mHistogramBuffer);
        /*!*/mKernels["histogram"].setArg(param++, pass);
        /*!*/mKernels["histogram"].setArg(param++, sizeof(cl_uint) * _RADIX * _ITEMS, NULL);
        /*!*/mKernels["histogram"].setArg(param++, _NKEYS);
        /*!*/mQueue.enqueueNDRangeKernel(mKernels["histogram"], 0, cl::NDRange(h_nbitems), cl::NDRange(h_nblocitems), NULL, PerfData.GetTrackerEvent("histogram", pass));

        /*!*/CompareIntBuffers(mQueue, mHistogramBuffer, mHistogramSBO);

        // ScanHistogram();
        const size_t sh1_nbitems = _RADIX * _GROUPS * _ITEMS / 2;
        const size_t sh1_nblocitems = sh1_nbitems / _HISTOSPLIT ;
        const int maxmemcache = glm::max(_HISTOSPLIT, _ITEMS * _GROUPS * _RADIX / _HISTOSPLIT);

        // Setup
        glUseProgram(g_SelectedProgram = mPrograms["scanhistograms1"]);
        glBindImageTexture(0, mHistogramTBO, 0, GL_FALSE, 0, GL_READ_WRITE,  GL_R32UI);
        glBindImageTexture(1, mGlobSumTBO, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

        // Execute shader
        glDispatchCompute(sh1_nbitems / sh1_nblocitems, 1, 1);

        /*!*/mKernels["scanhistograms"].setArg(0, mHistogramBuffer);
        /*!*/mKernels["scanhistograms"].setArg(1, sizeof(cl_uint)* maxmemcache, NULL);
        /*!*/mKernels["scanhistograms"].setArg(2, mGlobSumBuffer);
        /*!*/mQueue.enqueueNDRangeKernel(mKernels["scanhistograms"], 0, cl::NDRange(sh1_nbitems), cl::NDRange(sh1_nblocitems), NULL, PerfData.GetTrackerEvent("scanhistograms1", pass));
        /*!*/mQueue.finish();

        /*!*/CompareIntBuffers(mQueue, mHistogramBuffer, mHistogramSBO);
        /*!*/CompareIntBuffers(mQueue, mGlobSumBuffer, mGlobSumSBO);

        const size_t sh2_nbitems = _HISTOSPLIT / 2;
        const size_t sh2_nblocitems = sh2_nbitems;

        // Setup
        glUseProgram(g_SelectedProgram = mPrograms["scanhistograms2"]);
        glBindImageTexture(0, mGlobSumTBO, 0, GL_FALSE, 0, GL_READ_WRITE,  GL_R32UI);
        glBindImageTexture(1, mHistoTempTBO, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

        // Execute shader
        glDispatchCompute(sh2_nbitems / sh2_nblocitems, 1, 1);

        /*!*/mKernels["scanhistograms"].setArg(0, mGlobSumBuffer);
        /*!*/mKernels["scanhistograms"].setArg(2, mHistoTempBuffer);
        /*!*/mQueue.enqueueNDRangeKernel(mKernels["scanhistograms"], 0, cl::NDRange(sh2_nbitems), cl::NDRange(sh2_nblocitems), NULL, PerfData.GetTrackerEvent("scanhistograms2", pass));

        /*!*/CompareIntBuffers(mQueue, mGlobSumBuffer, mGlobSumSBO);
        // The diff here for i>0 should be okay because the rest of the data is never written to
        // TODO: Also why is that even needed at all? Seems like now that we need a second shader anyway
        // because of the compile-time local_size it might be better just to write a different shader
        // which gets rid of this temp buffer completely
        ///*!*/CompareIntBuffers(mQueue, mHistoTempBuffer, mHistoTempSBO);

        const size_t ph_nbitems = _RADIX * _GROUPS * _ITEMS / 2;
        const size_t ph_nblocitems = ph_nbitems / _HISTOSPLIT;

        // Setup
        glUseProgram(g_SelectedProgram = mPrograms["pastehistograms"]);
        glBindImageTexture(0, mHistogramTBO, 0, GL_FALSE, 0, GL_READ_WRITE,  GL_R32UI);
        glBindImageTexture(1, mGlobSumTBO, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);

        // Execute shader
        glDispatchCompute(ph_nbitems / ph_nblocitems, 1, 1);

        /*!*/param = 0;
        /*!*/mKernels["pastehistograms"].setArg(param++, mHistogramBuffer);
        /*!*/mKernels["pastehistograms"].setArg(param++, mGlobSumBuffer);
        /*!*/mQueue.enqueueNDRangeKernel(mKernels["pastehistograms"], 0, cl::NDRange(ph_nbitems), cl::NDRange(ph_nblocitems), NULL, PerfData.GetTrackerEvent("pastehistograms", pass));

        /*!*/CompareIntBuffers(mQueue, mHistogramBuffer, mHistogramSBO);

        // Reorder(pass);
        const size_t r_nblocitems = _ITEMS;
        const size_t r_nbitems = _GROUPS * _ITEMS;

        // Setup
        glUseProgram(g_SelectedProgram = mPrograms["reorder"]);
        glBindImageTexture(0, mInKeysTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32UI);
        glBindImageTexture(1, mOutKeysTBO, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);
        glBindImageTexture(2, mHistogramTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32UI);
        glBindImageTexture(3, mInPermutationTBO, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);
        glBindImageTexture(4, mOutPermutationTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32UI);
        glUniform1i(0, pass);
        glUniform1i(1, _NKEYS);

        // Execute shader
        glDispatchCompute(r_nbitems / r_nblocitems, 1, 1);

        /*!*/param = 0;
        /*!*/mKernels["reorder"].setArg(param++, mInKeysBuffer);
        /*!*/mKernels["reorder"].setArg(param++, mOutKeysBuffer);
        /*!*/mKernels["reorder"].setArg(param++, mHistogramBuffer);
        /*!*/mKernels["reorder"].setArg(param++, pass);
        /*!*/mKernels["reorder"].setArg(param++, mInPermutationBuffer);
        /*!*/mKernels["reorder"].setArg(param++, mOutPermutationBuffer);
        /*!*/mKernels["reorder"].setArg(param++, sizeof(cl_uint)* _RADIX * _ITEMS, NULL);
        /*!*/mKernels["reorder"].setArg(param++, _NKEYS);
        /*!*/mQueue.enqueueNDRangeKernel(mKernels["reorder"], 0, cl::NDRange(r_nbitems), cl::NDRange(r_nblocitems), NULL, PerfData.GetTrackerEvent("reorder", pass));

        /*!*/CompareIntBuffers(mQueue, mOutKeysBuffer, mOutKeysSBO);
        /*!*/CompareIntBuffers(mQueue, mOutPermutationBuffer, mOutPermutationSBO);

        GLuint tmpGL;

        tmpGL = mInKeysSBO;
        mInKeysSBO = mOutKeysSBO;
        mOutKeysSBO = tmpGL;
        tmpGL = mInKeysTBO;
        mInKeysTBO = mOutKeysTBO;
        mOutKeysTBO = tmpGL;

        tmpGL = mInPermutationSBO;
        mInPermutationSBO = mOutPermutationSBO;
        mOutPermutationSBO = tmpGL;
        tmpGL = mInPermutationTBO;
        mInPermutationTBO = mOutPermutationTBO;
        mOutPermutationTBO = tmpGL;

        /*!*/cl::Buffer tmp = mInKeysBuffer;
        /*!*/mInKeysBuffer = mOutKeysBuffer;
        /*!*/mOutKeysBuffer = tmp;

        /*!*/tmp = mInPermutationBuffer;
        /*!*/mInPermutationBuffer = mOutPermutationBuffer;
        /*!*/mOutPermutationBuffer = tmp;
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

    // Setup shader
    glUseProgram(g_SelectedProgram = mPrograms["sort_particles"]);
    glBindImageTexture(0, mInPermutationTBO, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);
    glBindImageTexture(1, mPositionsPingTBO, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(2, mPositionsPongTBO, 0, GL_FALSE, 0, GL_WRITE_ONLY,  GL_RGBA32F);
    glBindImageTexture(3, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(4, mPredictedPongTBO, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glUniform1i(0/*N*/,        Params.particleCount);

    // Execute shader
    glDispatchCompute(Params.particleCount, 1, 1);

    /*!*/CompareFloatBuffers(mQueue, mPositionsPingBuffer, mPositionsPingSBO);
    /*!*/CompareFloatBuffers(mQueue, mPredictedPingBuffer, mPredictedPingSBO);

    // Execute particle reposition
    /*!*/param = 0;
    /*!*/mKernels["sortParticles"].setArg(param++, mInPermutationBuffer);
    /*!*/mKernels["sortParticles"].setArg(param++, mPositionsPingBuffer);
    /*!*/mKernels["sortParticles"].setArg(param++, mPositionsPongBuffer);
    /*!*/mKernels["sortParticles"].setArg(param++, mPredictedPingBuffer);
    /*!*/mKernels["sortParticles"].setArg(param++, mPredictedPongBuffer);
    /*!*/mKernels["sortParticles"].setArg(param++, Params.particleCount);
    /*!*/mQueue.enqueueNDRangeKernel(mKernels["sortParticles"], 0, mGlobalRange, mLocalRange, NULL, PerfData.GetTrackerEvent("sortParticles"));

    /*!*/CompareFloatBuffers(mQueue, mPositionsPongBuffer, mPositionsPongSBO);
    /*!*/CompareFloatBuffers(mQueue, mPredictedPongBuffer, mPredictedPongSBO);

    // UnLock Yang buffer
    //mQueue.enqueueReleaseGLObjects(&sharedBuffers);

    // Double buffering of positions and velocity buffers
    GLuint tmpGL;
    
    tmpGL = mPositionsPingSBO;
    mPositionsPingSBO = mPositionsPongSBO;
    mPositionsPongSBO = tmpGL;
    tmpGL = mPositionsPingTBO;
    mPositionsPingTBO = mPositionsPongTBO;
    mPositionsPongTBO = tmpGL;

    tmpGL = mPredictedPingSBO;
    mPredictedPingSBO = mPredictedPongSBO;
    mPredictedPongSBO = tmpGL;
    tmpGL = mPredictedPingTBO;
    mPredictedPingTBO = mPredictedPongTBO;
    mPredictedPongTBO = tmpGL;

    /*!*/cl::BufferGL tmp1 = mPositionsPingBuffer;
    /*!*/mPositionsPingBuffer = mPositionsPongBuffer;
    /*!*/mPositionsPongBuffer = tmp1;

    /*!*/cl::Buffer tmp2 = mPredictedPingBuffer;
    /*!*/mPredictedPingBuffer = mPredictedPongBuffer;
    /*!*/mPredictedPongBuffer = tmp2;

    /*!*/GLuint tmp4 = mSharedPingBufferID;
    /*!*/mSharedPingBufferID = mSharedPongBufferID;
    /*!*/mSharedPongBufferID = tmp4;
}

void Simulation::Step()
{
    cycleCounter++;

    // Why is this here?
    glFinish();

    // Enqueue GL buffer acquire
    /*!*/vector<cl::Memory> sharedBuffers;
    /*!*/sharedBuffers.push_back(mPositionsPingBuffer);
    /*!*/sharedBuffers.push_back(mPositionsPongBuffer);
    /*!*/sharedBuffers.push_back(mParticlePosImg);
    /*!*/mQueue.enqueueAcquireGLObjects(&sharedBuffers);

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

        // Compute position delta
        this->computeDelta(i);
        // mQueue.finish();

        // printf("iteration %d\n", i);

        // cl_uint *stats = new cl_uint[2];
        // stats[0] = 0;
        // stats[1] = 0;
        // mQueue.enqueueReadBuffer(mStatsBuffer, CL_TRUE, 0, sizeof(cl_uint) * 2, stats);

        // std::cout << "hits: " << stats[0] << std::endl;
        // std::cout << "miss: " << stats[1] << std::endl;
        // std::cout << "total: " << stats[0] + stats[1] << std::endl;

        // stats[0] = 0;
        // stats[1] = 0;
        // mQueue.enqueueWriteBuffer(mStatsBuffer, CL_TRUE, 0, sizeof(cl_uint) * 2, stats);
        // mQueue.finish();

        // Update predicted position
        this->updatePredicted(i);
    }

    // Place density in "mPredictedPingBuffer[x].w"
    this->packData();

    // Recompute velocities
    this->updateVelocities();

    // Update vorticity and Viscosity
    this->applyViscosity();
    this->applyVorticity();

    // Update particle buffers

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
        f.write((const char *)mPositions, mBufferSizeParticles);
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

void Simulation::dumpData(vec4 * (&positions), vec4 * (&velocities))
{
    mQueue.enqueueReadBuffer(mPositionsPingBuffer, CL_TRUE, 0, mBufferSizeParticles, mPositions);
    mQueue.enqueueReadBuffer(mVelocitiesBuffer, CL_TRUE, 0, mBufferSizeParticles, mVelocities);

    // just a safety measure to be absolutely sure everything is transferred
    // from device to host
    mQueue.finish();

    positions = mPositions;
    velocities = mVelocities;
}
