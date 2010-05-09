#include <stdio.h>
#include <string.h>
#include <omnetpp.h>

/**
 * Module for WiMAX convergency traffic classification 
 */
class CommonPartSublayerFragmentation : public cSimpleModule
{
  private:
    int commonPartGateOut, commonPartGateIn,
    	securityGateIn, securityGateOut,
    	schedulingGateIn;
    	
    
  public:
	  CommonPartSublayerFragmentation();
    virtual ~CommonPartSublayerFragmentation();
    
  protected:
    void initialize();
    void handleMessage(cMessage *msg);
    void handleUpperMessage( cMessage *msg );
    void handleSelfMessage( cMessage *msg );
};


