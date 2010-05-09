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
//
// TODO auto-generated module
//
network SingleRadio
{
    submodules:
        channelControl: ChannelControl {
            @display("p=24,24");
        }
        mobileHost0: FixManetRoutingHost {
            @display("p=41,62;r=,,#000040");
        }
        mobileHost1: FixManetRoutingHost {
            @display("p=115,62;r=,,#000040");
        }
        mobileHost2: FixManetRoutingHost {
            @display("p=192,62;r=,,#000040");
        }
        mobileHost3: FixManetRoutingHost {
            @display("p=264,62;r=,,#000040");
        }
        mobileHost4: FixManetRoutingHost {
            @display("p=332,62;r=,,#000040");
        }
        mobileHost5: FixManetRoutingHost {
            @display("p=41,137;r=,,#000040");
        }
        mobileHost6: FixManetRoutingHost {
            @display("p=115,137;r=,,#000040");
        }
        mobileHost7: FixManetRoutingHost {
            @display("p=192,137;r=,,#000040");
        }
        mobileHost8: FixManetRoutingHost {
            @display("p=264,137;r=,,#000040");
        }
        mobileHost9: FixManetRoutingHost {
            @display("p=332,137;r=,,#000040");
        }
        mobileHost10: FixManetRoutingHost {
            @display("p=41,215;r=,,#000040");
        }
        mobileHost11: FixManetRoutingHost {
            @display("p=115,215;r=,,#000040");
        }
        mobileHost12: FixManetRoutingHost {
            @display("p=192,215;r=,,#000040");
        }
        mobileHost13: FixManetRoutingHost {
            @display("p=264,215;r=,,#000040");
        }
        mobileHost14: FixManetRoutingHost {
            @display("p=332,215;r=,,#000040");
        }
        flatNetworkConfigurator: FlatNetworkConfigurator {
            @display("p=298,17");
        }
}
