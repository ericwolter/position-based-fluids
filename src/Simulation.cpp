#include "Simulation.hpp"
#include "DataLoader.hpp"
#include "io/Parameters.hpp"
#include "ocl/OCLUtils.hpp"

#define _USE_MATH_DEFINES
#include <math.h>
#include <cstdio>
#include <sstream>

#if defined(__APPLE__)
#include <OpenGL/OpenGL.h>
#elif defined(UNIX)
#include <GL/glx.h>
#else // _WINDOWS
#include <Windows.h>
#include <GL/gl.h>
#endif

using std::vector;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::runtime_error;
using std::max;
using std::ceil;


#if !defined(USE_LINKEDCELL)
// RADIX SORT CONSTANTS
static const unsigned int _ITEMS = 16;
static const unsigned int _GROUPS = 16;
static const unsigned int _BITS = 6;
static const unsigned int _RADIX = 1 << _BITS;
static unsigned int _NKEYS = 0;
static const unsigned int _HISTOSPLIT = 512;
#endif


Simulation::Simulation(const cl::Context &clContext, const cl::Device &clDevice)
    : mCLContext(clContext),
      mCLDevice(clDevice),
      mPositions(NULL),
      mVelocities(NULL),
	  mPredictions(NULL),
	  mDeltas(NULL),
      mCells(NULL),
      mParticlesList(NULL),
      mWaveGenerator(0.0f)
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
#if !defined(USE_LINKEDCELL)
    delete[] mRadixCells;
#endif // USE_LINKEDCELL
}


void Simulation::CreateParticles()
{
	// Calc SmoothLen
	mSmoothLen = (Params.xMax - Params.xMin) / Params.xN;

    // Compute particle count per axis
	int ParticlesPerAxis = (int)ceil(pow(Params.particleCount, 1/3.0));

	// Build particles blcok
    float d = mSmoothLen * 0.990;
    float offsetX = 0.1;
    float offsetY = 0.1;
    float offsetZ = 0.1;
    for (cl_uint i = 0; i< Params.particleCount; i++)
    {
        cl_uint x = ((cl_uint)(i / pow(ParticlesPerAxis, 1)) % ParticlesPerAxis);
        cl_uint y = ((cl_uint)(i / pow(ParticlesPerAxis, 0)) % ParticlesPerAxis);
        cl_uint z = ((cl_uint)(i / pow(ParticlesPerAxis, 2)) % ParticlesPerAxis);

        mPositions[i].s[0] = offsetX + (x + (y % 2) * .5) * d;
        mPositions[i].s[1] = offsetY + (y) * d;
        mPositions[i].s[2] = offsetZ + (z + (y % 2) * .5) * d;
        mPositions[i].s[3] = 0;
    }
}

