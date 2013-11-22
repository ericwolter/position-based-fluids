#include "Parameters.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cassert>

using std::string;
using std::istringstream;
using std::ifstream;
using std::cout;
using std::cerr;
using std::endl;
using std::runtime_error;

// A global parameter object
Parameters Params;

// A variable that indicates a change in the paramaters
bool ParametersChanged;

void LoadParameters(string InputFile)
{
    string line; // Complete line
    string parameter; // Parameter found

    // Open file
    ifstream ifs( InputFile.c_str() );
    if ( !ifs )
        throw runtime_error("Could not open parameter file!");

    // Scan all lines
    while ( getline(ifs, line) )
    {
		// Check for errors (don't know why should that happen... but anyways...)
        if ( !ifs.good() )
        {
            cerr << "Error parsing parameters file" << endl;
            break;
        }

		// Skip empty lines
        if (line.size() == 0)
			continue;

		// Extract parameter name
        istringstream ss(line);
        if ( !(ss >> parameter) )
        {
            cerr << "Unable to read parameter" << endl;
			continue;
		}

		// Store value into relevent parameter
        /**/ if (parameter == "timestep_length")     ss >> Params.timeStepLength;
        else if (parameter == "sub_steps")           ss >> Params.subSteps;
        else if (parameter == "sim_iterations")      ss >> Params.simIterations;
        else if (parameter == "time_end")            ss >> Params.timeEnd;
        else if (parameter == "particle_count")      ss >> Params.particleCount;
        else if (parameter == "reset_sim_on_change") ss >> Params.resetSimOnChange;
        else if (parameter == "x_min")               ss >> Params.xMin;
        else if (parameter == "x_max")               ss >> Params.xMax;
        else if (parameter == "y_min")               ss >> Params.yMin;
        else if (parameter == "y_max")               ss >> Params.yMax;
        else if (parameter == "z_min")               ss >> Params.zMin;
        else if (parameter == "z_max")               ss >> Params.zMax;
		else if (parameter == "grid_res")            ss >> Params.gridRes;
        else if (parameter == "restdensity")         ss >> Params.restDensity;
		else if (parameter == "epsilon")             ss >> Params.epsilon;
		else if (parameter == "setup_spacing")       ss >> Params.setupSpacing;
		else
            cerr << "Unknown parameter " << parameter << endl << "Leaving it out." << endl;
    }

    ifs.close();

	// Compute fields
	Params.h   = 1.0f / Params.gridRes;
	Params.h_2 = Params.h * Params.h;
}
