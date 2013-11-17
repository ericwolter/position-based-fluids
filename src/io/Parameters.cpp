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

Parameters::Parameters()
{
}

void Parameters::LoadParameters(string InputFile)
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
        /**/ if (parameter == "timestep_length")     ss >> timeStepLength;
        else if (parameter == "time_end")            ss >> timeEnd;
        else if (parameter == "particle_count")      ss >> particleCount;
        else if (parameter == "reset_sim_on_change") ss >> resetSimOnChange;
        else if (parameter == "part_out_freq")       ss >> partOutFreq;
        else if (parameter == "part_out_name_base")  ss >> partOutNameBase;
        else if (parameter == "vtk_out_freq")        ss >> vtkOutFreq;
        else if (parameter == "vtk_out_name_base")   ss >> vtkOutNameBase;
        else if (parameter == "cl_workgroup_1dsize") ss >> clWorkGroupSize1D;
        else if (parameter == "x_min")               ss >> xMin;
        else if (parameter == "x_max")               ss >> xMax;
        else if (parameter == "y_min")               ss >> yMin;
        else if (parameter == "y_max")               ss >> yMax;
        else if (parameter == "z_min")               ss >> zMin;
        else if (parameter == "z_max")               ss >> zMax;
        else if (parameter == "x_n")                 ss >> xN;
        else if (parameter == "y_n")                 ss >> yN;
        else if (parameter == "z_n")                 ss >> zN;
        else if (parameter == "restdensity")         ss >> restDensity;
		else if (parameter == "grid_spacing")        ss >> grid_spacing;
		else
            cerr << "Unknown parameter " << parameter << endl << "Leaving it out." << endl;
    }

    ifs.close();
}
