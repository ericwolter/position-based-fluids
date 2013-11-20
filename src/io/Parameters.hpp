#pragma once

#include "../hesp.hpp"
#include <string>

using std::string;

struct Parameters
{
	cl_uint  particleCount;
	cl_uint  resetSimOnChange;
    cl_uint  subSteps;
    cl_uint  simIterations;
    cl_float timeStepLength;
    cl_float timeEnd;
    cl_float xMin;
    cl_float xMax;
    cl_float yMin;
    cl_float yMax;
    cl_float zMin;
    cl_float zMax;
    cl_uint  gridRes;
    cl_float restDensity;
	cl_float h;
	cl_float h_2;
	cl_float epsilon;
	cl_float setupSpacing;
};

// A function to load parameters from file
void LoadParameters(string InputFile);

// A global parameter object
extern Parameters Params;

// A variable that indicates a change in the paramaters
extern bool ParametersChanged;

