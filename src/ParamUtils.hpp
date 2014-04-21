#pragma once


#include <string>
using std::string;

#include "parameters.hpp"

// A function to load parameters from file
void LoadParameters(string InputFile);

// A global parameter object
extern Parameters Params;

// A variable that indicates a change in the paramaters
extern bool ParametersChanged;