void Simulation::InitKernels()
{
	// Setup OpenCL Ranges
    const cl_uint globalSize = ceil(Params.particleCount / 32.0f) * 32;
    mGlobalRange = cl::NDRange(globalSize);
    mLocalRange = cl::NullRange;

    // setup kernel sources
    OCLUtils clSetup;
    vector<string> kernelSources;
    string header = clSetup.readSource(getPathForKernel("hesp.hpp"));
    string source;

    source = clSetup.readSource(getPathForKernel("predict_positions.cl"));
    kernelSources.push_back(header + source);
    source = clSetup.readSource(getPathForKernel("init_cells_old.cl"));
    kernelSources.push_back(header + source);
    source = clSetup.readSource(getPathForKernel("update_cells.cl"));
    kernelSources.push_back(header + source);
    source = clSetup.readSource(getPathForKernel("compute_scaling.cl"));
    kernelSources.push_back(header + source);
    source = clSetup.readSource(getPathForKernel("compute_delta.cl"));
    kernelSources.push_back(header + source);
    source = clSetup.readSource(getPathForKernel("update_predicted.cl"));
    kernelSources.push_back(header + source);
    source = clSetup.readSource(getPathForKernel("update_velocities.cl"));
    kernelSources.push_back(header + source);
    source = clSetup.readSource(getPathForKernel("apply_vorticity_and_viscosity.cl"));
    kernelSources.push_back(header + source);
    source = clSetup.readSource(getPathForKernel("update_positions.cl"));
    kernelSources.push_back(header + source);
    source = clSetup.readSource(getPathForKernel("calc_hash.cl"));
    kernelSources.push_back(header + source);
#if !defined(USE_LINKEDCELL)
    source = clSetup.readSource(getPathForKernel("radix_histogram.cl"));
    kernelSources.push_back(header + source);
    source = clSetup.readSource(getPathForKernel("radix_scan.cl"));
    kernelSources.push_back(header + source);
    source = clSetup.readSource(getPathForKernel("radix_paste.cl"));
    kernelSources.push_back(header + source);
    source = clSetup.readSource(getPathForKernel("radix_reorder.cl"));
    kernelSources.push_back(header + source);
    source = clSetup.readSource(getPathForKernel("init_cells.cl"));
    kernelSources.push_back(header + source);
    source = clSetup.readSource(getPathForKernel("find_cells.cl"));
    kernelSources.push_back(header + source);
#endif
    std::ostringstream clflags;
    clflags << "-cl-mad-enable -cl-no-signed-zeros -cl-fast-relaxed-math ";

#ifdef USE_DEBUG
    clflags << "-DUSE_DEBUG ";
#endif // USE_DEBUG

#ifdef USE_LINKEDCELL
    clflags << "-DUSE_LINKEDCELL ";
#endif // USE_LINKEDCELL

    clflags << std::showpoint;
    clflags << "-DSYSTEM_MIN_X=" << Params.xMin << "f ";
    clflags << "-DSYSTEM_MAX_X=" << Params.xMax << "f ";
    clflags << "-DSYSTEM_MIN_Y=" << Params.yMin << "f ";
    clflags << "-DSYSTEM_MAX_Y=" << Params.yMax << "f ";
    clflags << "-DSYSTEM_MIN_Z=" << Params.zMin << "f ";
    clflags << "-DSYSTEM_MAX_Z=" << Params.zMax << "f ";
    clflags << "-DNUMBER_OF_CELLS_X=" << Params.xN << "f ";
    clflags << "-DNUMBER_OF_CELLS_Y=" << Params.yN << "f ";
    clflags << "-DNUMBER_OF_CELLS_Z=" << Params.zN << "f ";
    clflags << "-DCELL_LENGTH_X=" << (Params.xMax - Params.xMin) / Params.xN << "f ";
    clflags << "-DCELL_LENGTH_Y=" << (Params.yMax - Params.yMin) / Params.yN << "f ";
    clflags << "-DCELL_LENGTH_Z=" << (Params.zMax - Params.zMin) / Params.zN << "f ";
    clflags << "-DTIMESTEP=" << Params.timeStepLength << "f ";
    clflags << "-DREST_DENSITY=" << Params.restDensity << "f ";
    clflags << "-DPBF_H=" << mSmoothLen << "f ";
    clflags << "-DPBF_H_2=" << pow(mSmoothLen, 2) << "f ";
    clflags << "-DPOLY6_FACTOR=" << 315.0f / (64.0f * M_PI * pow(mSmoothLen, 9)) << "f ";
    clflags << "-DGRAD_SPIKY_FACTOR=" << 45.0f / (M_PI * pow(mSmoothLen, 6)) << "f ";

    cl::Program program = clSetup.createProgram(kernelSources, mCLContext, mCLDevice, clflags.str());

	// Build things...
    mKernels = clSetup.createKernelsMap(program);
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
#if !defined(USE_LINKEDCELL)
    delete[] mRadixCells;  mRadixCells = new cl_uint2[_NKEYS];
#endif // USE_LINKEDCELL

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
    mPositionsBuffer       = cl::BufferGL(mCLContext, CL_MEM_READ_WRITE, mSharingBufferID); // buffer could be changed to be CL_MEM_WRITE_ONLY but for debugging also reading it might be helpful
	mPredictedBuffer       = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mVelocitiesBuffer      = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mDeltaBuffer           = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mDeltaVelocityBuffer   = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mScalingFactorsBuffer  = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeScalingFactors);

    // Copy mPositions (Host) => mPositionsBuffer (GPU) (we have to lock the shared buffer)
	vector<cl::Memory> sharedBuffers;
    sharedBuffers.push_back(mPositionsBuffer);
    mQueue = cl::CommandQueue(mCLContext, mCLDevice);
    mQueue.enqueueAcquireGLObjects(&sharedBuffers);
    mQueue.enqueueWriteBuffer(mPositionsBuffer, CL_TRUE, 0, mBufferSizeParticles, mPositions);
    mQueue.enqueueReleaseGLObjects(&sharedBuffers);
    mQueue.finish();

	// Copy mVelocities (Host) => mVelocitiesBuffer (GPU)
	mQueue.enqueueWriteBuffer(mVelocitiesBuffer, CL_TRUE, 0, mBufferSizeParticles, mVelocities);
    mQueue.finish();

