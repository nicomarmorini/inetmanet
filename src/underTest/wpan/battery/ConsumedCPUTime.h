//
// Reliable Authenticated Communication algorithm (RAC)
// to inform the notification board about the consumed energy
//
#ifndef CONSUMED_CPU_TIME_H
#define CONSUMED_CPU_TIME_H

// SYSTEM INCLUDES
#include <omnetpp.h>

class ConsumedCPUTime : public cPolymorphic
{
	public:
		ConsumedCPUTime(simtime_t cpuTime = 0) : cPolymorphic(), consumedCPUTime(cpuTime) {};
		
		// OPERATIONS
		double  GetConsumedCPUTime() const { return consumedCPUTime; }
		void    SetConsumedCPUTime(double cpuTime) { consumedCPUTime = cpuTime; }

		/** @brief Enables inspection */
		std::string info() const {
			return simtimeToStrShort(consumedCPUTime);
		}


	private:
		// MEMBER VARIABLES
		simtime_t consumedCPUTime;
};

#endif
