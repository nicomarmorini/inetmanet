//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 
package inet.examples.adhoc.multiradio;
import inet.world.ChannelControl;
import inet.networklayer.autorouting.FlatNetworkConfigurator;
import inet.nodes.adhoc.MobileHost;
import inet.applications.tcpapp.TCPSessionApp;
import inet.linklayer.mfcore.WirelessMac;
import inet.networklayer.autorouting.NetworkConfigurator;
import inet.networklayer.manetrouting.ManetRouting;
import inet.nodes.adhoc.FixManetRoutingHost;
import inet.world.ChannelControlExtended;
import inet.networklayer.autorouting.FlatNetworkConfigurator6;
//
// TODO auto-generated module
//
network NetworkMultiRadioIPv4
{
    parameters:
        int numHosts;
    submodules:
        channelControl: ChannelControlExtended {
            @display("p=83,15");
        }
        mobileHost[numHosts]: MultiRadioIPv4Host {
            @display("r=,,#000040");
        }
        flatNetworkConfigurator: FlatNetworkConfigurator {
            @display("p=220,12");
        }
    connections allowunconnected:
}