#if !defined(USE_LINKEDCELL)
    // get closest multiple to of items/groups
    if (mNumParticles % (_ITEMS * _GROUPS) == 0)
    {
        _NKEYS = mNumParticles;
    }
    else
    {
        _NKEYS = mNumParticles + (_ITEMS * _GROUPS)
                 - mNumParticles % (_ITEMS * _GROUPS);
    }

    mRadixCellsBuffer = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint2) * _NKEYS);
    mRadixCellsOutBuffer = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint2) * _NKEYS);
    mFoundCellsBuffer = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_int2) * mNumberCells.s[0] * mNumberCells.s[1] * mNumberCells.s[2]);

    mRadixHistogramBuffer = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _RADIX * _ITEMS * _GROUPS);
    mRadixGlobSumBuffer = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, sizeof(cl_uint) * _HISTOSPLIT);
#endif // USE_LINKEDCELL
}

void Simulation::InitCells()
{
	// Calc SmoothLen
	mSmoothLen = (Params.xMax - Params.xMin) / Params.xN;

	// Compute Cell length
    mCellLength.s[0] = (Params.xMax - Params.xMin) / Params.xN;
    mCellLength.s[1] = (Params.yMax - Params.yMin) / Params.yN;
    mCellLength.s[2] = (Params.zMax - Params.zMin) / Params.zN;
    mCellLength.s[3] = 0.0f;

	// Allocate host buffers
	const cl_uint cellCount = Params.xN * Params.yN * Params.zN;
    delete[] mCells;         mCells         = new cl_int[cellCount];
    delete[] mParticlesList; mParticlesList = new cl_int[Params.particleCount];

    // Init cells
    for (cl_uint i = 0; i < cellCount; ++i)
        mCells[i] = END_OF_CELL_LIST;

    // Init particles
    for (cl_uint i = 0; i < Params.particleCount; ++i)
        mParticlesList[i] = END_OF_CELL_LIST;

    // Write buffer for cells
	mBufferSizeCells = cellCount * sizeof(cl_int);
    mCellsBuffer = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeCells);
    mQueue.enqueueWriteBuffer(mCellsBuffer, CL_TRUE, 0, mBufferSizeCells, mCells);

    // Write buffer for particles
    mParticlesListBuffer = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticlesList);
    mQueue.enqueueWriteBuffer(mParticlesListBuffer, CL_TRUE, 0, mBufferSizeParticlesList, mParticlesList);
}

void Simulation::updatePositions()
{
    mKernels["updatePositions"].setArg(0, mPositionsBuffer);
    mKernels["updatePositions"].setArg(1, mPredictedBuffer);
    mKernels["updatePositions"].setArg(2, mVelocitiesBuffer);
    mKernels["updatePositions"].setArg(3, mDeltaVelocityBuffer);
    mKernels["updatePositions"].setArg(4, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["updatePositions"], 0, mGlobalRange, mLocalRange);
}

