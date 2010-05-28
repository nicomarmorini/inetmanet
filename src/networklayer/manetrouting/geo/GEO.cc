/***************************************************************************
 *   Copyright (C) 2009: leonardo maccari: leonardo.maccari@unifi.it       *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

///
/// \file	GEO.cc
/// \brief	Implementation of GEO routing agent and related classes.
///


#include <math.h>
#include <limits.h>
#include <sstream>
#include <algorithm>
#include <sstream>

#include "UDPPacket.h"
#include "GEO.h"
#include "geo_m.h"
#include "uint128.h"
#include "IPControlInfo.h"
#include "IPv4InterfaceData.h"
#include "IPv6ControlInfo.h"
#include "RoutingTableAccess.h"
#include "InterfaceTableAccess.h"
#include "InterfaceTable.h"
#include "InterfaceEntry.h"
#include "Ieee80211MgmtBase.h"
#include "Ieee80211gMac.h"
//#include "geopkt_m.h"

#include "ControlManetRouting.h"
#include "Ieee802Ctrl_m.h"

Define_Module(GEO);



/********** GEO class **********/



// initialize the node list and their coordinates
// build the routing table for each node
// schedule an event for refresh
void
GEO::initialize(int stage) {

	if (stage==4)
	{
		EV << "Initializing GEO routing: stage 4 ";

//		WATCH(numForwardRoutes);
//		WATCH(numForwardStreams);
//		WATCH(numOriginaryStreams);
		WATCH(fwdRate);
//		WATCH(interarrivalTime);
//		WATCH(CW);
//		WATCH(qlen);
		for(int i = 0; i <3; ++i ) {
			mu.push_back(0.0);
			lambda.push_back(0.0);
			load_i.push_back(0.0);
		}
		// inizializzo i vector per calcolare il carico cosi' li posso guardare
		WATCH(mu[0]);
//		WATCH(mu[1]);
//		WATCH(mu[2]);
		WATCH(lambda[0]);
//		WATCH(lambda[1]);
//		WATCH(lambda[2]);
//		WATCH(load_i[0]);
//		WATCH(load_i[1]);
//		WATCH(load_i[2]);
//		WATCH(load_debug);
		WATCH(load);

		// Do some initializations of the configuration parameters
		// this seems to be done
		registerRoutingModule();
		routeRefreshInterval = par("routeRefreshInterval");
		routeFtableInterval = par("ftableRefreshInterval");
		GTmapsRefreshInterval = par("GTmapsRefreshInterval");
		// @TODO  to implement wise interface selection, we have to export the
		// scrambleRoutes function from IRoutingTable to here, or else we
		// cannot make it modular end easy to extend. This will make the
		// updating of metrics much less efficient. but it's needed.
		maxDistance = par("maxDistance");
		refreshTimer = new cMessage("refresh_route", REFRESH_ROUTES);
		refreshFtable = new cMessage("refresh_forward_table", REFRESH_FTABLE);

		refreshGTmaps = new cMessage("refresh_GT_maps", REFRESH_GT_MAPS);

		scheduleAt(simTime()+normal(1,0.2) , refreshTimer);
		rt = RoutingTableAccess().get();
		numInterfaces = InterfaceTableAccess().get()->getNumInterfaces() - 1; // don't count loopback
		randIfIndex = uniform(0,numInterfaces); // int random number between 0 and numInterfaces-1
		nodeClass = par("nodeClass");

		lonelyCount=0;
		noNextHopCount=0;

		// parse policy
		if (strcmp("RANDOM", par("policy").stringValue())==0)
			policy = RANDOM;
		if (strcmp("RAMEN", par("policy").stringValue())==0)
		{
			policy = RAMEN;
			if (strcmp("RANDOM", par("ifChoice").stringValue())==0)
				ifChoice = RANDOM_C;
			else if (strcmp("PREALLOC", par("ifChoice").stringValue())==0)
				ifChoice = PREALLOC_C;
			else if (strcmp("QUEUEP", par("ifChoice").stringValue())==0)
				ifChoice = QUEUEP_C;
			else if (strcmp("QUEUER", par("ifChoice").stringValue())==0)
				ifChoice = QUEUER_C;
			else
			{
				EV << "Unknown inner policy specified, using PREALLOC";
				ifChoice = PREALLOC_C;
			}

			if (strcmp("LINEAR", par("fitFunction").stringValue())== 0)
				fitFunction = LINEAR;
			else if (strcmp("POLYNOMIAL", par("fitFunction").stringValue())==0 )
				fitFunction = POLYNOMIAL;
			else if (strcmp("EXPONENTIAL", par("fitFunction").stringValue())==0 )
				fitFunction = EXPONENTIAL;
			else if (strcmp("EXPONENTIAL_RELATIVE", par("fitFunction").stringValue())==0 )
				fitFunction = EXPONENTIAL_RELATIVE;
			else {
				EV << "Unknown fitfunction, using LINEAR ";
				fitFunction = LINEAR;
			}
		}
		else if (strcmp("STATIC", par("policy").stringValue())==0)
		{
			policy = STATIC;
			if (nodeClass >= 0 && nodeClass <= numInterfaces){
				myClass = nodeClass;
				WATCH(nodeClass);
			}
			else {
				std::cerr << "Static Class out of range" << endl;
				exit(-1);
			}
			EV << "Choose STATIC policy";
			if (strcmp("RANDOM", par("ifChoice").stringValue())==0)
				ifChoice = RANDOM_C;
			else if (strcmp("PREALLOC", par("ifChoice").stringValue())==0)
				ifChoice = PREALLOC_C;
			else if (strcmp("QUEUER", par("ifChoice").stringValue())==0)
				ifChoice = QUEUER_C;
			else if (strcmp("QUEUEP", par("ifChoice").stringValue())==0)
				ifChoice = QUEUEP_C;
			else
			{
				EV << "Unknown inner policy specified, using PREALLOC";
				ifChoice = PREALLOC_C;
			}
		}

		else if (strcmp("GT", par("policy").stringValue())==0) {
			policy = GT;
			EV << "Policy selected: GT" << endl;
			if (strcmp("GT_RND", par("ifChoice").stringValue())==0)
				ifChoice = GT_RND;
			else if (strcmp("GT_FWD", par("ifChoice").stringValue())==0)
				ifChoice = GT_FWD;
			else if (strcmp("GT_NOLOAD", par("ifChoice").stringValue())==0)
				ifChoice = GT_NOLOAD;
//			else if (strcmp("GT_COOP", par("ifChoice").stringValue())==0)
//				ifChoice = GT_COOP;
		}

		else
		{
			EV << "Unknown policy specified, using RANDOM";
			policy = RANDOM;
		}
		// parse inner policy

		if(policy != RANDOM && policy != STATIC && policy != GT)
			scheduleAt(simTime()+normal(1,0.2) , refreshFtable);
		if (policy == GT)
			scheduleAt(simTime() + 1 + normal(1,0.2), refreshGTmaps); // apps start at T = 1, so I schedule first refresh starting from that instant

		InterfaceEntry * tmp;
		InterfaceEntry * lo;
		lo = InterfaceTableAccess().get()->getFirstLoopbackInterface();
		for (int j = 0; j<numInterfaces+1; j++ )
		{
			tmp = InterfaceTableAccess().get()->getInterface(j);
			if (tmp != lo)
			{
				interfaceData ifd;
				ifd.iface = tmp;
				ifd.gateIndex = tmp->getNetworkLayerGateIndex();

				cModule * wlanlist = getParentModule()->getParentModule()
				->getParentModule()->getSubmodule("wlan", ifd.gateIndex);
				ifd.mgmt = wlanlist->getSubmodule("mgmt",0);
				ifd.mac = wlanlist->getSubmodule("mac",0);
				ifd.zero();
				ifData.push_back(ifd);
			}
		}
	}

}

