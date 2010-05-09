
#ifndef __DHCPSERVER_H__
#define __DHCPSERVER_H__

#include <vector>
#include <map>
#include <omnetpp.h>
#include "UDPAppBase.h"
#include "DHCP_m.h"
#include "DHCPOptions.h"
#include "DHCPLease.h"
#include "InterfaceTable.h"
#include "ARP.h"

class INET_API DHCPServer : public UDPAppBase {

  public:
  protected:

	// Transmission Timer
	enum TIMER_TYPE {PROC_DELAY};

    // list of leased ip
    typedef std::map<IPAddress,DHCPLease> DHCPLeased;
    DHCPLeased leased;

    int numSent;
    int numReceived;

    int bootps_port;  // server
    int bootpc_port;  // client

    simtime_t proc_delay;  // process delay

    InterfaceEntry* ie; // interface to listen

  protected:
    virtual int numInitStages() const {return 4;}
    virtual void initialize(int stage);
    virtual void handleMessage(cMessage *msg);
    virtual void handleIncommingPacket(cPacket* pkt);
  protected:
    // search for a mac into the leased ip
    DHCPLease* getLeaseByMac(MACAddress mac);
    // get the next available lease to be assigned
    DHCPLease* getAvailableLease();

  public:
    void handleTimer(cMessage *msg);
    void processPacket(cPacket *msg);
    void sendOffer(DHCPLease* lease);
    void sendACK(DHCPLease* lease);
    virtual void sendToUDP(cPacket *msg, int srcPort, const IPvXAddress& destAddr, int destPort);
};

#endif

