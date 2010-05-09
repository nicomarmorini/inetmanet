/*****************************************************************************
 *
 * Copyright (C) 2002 Uppsala University.
 * Copyright (C) 2006 Malaga University.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Bj�n Wiberg <bjorn.wiberg@home.se>
 *          Erik Nordstr� <erik.nordstrom@it.uu.se>
 * Authors: Alfonso Ariza Quintana.<aarizaq@uma.ea>
 *
 *****************************************************************************/

#include <string.h>
#include <assert.h>
#include "aodv_uu_omnet.h"

#include "UDPPacket.h"
#include "IPControlInfo.h"
#include "IPv6ControlInfo.h"
#include "ICMPMessage_m.h"
#include "ICMPAccess.h"
#include "NotifierConsts.h"


#include "ProtocolMap.h"
#include "IPAddress.h"
#include "IPvXAddress.h"
#include "ControlManetRouting.h"
#include "Ieee802Ctrl_m.h"


const int UDP_HEADER_BYTES = 8;
typedef std::vector<IPAddress> IPAddressVector;

Define_Module(AODVUU);

/* Constructor for the AODVUU routing agent */

bool AODVUU::log_file_fd_init=false;
int AODVUU::log_file_fd = -1;

#ifdef AODV_GLOBAL_STATISTISTIC
bool AODVUU::iswrite = false;
int AODVUU::totalSend=0;
int AODVUU::totalRreqSend=0;
int AODVUU::totalRreqRec=0;
int AODVUU::totalRrepSend=0;
int AODVUU::totalRrepRec=0;
int AODVUU::totalRrepAckSend=0;
int AODVUU::totalRrepAckRec=0;
int AODVUU::totalRerrSend=0;
int AODVUU::totalRerrRec=0;
#endif

