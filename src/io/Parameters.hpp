#pragma once

#include "../hesp.hpp"
#include "Parameters.hpp"
#include <string>

using std::string;

class Parameters
{
public:
    Parameters();

	void LoadParameters(string InputFile);

	// A variable that indicates a change in the paramaters
	bool bParametersChanged;

	// [Parameters]
	cl_uint  particleCount;
	cl_uint  resetSimOnChange;
    cl_uint  subSteps;
    cl_uint  simIterations;
    cl_float timeStepLength;
    cl_float timeEnd;
    cl_uint  partOutFreq;
    string   partOutNameBase;
    cl_uint  vtkOutFreq;
    string   vtkOutNameBase;
    cl_uint  clWorkGroupSize1D;
    cl_float xMin;
    cl_float xMax;
    cl_float yMin;
    cl_float yMax;
    cl_float zMin;
    cl_float zMax;
    cl_float xN;
    cl_float yN;
    cl_float zN;
    cl_float restDensity;
	cl_float epsilon;
	cl_float grid_spacing;
};

// A global parameter object
extern Parameters Params;

