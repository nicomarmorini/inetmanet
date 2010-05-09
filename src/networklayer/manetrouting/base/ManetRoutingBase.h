/***************************************************************************
 *   Copyright (C) 2008 by Alfonso Ariza                                   *
 *   aarizaq@uma.es                                                        *
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
#ifndef __Manet_routing_base_h__
#define __Manet_routing_base_h__

#include <sys/time.h>
#include "compatibility.h"
#include "IPDatagram_m.h"
#include "IRoutingTable.h"
#include "NotificationBoard.h"
#include "IPv4InterfaceData.h"
#include "IInterfaceTable.h"
#include "IPvXAddress.h"
#include "uint128.h"
#include "NotifierConsts.h"
#include <vector>

class ManetRoutingBase;
class ManetTimer :  public cOwnedObject {
protected:
	ManetRoutingBase *		agent_;	///< OLSR agent which created the timer.
public:
	ManetTimer();
	ManetTimer(ManetRoutingBase* agent);
	virtual void expire()=0;
	virtual void removeQueueTimer();
	virtual void removeTimer();
	virtual void resched(double time);
	virtual void resched(simtime_t time);
	virtual ~ManetTimer();
};


typedef std::multimap <simtime_t, ManetTimer *> TimerMultiMap;
class INET_API ManetRoutingBase : public cSimpleModule, public INotifiable
{

	private:
		 IRoutingTable *inet_rt;
		 IInterfaceTable *inet_ift;
		 NotificationBoard *nb;
		 bool mac_layer_;
		 Uint128	hostAddress;
		 Uint128	routerId;
		 static const int maxInterfaces = 10;
		 double xPosition;
		 double yPosition;
		 double xPositionPrev;
		 double yPositionPrev;
		 simtime_t posTimer;
		 simtime_t posTimerPrev;
		 bool   regPosition;



		 typedef struct InterfaceIdentification
		 {
			 InterfaceEntry* interfacePtr;
			 int index;
			 inline  InterfaceIdentification & operator = (const  InterfaceIdentification& b)
			 {
				 interfacePtr=b.interfacePtr;
				 index=b.index;
				 return *this;
			 }
		 }InterfaceIdentification;
		 std::vector <InterfaceIdentification> interfaceVector;
		 TimerMultiMap *timerMultiMapPtr;
		 cMessage *timerMessagePtr;
	protected:
		~ManetRoutingBase();
		ManetRoutingBase(){regPosition=false; mac_layer_ = false;timerMessagePtr=NULL;timerMultiMapPtr=NULL;}

////////////////////////////////////////////
////////////////////////////////////////////
///     CRITICAL
//      must be executed in the initialize method with stage 4 o bigger, this method initialize all the estructures
/////////////////////////////////////////////
/////////////////////////////////////////////
		virtual void registerRoutingModule();

//
//
		virtual void createTimerQueue();
		virtual void scheduleEvent();
		virtual bool checkTimer(cMessage *msg);


/////////////////////////////////
//
//   encapsulate messages and send to the next layer
//
/////////////////////////////////////
		virtual void sendToIp (cPacket *, int, const Uint128 & , int ,int,double,const Uint128 & iface=0);
		virtual void sendToIp (cPacket *p, int port, int dest , int porDest, int ttl,double delay,int iface=0)
		{
			sendToIp (p, port , (Uint128) dest , porDest ,ttl,delay, (Uint128) iface);
		}
		virtual void sendToIp (cPacket *, int, const Uint128 & , int ,int,const Uint128 &iface =0);
		virtual void sendToIp (cPacket *, int, const Uint128 & , int ,int,double,int index=-1);
/////////////////////////////////
//
//   Ip4 routing table access routines
//
/////////////////////////////////////

//
// delete/actualize/insert and record in the routing table
//
		virtual void omnet_chg_rte (const Uint128 &dst, const Uint128 &gtwy, const Uint128 &netm,short int hops,bool del_entry,const Uint128 &iface=0);
		virtual void omnet_chg_rte (const struct in_addr &dst, const struct in_addr &gtwy, const struct in_addr &netm,short int hops,bool del_entry);
		virtual void omnet_chg_rte (const struct in_addr &, const struct in_addr &, const struct in_addr &,short int,bool,const struct in_addr &);
		virtual void omnet_chg_rte (const Uint128 &dst, const Uint128 &gtwy, const Uint128 &netm,short int hops,bool del_entry,int);
		virtual void omnet_chg_rte (const struct in_addr &, const struct in_addr &, const struct in_addr &,short int,bool,int);


		virtual void deleteIpEntry(const Uint128 &dst){omnet_chg_rte (dst,dst,dst,0,true);}
		virtual void setIpEntry(const Uint128 &dst, const Uint128 &gtwy, const Uint128 &netm,short int hops,const Uint128 &iface=0){omnet_chg_rte (dst,gtwy,netm,hops,false,iface);}
//
// Check if it exists in the ip4 routing table the address dst
// if it doesn't exist return ALLONES_ADDRESS
//
		virtual Uint128 omnet_exist_rte (Uint128 dst);

//
// Check if it exists in the ip4 routing table the address dst
// if it doesn't exist return false
//
        virtual bool omnet_exist_rte (struct in_addr dst);
		virtual void omnet_clean_rte ();

/////////////////////////
//  Cross layer routines
/////////////////////////

//
// Activate the LLF break
//
		virtual void linkLayerFeeback();
//
//      activate the promiscuous option
//
		virtual void linkPromiscuous();

//      activate the full promiscuous option
//
		virtual void linkFullPromiscuous();
//
//     activate the register position. For position protocols
//     this method must be activated in the stage 0 to register the initial node position
//
		virtual void registerPosition();

//
// Link layer feedback routines
//
		virtual void receiveChangeNotification(int category, const cPolymorphic *details);
		virtual void processLinkBreak(const cPolymorphic *details);
		virtual void processPromiscuous (const cPolymorphic *details);
		virtual void processFullPromiscuous (const cPolymorphic *details);
//
//  Replacement for gettimeofday(), used for timers.
//  The timeval should only be interpreted as number of seconds and
//  fractions of seconds since the start of the simulation.
//
		int gettimeofday(struct timeval *, struct timezone *);

////////////////////////
//   Information access routines
////////////////////////

//
//      get the address of the first wlan interface
//
		Uint128 getAddress() const {return hostAddress;}
		Uint128 getRouterId() const {return routerId;}
//
// Return true if the routing protocols is execure in the mac layer
//
		bool isInMacLayer() const {return mac_layer_;}

//
// get the i-esime interface
//
		InterfaceEntry * getInterfaceEntry (int index) const {return inet_ift->getInterface(index);}
//
// Total number of interfaces
//
		int getNumInterfaces() const {return inet_ift->getNumInterfaces();}

// Check if the address is local
		bool isIpLocalAddress (const IPAddress& dest) const;
		bool isLocalAddress (const Uint128& dest) const;

///////////////
// wlan Interface access routines
//////////////////

//
// Get the index of interface with the same address that add
//
		int getWlanInterfaceIndexByAddress (Uint128 =0);

//
// Get the interface with the same address that add
//
		InterfaceEntry * getInterfaceWlanByAddress (Uint128 =0) const;

//
// get number wlan interfaces
//
		int getNumWlanInterfaces() const {return interfaceVector.size();}
//
// Get the index used in the general interface table
//
		int getWlanInterfaceIndex (int i) const;
//
// Get the i-esime wlan interface
//
		InterfaceEntry *getWlanInterfaceEntry (int i) const;

//
// DSDV routines
//
//
		void setTimeToLiveRoutingEntry(simtime_t a){inet_rt->setTimeToLiveRoutingEntry(a);}
		simtime_t getTimeToLiveRoutingEntry()  const {return inet_rt->getTimeToLiveRoutingEntry();}

//
//     Access to the node position
//
		double getXPos();
		double getYPos();
		double getSpeed();

	public:
// Routing information access
		virtual uint32_t getRoute(const Uint128 &,Uint128 add[])= 0;
		virtual bool getNextHop(const Uint128 &,Uint128 &add,int &iface)= 0;
		virtual void setRefreshRoute(const Uint128 &, const Uint128 &,const Uint128&,const Uint128&)= 0;
		virtual bool isProactive()=0;
		virtual bool isOurType(cPacket *)=0;
		virtual bool getDestAddress(cPacket *,Uint128 &)=0;
		TimerMultiMap *getTimerMultimMap()const {return timerMultiMapPtr;}

};

#define interface80211ptr getInterfaceWlanByAddress()
#endif

