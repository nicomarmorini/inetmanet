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
//

package inet.experimental.linklayer.ieee80211.mgmt;

simple Ieee80211Mesh
{
    parameters:
        int frameCapacity = default(100);
        bool useProactive= default(true);
        bool useReactive= default(true);
        bool UseLwMpls=default(false);
        bool ProactiveFeedback=default(true);
 //       int Willingness = default(3); //ms
 //       int Hello_ival = default(2); // sec
 //       int Tc_ival = default(5); // sec
 //       int Mid_ival = default(5); // sec
 //       int use_mac = default(0);

    gates:
        input  routingInProactive;
        output routingOutProactive;
        input  routingInReactive;
        output routingOutReactive;
        input  uppergateIn;
        output uppergateOut;
        input  macIn[];
        output macOut[];
}
