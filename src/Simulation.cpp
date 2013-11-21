#include "Simulation.hpp"
#include "DataLoader.hpp"
#include "io/Parameters.hpp"
#include "ocl/OCLUtils.hpp"

#define _USE_MATH_DEFINES
#include <math.h>
#include <cstdio>
#include <sstream>
#include <list>

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


Simulation::Simulation(const cl::Context &clContext, const cl::Device &clDevice)
    : mCLContext(clContext),
      mCLDevice(clDevice),
      mPositions(NULL),
      mVelocities(NULL),
	  mPredictions(NULL),
	  mDeltas(NULL),
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
	int ParticlesPerAxis = (int)ceil(pow(Params.particleCount, 1/3.0));

	// Build particles blcok
	float d = Params.h * Params.setupSpacing;
    float offsetX = (1.0f - ParticlesPerAxis * d) / 2.0f;
    float offsetY = 0.3;
    float offsetZ = (1.0f - ParticlesPerAxis * d) / 2.0f;
    for (cl_uint i = 0; i< Params.particleCount; i++)
    {
        cl_uint x = ((cl_uint)(i / pow(ParticlesPerAxis, 1)) % ParticlesPerAxis);
        cl_uint y = ((cl_uint)(i / pow(ParticlesPerAxis, 0)) % ParticlesPerAxis);
        cl_uint z = ((cl_uint)(i / pow(ParticlesPerAxis, 2)) % ParticlesPerAxis);

        mPositions[i].s[0] = offsetX + (x /*+ (y % 2) * .5*/) * d;
        mPositions[i].s[1] = offsetY + (y) * d;
        mPositions[i].s[2] = offsetZ + (z /*+ (y % 2) * .5*/) * d;
        mPositions[i].s[3] = 0;
    }
}

const std::string* Simulation::KernelFileList()
{
	static const std::string kernels[] = 
	{	
		"predict_positions.cl",
		"init_cells_old.cl",
		"update_cells.cl",
		"compute_scaling.cl",
		"compute_delta.cl",
		"update_predicted.cl",
		"update_velocities.cl",
        "apply_viscosity.cl",
        "apply_vorticity.cl",
		"update_positions.cl",
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
	const std::string* pKernels = KernelFileList();
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
    clflags << "-DSYSTEM_MIN_X="      << Params.xMin << "f ";
    clflags << "-DSYSTEM_MAX_X="      << Params.xMax << "f ";
    clflags << "-DSYSTEM_MIN_Y="      << Params.yMin << "f ";
    clflags << "-DSYSTEM_MAX_Y="      << Params.yMax << "f ";
    clflags << "-DSYSTEM_MIN_Z="      << Params.zMin << "f ";
    clflags << "-DSYSTEM_MAX_Z="      << Params.zMax << "f ";
	clflags << "-DGRID_RES="          << (int)Params.gridRes << " ";
	clflags << "-DGRID_SIZE="         << (int)(Params.gridRes * Params.gridRes * Params.gridRes) << " ";
    clflags << "-DTIMESTEP="          << Params.timeStepLength << "f ";
    clflags << "-DREST_DENSITY="      << Params.restDensity << "f ";
	clflags << "-DPBF_H="             << Params.h << "f ";
    clflags << "-DPBF_H_2="           << Params.h_2 << "f ";
	clflags << "-DEPSILON="           << Params.epsilon << "f ";
    clflags << "-DPOLY6_FACTOR="      << 315.0f / (64.0f * M_PI * pow(Params.h, 9)) << "f ";
    clflags << "-DPOLY6_FACTOR="      << 315.0f / (64.0f * M_PI * pow(Params.h, 9)) << "f ";
    clflags << "-DGRAD_SPIKY_FACTOR=" << 45.0f / (M_PI * pow(Params.h, 6)) << "f ";

	// Compile kernels
    cl::Program program = clSetup.createProgram(kernelSources, mCLContext, mCLDevice, clflags.str());
	if (program() == 0)
		return false;

	// Build kernels table
	mKernels = clSetup.createKernelsMap(program);
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
    mOmegaBuffer           = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeParticles);
    mScalingFactorsBuffer  = cl::Buffer(mCLContext, CL_MEM_READ_WRITE, mBufferSizeScalingFactors);


	if (mQueue() != 0)
		mQueue.flush();

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
}