void
GEO::updateRoutes()
{
	int freshroutecounter = 0;
	rt->flushTable();
	// refresh all the routing tables

	EV << "Refreshing routing tables " << std::endl;

	ChannelControlExtended *cc = ChannelControlExtended::get();
	std::set<ChannelControlExtended::HostRefExtended>::const_iterator neighi;
	std::list<ChannelControlExtended::HostEntryExtended>::const_iterator listi;

	ChannelControlExtended::HostRefExtended nextHop;
	// get a reference to the host I'm attached to
	me = static_cast<ChannelControlExtended::HostRefExtended>(
			cc->lookupHost(getParentModule()->getParentModule()->getParentModule()));
	ChannelControlExtended::ModuleList  neighlist = cc->getNeighbors(me);

	// get a list of neighbors from CC
	std::set<ChannelControlExtended::HostRefExtended> myNeighbors;
	for (std::vector<cModule*>::iterator i = neighlist.begin(); i != neighlist.end(); i++)
	{
		myNeighbors.insert(static_cast<ChannelControlExtended::HostRefExtended>(cc->lookupHost(*i)));
	}
	// get a list of all the nodes from CC
	const std::list<ChannelControlExtended::HostEntryExtended> & hostList = cc->getHostList();


	if (myNeighbors.size() == 0)
	{
		EV << "I'm feeling lonely, no neighbors reachable" << std::endl;
		lonelyCount++;
		return; // I'm feeling lonely
	}
	// for each node in the network, get the closest neighbor and add a
	// route for each of the nodes' interface IP through the neighbor.

	Coord candidate_coord, my_coord;
	my_coord = cc->getHostPosition(me);

	for (listi = hostList.begin(); listi != hostList.end(); listi++) // for each node in the net
	{
		if(&(*listi) != me)
		{
			const ChannelControlExtended::HostEntryExtended * const t = &(*listi); // target host
			const Coord& tgt_coord = cc->getConstHostPosition(t);
			nextHop = me;
			candidate_coord = cc->getHostPosition(nextHop);
			for (neighi = myNeighbors.begin(); neighi != myNeighbors.end(); neighi++) // for each neighbor
			{

				if (tgt_coord.distance(cc->getHostPosition(*neighi)) < tgt_coord.distance(candidate_coord) &&
						my_coord.distance(cc->getHostPosition(*neighi)) < maxDistance)
				{
					nextHop = *neighi;
					candidate_coord = cc->getHostPosition(nextHop);
				}
			}
			if (nextHop == me) // didn't find a nexthop to the target, we have no route.
			{
				noNextHopCount++;
				continue;
			}
			// find the number of interfaces,
			// for each interface, find the IP address
			// put a different route for each of them, with metric 1

			cModule * target_host = t->host; // cModule for target host
			cModule * next_hop = nextHop->host; // cModule for next hop

			// get insterface table for target host
			cModule * ift_t_m =  target_host->getModuleByRelativePath("interfaceTable");
			InterfaceTable * ift_t = NULL;
			// @FIXME in the previous cicle we should check if this node has
			// interfaces before chosing it as a next_hop, here it is too late
			cModule * ift_n_m =  next_hop->getModuleByRelativePath("interfaceTable");
			InterfaceTable * ift_n = NULL;

			// Does the target have an interfacetable?
			if (ift_t_m)
				ift_t = dynamic_cast<InterfaceTable *>(ift_t_m);
				else
					return;


			// Does the nexthop have an interfacetable?
			if (ift_n_m)
				ift_n = dynamic_cast<InterfaceTable *>(ift_n_m);
				else
					EV << "ERROR, using a next hop node without interface table!" << std::endl;


			IInterfaceTable * my_ift = InterfaceTableAccess().get();
			if (!my_ift)
			{
				EV << "ERROR: this node has no interfaces" << std::endl;
				return;
			}

			// @FIXME are these interfaces connected??
			// try with getNodeOutputGateId()->isconnected()
			for (int i=0; i< ift_t->getNumInterfaces(); i++)
			{
				InterfaceEntry * ife_t = ift_t->getInterface(i);
				// @FIXME check if this is a wlan interface, not if it is
				// not only lo0
				if(ife_t->getName() != std::string("lo0"))
				{
					IPv4InterfaceData * ipdata_t = ife_t->ipv4Data();
					IPAddress ip_t = ipdata_t->getIPAddress();
					for (int j=0; j< ift_n->getNumInterfaces(); j++)
					{
						InterfaceEntry * ife_n = ift_n->getInterface(j);
						// @FIXME check if this is a wlan interface, not if it is
						// not only lo0
						if(ife_n->getName() != std::string("lo0"))
						{
							IPv4InterfaceData * ipdata_n = ife_n->ipv4Data();
							IPAddress ip_n = ipdata_n->getIPAddress();

							InterfaceEntry * my_if = my_ift->getInterfaceByName(ife_n->getName());

							IPRoute *e = new IPRoute();
							e->setHost(ip_t);
							e->setNetmask(IPAddress::ALLONES_ADDRESS);
							e->setGateway(ip_n);
							e->setType(IPRoute::REMOTE);
							e->setSource(IPRoute::MANET);
							e->setMetric(1);
							e->setInstallTime (simTime());
							// @TODO get the interface that I own that has the same channel
							// of the interface of the nextHop. Now I just get the interface
							// with the same name, meaning that there is a strict association
							// between interfacenumbers and channels
							e->setInterface(my_if);
							rt->addRoute(e);
							freshroutecounter++;
						}
					}

				}
			}

		}
	}
	EV <<  freshroutecounter << " routes updated " << endl;
}

