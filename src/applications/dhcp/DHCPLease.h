#ifndef __DHCPLEASE_H__
#define __DHCPLEASE_H__

#include <omnetpp.h>
#include "Byte.h"
#include "IPAddress.h"
#include "MACAddress.h"
#include "ARP.h"

class DHCPLease {
   public:
      long xid;
      IPAddress ip;
      MACAddress mac;
      IPAddress gateway;
      IPAddress network;
      IPAddress netmask;
      IPAddress dns;
      IPAddress ntp;
      IPAddress server_id;
      std::string host_name;
      simtime_t lease_time;
      simtime_t renewal_time;
      simtime_t rebind_time;
      Byte parameter_request_list;
      bool leased;
 
      friend std::ostream& operator << (std::ostream& os, DHCPLease obj) {
         os << "xid:" << obj.xid << " ip:" <<  obj.ip << " network:" << obj.network << " netmask:" << obj.netmask << " MAC:" << obj.mac << endl; 
         return(os);
       }
};
#endif
