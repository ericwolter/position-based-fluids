#define _USE_MATH_DEFINES
#include <math.h>

#include <cstdlib>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <stdexcept>


#if defined(__APPLE__)
#include <OpenGL/OpenGL.h>
#elif defined(UNIX)
#include <GL/glx.h>
#else // _WINDOWS
#include <Windows.h>
#include <GL/gl.h>
#endif

#include "hesp.hpp"
#include "ocl/OCLUtils.hpp"
#include "visual/visual.hpp"
#include "Simulation.hpp"
#include "Runner.hpp"
#include "DataLoader.hpp"

static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;


using std::ofstream;
using std::vector;
using std::cout;
using std::cerr;
using std::cin;
using std::endl;
using std::string;
using std::exception;
using std::runtime_error;



int main()
{
    try
    {
        // Create rendering window
        CVisual renderer(WINDOW_WIDTH, WINDOW_HEIGHT);
        renderer.initWindow("PBF Project");

		// Select OpenCL platform/device
        OCLUtils clUtils;
        //cl::Platform platform = clUtils.selectPlatform();
        cl::Platform platform = clUtils.getPlatforms()[1];

#if defined(__APPLE__)
        CGLContextObj glContext = CGLGetCurrentContext();
        CGLShareGroupObj shareGroup = CGLGetShareGroup(glContext);

        cl_context_properties properties[] =
        {
            CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
            (cl_context_properties)shareGroup,
            0
        };
#elif defined(UNIX)
        cl_context_properties properties[] =
        {
            CL_GL_CONTEXT_KHR, (cl_context_properties) glXGetCurrentContext(),
            CL_GLX_DISPLAY_KHR, (cl_context_properties) glXGetCurrentDisplay(),
            CL_CONTEXT_PLATFORM, (cl_context_properties) (platform)(),
            0
        };
#elif defined(_WINDOWS)
        cl_context_properties properties[] =
        {
            CL_GL_CONTEXT_KHR, (cl_context_properties) wglGetCurrentContext(),
            CL_WGL_HDC_KHR, (cl_context_properties) wglGetCurrentDC(),
            CL_CONTEXT_PLATFORM, (cl_context_properties) (platform)(),
            0
        };
#else
        cl_context_properties properties[] =
        {
            CL_GL_CONTEXT_KHR, (cl_context_properties) glXGetCurrentContext(),
            CL_GLX_DISPLAY_KHR, (cl_context_properties) glXGetCurrentDisplay(),
            CL_CONTEXT_PLATFORM, (cl_context_properties) (platform)(),
            0
        };
#endif // __APPLE__

        // Get a vector of devices on this platform
        vector<cl::Device> devices;
        platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);
        cl::Device device = devices.at(0);
        cl::Context context = clUtils.createContext(properties);

        // Create simulation object
        Simulation simulation(context, device);

        // Create runner object
        Runner runner;
        runner.run(simulation, renderer);

    }
    catch (const cl::Error &ecl)
    {
        cerr << "OpenCL Error caught: " << ecl.what() << "(" << ecl.err() << ")" << endl;
        exit(-1);
    }
    catch (const exception &e)
    {
        cerr << "STD Error caught: " << e.what() << endl;
        exit(-1);
    }

    return 0;
}