void NS_CLASS initialize(int stage)
{
	list_t *lista_ptr;
	/*
	   Enable usage of some of the configuration variables from Tcl.

	   Note: Do NOT change the values of these variables in the constructor
	   after binding them! The desired default values should be set in
	   ~ns/tcl/lib/ns-default.tcl instead.
	 */
	if (stage==4)
	{
#ifndef AODV_GLOBAL_STATISTISTIC
		iswrite = false;
		totalSend=0;
		totalRreqSend=0;
		totalRreqRec=0;
		totalRrepSend=0;
		totalRrepRec=0;
		totalRrepAckSend=0;
		totalRrepAckRec=0;
		totalRerrSend=0;
		totalRerrRec=0;
#endif
		log_to_file = 0;
		hello_jittering = 0;
		optimized_hellos = 0;
		expanding_ring_search = 0;
		local_repair = 0;
		debug=0;
		rreq_gratuitous =0;

		//sendMessageEvent = new cMessage();

		if ((bool)par("log_to_file"))
			log_to_file = 1;

		if ((bool) par("hello_jittering"))
			hello_jittering = 1;

		if ((bool)par("optimized_hellos"))
			optimized_hellos  = 1;

		if ((bool)par("expanding_ring_search"))
			expanding_ring_search = 1;

		if ((bool) par("local_repair"))
			local_repair = 1;

		if ((bool)par("rreq_gratuitous"))
			rreq_gratuitous = 1;

		if ((bool)par("debug"))
			debug = 1;

		useIndex = par("UseIndex");
		unidir_hack = (int) par("unidir_hack");

		receive_n_hellos	= (int) par("receive_n_hellos");
		wait_on_reboot = (int) par ("wait_on_reboot");
		rt_log_interval = (int) par("rt_log_interval");	// Note: in milliseconds!
		ratelimit = (int) par("ratelimit");
		llfeedback = 0;
		if (par("llfeedback"))
			llfeedback = 1;
		internet_gw_mode = (int) par("internet_gw_mode");
		gateWayAddress = new IPAddress(par("internet_gw_address").stringValue());

		if (llfeedback)
		{
			active_route_timeout = ACTIVE_ROUTE_TIMEOUT_LLF;
			ttl_start = TTL_START_LLF;
			delete_period =  DELETE_PERIOD_LLF;
		}
		else
		{
			active_route_timeout = (int) par("active_timeout");// ACTIVE_ROUTE_TIMEOUT_HELLO;
			ttl_start = TTL_START_HELLO;
			delete_period = DELETE_PERIOD_HELLO;
		}

	/* Initialize common manet routing protocol structures */
		registerRoutingModule();
		if (llfeedback)
			linkLayerFeeback();

	/* From main.c */
		progname = strdup("AODV-UU");
		/* From debug.c */
	/* Note: log_nmsgs was never used anywhere */
		log_nmsgs = 0;
		log_rt_fd = -1;
#ifndef  _WIN32

		if (debug && !log_file_fd_init)
		{
			log_file_fd = -1;
			openlog("aodv-uu ",0,LOG_USER);
			log_init();
			log_file_fd_init=true;
		}
#else
		debug = 0;
#endif
  /* Set host parameters */
		memset(&this_host, 0, sizeof(struct host_info));
		memset(dev_indices, 0, sizeof(unsigned int) * MAX_NR_INTERFACES);
		this_host.seqno = 1;
		this_host.rreq_id = 0;
		this_host.nif = 1;


		for (int i = 0; i < MAX_NR_INTERFACES; i++)
			DEV_NR(i).enabled=0;

		for (int i = 0; i <getNumInterfaces(); i++)
		{
			DEV_NR(i).ifindex = i;
			dev_indices[i] = i;
			strcpy(DEV_NR(i).ifname, getInterfaceEntry(i)->getName());
			if (!isInMacLayer())
			{
				DEV_NR(i).netmask.s_addr =
					getInterfaceEntry(i)->ipv4Data()->getIPAddress().getNetworkMask().getInt();
				DEV_NR(i).ipaddr.s_addr =
					getInterfaceEntry(i)->ipv4Data()->getIPAddress().getInt();
			}
			else
			{
				DEV_NR(i).netmask.s_addr = MACAddress::BROADCAST_ADDRESS;
				DEV_NR(i).ipaddr.s_addr =getInterfaceEntry(i)->getMacAddress();

			}
		}
	/* Set network interface parameters */
		for (int i=0;i < getNumWlanInterfaces();i++)
		{
			DEV_NR(getWlanInterfaceIndex(i)).enabled = 1;
			DEV_NR(getWlanInterfaceIndex(i)).sock = -1;
			DEV_NR(getWlanInterfaceIndex(i)).broadcast.s_addr = AODV_BROADCAST;
		}

		NS_DEV_NR = getWlanInterfaceIndexByAddress();
		NS_IFINDEX = getWlanInterfaceIndexByAddress();


		lista_ptr=&rreq_records;
		INIT_LIST_HEAD(&rreq_records);
		lista_ptr=&rreq_blacklist;
		INIT_LIST_HEAD(&rreq_blacklist);
		lista_ptr=&seekhead;
		INIT_LIST_HEAD(&seekhead);
		lista_ptr=&TQ;
		INIT_LIST_HEAD(&TQ);

		/* Initialize data structures */
		worb_timer.data = NULL;
		worb_timer.used = 0;
		hello_timer.data = NULL;
		hello_timer.used = 0;
		rt_log_timer.data = NULL;
		rt_log_timer.used = 0;


		strcpy(nodeName,getParentModule()->getParentModule()->getFullName());
		aodv_socket_init();
		rt_table_init();
		packet_queue_init();
		startAODVUUAgent();

		is_init=true;
		// Initialize the timer
		scheduleNextEvent();
		ev << "Aodv active"<< "\n";
	}
}

/* Destructor for the AODV-UU routing agent */
NS_CLASS ~ AODVUU()
{

	list_t *tmp = NULL, *pos = NULL;
    	for (int i = 0; i < RT_TABLESIZE; i++) {
		list_foreach_safe(pos, tmp, &rt_tbl.tbl[i]) {
			rt_table_t *rt = (rt_table_t *) pos;
			list_detach(&rt->l);
			precursor_list_destroy(rt);
			free(rt);
		}
	}

	while (!list_empty(&rreq_records))
	{
		pos = list_first(&rreq_records);
		list_detach(pos);
		if (pos) free(pos);
	}

	while (!list_empty(&rreq_blacklist))
	{
		pos = list_first(&rreq_blacklist);
		list_detach(pos);
		if (pos) free(pos);
	}

	while (!list_empty(&seekhead))
	{
		pos = list_first(&seekhead);
		list_detach(pos);
		if (pos) free(pos);
	}
	packet_queue_destroy();
	cancelAndDelete(sendMessageEvent);
	log_cleanup();
	delete gateWayAddress;
}