void Simulation::updateVelocities()
{
    mKernels["updateVelocities"].setArg(0, mPositionsBuffer);
    mKernels["updateVelocities"].setArg(1, mPredictedBuffer);
    mKernels["updateVelocities"].setArg(2, mVelocitiesBuffer);
    mKernels["updateVelocities"].setArg(3, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["updateVelocities"], 0, mGlobalRange, mLocalRange);
}

void Simulation::applyVorticityAndViscosity()
{
    mKernels["applyVorticityAndViscosity"].setArg(0, mPredictedBuffer);
    mKernels["applyVorticityAndViscosity"].setArg(1, mVelocitiesBuffer);
    mKernels["applyVorticityAndViscosity"].setArg(2, mDeltaVelocityBuffer);
#if defined(USE_LINKEDCELL)
    mKernels["applyVorticityAndViscosity"].setArg(3, mCellsBuffer);
    mKernels["applyVorticityAndViscosity"].setArg(4, mParticlesListBuffer);
#else
    mKernels["applyVorticityAndViscosity"].setArg(3, mRadixCellsBuffer);
    mKernels["applyVorticityAndViscosity"].setArg(4, mFoundCellsBuffer);
#endif // USE_LINKEDCELL
    mKernels["applyVorticityAndViscosity"].setArg(5, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["applyVorticityAndViscosity"], 0, mGlobalRange, mLocalRange);
}

void Simulation::predictPositions()
{
    mKernels["predictPositions"].setArg(0, mPositionsBuffer);
    mKernels["predictPositions"].setArg(1, mPredictedBuffer);
    mKernels["predictPositions"].setArg(2, mVelocitiesBuffer);
    mKernels["predictPositions"].setArg(3, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["predictPositions"], 0, mGlobalRange, mLocalRange);
}

void Simulation::updatePredicted()
{
    mKernels["updatePredicted"].setArg(0, mPredictedBuffer);
    mKernels["updatePredicted"].setArg(1, mDeltaBuffer);
    mKernels["updatePredicted"].setArg(2, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["updatePredicted"], 0, mGlobalRange, mLocalRange);
}

void Simulation::computeDelta()
{
    mKernels["computeDelta"].setArg(0, mDeltaBuffer);
    mKernels["computeDelta"].setArg(1, mPredictedBuffer);
    mKernels["computeDelta"].setArg(2, mScalingFactorsBuffer);
#if defined(USE_LINKEDCELL)
    mKernels["computeDelta"].setArg(3, mCellsBuffer);
    mKernels["computeDelta"].setArg(4, mParticlesListBuffer);
#else
    mKernels["computeDelta"].setArg(3, mRadixCellsBuffer);
    mKernels["computeDelta"].setArg(4, mFoundCellsBuffer);
#endif
    mKernels["computeDelta"].setArg(5, mWaveGenerator);
    mKernels["computeDelta"].setArg(6, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["computeDelta"], 0,
                                mGlobalRange, mLocalRange);
}

void Simulation::computeScaling()
{
    mKernels["computeScaling"].setArg(0, mPredictedBuffer);
    mKernels["computeScaling"].setArg(1, mScalingFactorsBuffer);
#if defined(USE_LINKEDCELL)
    mKernels["computeScaling"].setArg(2, mCellsBuffer);
    mKernels["computeScaling"].setArg(3, mParticlesListBuffer);
#else
    mKernels["computeScaling"].setArg(2, mRadixCellsBuffer);
    mKernels["computeScaling"].setArg(3, mFoundCellsBuffer);
#endif
	mKernels["computeScaling"].setArg(4, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["computeScaling"], 0,
                                mGlobalRange, mLocalRange);
}

void Simulation::updateCells()
{
    mKernels["initCellsOld"].setArg(0, mCellsBuffer);
    mKernels["initCellsOld"].setArg(1, mParticlesListBuffer);
    mKernels["initCellsOld"].setArg(2, (cl_uint)(Params.xN * Params.yN * Params.zN));
    mKernels["initCellsOld"].setArg(3, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["initCellsOld"], 0, cl::NDRange(max(mBufferSizeParticlesList, mBufferSizeCells)), mLocalRange);

    mKernels["updateCells"].setArg(0, mPredictedBuffer);
    mKernels["updateCells"].setArg(1, mCellsBuffer);
    mKernels["updateCells"].setArg(2, mParticlesListBuffer);
    mKernels["updateCells"].setArg(3, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["updateCells"], 0, mGlobalRange, mLocalRange);
}

