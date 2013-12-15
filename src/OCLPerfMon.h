#pragma once

#include "hesp.hpp"

#include <map>
#include <map>
#include <string.h>

using namespace std;

// Single performance tracker struct
typedef struct
{
    // Event details
    string eventName;
    cl::Event event;

    // last measurement
    cl_ulong time_start; // [nano sec]
    cl_ulong time_end;   // [nano sec]
    double total_time;   // [millisec]
    double last_time;    // [millisec]

    // User define type
    int Tag;

} PM_PERFORMANCE_TRACKER;


// Main OpenCL Performance Monitor Class
class OCLPerfMon
{
private:
    // Map to translate trackerName to event index inside "Trackers" vector
    map<string, int> m_TrackerMap;

public:
    // A list of all existing measurement events
    vector<PM_PERFORMANCE_TRACKER *> Trackers;

    // use to get the event
    cl::Event *GetTrackerEvent(string trackerName, int iterationIndex = -1);

    // A method to compute execution time for each tracker (needs to be called after clFinish)
    void UpdateTimings();
};
