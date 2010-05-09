// Simple DHCP Server
//
// Only assign a simple network with a exclusion list
//
// Juan-Carlos Maureira
// INRIA 2008

package inet.applications.dhcp;

import inet.applications.udpapp.UDPApp;

simple DHCPServer like UDPApp
{
    parameters:
        string net;  		// IP network to assign
        string mask;  		// netmask to assign
        string ip_begin;	// ip to start to lease
        int client_num; 	// number of ip to lease
        string gateway;		// gateway to assign
        string dns;			// dns to assign
        int lease_time;		// lease time in seconds
        string iface;		// interface to listen
        @display("i=device/card");
    gates:
        input udpIn;
        output udpOut;
}



