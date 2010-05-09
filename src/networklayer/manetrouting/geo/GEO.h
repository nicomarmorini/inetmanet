/***************************************************************************
 *   Copyright (C) 2009 by Leonardo Maccari, University of Florence        *
 *   leonardo.maccari@unifi.it                                             *
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
/// \file	GEO.h
/// \brief	Header file for GEO agent and related classes.

#ifndef __GEO_omnet_h__
#define __GEO_omnet_h__

#include "ManetRoutingBase.h"
#include "ChannelControlExtended.h"
#include "Coord.h"
#include "IRoutingTable.h"

#include "IP.h"

#include <map>
#include <vector>
#include <assert.h>




/********** Useful macros **********/

/// Returns maximum of two numbers.
#ifndef MAX
#define	MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

/// Returns minimum of two numbers.
#ifndef MIN
#define	MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define IP_BROADCAST     ((uint32_t) 0xffffffff)

#define GEO_PORT 666

/// Gets current time from the scheduler.


enum policy_type {
	RANDOM,
	RAMEN,
	GT,
	STATIC
};


enum {
	REFRESH_ROUTES,
	REFRESH_FTABLE,
	REFRESH_GT_MAPS
};

enum ifChoice_t{
	RANDOM_C,
	PREALLOC_C,
	QUEUER_C,
	QUEUEP_C,
	GT_RND,
	GT_FWD,
	GT_NOLOAD
};

enum fitFunction_t{
	LINEAR,
	EXPONENTIAL,
	EXPONENTIAL_RELATIVE,
	POLYNOMIAL
};





#define REF_ROT 2

class GEO;			// forward declaration
struct interfaceData;


///
/// \brief Routing agent which implements a shortest path geo-located
//  algorithm
///


class GEO : public ManetRoutingBase {
private:
	double routeRefreshInterval;  // routing table refresh interval in mseconds, default 100 (Should be double ?!?)
	double routeFtableInterval;  // interval of forward route table refresh (Should be double ?!?)
	double GTmapsRefreshInterval; // interval of GT maps refresh (Should be double ?!?)
	int maxDistance;  // max distance for the "neighbors"
	policy_type policy;
	ifChoice_t ifChoice;
	fitFunction_t fitFunction;
	cMessage * refreshTimer;
	cMessage * refreshFtable;

	cMessage* refreshGTmaps;

	ChannelControlExtended::HostRefExtended me;
	// buffer map
	std::map<std::pair<Uint128,Uint128>, bool> forwardRoutes;

	// Variables for GT policy
	std::map<std::pair<IPAddress, IPAddress>, bool> forwardStreams;    // only forwarded streams i.e. that doesn't originate from the node itself
	std::map<std::pair<IPAddress, IPAddress>, bool> originaryStreams;  // streams that originate in the node
	int numForwardStreams;
	int numOriginaryStreams;
	double fwdRate;
//	double qlen;
//	double CW;
	std::vector<double> mu;
	std::vector<double> lambda;
	std::vector<double> load_i;
	double load;
	double load_debug;
//	int notScrambled;
   cStdDev *forwardRate;
   cStdDev *carico;

   simtime_t t_prec;
   cStdDev *interarrivalTime;

//	std::vector<double> fwdRate_v;
//	std::vector<double> load_v;


	// num of routes that are crossing this node or starting from here
	int numForwardRoutes;
	// map of neighbors with traffic load
	std::map<IPAddress, int> neighborMap;
	// history of samples (might be useful for debugging)
	std::vector<double> numForwardRoutes_v;
	cOutVector neighs;
	std::vector<double> myClass_v;
	std::vector<interfaceData> ifData;
	// we refresh neighbor list each REF_ROT REFRESH_FTABLE intervals
	int refreshRotator;
	int numInterfaces;
	int randIfIndex; // used for RAMEN RANDOM policy
	// my priority class
	double myClass;

	void updateClass();
	int nodeClass;
	int lonelyCount;
  	int noNextHopCount;
protected:
	// Omnet INET variables and functions
	int numInitStages() const  {return 5;}
	IRoutingTable *rt;

	virtual void initialize(int stage);
	void updateRoutes();
	int randomizeRoutes(IPAddress tgt);
	int randomizeRoutes(IPAddress tgt, bool fwd);
	void handleMessage (cMessage *msg);
	// void collectDelay(simtime_t delay);
	virtual void processLinkBreak(const cPolymorphic *details){};
	virtual void processPromiscuous(const cPolymorphic *details){};
	void finish();

public:
	GEO()
	: routeRefreshInterval(100),  routeFtableInterval(0), GTmapsRefreshInterval(0), maxDistance(100),
	policy(RANDOM), refreshTimer(0), refreshFtable(0), refreshGTmaps(0), me(0), load_debug(1),
	forwardRoutes(), forwardStreams(), originaryStreams(), numForwardRoutes(0), numForwardStreams(0), numOriginaryStreams(0),
	forwardRate(0), fwdRate(0),  /* qlen(0), CW(0), */ load(1), t_prec(1.0), neighborMap(), neighs("number of neighbors"),
	myClass(0), randIfIndex(0), //myClass_v("class"), numForwardRoutes_v("forward routes"),
	ifData(), refreshRotator(1)
	{ forwardRate = new cStdDev("Forwarding rate");  carico = new cStdDev("Load"); interarrivalTime = new cStdDev("Interarrival time"); }
	uint32_t getRoute(const Uint128 &,Uint128 add[]);
	bool getNextHop(const Uint128 &,Uint128 &add,int &iface);
	void setRefreshRoute(const Uint128 &, const Uint128 &,const Uint128&,const Uint128&) {};
	bool isProactive() {return true;};
	bool isOurType(cPacket *);
	bool getDestAddress(cPacket *,Uint128 &) {return false;};
	void updateDisplayString();

	~GEO();

};


struct interfaceData {
	friend std::ostream& operator<<(std::ostream& output, const interfaceData& p);

	InterfaceEntry * iface;
	cModule * mac;
	cModule * mgmt;
	int gateIndex;
	int mgmtNumQueueReceived;
	int mgmtNumQueueDropped;
	int mgmtNumDataQueued;
	int mgmtNumMgmtQueued;
	int macNumCollision;
	int macNumRetransmission;
	int macNumNoRetransmission;
	int macNumSent;
	int macNumReceived;
   int macNumSelfReceived;
   int mgmtFrameCapacity;

	int mgmtNumQueueReceived_p;
	int mgmtNumQueueDropped_p;
	int macNumCollision_p;
	int macNumRetransmission_p;
	int macNumNoRetransmission_p;
	int macNumSent_p;
	int 	macNumReceived_p;
	int 	macNumSelfReceived_p;
	double macCWmean;
	double macMeanServiceTime;


	void updateValues();
	void zero();
};

bool sortByCollision(struct interfaceData first, struct interfaceData second)
{
	return first.macNumCollision < second.macNumCollision;
}
#endif