void GEO::handleMessage (cMessage *msg)
{
	if (msg->isSelfMessage())
	{
		geo_pkt * broadp = new geo_pkt; //FIXME dellocate this frame
		switch(msg->getKind()){
		case REFRESH_ROUTES:
			updateRoutes();
			scheduleAt( simTime()+routeRefreshInterval ,refreshTimer); // TODO: add jitter here
			break;
		case REFRESH_FTABLE: // used only if policy != RANDOM
			broadp->setTrafficLoad(numForwardRoutes);
			broadp->setByteLength(2*sizeof(int));
			broadp->setNodeId(rt->getRouterId().getInt());

			// if we don't specify an interface index (-1) we send one broadcast
			// per interface (FIXME really?)

			sendToIp (broadp, GEO_PORT, IPAddress::ALLONES_ADDRESS, GEO_PORT, 1 , (Uint128) 0);

			numForwardRoutes_v.push_back(numForwardRoutes);
			neighs.record(neighborMap.size());
			//        for (std::vector<struct interfaceData>::iterator kk = ifData.begin();
			//            kk != ifData.end(); kk++)
			//          std::cout << *kk ;
			for (std::vector<interfaceData>::iterator ii = ifData.begin(); ii != ifData.end(); ii++)
			{
				ii->updateValues();
			}
			sort(ifData.begin(), ifData.end(), sortByCollision);
			//        std::cout << " ****** AFTER ******** " << std::endl;
			//        for (std::vector<struct interfaceData>::iterator kk = ifData.begin();
			//            kk != ifData.end(); kk++)
			//          std::cout << *kk ;

			updateClass();
			updateDisplayString();
			forwardRoutes.clear();
			numForwardRoutes = 0;
			scheduleAt(simTime()+routeFtableInterval ,refreshFtable);
			if (! refreshRotator % REF_ROT)
			{
				refreshRotator = 1;
				neighborMap.clear();
			}
			else
				refreshRotator++;

			break;
		case REFRESH_GT_MAPS:
			forwardRate->collect(fwdRate);
			carico->collect(load);
			updateDisplayString();
			forwardStreams.clear();
			originaryStreams.clear();
			numForwardStreams = 0;
			numOriginaryStreams = 0;
			fwdRate = 0;
			load = 1;
			scheduleAt(simTime()+GTmapsRefreshInterval, refreshGTmaps);
			break;

		default:
			EV << "Unknown self-message received" << endl;
			cancelAndDelete(msg);
			break;
		}

	} else
	{
		if (dynamic_cast<ControlManetRouting *>(msg))
		{
			ControlManetRouting * control =  check_and_cast <ControlManetRouting *> (msg);
			if (control->getOptionCode()== MANET_ROUTE_UPDATE)
			{
				const IPAddress da = control->getDestAddress();
				if (policy == GT) {
					const IPAddress sa = control->getSrcAddress();
					if (!sa.isUnspecified() && !da.isUnspecified()) // packet comes from lower layer
					{
						EV << "Packet from stream (" << sa << ", " << da << ") arrived." << endl;

						if (rt->isLocalAddress(da)) {
							EV << "It's for one of my interfaces.. delivering it!" << endl;
						}

						else  // the packet is not for this node i.e. I have to forward it
						{
							if (randomizeRoutes(da, true)) {
								interarrivalTime->collect(simTime() - t_prec);
								t_prec = simTime();
								EV << "I have to forward it." << endl;
								std::pair<std::map<std::pair<IPAddress, IPAddress>, bool>::iterator, bool> it = forwardStreams.insert(std::make_pair(std::make_pair(sa,da), 1) );
								if (it.second) {
									numForwardStreams = forwardStreams.size();
									EV << "It's a new stream.\n";
									EV << "Now there are " << numForwardStreams << " active forward streams." << endl;
								}
								else {
									EV << "It belongs to an already active stream." << endl;
									EV << "There are " << numForwardStreams << " active forward streams." << endl;
								}
								fwdRate = (double)  numForwardStreams / (numForwardStreams + numOriginaryStreams);
								EV << "Forwarding rate is: " << fwdRate  << endl;
								rt->invalidateCache();
							}
							else {
								EV << "ERROR, not scrambling routes";
							}

						}
					}
					else if (sa.isUnspecified() && !da.isUnspecified())  // packet is originated in this node
					{
						if (randomizeRoutes(da, false)) {
							interarrivalTime->collect(simTime() - t_prec);
							t_prec = simTime();
							std::pair<std::map<std::pair<IPAddress, IPAddress>, bool>::iterator, bool> it = originaryStreams.insert(std::make_pair(std::make_pair(sa,da), 1) );
							if (it.second) {
								EV << "New stream, from this node to " << da << " has been generated." << endl;
								numOriginaryStreams = originaryStreams.size();
								EV << "Now there are " << numOriginaryStreams << " active originary streams" << endl;
							}
							else {
								EV << "Packet belonging to the active originary stream to " << da << " must be sent." << endl;
								EV << " There are " << numOriginaryStreams << " active originary streams" << endl;
							}
							fwdRate = (double)  numForwardStreams / (numForwardStreams + numOriginaryStreams);
							EV << "Forwarding rate is: " << fwdRate  << endl;

							rt->invalidateCache();
						}
						else {
							EV << "ERROR, not scrambling routes";
						}

					}

				}

				else if (policy != GT) {
					if (randomizeRoutes(da))
					{
						EV << "routes scrambled";
						const IPAddress sa = control->getSrcAddress();
						if (!sa.isUnspecified() && !da.isUnspecified())
						{
							if (forwardRoutes.insert(std::make_pair(std::make_pair(sa,da), 1) ).second)
								numForwardRoutes = forwardRoutes.size();
						}
						else if (sa.isUnspecified() && !da.isUnspecified())  // packet is originated in this node
						{
							if (forwardRoutes.insert(std::make_pair(std::make_pair(rt->getRouterId(),da), 1) ).second)
								numForwardRoutes = forwardRoutes.size();
						}
						rt->invalidateCache();

					}
					else
						EV << "ERROR, not scrambling routes";
				}

			}
			delete msg;
		}
		else if (dynamic_cast<UDPPacket  *>(msg)) // GEO packets
		{
			UDPPacket * udppkt = check_and_cast<UDPPacket * >(msg);

			if(udppkt->getDestinationPort() != GEO_PORT)
			{
				delete msg;
				return;
			}
			cMessage * msg_aux = udppkt->decapsulate();
			if(dynamic_cast<geo_pkt *>(msg_aux))
			{
				geo_pkt * pkt  = check_and_cast<geo_pkt *>(msg_aux);
				neighborMap.erase(IPAddress(pkt->getNodeId()));
				neighborMap.insert(std::make_pair(IPAddress(pkt->getNodeId()), pkt->getTrafficLoad()));
			}
			// we have to delete the original message and all the messages that
			// we decapsulated that are local created copies
			if (msg_aux)
				delete msg_aux;
			delete msg;
		}
	}
}

