#pragma once

// OpenCl Pragmas
#include "hesp.hpp"

#include <string>
#include <map>

using namespace std;

class OCL_Logger
{
private:
    int        m_bufferSize;
    int        m_lastReportIndex;
    cl::Buffer m_debugBuf;
    int*       m_localBuf;
    map<int/*msgID*/, string/*message*/> m_msgMap;

public:
    OCL_Logger();

    void StartKernelProcessing(cl::Context context, cl::Device device, int bufferSize);

    string PatchKernel(string kernelSource);

    cl::Buffer& GetDebugBuffer();

    void CycleExecute(cl::CommandQueue queue);
};
