#define _USE_MATH_DEFINES
#include <math.h>
#include <cstdlib>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <stdexcept>
using namespace std;

#include "Precomp_OpenGL.h"
#include "visual/visual.hpp"
#include "Simulation.hpp"
#include "Runner.hpp"
#include "Resources.hpp"

static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;

int main()
{
    try
    {
        // Create rendering window
        CVisual renderer(WINDOW_WIDTH, WINDOW_HEIGHT);
        renderer.initWindow("PBF Project");

        // Create simulation object
        Simulation simulation;

        // Create runner object
        Runner runner;
        runner.run(simulation, renderer);
    }
    catch (const exception &e)
    {
        cerr << "STD Error caught: " << e.what() << endl;
        getchar();
        exit(-1);
    }

    return 0;
}
