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
#include "hesp.hpp"
#include "ocl/OCLUtils.hpp"
#include "visual/visual.hpp"
#include "Simulation.hpp"
#include "Runner.hpp"
#include "Resources.hpp"

static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;

void SelectOpenCLDevice(cl::Platform &platform, cl::Device &device)
{
    // Scan platforms/devices for most sutable option
    cl_int      BestOption        = -1;
    cl_int      BestOption_Clocks = 0;
    vector<pair<cl::Platform, cl::Device> > deviceOptions;
    vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    for (vector<cl::Platform>::const_iterator cit = platforms.begin(); cit != platforms.end(); cit++)
    {
        // Print platform name
        cout << "  Platform [" << cit->getInfo<CL_PLATFORM_NAME>() << "] (" << cit->getInfo<CL_PLATFORM_VERSION>() << ")" << endl;

        // Get platform devices
        vector<cl::Device> devices;
        cit->getDevices(CL_DEVICE_TYPE_GPU, &devices);
        for (vector<cl::Device>::const_iterator dit = devices.begin(); dit != devices.end(); dit++)
        {
            // Add to options
            deviceOptions.push_back(make_pair(*cit, *dit));

            // Check if device support the required expenstions
            string extenstions = " " + dit->getInfo<CL_DEVICE_EXTENSIONS>() + " ";
#if defined(__APPLE__)
            bool support_gl_sharing = extenstions.find(" cl_APPLE_gl_sharing ") != string::npos;
#else
            bool support_gl_sharing = extenstions.find(" cl_khr_gl_sharing ") != string::npos;
#endif

            // Check clock
            cl_int clockFreq    = dit->getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>();
            cl_int computeUnits = dit->getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
            cl_int TotalClock   = clockFreq * computeUnits;

            // Check agaist "best" option
            if (support_gl_sharing && (BestOption_Clocks < TotalClock))
            {
                BestOption = deviceOptions.size() - 1;
                BestOption_Clocks = TotalClock;
            }

            // Print details
            cout << "    #" << deviceOptions.size() << " => " << dit->getInfo<CL_DEVICE_NAME>() << ":" << endl;
            cout << "          Support OpenGL sharing: " << support_gl_sharing << endl;
            cout << "          TotalClocks=" << TotalClock << " (Clock=" << clockFreq << " Units=" << computeUnits << ")" << endl;
            //cout << "          EXT: " << dit->getInfo<CL_DEVICE_EXTENSIONS>() << endl;
        }

        cout << endl;
    }

    // CUSTOM CHANGE "BestOption" HERE... (but don't commit it)
    // BestOption = ...;
    BestOption = 0;

    // Check if found atleast one device
    if (BestOption == -1)
        throw runtime_error("No devices were found.");

    // Assign selection
    platform = deviceOptions[BestOption].first;
    device   = deviceOptions[BestOption].second;
    cout << "Selected device is #" << BestOption << " => " << device.getInfo<CL_DEVICE_NAME>() << endl;
}

int main()
{
    try
    {
        // Create rendering window
        CVisual renderer(WINDOW_WIDTH, WINDOW_HEIGHT);
        renderer.initWindow("PBF Project");

        // Select OpenCL device
        cl::Platform ocl_platform;
        cl::Device   ocl_device;
        SelectOpenCLDevice(ocl_platform, ocl_device);


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
            CL_CONTEXT_PLATFORM, (cl_context_properties) (ocl_platform)(),
            0
        };
#elif defined(_WINDOWS)
        cl_context_properties properties[] =
        {
            CL_GL_CONTEXT_KHR, (cl_context_properties) wglGetCurrentContext(),
            CL_WGL_HDC_KHR, (cl_context_properties) wglGetCurrentDC(),
            CL_CONTEXT_PLATFORM, (cl_context_properties) (ocl_platform)(),
            0
        };
#else
        cl_context_properties properties[] =
        {
            CL_GL_CONTEXT_KHR, (cl_context_properties) glXGetCurrentContext(),
            CL_GLX_DISPLAY_KHR, (cl_context_properties) glXGetCurrentDisplay(),
            CL_CONTEXT_PLATFORM, (cl_context_properties) (ocl_platform)(),
            0
        };
#endif // __APPLE__

        // Get context for device
        std::vector<cl::Device> devices;
        devices.push_back(ocl_device);
        cl::Context context = cl::Context(devices, properties);

        // Create simulation object
        Simulation simulation(context, ocl_device);

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
