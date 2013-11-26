#include "OCLPerfMon.h"

cl::Event* OCLPerfMon::GetTrackerEvent(string trackerName, int iterationIndex)
{
	// Do we need to add iteration index to string?
	if (iterationIndex != -1)
	{
		char buff[10];
		sprintf(buff, "_%d", iterationIndex);
		trackerName += buff;
	}

	// Try to get the event
	map<string, int>::iterator item = m_TrackerMap.find(trackerName);
	
	// Create if not fond
	if (item == m_TrackerMap.end())
	{
		// Create struct
		PM_PERFORMANCE_TRACKER* pTracker = new PM_PERFORMANCE_TRACKER;
		memset(pTracker, 0, sizeof(PM_PERFORMANCE_TRACKER));
		pTracker->eventName = trackerName;
		
		// Add to map & vector
		m_TrackerMap[trackerName] = Trackers.size();
		Trackers.push_back(pTracker);

		// Re-find item
		item = m_TrackerMap.find(trackerName);
	}

	// Return point to event
	return &Trackers[item->second]->event;
}
	
void OCLPerfMon::UpdateTimings()
{
	for (size_t i = 0; i < Trackers.size(); i++)
	{
		// get start and stop times
		Trackers[i]->time_start = Trackers[i]->event.getProfilingInfo<CL_PROFILING_COMMAND_START>();
		Trackers[i]->time_end   = Trackers[i]->event.getProfilingInfo<CL_PROFILING_COMMAND_END>();

		// Compute total time
		Trackers[i]->total_time = (Trackers[i]->time_end - Trackers[i]->time_start) / 1000000.0;
	}
}