void Simulation::InitCells()
{
	// Allocate host buffers
	const cl_uint cellCount = Params.gridRes * Params.gridRes * Params.gridRes;
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

void Simulation::applyViscosity()
{
    mKernels["applyViscosity"].setArg(0, mPredictedBuffer);
    mKernels["applyViscosity"].setArg(1, mVelocitiesBuffer);
    mKernels["applyViscosity"].setArg(2, mDeltaVelocityBuffer);
    mKernels["applyViscosity"].setArg(3, mOmegaBuffer);
    mKernels["applyViscosity"].setArg(4, mCellsBuffer);
    mKernels["applyViscosity"].setArg(5, mParticlesListBuffer);
    mKernels["applyViscosity"].setArg(6, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["applyViscosity"], 0, mGlobalRange, mLocalRange);
}

void Simulation::applyVorticity()
{
    mKernels["applyVorticity"].setArg(0, mPredictedBuffer);
    mKernels["applyVorticity"].setArg(1, mDeltaVelocityBuffer);
    mKernels["applyVorticity"].setArg(2, mOmegaBuffer);
    mKernels["applyVorticity"].setArg(3, mCellsBuffer);
    mKernels["applyVorticity"].setArg(4, mParticlesListBuffer);
    mKernels["applyVorticity"].setArg(5, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["applyVorticity"], 0, mGlobalRange, mLocalRange);
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

void Simulation::computeDelta(cl_float waveGenerator)
{
    mKernels["computeDelta"].setArg(0, mDeltaBuffer);
    mKernels["computeDelta"].setArg(1, mPredictedBuffer);
    mKernels["computeDelta"].setArg(2, mScalingFactorsBuffer);
    mKernels["computeDelta"].setArg(3, mCellsBuffer);
    mKernels["computeDelta"].setArg(4, mParticlesListBuffer);
    mKernels["computeDelta"].setArg(5, waveGenerator);
    mKernels["computeDelta"].setArg(6, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["computeDelta"], 0,
                                mGlobalRange, mLocalRange);
}

void Simulation::computeScaling()
{
    mKernels["computeScaling"].setArg(0, mPredictedBuffer);
    mKernels["computeScaling"].setArg(1, mScalingFactorsBuffer);
    mKernels["computeScaling"].setArg(2, mCellsBuffer);
    mKernels["computeScaling"].setArg(3, mParticlesListBuffer);
	mKernels["computeScaling"].setArg(4, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["computeScaling"], 0,
                                mGlobalRange, mLocalRange);
}

void Simulation::updateCells()
{
    mKernels["initCellsOld"].setArg(0, mCellsBuffer);
    mKernels["initCellsOld"].setArg(1, mParticlesListBuffer);
	mKernels["initCellsOld"].setArg(2, (cl_uint)(Params.gridRes * Params.gridRes * Params.gridRes));
    mKernels["initCellsOld"].setArg(3, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["initCellsOld"], 0, cl::NDRange(max(mBufferSizeParticlesList, mBufferSizeCells)), mLocalRange);

    mKernels["updateCells"].setArg(0, mPredictedBuffer);
    mKernels["updateCells"].setArg(1, mCellsBuffer);
    mKernels["updateCells"].setArg(2, mParticlesListBuffer);
    mKernels["updateCells"].setArg(3, Params.particleCount);

    mQueue.enqueueNDRangeKernel(mKernels["updateCells"], 0, mGlobalRange, mLocalRange);
}

void Simulation::Step(bool bPauseSim, cl_float waveGenerator)
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
	this->updateCells();
	
	for (unsigned int i = 0; i < Params.simIterations; ++i)
    {
		// Compute scaling value
        this->computeScaling();
        /*mQueue.enqueueReadBuffer(mPredictedBuffer, CL_TRUE, 0, mBufferSizeParticles, mPredictions);
        if (mQueue.finish() != CL_SUCCESS)
        	_asm nop;*/

		// Compute position delta
        this->computeDelta(waveGenerator);
        /*mQueue.enqueueReadBuffer(mDeltaBuffer, CL_TRUE, 0, mBufferSizeParticles, mDeltas);
        if (mQueue.finish() != CL_SUCCESS)
        	_asm nop;*/

		// Update predicted position
        this->updatePredicted();
    }

	// Recompute velocities
    this->updateVelocities();

	// Update vorticity and Viscosity
    this->applyViscosity();
    this->applyVorticity();

	// Update particle postions
	if (!bPauseSim)
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