/*
  Moves pending packets with a certain next hop from the interface
  queue to the packet buffer or simply drops it.
*/

/* Called for packets whose delivery fails at the link layer */
void NS_CLASS packetFailed(IPDatagram *dgram)
{
	rt_table_t *rt_next_hop, *rt;
	struct in_addr dest_addr, src_addr, next_hop;

	src_addr.s_addr = dgram->getSrcAddress().getInt();
	dest_addr.s_addr = dgram->getDestAddress().getInt();


	DEBUG(LOG_DEBUG, 0, "Got failure callback");
	/* We don't care about link failures for broadcast or non-data packets */
	if (dgram->getDestAddress().getInt() == IP_BROADCAST ||
		dgram->getDestAddress().getInt() == AODV_BROADCAST) {
		DEBUG(LOG_DEBUG, 0, "Ignoring callback");
		scheduleNextEvent();
		return;
	}


	DEBUG(LOG_DEBUG, 0, "LINK FAILURE for next_hop=%s dest=%s uid=%d",ip_to_str(next_hop), ip_to_str(dest_addr), ch->uid());

	if (seek_list_find(dest_addr)) {
		DEBUG(LOG_DEBUG, 0, "Ongoing route discovery, buffering packet...");
		packet_queue_add((IPDatagram *)dgram->dup(), dest_addr);
		scheduleNextEvent();
		return;
	}


	rt = rt_table_find(dest_addr);

	if (!rt || rt->state == INVALID)
	{
		scheduleNextEvent();
		return;
	}
	next_hop.s_addr = rt->next_hop.s_addr;
	rt_next_hop = rt_table_find(next_hop);

	if (!rt_next_hop || rt_next_hop->state == INVALID)
	{
		scheduleNextEvent();
		return;
	}

	/* Do local repair? */
	if (local_repair && rt->hcnt <= MAX_REPAIR_TTL)
	/* && ch->num_forwards() > rt->hcnt */
	 {
		/* Buffer the current packet */
		packet_queue_add((IPDatagram *) dgram->dup(), dest_addr);

	// In omnet++ it's not possible to access to the mac queue
	//	/* Buffer pending packets from interface queue */
	//	interfaceQueue((nsaddr_t) next_hop.s_addr, IFQ_BUFFER);
	//	/* Mark the route to be repaired */
		rt_next_hop->flags |= RT_REPAIR;
		neighbor_link_break(rt_next_hop);
		rreq_local_repair(rt, src_addr, NULL);
	}
	else
	{
	/* No local repair - just force timeout of link and drop packets */
		neighbor_link_break(rt_next_hop);
// In omnet++ it's not possible to access to the mac queue
//	interfaceQueue((nsaddr_t) next_hop.s_addr, IFQ_DROP);
	}
	scheduleNextEvent();
}


