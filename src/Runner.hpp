#ifndef __RUNNER_HPP
#define __RUNNER_HPP

#include "hesp.hpp"
#include "Simulation.hpp"
#include "visual/visual.hpp"

#include <sys/stat.h>
#include <list>
#include <string>
using namespace std;

class Runner
{
private:
    // Avoid copy
    Runner &operator=(const Runner &other);
    Runner (const Runner &other);

    // A method that checks if things have changed
    bool DetectResourceChanges();

	// A list of files to track for changes
	list<pair<string, time_t>> mFilesTrack;

public:
	Runner() {};

    void run(Simulation &simulation, CVisual &renderer); 

};

#endif // __RUNNER_HPP
