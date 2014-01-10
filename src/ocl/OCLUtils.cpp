#include "OCLUtils.hpp"

using namespace std;

ostream &operator<<(ostream &os, const cl::Platform &platform)
{
    os << "CL_PLATFORM_VERSION    = " << platform.getInfo<CL_PLATFORM_VERSION>()    << endl;
    os << "CL_PLATFORM_NAME       = " << platform.getInfo<CL_PLATFORM_NAME>()       << endl;
    os << "CL_PLATFORM_VENDOR     = " << platform.getInfo<CL_PLATFORM_VENDOR>()     << endl;
    os << "CL_PLATFORM_EXTENSIONS = " << platform.getInfo<CL_PLATFORM_EXTENSIONS>() << endl;

    return os;
}

ostream &operator<<(ostream &os, const cl::Device &device)
{
    os << "CL_DEVICE_EXTENSIONS                    = " << device.getInfo<CL_DEVICE_EXTENSIONS>()               << endl;
    os << "CL_DEVICE_GLOBAL_MEM_SIZE               = " << device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>()          << endl;
    os << "CL_DEVICE_LOCAL_MEM_SIZE                = " << device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>()           << endl;
    os << "CL_DEVICE_MAX_CLOCK_FREQUENCY           = " << device.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>()      << endl;
    os << "CL_DEVICE_MAX_COMPUTE_UNITS             = " << device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>()        << endl;
    os << "CL_DEVICE_MAX_CONSTANT_ARGS             = " << device.getInfo<CL_DEVICE_MAX_CONSTANT_ARGS>()        << endl;
    os << "CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE      = " << device.getInfo<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>() << endl;
    os << "CL_DEVICE_MAX_MEM_ALLOC_SIZE            = " << device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>()       << endl;
    os << "CL_DEVICE_MAX_PARAMETER_SIZE            = " << device.getInfo<CL_DEVICE_MAX_PARAMETER_SIZE>()       << endl;
    os << "CL_DEVICE_MAX_WORK_GROUP_SIZE           = " << device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>()      << endl;
    os << "CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS      = " << device.getInfo<CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS>() << endl;

    vector<size_t> vecSizes = device.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>();

    os << "CL_DEVICE_MAX_WORK_ITEM_SIZES           = ";
    os << "[";

    for (unsigned int i = 0; i < vecSizes.size(); i++)
    {
        os << vecSizes.at(i);
        if (i < vecSizes.size() - 1)
            os << ", ";
    }

    os << "]" << endl;

    os << "CL_DEVICE_NAME                          = " << device.getInfo<CL_DEVICE_NAME>()    << endl;
    os << "CL_DEVICE_VENDOR                        = " << device.getInfo<CL_DEVICE_VENDOR>()  << endl;
    os << "CL_DEVICE_VERSION                       = " << device.getInfo<CL_DEVICE_VERSION>() << endl;
    os << "CL_DRIVER_VERSION                       = " << device.getInfo<CL_DRIVER_VERSION>() << endl;
    os << "CL_DEVICE_PROFILE                       = " << device.getInfo<CL_DEVICE_PROFILE>() << endl;

    return os;
}

vector<cl::Device> OCLUtils::getDevices(const cl::Platform &platform, const cl_device_type deviceType) const
{
    unsigned int numberDevices = 0;
    vector<cl::Device> devices;

    // Get a vector of devices on this platform
    platform.getDevices(deviceType, &devices);

    if (devices.size() == 0)
    {
        throw runtime_error("No devices found!");
    }

    // Print info about found devices
    cout << "Found devices: " << endl;

    for (vector<cl::Device>::const_iterator cit = devices.begin();
            cit != devices.end(); ++cit)
    {
        cout << "Device number #" << numberDevices << " :" << endl;

        // print device info
        cout << *cit << endl;

        ++numberDevices;
    }

    return devices;
}

string OCLUtils::readSource(const string &filename) const
{
    // Open file
    ifstream ifs( filename.c_str() );
    string source;

    // Make sure that file was opened
    if ( !ifs.is_open() )
        throw runtime_error("Could not open File!");

    // read content
    source = string( istreambuf_iterator<char>(ifs), istreambuf_iterator<char>() );

    return source;
}

cl::Program OCLUtils::createProgram(const vector<string> &sources, const cl::Context &context, const vector<cl::Device> &devices, const string compileOptions) const
{
    cl::Program program;
    cl::Program::Sources programSource;

    // Append sources to program list
    for (vector<string>::const_iterator cit = sources.begin(); cit != sources.end(); ++cit)
        programSource.push_back( std::make_pair( cit->c_str(), cit->length() ) );

    // Make program of the source code in the context
    program = cl::Program(context, programSource);

    try
    {
        // Build program for these devices
        program.build( devices, compileOptions.c_str() );

    }
    catch (const cl::Error &e)
    {
        cerr << this->getBuildLog(program, devices) << endl;

        throw e;
    }

    string log = this->getBuildLog(program, devices);
    
    if(log.length() > 0) 
    {
        std::cout << log << std::endl;
    }

    return program;
}

cl::Program OCLUtils::createProgram(const vector<string> &sources, const cl::Context &context, const cl::Device &device, const string compileOptions) const
{
    vector<cl::Device> devices;

    devices.push_back(device);

    return this->createProgram(sources, context, devices, compileOptions);
}

map<string, cl::Kernel> OCLUtils::createKernelsMap(cl::Program &program) const
{
    map<string, cl::Kernel> ret;
    vector<cl::Kernel> kernels;

    program.createKernels(&kernels);

    for (vector<cl::Kernel>::const_iterator cit = kernels.begin();
            cit != kernels.end(); ++cit)
    {
        string kernelName = cit->getInfo<CL_KERNEL_FUNCTION_NAME>();

        ret[kernelName] = *cit;
    }

    return ret;
}

string OCLUtils::getBuildLog(const cl::Program &program, const cl::Device &device) const
{
    string ret;

    program.getBuildInfo(device, CL_PROGRAM_BUILD_LOG, &ret);

    return ret;
}

string OCLUtils::getBuildLog(const cl::Program &program, const vector<cl::Device> &devices) const
{
    string ret;
    string tmp;

    for (vector<cl::Device>::const_iterator cit = devices.begin(); cit != devices.end(); ++cit)
        ret += this->getBuildLog(program, *cit);

    return ret;
}

