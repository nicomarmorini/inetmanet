//
// DHCP Client
//
// Juan-Carlos Maureira
// INRIA 2008

package inet.applications.dhcp;

import inet.applications.udpapp.UDPApp;

simple DHCPClient like UDPApp
{
    parameters:
        string iface = default("wlan");
        @display("i=device/card");
    gates:
        input udpIn;
        output udpOut;
}



