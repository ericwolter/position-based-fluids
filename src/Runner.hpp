#ifndef __RUNNER_HPP
#define __RUNNER_HPP

#include "hesp.hpp"
#include "Simulation.hpp"
#include "visual/visual.hpp"

class Runner
{
private:
    // A list of files to track for changes
    list<pair<string, time_t> > mKernelFilesTracker;
    list<pair<string, time_t> > mShaderFilesTracker;

public:
    void run(Simulation &simulation, CVisual &renderer); 

};

#endif // __RUNNER_HPP