/* Entry-level packet reception */
void NS_CLASS handleMessage (cMessage *msg)
{
	AODV_msg *aodvMsg=NULL;
	IPDatagram * ipDgram=NULL;
	UDPPacket * udpPacket=NULL;

	cMessage *msg_aux;
	struct in_addr src_addr;
	struct in_addr dest_addr;

	if (is_init==false)
		opp_error ("Aodv has not been initialized ");
	if (msg==sendMessageEvent)
	{
	// timer event
		scheduleNextEvent();
		return;
	}
	/* Handle packet depending on type */
	if (dynamic_cast<ControlManetRouting *>(msg)){
		ControlManetRouting * control =  check_and_cast <ControlManetRouting *> (msg);
		if (control->getOptionCode()== MANET_ROUTE_NOROUTE)
		{
			ipDgram = (IPDatagram*) control->decapsulate();
			cPolymorphic * ctrl = ipDgram->removeControlInfo();
			unsigned int ifindex = NS_IFINDEX;	/* Always use ns interface */
			if (ctrl)
			{
				if (dynamic_cast<Ieee802Ctrl*> (ctrl))
				{
					Ieee802Ctrl *ieeectrl = dynamic_cast<Ieee802Ctrl*> (ctrl);
					Uint128 address = ieeectrl->getDest();
					int index = getWlanInterfaceIndexByAddress(address);
					if (index!=-1)
						ifindex = index;
				}
				delete ctrl;
			}


			EV << "Aodv rec datagram  " << ipDgram->getName() << " with dest=" << ipDgram->getDestAddress().str() << "\n";
			processPacket(ipDgram,ifindex);   // Data path
		}
		else if (control->getOptionCode()== MANET_ROUTE_UPDATE)
		{
 			src_addr.s_addr = control->getSrcAddress();
			dest_addr.s_addr = control->getDestAddress();
			rt_table_t * fwd_rt = rt_table_find(dest_addr);
			rt_table_t * rev_rt = rt_table_find(src_addr);
			rt_table_update_route_timeouts(fwd_rt, rev_rt);
		}
		delete msg;
		scheduleNextEvent();
		return;
	}
	else if (dynamic_cast<UDPPacket *>(msg) || dynamic_cast<AODV_msg  *>(msg))
	{
		udpPacket = NULL;
		if (!isInMacLayer())
		{
			udpPacket = check_and_cast<UDPPacket*>(msg);
			if (udpPacket->getDestinationPort()!= 654)
			{
				delete  msg;
				scheduleNextEvent();
				return;
			}
			msg_aux  = udpPacket->decapsulate();
		}
		else
			msg_aux = msg;

		if (dynamic_cast<AODV_msg  *>(msg_aux))
		{
			aodvMsg = check_and_cast  <AODV_msg *>(msg_aux);
			if (!isInMacLayer())
			{
				IPControlInfo *controlInfo = check_and_cast<IPControlInfo*>(udpPacket->removeControlInfo());
				src_addr.s_addr = controlInfo->getSrcAddr().getInt();
				aodvMsg->setControlInfo(controlInfo);
			}
			else
			{
				Ieee802Ctrl *controlInfo = check_and_cast<Ieee802Ctrl*>(udpPacket->getControlInfo());
				src_addr.s_addr = controlInfo->getSrc();
			}
		}
		else
		{
			if (udpPacket)
				delete udpPacket;
			delete msg_aux;
			scheduleNextEvent();
			return;

		}

		if (udpPacket)
			delete udpPacket;
	}
	else
	{
		delete msg;
		scheduleNextEvent();
		return;
	}
	/* Detect routing loops */
	if (isLocalAddress(src_addr.s_addr))
	{
		delete aodvMsg;
		aodvMsg=NULL;
		scheduleNextEvent();
		return;
	}
	recvAODVUUPacket(aodvMsg);
	scheduleNextEvent();
}
/*
	case PT_ENCAPSULATED:
	// Decapsulate...
	if (internet_gw_mode) {
		rt_table_t *rev_rt, *next_hop_rt;
		 rev_rt = rt_table_find(saddr);

		 if (rev_rt && rev_rt->state == VALID) {
		 rt_table_update_timeout(rev_rt, ACTIVE_ROUTE_TIMEOUT);

		 next_hop_rt = rt_table_find(rev_rt->next_hop);

		 if (next_hop_rt && next_hop_rt->state == VALID &&
			 rev_rt && next_hop_rt->dest_addr.s_addr != rev_rt->dest_addr.s_addr)
			 rt_table_update_timeout(next_hop_rt, ACTIVE_ROUTE_TIMEOUT);
		 }
		 p = pkt_decapsulate(p);

		 target_->recv(p, (Handler *)0);
		 break;
	}

	processPacket(p);   // Data path
	}
*/