#if !defined(USE_LINKEDCELL)
void Simulation::radix(void)
{
    const static unsigned int _CELLSTOTAL = mNumberCells.s[0]
                                            * mNumberCells.s[1]
                                            * mNumberCells.s[2];
    // round up to the next power of 2
    const static unsigned int _TOTALBITS = ceilf(ceilf( log2f(_CELLSTOTAL) )
                                           / (float) _BITS) * _BITS;
    const static unsigned int _MAXINT = 1 << (_TOTALBITS - 1);
    const static unsigned int _PASS = _TOTALBITS / _BITS;
    static const int _MAXMEMCACHE = std::max(_HISTOSPLIT, _ITEMS * _GROUPS
                                    * _RADIX / _HISTOSPLIT);

    cl::Kernel calcHashKernel = mKernels["calcHash"];

    mKernels["calcHash"].setArg(0, mPredictedBuffer);
    mKernels["calcHash"].setArg(1, mRadixCellsBuffer);
    mKernels["calcHash"].setArg(2, _MAXINT);
    mKernels["calcHash"].setArg(3, mNumParticles);
    mKernels["calcHash"].setArg(4, _NKEYS);

    mQueue.enqueueNDRangeKernel(mKernels["calcHash"], cl::NullRange,
                                cl::NDRange(_NKEYS), cl::NullRange);

    cl::Kernel reorderKernel = mKernels["reorder"];

    for (unsigned int pass = 0; pass < _PASS; pass++ )
    {
        //histogram
        mKernels["histogram"].setArg(0, mRadixCellsBuffer);
        mKernels["histogram"].setArg(1, mRadixHistogramBuffer);
        mKernels["histogram"].setArg(2, pass);
        mKernels["histogram"].setArg(3, sizeof(cl_uint) * _RADIX * _ITEMS, NULL);
        mKernels["histogram"].setArg(4, _NKEYS);
        mKernels["histogram"].setArg(5, _RADIX);
        mKernels["histogram"].setArg(6, _BITS);

        mQueue.enqueueNDRangeKernel(mKernels["histogram"], cl::NullRange,
                                    cl::NDRange(_ITEMS * _GROUPS),
                                    cl::NDRange(_ITEMS));

        //scan
        mKernels["scan"].setArg(0, mRadixHistogramBuffer);
        mKernels["scan"].setArg(1, sizeof(cl_uint) * _MAXMEMCACHE, NULL);
        mKernels["scan"].setArg(2, mRadixGlobSumBuffer);

        mQueue.enqueueNDRangeKernel(mKernels["scan"], cl::NullRange,
                                    cl::NDRange(_RADIX * _GROUPS * _ITEMS / 2),
                                    cl::NDRange((_RADIX * _GROUPS * _ITEMS / 2)
                                                / _HISTOSPLIT));

        mKernels["scan"].setArg(0, mRadixGlobSumBuffer);
        mKernels["scan"].setArg(2, mRadixHistogramBuffer);

        mQueue.enqueueNDRangeKernel(mKernels["scan"], cl::NullRange,
                                    cl::NDRange(_HISTOSPLIT / 2),
                                    cl::NDRange(_HISTOSPLIT / 2));

        mKernels["paste"].setArg(0, mRadixHistogramBuffer);
        mKernels["paste"].setArg(1, mRadixGlobSumBuffer);

        mQueue.enqueueNDRangeKernel(mKernels["paste"], cl::NullRange,
                                    cl::NDRange(_RADIX * _GROUPS * _ITEMS / 2),
                                    cl::NDRange((_RADIX * _GROUPS * _ITEMS / 2)
                                                / _HISTOSPLIT));

        //reorder
        mKernels["reorder"].setArg(0, mRadixCellsBuffer);
        mKernels["reorder"].setArg(1, mRadixCellsOutBuffer);
        mKernels["reorder"].setArg(2, mRadixHistogramBuffer);
        mKernels["reorder"].setArg(3, pass);
        mKernels["reorder"].setArg(4, sizeof(cl_uint) * _RADIX * _ITEMS, NULL);
        mKernels["reorder"].setArg(5, _NKEYS);
        mKernels["reorder"].setArg(6, _RADIX);
        mKernels["reorder"].setArg(7, _BITS);

        mQueue.enqueueNDRangeKernel(mKernels["reorder"], cl::NullRange,
                                    cl::NDRange(_ITEMS * _GROUPS),
                                    cl::NDRange(_ITEMS));

        cl::Buffer tmp = mRadixCellsBuffer;

        mRadixCellsBuffer = mRadixCellsOutBuffer;
        mRadixCellsOutBuffer = tmp;
    }

    mKernels["initCells"].setArg(0, mFoundCellsBuffer);
    mKernels["initCells"].setArg(1, mNumberCells.s[0]
                                 * mNumberCells.s[1] * mNumberCells.s[2]);

    mQueue.enqueueNDRangeKernel(mKernels["initCells"], cl::NullRange,
                                cl::NDRange(mNumberCells.s[0]
                                            * mNumberCells.s[1] * mNumberCells.s[2]),
                                cl::NullRange);

    mKernels["findCells"].setArg(0, mRadixCellsBuffer);
    mKernels["findCells"].setArg(1, mFoundCellsBuffer);
    mKernels["findCells"].setArg(2, mNumParticles);

    mQueue.enqueueNDRangeKernel(mKernels["findCells"], cl::NullRange,
                                cl::NDRange(mNumParticles), cl::NullRange);
}
#endif

