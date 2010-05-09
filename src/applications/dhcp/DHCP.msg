//
// DHCP Messages 
//
// To be used by the DHCP client and server
//
// Juan-Carlos Maureira
// INRIA 2009

cplusplus {{
#include "MACAddress.h"
#include "IPAddress.h"
#include "DHCPOptions.h"
}}


class noncobject MACAddress;
class noncobject IPAddress;
class noncobject DHCPOption;

enum DHCP_OPCODE
{
    BOOTREQUEST = 1;
    BOOTREPLY = 2;
}

enum DHCP_TYPE
{
    DHCPDISCOVER = 1;
    DHCPOFFER = 2;
    DHCPREQUEST = 3;
    DHCPDECLINE = 4;
    DHCPACK = 5;
    DHCPNAK = 6;
    DHCPRELEASE = 7;
    DHCPINFORM = 8;
    DHCPFORCERENEW = 9;
    DHCPLEASEQUERY = 10;
    DHCPLEASEUNASSIGNED = 11;
    DHCPLEASEUNKNOWN = 12;
    DHCPLEASEACTIVE = 13;
}

packet DHCPMessage
{
    int op;
    int htype;
    int hlen;
    int hops;
    long xid;
    int secs;
    long flags;
    IPAddress ciaddr;
    IPAddress yiaddr;
    IPAddress giaddr;
    MACAddress chaddr;
    string sname;
    string file;
    DHCPOption options;
}