/* Starts the AODV-UU routing agent */
int NS_CLASS startAODVUUAgent()
{

	/* Set up the wait-on-reboot timer */
	if (wait_on_reboot) {
		timer_init(&worb_timer, &NS_CLASS wait_on_reboot_timeout, &wait_on_reboot);
		timer_set_timeout(&worb_timer, DELETE_PERIOD);
		DEBUG(LOG_NOTICE, 0, "In wait on reboot for %d milliseconds.",DELETE_PERIOD);
	}
	/* Schedule the first HELLO */
	if (!llfeedback && !optimized_hellos)
		hello_start();

	/* Initialize routing table logging */
	if (rt_log_interval)
		log_rt_table_init();

	/* Initialization complete */
	initialized = 1;

	DEBUG(LOG_DEBUG, 0, "Routing agent with IP = %s : %d started.",
		  ip_to_str(DEV_NR(NS_DEV_NR).ipaddr), DEV_NR(NS_DEV_NR).ipaddr);

	DEBUG(LOG_DEBUG, 0, "Settings:");
	DEBUG(LOG_DEBUG, 0, "unidir_hack %s", unidir_hack ? "ON" : "OFF");
	DEBUG(LOG_DEBUG, 0, "rreq_gratuitous %s", rreq_gratuitous ? "ON" : "OFF");
	DEBUG(LOG_DEBUG, 0, "expanding_ring_search %s", expanding_ring_search ? "ON" : "OFF");
	DEBUG(LOG_DEBUG, 0, "local_repair %s", local_repair ? "ON" : "OFF");
	DEBUG(LOG_DEBUG, 0, "receive_n_hellos %s", receive_n_hellos ? "ON" : "OFF");
	DEBUG(LOG_DEBUG, 0, "hello_jittering %s", hello_jittering ? "ON" : "OFF");
	DEBUG(LOG_DEBUG, 0, "wait_on_reboot %s", wait_on_reboot ? "ON" : "OFF");
	DEBUG(LOG_DEBUG, 0, "optimized_hellos %s", optimized_hellos ? "ON" : "OFF");
	DEBUG(LOG_DEBUG, 0, "ratelimit %s", ratelimit ? "ON" : "OFF");
	DEBUG(LOG_DEBUG, 0, "llfeedback %s", llfeedback ? "ON" : "OFF");
	DEBUG(LOG_DEBUG, 0, "internet_gw_mode %s", internet_gw_mode ? "ON" : "OFF");
	DEBUG(LOG_DEBUG, 0, "ACTIVE_ROUTE_TIMEOUT=%d", ACTIVE_ROUTE_TIMEOUT);
	DEBUG(LOG_DEBUG, 0, "TTL_START=%d", TTL_START);
	DEBUG(LOG_DEBUG, 0, "DELETE_PERIOD=%d", DELETE_PERIOD);

	/* Schedule the first timeout */
	scheduleNextEvent();
	return 0;

}



// for use with gateway in the future
IPDatagram * NS_CLASS pkt_encapsulate(IPDatagram *p, IPAddress gateway)
{
	IPDatagram *datagram = new IPDatagram(p->getName());
	datagram->setByteLength(IP_HEADER_BYTES);
	datagram->encapsulate(p);

	// set source and destination address
	datagram->setDestAddress(gateway);

	IPAddress src = p->getSrcAddress();

	// when source address was given, use it; otherwise it'll get the address
	// of the outgoing interface after routing
	// set other fields
	datagram->setDiffServCodePoint(p->getDiffServCodePoint());
	datagram->setIdentification(p->getIdentification());
	datagram->setMoreFragments(false);
	datagram->setDontFragment (p->getDontFragment());
	datagram->setFragmentOffset(0);
	datagram->setTimeToLive(
		   p->getTimeToLive() > 0 ?
		   p->getTimeToLive() :
		   0);

	datagram->setTransportProtocol(IP_PROT_IP);
	return datagram;
}



IPDatagram *NS_CLASS pkt_decapsulate(IPDatagram *p)
{

	if (p->getTransportProtocol() == IP_PROT_IP) {
		IPDatagram *datagram = check_and_cast  <IPDatagram *>(p->decapsulate());
		datagram->setTimeToLive(p->getTimeToLive());
		delete p;
		return datagram;
	}
	return NULL;
}