void GEO::updateDisplayString()
{
	if(ev.isGUI())
	{
		std::stringstream tmp;

		if (policy != GT) {
			tmp << "forwardroutes " << numForwardRoutes << std::endl << "num neighs " << neighborMap.size()
			<< std::endl << "myClass " << myClass;
		}
		else {
			tmp << "Forward rate " << fwdRate << endl << "load " << load << endl << "tentative myClass " << fwdRate * load << endl;
		}

		getParentModule()->getParentModule()->getParentModule()->bubble(tmp.str().c_str());
		cDisplayString disp = getParentModule()->getParentModule()->getParentModule()->getDisplayString();
		disp.insertTag("t");
		disp.setTagArg("t",0,tmp.str().c_str()); // FIXME why this doesn't update the displaystring??
	}
}

void GEO::updateClass()
{
	if(neighborMap.empty())
	{
		myClass = numInterfaces;
		//      std::clog << " no nighbors, myClass  " << myClass << std::endl;
		myClass_v.push_back(myClass);
		return;
	}
	std::pair< IPAddress, int>  lowest = *(neighborMap.begin());
	std::pair< IPAddress, int>  highest = *(--(neighborMap.end()));

	for(std::map<IPAddress, int>::iterator ii=neighborMap.begin(); ii != neighborMap.end(); ii++)
	{
		if (ii->second < lowest.second)
		{
			lowest.first = ii->first;
			lowest.second = ii->second;
		}
		if (ii->second > highest.second)
		{
			highest.first = ii->first;
			highest.second = ii->second;
		}
	}


	if (highest.second <= numForwardRoutes)
		myClass =  numInterfaces;
	else if (lowest.second >= numForwardRoutes)
		myClass = 1;
	else // linear increment
	if(fitFunction == LINEAR)
		myClass = numForwardRoutes * ( (double) numInterfaces - 1 )/(double)(highest.second - lowest.second) +
		(highest.second - numInterfaces * (double) lowest.second)/(double)(highest.second - lowest.second);
	else if (fitFunction == EXPONENTIAL_RELATIVE)
	{
		if (highest.second-lowest.second)
		{
			double base = pow(numInterfaces, (1/(double)(highest.second-lowest.second)));
			myClass = pow(base, numForwardRoutes-lowest.second );
		} else
			myClass =  numInterfaces;
	}
	else if (fitFunction == EXPONENTIAL)
	{
		double base = pow(numInterfaces, (1/(double)highest.second));
		myClass = pow(base, numForwardRoutes );
	}
	std::clog << "highest " << highest.first << " " << highest.second;
	std::clog << " lowest " << lowest.first << " " << lowest.second;
	std::clog << " myClass  " << myClass << std::endl;
	myClass_v.push_back(myClass);
}

