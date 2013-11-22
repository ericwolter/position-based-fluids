#pragma once

#include "hesp.hpp"
#include "parameters.hpp"

#include <string>
using std::string;

// A function to load parameters from file
void LoadParameters(string InputFile);

// A global parameter object
extern Parameters Params;

// A variable that indicates a change in the paramaters
extern bool ParametersChanged;
