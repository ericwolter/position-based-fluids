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
#include "io/Parameters.hpp"
#include "Particle.hpp"

#include <GLFW/glfw3.h>

// Macro used for the end of cell list
static const int END_OF_CELL_LIST = -1;

using std::map;
using std::vector;
using std::string;


/**
*  \brief CParser
*/
class Simulation
{
private:
    // Avoid copy
    Simulation &operator=(const Simulation &other);
    Simulation (const Simulation &other);

	void CreateParticles();

public:

    /**
    *  \brief  Default constructor.
    */
    explicit Simulation(const cl::Context &clContext, const cl::Device &clDevice);

    /**
    *  \brief  Destructor.
    */
    ~Simulation ();

	/**
    *  \brief  Create all buffer and particles
    */
	void InitBuffers();

	/**
    *  \brief Init Grid
    */
    void InitCells();

	/**
    *  \brief Load and build kernels
    */
    bool InitKernels();

	/**
    *  \brief Perform single simulation step
    */
    void Step(bool bPauseSim, cl_float waveGenerator);

	/**
    *  \brief Copy current positions and velocities
    */
    void dumpData( cl_float4 * (&positions), cl_float4 * (&velocities) );
    
	/**
    *  \brief Get a list of kernel files
    */
	const std::string* KernelFileList();

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

	// OCL buffer sizes
    size_t mBufferSizeParticles;
    size_t mBufferSizeCells;
    size_t mBufferSizeParticlesList;
    size_t mBufferSizeScalingFactors;

    // The host memory holding the simulation data
    cl_float4 *mPositions;
    cl_float4 *mVelocities;
    cl_float4 *mPredictions;
    cl_float4 *mDeltas;
#if !defined(USE_LINKEDCELL)
    cl_uint2 *mRadixCells;
#endif // USE_LINKEDCELL

    // The device memory buffers holding the simulation data
    cl::Buffer mCellsBuffer;
    cl::Buffer mParticlesListBuffer;
    cl::Buffer mPositionsBuffer;
    cl::Buffer mPredictedBuffer;
    cl::Buffer mVelocitiesBuffer;
    cl::Buffer mScalingFactorsBuffer;
    cl::Buffer mDeltaBuffer;
    cl::Buffer mDeltaVelocityBuffer;
    cl::Buffer mOmegaBuffer;

#if !defined(USE_LINKEDCELL)
    cl::Buffer mRadixCellsBuffer;
    cl::Buffer mRadixHistogramBuffer;
    cl::Buffer mRadixGlobSumBuffer;
    cl::Buffer mRadixCellsOutBuffer;
    cl::Buffer mFoundCellsBuffer;
#endif // USE_LINKEDCELL

    // Lengths of each cell in each direction
    cl_float4 mCellLength;

    // Array for the cells
    cl_int *mCells;
    cl_int *mParticlesList;

    GLuint mSharingBufferID;

    // Private member functions
    void updateCells();
    void updatePositions();
    void updateVelocities();
    void applyViscosity();
    void applyVorticity();
    void predictPositions();
    void updatePredicted();
    void computeScaling();
    void computeDelta(cl_float waveGenerator);
#if !defined(USE_LINKEDCELL)
    void radix(void);
#endif // USE_LINKEDCELL

};

#endif // __SIMULATION_HPP