/*
  Reschedules the timer queue timer to go off at the time of the
  earliest event (so that the timer queue will be investigated then).
  Should be called whenever something might have changed the timer queue.
*/
void NS_CLASS scheduleNextEvent()
{
	struct timeval *timeout;
	double delay;
	simtime_t timer;
	timeout = timer_age_queue();
	if (timeout)
	{
		delay  = (double)(((double)timeout->tv_usec/(double)1000000.0) +(double)timeout->tv_sec);
		timer = simTime()+delay;
		if (sendMessageEvent->isScheduled())
		{
			if (timer < sendMessageEvent->getArrivalTime()) {
				cancelEvent(sendMessageEvent);
				scheduleAt(timer, sendMessageEvent);
			}
		}
		else
		{
			scheduleAt(timer, sendMessageEvent);
		}
	}
}




/*
  Replacement for if_indextoname(), used in routing table logging.
*/
const char *NS_CLASS if_indextoname(int ifindex, char *ifname)
{
 	InterfaceEntry *   ie;
	assert(ifindex >= 0);
	ie = getInterfaceEntry(ifindex);
	return ie->getName();
}



void NS_CLASS recvAODVUUPacket(cMessage * msg)
{
	struct in_addr src, dst;
	int ttl;
	int interfaceId;

	AODV_msg *aodv_msg = check_and_cast<AODV_msg *> (msg);
	int len = aodv_msg->getByteLength();
	int ifIndex=NS_IFINDEX;

	if (!isInMacLayer())
	{
		IPControlInfo *ctrl = check_and_cast<IPControlInfo *>(msg->getControlInfo());
		IPvXAddress srcAddr = ctrl->getSrcAddr();
		IPvXAddress destAddr = ctrl->getDestAddr();
		ttl =  ctrl->getTimeToLive();
		src.s_addr = srcAddr.get4().getInt();
		dst.s_addr =  destAddr.get4().getInt();
		interfaceId = ctrl->getInterfaceId();

	}
	else
	{
		Ieee802Ctrl *ctrl = check_and_cast<Ieee802Ctrl *>(msg->getControlInfo());
		ttl = aodv_msg->ttl;
		src.s_addr = ctrl->getSrc();
		dst.s_addr =  ctrl->getDest();
	}

	InterfaceEntry *   ie;
	for (int i = 0; i < getNumWlanInterfaces(); i++)
	{
		ie = getWlanInterfaceEntry(i);
		if (!isInMacLayer())
		{
			IPv4InterfaceData *ipv4data = ie->ipv4Data();
			if (interfaceId ==ie->getInterfaceId())
				ifIndex=getWlanInterfaceIndex(i);

			if (ipv4data->getIPAddress().getInt()== src.s_addr)
			{
				delete   aodv_msg;
				return;
			}
		}
		else
		{
			Uint128 add = ie->getMacAddress();
			if (add== src.s_addr)
			{
				delete   aodv_msg;
				return;
			}
		}
	}
	aodv_socket_process_packet(aodv_msg, len, src, dst, ttl, ifIndex);
	delete   aodv_msg;
}