int GEO::randomizeRoutes(IPAddress tgt)
{
	// get all the routes that go to tgt, one for each interface
	// decide on which interface we are allowed to transmit based on our
	// class.  set into the routes the metric accordingly

	if(policy == RAMEN || policy == STATIC)
	{
		// get a list of routes with corresponding interfaces
		std::multimap<const IPRoute *, int> tmp = rt->findRouteList(tgt,
				IPAddress::UNSPECIFIED_ADDRESS,IPAddress::UNSPECIFIED_ADDRESS,0,0);

		if (tmp.size() == 0)
			return 0;
		if ( ifChoice == QUEUEP_C || ifChoice == QUEUER_C)
		{
			if (policy == STATIC){
				for (std::vector<interfaceData>::iterator ii = ifData.begin(); ii != ifData.end(); ii++)
				{
					ii->updateValues();
				}
				sort(ifData.begin(), ifData.end(), sortByCollision);
			}

			int k = 0;
			if (ifChoice == QUEUER_C)
			{
				k += randIfIndex;
				k = k % numInterfaces;
			}
			long queued=ifData[k].mgmtNumDataQueued;
			InterfaceEntry * tgtiface = ifData[k].iface;
			for (int n = 0; n < myClass; ++n )
			{
				if (queued > ifData[k].mgmtNumDataQueued)
				{
					queued=ifData[k].mgmtNumDataQueued;
					tgtiface = ifData[k].iface;
					k = (k+1)%numInterfaces;
				}
			}
			//	std::cout<<"Scelta l'interfaccia "<<tgtiface->getInterfaceId()<<" con coda lunga "<<queued<<std::endl;
			//int k = uniform(0, myClass);
			//InterfaceEntry * tgtiface = ifData[k].iface;

			for (std::multimap<const IPRoute *, int>::iterator ii = tmp.begin(); ii!= tmp.end(); ii++)
			{
				InterfaceEntry * iface = ii->first->getInterface();
				if (iface == tgtiface)
					rt->setMetric(ii->second, 1);
				else
					rt->setMetric(ii->second, 10);
			}
		} else if (ifChoice == PREALLOC_C || ifChoice == RANDOM_C)
		{

			int k = uniform(0, myClass);

			if(ifChoice==RANDOM_C)
				k = (k + randIfIndex) % numInterfaces;

			// set to 1 the metric for route with interface k and to 10 the
			// others
			for (std::multimap<const IPRoute *, int>::iterator ii = tmp.begin(); ii!= tmp.end(); ii++)
			{
				int ifaceindex = ii->first->getInterface()->getNetworkLayerGateIndex();
				if(ifaceindex == k)
				{
					rt->setMetric(ii->second, 1);
				}
				else
					rt->setMetric(ii->second, 10);
			}
		}
		return 1;

	}
	else if(policy == RANDOM)
	{
		if(rt->scrambleRoutes(tgt, 100))
		{
			EV <<  "routes randomized " << endl;
			return 1;
		}
		else
			return 0;
	}
}

