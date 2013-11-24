#include "ParamUtils.hpp"

#include <algorithm>
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

		// Remove comments
		line.erase( std::find( line.begin(), line.end(), '#' ), line.end() );

		// Skip empty lines
        if (line.size() == 0)
			continue;

		// Convert line to lower case
		transform(line.begin(), line.end(), line.begin(), ::tolower);

		// Extract parameter name
        istringstream ss(line);
        if ( !(ss >> parameter) )
        {
            cerr << "Unable to read parameter" << endl;
			continue;
		}

		// Store value into relevent parameter
        /**/ if (parameter == "resetsimonchange")    ss >> Params.resetSimOnChange;
        else if (parameter == "particlecount")       ss >> Params.particleCount;
        else if (parameter == "xmin")                ss >> Params.xMin;
        else if (parameter == "xmax")                ss >> Params.xMax;
        else if (parameter == "ymin")                ss >> Params.yMin;
        else if (parameter == "ymax")                ss >> Params.yMax;
        else if (parameter == "zmin")                ss >> Params.zMin;
        else if (parameter == "zmax")                ss >> Params.zMax;
		else if (parameter == "wavegenamp")          ss >> Params.waveGenAmp;
		else if (parameter == "wavegenfreq")         ss >> Params.waveGenFreq;
		else if (parameter == "wavegenduty")         ss >> Params.waveGenDuty;
		else if (parameter == "timestep")            ss >> Params.timeStep;
        else if (parameter == "simiterations")       ss >> Params.simIterations;
        else if (parameter == "substeps")            ss >> Params.subSteps;
		else if (parameter == "gridres")             ss >> Params.gridRes;
        else if (parameter == "restdensity")         ss >> Params.restDensity;
		else if (parameter == "epsilon")             ss >> Params.epsilon;
		else if (parameter == "garvity")             ss >> Params.garvity;
		else if (parameter == "vorticityfactor")     ss >> Params.vorticityFactor;
		else if (parameter == "viscosityfactor")     ss >> Params.viscosityFactor;
		else if (parameter == "surfacetenstionk")    ss >> Params.surfaceTenstionK;
		else if (parameter == "surfacetenstiondist") ss >> Params.surfaceTenstionDist;
		else if (parameter == "setupspacing")        ss >> Params.setupSpacing;
		else
            cerr << "Unknown parameter " << parameter << endl << "Leaving it out." << endl;
    }

    ifs.close();

	// Compute fields
	Params.h   = 1.0f / Params.gridRes;
	Params.h_2 = Params.h * Params.h;
}