void NS_CLASS processPacket(IPDatagram * p,unsigned int ifindex)
{
	rt_table_t *fwd_rt, *rev_rt;
	struct in_addr dest_addr, src_addr;
	u_int8_t rreq_flags = 0;
	struct ip_data *ipd = NULL;


	fwd_rt = NULL;		/* For broadcast we provide no next hop */
	ipd = NULL;			/* No ICMP messaging */

	bool isLocal=true;

	IPAddressVector phops;

	src_addr.s_addr = p->getSrcAddress().getInt();
	dest_addr.s_addr = p->getDestAddress().getInt();

	InterfaceEntry *   ie;

	if(!p->getSrcAddress().isUnspecified())
	{
	       isLocal = isLocalAddress (p->getSrcAddress());
	}

	ie = getInterfaceEntry (ifindex);
	if (p->getTransportProtocol()==IP_PROT_TCP)
		rreq_flags |= RREQ_GRATUITOUS;

	/* If this is a TCP packet and we don't have a route, we should
	   set the gratuituos flag in the RREQ. */
	phops = ie->ipv4Data()->getMulticastGroups();
	IPAddress mcastAdd;
	bool isMcast=false;
	for (unsigned int  i=0;i<phops.size();i++){
		mcastAdd = phops[i];
		if (dest_addr.s_addr == mcastAdd.getInt())
			isMcast=true;
	}


	/* If the packet is not interesting we just let it go through... */
	if (dest_addr.s_addr == AODV_BROADCAST ||isMcast) {
		send(p,"to_ip");
		return;
	}
	/* Find the entry of the neighboring node and the destination  (if any). */
	rev_rt = rt_table_find(src_addr);
	fwd_rt = rt_table_find(dest_addr);

#ifdef CONFIG_GATEWAY
	/* Check if we have a route and it is an Internet destination (Should be
	 * encapsulated and routed through the gateway). */
	if (fwd_rt && (fwd_rt->state == VALID) &&
	(fwd_rt->flags & RT_INET_DEST)) {
	/* The destination should be relayed through the IG */

		rt_table_update_timeout(fwd_rt, ACTIVE_ROUTE_TIMEOUT);

		p = pkt_encapsulate(p, *gateWayAddress);

		if (p == NULL) {
			DEBUG(LOG_ERR, 0, "IP Encapsulation failed!");
		   return;
		}
	/* Update pointers to headers */
		dest_addr.s_addr = gateWayAddress->getInt();
		fwd_rt = rt_table_find(dest_addr);
	}
#endif /* CONFIG_GATEWAY */

	/* UPDATE TIMERS on active forward and reverse routes...  */
	rt_table_update_route_timeouts(fwd_rt, rev_rt);

	/* OK, the timeouts have been updated. Now see if either: 1. The
	   packet is for this node -> ACCEPT. 2. The packet is not for this
	   node -> Send RERR (someone want's this node to forward packets
	   although there is no route) or Send RREQ. */

	if (!fwd_rt || fwd_rt->state == INVALID ||
		(fwd_rt->hcnt == 1 && (fwd_rt->flags & RT_UNIDIR))) {

	/* Check if the route is marked for repair or is INVALID. In
	 * that case, do a route discovery. */
		struct in_addr rerr_dest;

		if (isLocal)
			goto route_discovery;

		if (fwd_rt && (fwd_rt->flags & RT_REPAIR))
			goto route_discovery;



		RERR *rerr;
		DEBUG(LOG_DEBUG, 0,
		  "No route, src=%s dest=%s prev_hop=%s - DROPPING!",
		  ip_to_str(src_addr), ip_to_str(dest_addr));
		if (fwd_rt) {
			rerr = rerr_create(0, fwd_rt->dest_addr,fwd_rt->dest_seqno);
			rt_table_update_timeout(fwd_rt, DELETE_PERIOD);
		}
		else
			rerr = rerr_create(0, dest_addr, 0);
		DEBUG(LOG_DEBUG, 0, "Sending RERR to prev hop %s for unknown dest %s",
						ip_to_str(src_addr), ip_to_str(dest_addr));

	    /* Unicast the RERR to the source of the data transmission
	     * if possible, otherwise we broadcast it. */

		if (rev_rt && rev_rt->state == VALID)
			rerr_dest = rev_rt->next_hop;
		else
			rerr_dest.s_addr = AODV_BROADCAST;

		aodv_socket_send((AODV_msg *) rerr, rerr_dest,RERR_CALC_SIZE(rerr),
					1, &DEV_IFINDEX(ifindex));
		if (wait_on_reboot) {
			DEBUG(LOG_DEBUG, 0, "Wait on reboot timer reset.");
			timer_set_timeout(&worb_timer, DELETE_PERIOD);
		}

		drop (p);
	/* DEBUG(LOG_DEBUG, 0, "Dropping pkt uid=%d", ch->uid()); */
	//	icmpAccess.get()->sendErrorMessage(p, ICMP_DESTINATION_UNREACHABLE, 0);
		return;

	route_discovery:
	/* Buffer packets... Packets are queued by the ip_queue.o
	   module already. We only need to save the handle id, and
	   return the proper verdict when we know what to do... */

		packet_queue_add(p, dest_addr);

		if (fwd_rt && (fwd_rt->flags & RT_REPAIR))
			rreq_local_repair(fwd_rt, src_addr, ipd);
		else
			rreq_route_discovery(dest_addr, rreq_flags, ipd);

		return;

	}
	else {
	/* DEBUG(LOG_DEBUG, 0, "Sending pkt uid=%d", ch->uid()); */
		send(p,"to_ip");
	/* When forwarding data, make sure we are sending HELLO messages */
		gettimeofday(&this_host.fwd_time, NULL);

		if (!llfeedback && optimized_hellos)
		    hello_start();
	}
}