int GEO::randomizeRoutes(IPAddress tgt, bool fwd)
{
	// get all the routes that go to tgt, one for each interface
	// decide on which interface we are allowed to transmit based on our
	// class.  set into the routes the metric accordingly
	std::multimap<const IPRoute *, int> tmp = rt->findRouteList(tgt,
		IPAddress::UNSPECIFIED_ADDRESS,IPAddress::UNSPECIFIED_ADDRESS,0,0);

	if (tmp.size() == 0)
		return 0;
	for (std::vector<interfaceData>::iterator ii = ifData.begin(); ii != ifData.end(); ii++)
	{
		ii->updateValues();
	}

	for(int k=0; k<numInterfaces; ++k)
	{
	mu[k]=ifData[k].macMeanServiceTime;
	lambda[k]+=ifData[k].mgmtNumQueueReceived;
	}
	load = (double) lambda[0] / (simTime()-1)*mu[0];
	if (load > 1) {
		load = 1;
	}
	int k = randIfIndex;
	if (ifChoice == GT_FWD || ifChoice == GT_NOLOAD) {
		if (fwd == false) {
			// handle originary packet: if the node does "enough" forwarding he can use the 2nd and 3rd channel for his packets; otherwise he's stuck on 1st channel
			if (ifChoice == GT_FWD) {
				myClass = sqrt(fwdRate * load) * numInterfaces;
				k += myClass > 1 ? uniform(0, sqrt(fwdRate * load) * (numInterfaces - 1)) + 1 : 0;
				k = k % numInterfaces;
			}
			else if (ifChoice == GT_NOLOAD) {
				myClass = fwdRate * numInterfaces;
				k += myClass > 1 ? uniform(0,fwdRate*(numInterfaces-1)) + 1 : 0;
				k = k % numInterfaces;
			}
			EV << "Interface selected: " << k << endl;
			for (std::multimap<const IPRoute *, int>::iterator ii = tmp.begin(); ii!= tmp.end(); ii++) {
				int ifaceindex = ii->first->getInterface()->getNetworkLayerGateIndex();
				if(ifaceindex == k) {
					rt->setMetric(ii->second, 1);
				}
				else
					rt->setMetric(ii->second, 10);
			}
			return 1;
		}
		else {
			// handle fwd packet: select the first channel
			for (std::multimap<const IPRoute *, int>::iterator ii = tmp.begin(); ii!= tmp.end(); ii++) {
				int ifaceindex = ii->first->getInterface()->getNetworkLayerGateIndex();
				if(ifaceindex == k) {
					rt->setMetric(ii->second, 1);
				}
				else
					rt->setMetric(ii->second, 10);
			}
			return 1;
		}
	}
//	if (ifChoice == GT_COOP) {
//		if (fwd == true) {
//			// handle fwd packet: if the node does "enough" forwarding he can use the 2nd and 3rd channel for his packets; otherwise he's stuck on 1st channel
//			int k = 0;
//			myClass = sqrt(fwdRate * load) * numInterfaces;
//			k = myClass > 1 ? uniform(0, sqrt(fwdRate * load) * (numInterfaces - 1)) + 1 : 0;
//			EV << "Interface selected: " << k << endl;
//			for (std::multimap<const IPRoute *, int>::iterator ii = tmp.begin(); ii!= tmp.end(); ii++) {
//				int ifaceindex = ii->first->getInterface()->getNetworkLayerGateIndex();
//				if(ifaceindex == k) {
//					rt->setMetric(ii->second, 1);
//				}
//				else
//					rt->setMetric(ii->second, 10);
//			}
//			return 1;
//		}
//		else {
//			// handle orig packet: select the first channel
//			for (std::multimap<const IPRoute *, int>::iterator ii = tmp.begin(); ii!= tmp.end(); ii++) {
//				int ifaceindex = ii->first->getInterface()->getNetworkLayerGateIndex();
//				if(ifaceindex == 0) {
//					rt->setMetric(ii->second, 1);
//				}
//				else
//					rt->setMetric(ii->second, 10);
//			}
//			return 1;
//		}
//	}

	if (ifChoice == GT_RND) {
		myClass = sqrt(fwdRate * load) * numInterfaces;
		int k = uniform(0, myClass);
		k = (k + randIfIndex) % numInterfaces;
		EV << "fwdRate * load = " << fwdRate * load << endl;
		EV << "Sqrt is: " << sqrt(fwdRate * load) << endl;
		EV << "Interface selected: " << k << endl;
		for (std::multimap<const IPRoute *, int>::iterator ii = tmp.begin(); ii!= tmp.end(); ii++) {
			int ifaceindex = ii->first->getInterface()->getNetworkLayerGateIndex();
			if(ifaceindex == k) {
				rt->setMetric(ii->second, 1);
			}
			else
				rt->setMetric(ii->second, 10);
		}
		return 1;
	}

}


