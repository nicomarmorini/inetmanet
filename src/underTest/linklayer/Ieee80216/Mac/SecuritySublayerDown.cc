
#include "SecuritySublayerDown.h"
#include "PhyControlInfo_m.h"

Define_Module(SecuritySublayerDown);

SecuritySublayerDown::SecuritySublayerDown()
{
    endTransmissionEvent = NULL;
}

SecuritySublayerDown::~SecuritySublayerDown()
{
    cancelAndDelete(endTransmissionEvent);
}

void SecuritySublayerDown::initialize()
{

}

void SecuritySublayerDown::handleMessage(cMessage *msg)
{	
    if (msg->arrivedOn("uppergateIn"))
    {
	send(msg,"lowergateOut");
    }
    else
    {
    ev << "nothing" << endl;
    }

}