struct dev_info NS_CLASS dev_ifindex (int ifindex)
{
     int index = ifindex2devindex(ifindex);
     return  (this_host.devs[index]);
}

struct dev_info NS_CLASS dev_nr(int n)
{
    return (this_host.devs[n]);
}

int NS_CLASS ifindex2devindex(unsigned int ifindex)
{
	int i;
	for (i = 0; i < this_host.nif; i++)
		if (dev_indices[i] == ifindex)
			return i;
	return -1;
}


void NS_CLASS processLinkBreak(const cPolymorphic *details)
{
	IPDatagram	*dgram=NULL;
	if (llfeedback)
	{
		if (dynamic_cast<IPDatagram *>(const_cast<cPolymorphic*> (details)))
		{
			dgram = check_and_cast<IPDatagram *>(details);
			packetFailed(dgram);
			return;
		}
	}
}


void NS_CLASS finish()
{

	simtime_t t = simTime();
	if (t==0)
		return;
	if (iswrite)
		return;
	iswrite=true;
	recordScalar("simulated time", t);
	recordScalar("Aodv totalSend ", totalSend);
	recordScalar("rreq send", totalRreqSend);
	recordScalar("rreq rec", totalRreqRec);
	recordScalar("rrep send", totalRrepSend);
	recordScalar("rrep rec", totalRrepRec);
	recordScalar("rrep ack send", totalRrepAckSend);
	recordScalar("rrep ack rec", totalRrepAckRec);
	recordScalar("rerr send", totalRerrSend);
	recordScalar("rerr rec", totalRerrRec);
}


uint32_t NS_CLASS getRoute(const Uint128 &dest,Uint128 add[])
{
	return 0;
}


bool  NS_CLASS getNextHop(const Uint128 &dest,Uint128 &add, int &iface)
{
	struct in_addr destAddr;
	destAddr.s_addr = dest;
	rt_table_t * fwd_rt = rt_table_find(destAddr);
	if (!fwd_rt)
		return false;
	if (!fwd_rt->state != VALID)
		return false;
	add = fwd_rt->next_hop.s_addr;
	InterfaceEntry * ie = getInterfaceEntry (fwd_rt->ifindex);
	iface = ie->getInterfaceId();
	return true;
}

bool NS_CLASS isProactive()
{
	return false;
}

void NS_CLASS setRefreshRoute(const Uint128 &src,const Uint128 &dest,const Uint128 &gtw,const Uint128& prev)
{
		struct in_addr dest_addr, src_addr, next_hop;
	 	src_addr.s_addr = src;
		dest_addr.s_addr = dest;
		next_hop.s_addr = gtw;
		rt_table_t * fwd_rt = rt_table_find(dest_addr);
		rt_table_t * rev_rt = rt_table_find(src_addr);
		if (!rev_rt)
		{
		// Gratuitous Return Path

			struct in_addr node_addr;
			struct in_addr  ip_src;
			node_addr.s_addr = src;
			ip_src.s_addr = prev;
			rt_table_insert(node_addr, ip_src,0,0, ACTIVE_ROUTE_TIMEOUT, VALID, RT_INET_DEST,NS_DEV_NR);
		}
		if (gtw!=0)
		{
			if ((fwd_rt &&(fwd_rt->next_hop.s_addr==gtw))|| (rev_rt &&(rev_rt->next_hop.s_addr==gtw)))
				rt_table_update_route_timeouts(fwd_rt, rev_rt);
		}
		else
			rt_table_update_route_timeouts(fwd_rt, rev_rt);

}


bool NS_CLASS isOurType(cPacket * msg)
{
	AODV_msg *re = dynamic_cast <AODV_msg *>(msg);
	if (re)
		return true;
	return false;
}

bool NS_CLASS getDestAddress(cPacket *msg,Uint128 &dest)
{
	RREQ *rreq = dynamic_cast <RREQ *>(msg);
	if (!rreq)
		return false;
	dest = rreq->dest_addr;
	return true;

}