bool  GEO::getNextHop(const Uint128 &dest,Uint128 &add, int &iface)
{
	return true;
}

// see /experimental/linklayer/
bool GEO::isOurType(cPacket * msg)
{
	geo_pkt * pkt = dynamic_cast<geo_pkt  *>(msg);
	if (pkt)
		return true;
	return false;
}


uint32_t GEO::getRoute(const Uint128 &dest,Uint128 add[])
{
	return 0;
}

// FIXME questa funzione e il distruttore non viene chiamato!!
void GEO::finish()
{
	if(refreshTimer)
		cancelAndDelete(refreshTimer);
	if(refreshFtable)
		cancelAndDelete(refreshFtable);
	if(refreshGTmaps)
		cancelAndDelete(refreshGTmaps);
	int roundup_f = ceil( ( (double) numForwardRoutes_v.size()) /10);
	int roundup_c = ceil( ( (double) myClass_v.size()) /10);
	if (policy != RANDOM)
	{
		double sum = 0;
		// don't use the first 10% samples of vectors
		for (std::vector<double>::iterator ii = myClass_v.begin()+roundup_c; ii != myClass_v.end(); ii++ )
		{
			sum += *ii;
		}
		sum = sum/(myClass_v.size()-3);
//		std::clog << "avgclass " << ChannelControlExtended::get()->getHostPosition(me).x
//		<< " " << ChannelControlExtended::get()->getHostPosition(me).y
//		<< " "<< sum << std::endl;
		myClass_v.clear();
		sum = 0;
		for (std::vector<double>::iterator ii = numForwardRoutes_v.begin()+roundup_f; ii != numForwardRoutes_v.end(); ii++ )
		{
			sum += *ii;
		}
		sum = sum/(numForwardRoutes_v.size()-3);
//		std::clog << "neighs " << ChannelControlExtended::get()->getHostPosition(me).x
//		<< " " << ChannelControlExtended::get()->getHostPosition(me).y
//		<< " "<< sum << std::endl;
		numForwardRoutes_v.clear();
	}
	recordScalar("Position X", ChannelControlExtended::get()->getHostPosition(me).x);
	recordScalar("Position Y", ChannelControlExtended::get()->getHostPosition(me).y);
	recordScalar("lonelyCount",lonelyCount);
	recordScalar("noNextHopCount",noNextHopCount);
	if (policy == GT) {
		recordScalar("Forwarding rate", forwardRate->getMean());
		recordScalar("Load", carico->getMean());
		// recordScalar("Lambda", lambda);
		// recordScalar("Mu", mu);
		// recordScalar("Carico", load);
		recordScalar("Interarrival time", interarrivalTime->getMean());
//		std::clog << "fwdRate " << ChannelControlExtended::get()->getHostPosition(me).x
//		<< " " << ChannelControlExtended::get()->getHostPosition(me).y
//		<< " "<< forwardRate->getMean() << std::endl;
//		std::clog << "Load " << ChannelControlExtended::get()->getHostPosition(me).x
//		<< " " << ChannelControlExtended::get()->getHostPosition(me).y
//		<< " "<< carico->getMean() << std::endl;
	}
	if(forwardRate)
		delete(forwardRate);
	if(carico)
		delete(carico);
	if(interarrivalTime)
		delete(interarrivalTime);

}

