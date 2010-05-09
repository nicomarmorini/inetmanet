//
// Copyright (C) 2008 Alfonso Ariza
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//


package inet.experimental.nodes.adhoc;

import inet.applications.pingapp.PingApp;
import inet.applications.tcpapp.TCPApp;
import inet.applications.udpapp.UDPApp;
import inet.base.NotificationBoard;
import inet.mobility.BasicMobility;
import inet.experimental.linklayer.ieee80211.Ieee80211NicMesh;
import inet.experimental.linklayer.ieee80211.Ieee80211aNicMesh;
import inet.experimental.linklayer.ieee80211.Ieee80211gNicMesh;
import inet.networklayer.common.InterfaceTable;
import inet.networklayer.ipv4.RoutingTable;
import inet.nodes.inet.NetworkLayerGlobalArp;
import inet.transport.tcp.TCP;
import inet.transport.udp.UDP;
import inet.networklayer.manetrouting.ManetRouting;


//
// Models a mobile host with a wireless (802.11b) card. This module is
// a WirelessHost less the \PPP and Ethernet interfaces, plus mobility
// support (BasicMobility). It is intended mainly for mobile ad-hoc
// networks.
//
module MobileManetRoutingMesh
{
    parameters:
        @node();
        int numTcpApps = default(0);
        int numUdpApps = default(0);
        string tcpAppType = default("");
        string udpAppType = default("");
        string routingFile = default("");
        string mobilityType = default("NullMobility");
        @display("i=device/pocketpc_s");

    gates:
        input radioIn @directIn;
    submodules:


       notificationBoard: NotificationBoard {
            parameters:
                @display("p=60,70");
        }
        interfaceTable: InterfaceTable {
            parameters:
                @display("p=60,150");
        }
        routingTable: RoutingTable {
            parameters:
                IPForward = true;
                routerId = "";
                routingFile = routingFile;
                @display("p=60,230");
        }
        tcpApp[numTcpApps]: <tcpAppType> like TCPApp {
            parameters:
                @display("p=163,67");
        }
        tcp: TCP {
            parameters:
                @display("p=163,154");
        }
        udpApp[numUdpApps]: <udpAppType> like UDPApp {
            parameters:
                @display("p=272,67");
        }
        udp: UDP {
            parameters:
                @display("p=272,154");
        }
        pingApp: PingApp {
            parameters:
                @display("p=343,200");
        }
        networkLayer: NetworkLayerGlobalArp {
            parameters:
                proxyARP = false;
		globalARP = true;
                @display("p=248,247;q=queue");
            gates:
                ifIn[1];
                ifOut[1];
        }
// the name of radio interface must have "wlan", valid examples wlan, wlan802, localwlan ....
        wlan: Ieee80211gNicMesh {
            parameters:
                @display("p=248,349;q=queue");
        }
        mobility: <mobilityType> like BasicMobility {
            parameters:
                @display("p=149,307");
        }

       connections allowunconnected:
        for i=0..numTcpApps-1 {
            tcpApp[i].tcpOut --> tcp.appIn++;
            tcpApp[i].tcpIn <-- tcp.appOut++;
        }

        tcp.ipOut --> networkLayer.tcpIn;
        tcp.ipIn <-- networkLayer.TCPOut;

        for i=0..numUdpApps-1 {
            udpApp[i].udpOut --> udp.appIn++;
            udpApp[i].udpIn <-- udp.appOut++;
        }

        udp.ipOut --> networkLayer.udpIn;
        udp.ipIn <-- networkLayer.udpOut;

        networkLayer.pingOut --> pingApp.pingIn;
        networkLayer.pingIn <-- pingApp.pingOut;

        // connections to network outside
        radioIn --> wlan.radioIn;
        wlan.uppergateOut --> networkLayer.ifIn[0];
        wlan.uppergateIn <-- networkLayer.ifOut[0];
}


