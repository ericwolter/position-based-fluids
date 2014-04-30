#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <map>

// OpenCl Pragmas
#include "../hesp.hpp"

using namespace std;

#define DivCeil(num, divider) (((num) + (divider) - 1) / (divider)) 
#define IntCeil(num, divider) ((((num) + (divider) - 1) / (divider)) * (divider))
#define SWAP(t, a, b) { t tmp = a; a = b; b = tmp; } 

class OCLUtils
{
private:
    // Avoid Copy
    OCLUtils (const OCLUtils &other);
    OCLUtils &operator=(const OCLUtils &other);

public:
    explicit OCLUtils () {}

    vector<cl::Device> getDevices(const cl::Platform &platform, const cl_device_type deviceType = CL_DEVICE_TYPE_ALL) const;

    cl::Program createProgram(const vector<string> &sources, const cl::Context &context, const vector<cl::Device> &devices, const string compileOptions = "") const;

    cl::Program createProgram(const vector<string> &sources, const cl::Context &context, const cl::Device &device, const string compileOptions = "") const;

    map<string, cl::Kernel> createKernelsMap(cl::Program &program) const;

    string readSource(const string &filename) const;

    string getBuildLog(const cl::Program &program, const cl::Device &device) const;

    string getBuildLog(const cl::Program &program, const vector<cl::Device> &devices) const;
};

// Stream operator for platform.
ostream &operator<<(ostream &os, const cl::Platform &platform);

// Stream operator for device.
ostream &operator<<(ostream &os, const cl::Device &device);

class OCL_CopyBufferToHost
{
public:
    cl_uint size;
    byte* pBytes;
    float* pFloats;
    int* pIntegers;

    OCL_CopyBufferToHost(cl::CommandQueue queue, cl::Buffer buffer);
    ~OCL_CopyBufferToHost();
};