GEO::~GEO(){
}

void interfaceData::updateValues(void)
{
	Ieee80211MgmtBase * mgmtBase = dynamic_cast<Ieee80211MgmtBase* >(mgmt);
	// @FIXME this works only with 11g nodes!! the MAC layers have been
	// brutally copied in a/g/b classes and do not derive from a common
	// class
	Ieee80211gMac * mactmp = dynamic_cast<Ieee80211gMac* >(mac);
	if(!mactmp)
	{
		EV << "Error in conversion to Ieee80211g mac, check interfaceData::updateValues() code "<<std::endl;
		exit(-1);
	}
	mgmtNumQueueReceived = mgmtBase->getNumDataFramesReceived() - mgmtNumQueueReceived_p;
	mgmtNumQueueDropped = mgmtBase->getNumDataFramesDropped() - mgmtNumQueueDropped_p ;
	mgmtNumDataQueued = mgmtBase->getDataQueueLength();
	mgmtNumMgmtQueued = mgmtBase->getMgmtQueueLength();
	mgmtFrameCapacity = mgmtBase->getFrameCapacity();
	macNumCollision =  mactmp->getNumCollision() - macNumCollision_p;
	macNumRetransmission = mactmp->getNumRetry() - macNumRetransmission_p;
	macNumNoRetransmission = mactmp->getNumSentWithoutRetry() - macNumNoRetransmission_p;

	mgmtNumQueueReceived_p = mgmtBase->getNumDataFramesReceived();
	mgmtNumQueueDropped_p = mgmtBase->getNumDataFramesDropped() ;
	macNumCollision_p =  mactmp->getNumCollision();
	macNumRetransmission_p = mactmp->getNumRetry();
	macNumNoRetransmission_p = mactmp->getNumSentWithoutRetry();

	macNumSent = mactmp->getNumSent() - macNumSent_p;
	macNumReceived = mactmp->getNumReceived() - macNumReceived_p;
	macNumSelfReceived= mactmp->getNumSelfReceived() - macNumSelfReceived_p;
	macCWmean = mactmp->getMeanCW();
	macMeanServiceTime = mactmp->getMeanServiceTime();

	macNumSent_p = mactmp->getNumSent();
	macNumReceived_p = mactmp->getNumReceived();
	macNumSelfReceived_p = mactmp->getNumSelfReceived();

}

void interfaceData::zero()
{
	mgmtNumQueueReceived = 0;
	mgmtNumQueueDropped = 0;
	macNumCollision = 0;
	macNumRetransmission = 0;
	macNumSent = 0;
	macNumReceived = 0;
	macNumSelfReceived = 0;
	macCWmean = 0;
	mgmtFrameCapacity = 0;
	macMeanServiceTime = 0;

	mgmtNumQueueReceived_p = 0;
	mgmtNumQueueDropped_p = 0;
	macNumCollision_p = 0;
	macNumRetransmission_p = 0;
	macNumSent_p = 0;
	macNumReceived_p = 0;
	macNumSelfReceived_p = 0;
}

std::ostream& operator<<(std::ostream &output, const struct interfaceData& dd)
{
	output << "iface index: " << dd.gateIndex << std::endl;
	output << " mgmtNumQueueReceived   = " <<  dd.mgmtNumQueueReceived << std::endl;
	output << " mgmtNumQueueDropped    = " <<  dd.mgmtNumQueueDropped << std::endl;
	output << " macNumCollision        = " <<  dd.macNumCollision<< std::endl;
	output << " macNumRetransmission   = " <<  dd.macNumRetransmission<< std::endl;
	output << " mgmtNumQueueReceived_p = " <<  dd.mgmtNumQueueReceived_p<< std::endl;
	output << " mgmtNumQueueDropped_p  = " <<  dd.mgmtNumQueueDropped_p<< std::endl;
	output << " macNumCollision_p      = " <<  dd.macNumCollision_p << std::endl;
	output << " macNumRetransmission_p = " <<  dd.macNumRetransmission_p<< std::endl;

	return output;
}