void Simulation::Step()
{
	// Why is this here?
    glFinish();

	// Enqueue GL buffer acquire
	vector<cl::Memory> sharedBuffers;
    sharedBuffers.push_back(mPositionsBuffer);
    mQueue.enqueueAcquireGLObjects(&sharedBuffers);
    
	// Predicit positions 
    this->predictPositions();

	// Update cells
	#if defined(USE_LINKEDCELL)
		this->updateCells();
	#else
	    this->radix();
	#endif

    const unsigned int solver_iterations = 4;
    for (unsigned int i = 0; i < solver_iterations; ++i)
    {
		// Compute scaling value
        this->computeScaling();
        /*mQueue.enqueueReadBuffer(mPredictedBuffer, CL_TRUE, 0, mBufferSizeParticles, mPredictions);
        if (mQueue.finish() != CL_SUCCESS)
        	_asm nop;*/

		// Compute position delta
        this->computeDelta();
        /*mQueue.enqueueReadBuffer(mDeltaBuffer, CL_TRUE, 0, mBufferSizeParticles, mDeltas);
        if (mQueue.finish() != CL_SUCCESS)
        	_asm nop;*/

		// Update predicted position
        this->updatePredicted();
    }

	// Recompute velocities
    this->updateVelocities();

	// Update vorticity and Viscosity
	this->applyVorticityAndViscosity();

	// Update particle postions
	this->updatePositions();

	// Release OpenGL shared object, allowing openGL do to it's thing...
    mQueue.enqueueReleaseGLObjects(&sharedBuffers);
    mQueue.finish(); 
}

void Simulation::dumpData( cl_float4 * (&positions), cl_float4 * (&velocities) )
{
    mQueue.enqueueReadBuffer(mPositionsBuffer, CL_TRUE, 0, mBufferSizeParticles, mPositions);
    mQueue.enqueueReadBuffer(mVelocitiesBuffer, CL_TRUE, 0, mBufferSizeParticles, mVelocities);

    // just a safety measure to be absolutely sure everything is transferred
    // from device to host
    mQueue.finish();

    positions = mPositions;
    velocities = mVelocities;
}
