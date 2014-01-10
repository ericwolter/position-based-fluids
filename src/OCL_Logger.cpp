#include "OCL_Logger.h"

#include <iostream>
#include <sstream>
#include <regex>

OCL_Logger::OCL_Logger() : 
    m_debugBuf(),
    m_msgMap(),
    m_localBuf(NULL),
    m_lastReportIndex(0),
    m_bufferSize(0)
{
}

void OCL_Logger::StartKernelProcessing(cl::Context context, cl::Device device, int bufferSize)
{
    // Save buffer size
    m_bufferSize = bufferSize;

    // Reset report index
    m_lastReportIndex = 0;

    // Allocate local buffer
    delete[] m_localBuf; m_localBuf = new int[bufferSize / 4];
    memset(m_localBuf, 0, bufferSize);

    // Allocate GPU buffer
    m_debugBuf = cl::Buffer(context, CL_MEM_READ_WRITE, bufferSize); 

    // Reset buffer
    cl::CommandQueue queue = cl::CommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE);
    queue.enqueueWriteBuffer(m_debugBuf, CL_TRUE, 0, bufferSize, m_localBuf);
    queue.finish();

    // Reset message map
    m_msgMap.clear();
}

string OCL_Logger::PatchKernel(string kernelSource)
{
    // Patch printf
    regex rgx("(TextToID[ \\t]*\\()([^\\)]*)");
    auto words_begin  = sregex_iterator(kernelSource.begin(), kernelSource.end(), rgx);
    auto words_end    = sregex_iterator();

    // Go  over all matches
    int prevPos = 0;
    ostringstream modifedSource;
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) 
    {
        smatch match = *i;

        // write [prevPos..match[2].begin] -> modifedSource
        modifedSource << kernelSource.substr(prevPos, match.position(2) - prevPos); 

        // write patched string
        int msgId = 100 + m_msgMap.size();
        modifedSource << msgId;

        // Store message in message map
        m_msgMap[msgId] = match.str(2).c_str();

        // move prevPos
        prevPos = match.position(2) + match.length(2);
    }

    // Add rest of the file
    modifedSource << kernelSource.substr(prevPos, kernelSource.length() - prevPos);

    // return modified string
    return modifedSource.str();
}

cl::Buffer& OCL_Logger::GetDebugBuffer()
{
    return m_debugBuf;
}

void OCL_Logger::CycleExecute(cl::CommandQueue queue)
{
    // Read debug buffer
    queue.enqueueReadBuffer(m_debugBuf, CL_TRUE, 0, m_bufferSize, m_localBuf);
    queue.finish();

    // Update report index
    int reportIndex = m_localBuf[0];
    int prevReportIndex = m_lastReportIndex;
    m_lastReportIndex = reportIndex;

    // Check if there was an buffer warping
    int queueSize = m_bufferSize / 4 - 1;
    if (reportIndex - prevReportIndex >= queueSize)
    {
        cout << "Logger Error: buffer is too small" << endl;
        return ;
    }

    // Process reports
    while (prevReportIndex != reportIndex)
    {
        // get report size & code
        int reportSize = m_localBuf[1 + (prevReportIndex++ % queueSize)];
        int reportCode = m_localBuf[1 + (prevReportIndex++ % queueSize)];

        // print header
        cout << m_msgMap[reportCode] << " ";

        // print values
        for (int i = 0; i < reportSize - 2; i++)
            cout << *((float*)(&m_localBuf[1 + (prevReportIndex++ % queueSize)])) << " ";

        // end line
        cout << endl;
    }
}
