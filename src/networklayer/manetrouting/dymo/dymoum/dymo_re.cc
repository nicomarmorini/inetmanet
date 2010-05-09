/***************************************************************************
 *   Copyright (C) 2005 by Francisco J. Ros                                *
 *   fjrm@dif.um.es                                                        *
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
#define NS_PORT
#define OMNETPP

#ifdef NS_PORT
#ifndef OMNETPP
#include "ns/dymo_um.h"
#else
#include "../dymo_um_omnet.h"
#include "MeshControlInfo_m.h"
#endif
#else
#include <string.h>

#include "dymo_re.h"
#include "dymo_generic.h"
#include "dymo_socket.h"
#include "icmp_socket.h"
#include "dymo_timeout.h"
#include "rtable.h"
#include "pending_rreq.h"
#include "blacklist.h"

extern int no_path_acc, s_bit;
#endif	/* NS_PORT */

RE *NS_CLASS re_create_rreq(struct in_addr target_addr,
		u_int32_t target_seqnum,
		struct in_addr re_node_addr,
		u_int32_t re_node_seqnum,
		u_int8_t prefix, u_int8_t g,
		u_int8_t ttl, u_int8_t thopcnt)
{
	RE *re;
#ifdef OMNETPP
	re		= new RE("RE_DymoMsg_RREQ");
	re->newBocks(1);
#else
	re		= (RE *) dymo_socket_new_element();
#endif
	re->m		= 0;
	re->h		= 0;
	re->type	= DYMO_RE_TYPE;
	re->a		= 1;
	re->s		= 0;
	re->i		= 0;
	re->res1	= 0;
	re->res2	= 0;
	re->ttl		= ttl;
	re->len		= RE_BASIC_SIZE + RE_BLOCK_SIZE;
	re->thopcnt	= thopcnt;
	re->target_addr		= target_addr.s_addr;
	re->target_seqnum	= htonl(target_seqnum);

	re->re_blocks[0].g		= g;
	re->re_blocks[0].prefix		= prefix;
	re->re_blocks[0].res		= 0;
	re->re_blocks[0].re_hopcnt	= 0;
	re->re_blocks[0].re_node_addr	= re_node_addr.s_addr;
	re->re_blocks[0].re_node_seqnum	= htonl(re_node_seqnum);

	return re;
}

RE *NS_CLASS re_create_rrep(struct in_addr target_addr,
		u_int32_t target_seqnum,
		struct in_addr re_node_addr,
		u_int32_t re_node_seqnum,
		u_int8_t prefix, u_int8_t g,
		u_int8_t ttl, u_int8_t thopcnt)
{
	RE *re;

#ifdef OMNETPP
	re		= new RE("RE_DymoMsg_RREP");
	re->newBocks(1);
#else
	re		= (RE *) dymo_socket_new_element();
#endif
	re->m		= 0;
	re->h		= 0;
	re->type	= DYMO_RE_TYPE;
	re->a		= 0;
	re->s		= s_bit;
	re->i		= 0;
	re->res1	= 0;
	re->res2	= 0;
	re->ttl		= ttl;
	re->len		= RE_BASIC_SIZE + RE_BLOCK_SIZE;
	re->thopcnt	= thopcnt;
	re->target_addr		=  target_addr.s_addr;
	re->target_seqnum	= htonl(target_seqnum);

	re->re_blocks[0].g		= g;
	re->re_blocks[0].prefix		= prefix;
	re->re_blocks[0].res		= 0;
	re->re_blocks[0].re_hopcnt	= 0;
	re->re_blocks[0].re_node_addr	=  re_node_addr.s_addr;
	re->re_blocks[0].re_node_seqnum	= htonl(re_node_seqnum);

	return re;
}

void NS_CLASS re_process(RE *re,struct in_addr ip_src, u_int32_t ifindex)
{
	struct in_addr node_addr;
	rtable_entry_t *entry;
	int i;
	int mustAnswer;

#ifdef OMNETPP
	int num_blk_del;
#endif
	// Assure that there is a block at least
	if (re_numblocks(re) <= 0)
	{
		dlog(LOG_WARNING, 0, __FUNCTION__, "malformed RE received");
#ifdef OMNETPP
		delete re;
		re=NULL;
#endif
		return;
	}

	// Check if the message is a RREQ and the previous hop is blacklisted
	if (re->a && blacklist_find(ip_src))
	{
		dlog(LOG_DEBUG, 0, __FUNCTION__, "ignoring RREQ because "
			"previous hop (%s) is blacklisted", ip2str(ip_src.s_addr));
#ifdef OMNETPP
		delete re;
		re=NULL;
#endif
		return;
	}

	/*
	 * Add route to neighbor
	 *
	 * NOTE: this isn't in the spec. Motivation: suppose that path
	 * accumulation is disabled and the S-bit is active. Then, after
	 * receiving a RREP the node sends an ICMP ECHOREPLY message to
	 * the neighbor. If we don't create this route, it'd be needed
	 * a new route discovery. There are other (finer grain) possible
	 * solutions but this seems to be ok.
	 *
	 */
	entry = rtable_find(ip_src);
	if (re->s)
	{
		if (!entry)
			rtable_insert(
				ip_src,		// dest
				ip_src,		// nxt hop
				ifindex,	// iface
				0,		// seqnum
				0,		// prefix
				1,		// hop count
				0);		// is gw
		icmp_reply_send(ip_src, &DEV_IFINDEX(ifindex));
	}

	// Process blocks
#ifdef OMNETPP
	num_blk_del = 0;
#endif
	for (i = 0; i < re_numblocks(re); i++)
	{
		node_addr.s_addr	= re->re_blocks[i].re_node_addr;
		entry			= rtable_find(node_addr);
#if 1
		if (entry)
		{
			struct re_block *block;
			u_int8_t is_rreq;
			struct in_addr dest_addr;
			u_int32_t seqnum;
			int rb_state;

			block = &re->re_blocks[i];
			if (!block)
			{
				dest_addr.s_addr	= block->re_node_addr;
				seqnum			= block->re_node_seqnum;
				if (isInMacLayer())
				{
					if (isLocalAddress (block->re_node_addr))
						rb_state = RB_SELF_GEN;
					else if (!entry)
						rb_state = RB_FRESH;
					else if (seqnum > entry->rt_seqnum)
						rb_state = RB_FRESH;
					else if (seqnum == entry->rt_seqnum && block->re_hopcnt < entry->rt_hopcnt)
						rb_state = RB_FRESH;
					else if (seqnum ==0 &&  ((block->re_hopcnt < entry->rt_hopcnt)|| (entry->rt_hopcnt==0)))
						rb_state = RB_FRESH; // Assume Olsr update
					else
						rb_state = RB_STALE;
				}
				else
				{
					rb_state = re_info_type(block, entry, is_rreq);
				}
				if (rb_state==RB_FRESH)
					EV <<"Error <\n";
			}
		}
#endif
		if (re_process_block(&re->re_blocks[i], re->a, entry, ip_src, ifindex))
		{
			// stale information: drop packet if first block,
			// drop block otherwise
			if (i == 0)
			{
#ifdef OMNETPP
				delete re;
				re=NULL;
#endif
				return;
			}
			else {
				int n = re_numblocks(re) - i - 1;

				memmove(&re->re_blocks[i], &re->re_blocks[i+1],
					n * sizeof(struct re_block));
				memset(&re->re_blocks[i + n], 0,
					sizeof(struct re_block));
#ifdef OMNETPP
				num_blk_del++;
#endif
				re->len -= RE_BLOCK_SIZE;
				i--;
			}
		}
	}


	mustAnswer = 0;
#ifdef OMNETPP
	re->delBocks(num_blk_del);
	if (re->a)
		totalRreqRec++;
	else
		totalRrepRec++;
#endif
	mustAnswer = re_mustAnswer(re,ifindex);

#ifdef OMNETPP
	if (!mustAnswer && re->getControlInfo())
		delete re->removeControlInfo();
#endif
	if (mustAnswer){
		// If A-bit is set, a RE is sent back
		switch (mustAnswer){
		case 1:
			if (re->a)
			{
				struct in_addr target_addr;
				u_int32_t target_seqnum;
				node_addr.s_addr	= re->re_blocks[0].re_node_addr;
				target_addr.s_addr	= re->target_addr;
				target_seqnum		= ntohl(re->target_seqnum);
				if (!target_seqnum ||
						((int32_t) target_seqnum) - ((int32_t) this_host.seqnum) > 0 ||
						(target_seqnum == this_host.seqnum && re->thopcnt < re->re_blocks[0].re_hopcnt))
					INC_SEQNUM(this_host.seqnum);
				RE *rrep = re_create_rrep(
						node_addr,
						ntohl(re->re_blocks[0].re_node_seqnum),
						target_addr,
						this_host.seqnum,
						this_host.prefix,
						this_host.is_gw,
						NET_DIAMETER,
						re->re_blocks[0].re_hopcnt);
				re_send_rrep(rrep);
			}
			break;
		case 2:
		{
			struct in_addr target_addr;
			node_addr.s_addr	= re->re_blocks[0].re_node_addr;
			target_addr.s_addr	= re->target_addr;
			re_intermediate_rrep (node_addr, target_addr, entry,ifindex);
		}
		break;
		case 3:
		{
			re_answer (re,ifindex);
		}
		break;
		}
#ifdef OMNETPP
		if (!isInMacLayer())
		{
			cMessage *msg = re->decapsulate();
			if (msg)
				send(msg,"to_ip");
		}
		delete re;
		re=NULL;
#endif
	}
	// Otherwise the RE is considered to be forwarded
	else if (generic_postprocess((DYMO_element *) re))
	{
		if (!no_path_acc)
		{
			int n = re_numblocks(re);
#ifdef OMNETPP
			re->newBocks(1);
#endif

			INC_SEQNUM(this_host.seqnum);
			re->re_blocks[n].g		= this_host.is_gw;
			re->re_blocks[n].prefix		= this_host.prefix;
			re->re_blocks[n].res		= 0;
			re->re_blocks[n].re_hopcnt	= 0;
			re->re_blocks[n].re_node_seqnum	= htonl(this_host.seqnum);

			re->len += RE_BLOCK_SIZE;

			// If this is a RREQ
			if (re->a)
				re_forward_rreq_path_acc(re, n);
			// Else if this is a RREP
			else
			{
				re->re_blocks[n].re_node_addr	=
					 DEV_IFINDEX(ifindex).ipaddr.s_addr;
				re_forward_rrep_path_acc(re);
			}
		}
		else
			re_forward(re);
	}
#ifdef OMNETPP
	else
	{
		delete re;
		re=NULL;
	}
#endif

}

int NS_CLASS re_process_block(struct re_block *block, u_int8_t is_rreq,
	rtable_entry_t *entry,struct in_addr ip_src, u_int32_t ifindex)
{
	struct in_addr dest_addr;
	u_int32_t seqnum;
	int rb_state;

	if (!block)
		return -1;

	dest_addr.s_addr	= block->re_node_addr;
	seqnum			= ntohl(block->re_node_seqnum);

	// Increment block hop count
	block->re_hopcnt++;

#ifdef OMNETPP
	if (isInMacLayer())
	{
		if (isLocalAddress (block->re_node_addr))
			rb_state = RB_SELF_GEN;
		else if (!entry)
			rb_state = RB_FRESH;
		else if (seqnum > entry->rt_seqnum)
			rb_state = RB_FRESH;
		else if ((seqnum == entry->rt_seqnum) && (block->re_hopcnt < entry->rt_hopcnt))
			rb_state = RB_FRESH;
		else if ((seqnum ==0) && (block->re_hopcnt < entry->rt_hopcnt))
			rb_state = RB_FRESH; // Asume Olsr update
		else
			rb_state = RB_STALE;
	}
	else
	{
		rb_state = re_info_type(block, entry, is_rreq);
	}
#else
	rb_state = re_info_type(block, entry, is_rreq);
#endif
	if (rb_state != RB_FRESH)
	{
		dlog(LOG_DEBUG, 0, __FUNCTION__, "ignoring a %s RE block",
			(rb_state == RB_STALE ? "stale" : (rb_state == RB_LOOP_PRONE ?
			"loop-prone" : (rb_state == RB_INFERIOR ? "inferior" :
			"self-generated"))));
		return -1;
	}
	// Create/update a route towards RENodeAddress
	if (entry)
		rtable_update(
			entry,			// routing table entry
			dest_addr,		// dest
			ip_src,			// nxt hop
			ifindex,		// iface
			seqnum,			// seqnum
			block->prefix,		// prefix
			block->re_hopcnt,	// hop count
			block->g);		// is gw
	else
		rtable_insert(
			dest_addr,		// dest
			ip_src,			// nxt hop
			ifindex,		// iface
			seqnum,			// seqnum
			block->prefix,		// prefix
			block->re_hopcnt,	// hop count
			block->g);		// is gw

	return 0;
}

void NS_CLASS __re_send(RE *re)
{
	struct in_addr dest_addr;
	int i;

#ifdef OMNETPP
	cPacket * pkt = dynamic_cast<cPacket *> (re->getEncapsulatedMsg());

	if (pkt)
	{
		re->setByteLength(pkt->getByteLength());
	}
	else
		re->setByteLength(0);
#endif

	// If it is a RREQ
	if (re->a)
	{
		dest_addr.s_addr = DYMO_BROADCAST;

		// Queue the new RE

#ifdef OMNETPP
		int cont = numInterfacesActive;
		double delay = -1;

		if (par("EqualDelay"))
			delay = par("broadCastDelay");

		// Send RE over all enabled interfaces
		for (i = 0; i < DYMO_MAX_NR_INTERFACES; i++)
			if (DEV_NR(i).enabled)
			{
				if (cont>1)
					dymo_socket_queue((DYMO_element *) re->dup());
				else
					dymo_socket_queue((DYMO_element *) re);
				dymo_socket_send(dest_addr, &DEV_NR(i),delay);
				cont--;
			}
#else

		re = (RE *) dymo_socket_queue((DYMO_element *) re);

		// Send RE over all enabled interfaces
		for (i = 0; i < DYMO_MAX_NR_INTERFACES; i++)
			if (DEV_NR(i).enabled)
				dymo_socket_send(dest_addr, &DEV_NR(i));
#endif
	}
	// Else if RREP
	else
	{
		dest_addr.s_addr = re->target_addr;
		rtable_entry_t *entry = rtable_find(dest_addr);
		if (entry && entry->rt_state == RT_VALID)
		{
			dest_addr.s_addr = entry->rt_nxthop_addr.s_addr;

			// Queue the new RE
			re = (RE *) dymo_socket_queue((DYMO_element *) re);

			// Send RE over appropiate interface
			if (DEV_IFINDEX(entry->rt_ifindex).enabled)
				dymo_socket_send(dest_addr, &DEV_IFINDEX(entry->rt_ifindex));

			// Add next hop to the blacklist until we receive a
			// unicast message from it
			if (re->s)
			{
				blacklist_t *blacklist
					= blacklist_add(dest_addr);

				timer_init(&blacklist->timer, &NS_CLASS blacklist_timeout,
					blacklist);
				timer_set_timeout(&blacklist->timer, BLACKLIST_TIMEOUT);
				timer_add(&blacklist->timer);
			}
		}
		else
			delete re;
	}
	re=NULL;
}

void NS_CLASS re_send_rrep(RE *rrep)
{
	dlog(LOG_DEBUG, 0, __FUNCTION__, "sending RREP to %s",
		ip2str(rrep->target_addr));
	__re_send(rrep);
}

void NS_CLASS re_send_rreq(struct in_addr dest_addr, u_int32_t seqnum,
	u_int8_t thopcnt)
{
	int i;
	RE *rreq;
	struct in_addr bcast_addr;

	dlog(LOG_DEBUG, 0, __FUNCTION__, "sending RREQ to find %s",
		ip2str(dest_addr.s_addr));

	bcast_addr.s_addr = DYMO_BROADCAST;

	INC_SEQNUM(this_host.seqnum);
	for (i = 0; i < DYMO_MAX_NR_INTERFACES; i++)
	{
		if (DEV_NR(i).enabled)
		{
			rreq = re_create_rreq(dest_addr, seqnum,
				DEV_NR(i).ipaddr, this_host.seqnum,
				this_host.prefix, this_host.is_gw,
				NET_DIAMETER, thopcnt);
#ifdef OMNETPP
			if (attachPacket)
			{
				cPacket * pkt = get_packet_queue(dest_addr);
				if (pkt)
					rreq->encapsulate(pkt->dup());
			}
#endif
			dymo_socket_queue((DYMO_element *) rreq);
			dymo_socket_send(bcast_addr, &DEV_NR(i));
		}
	}
}

void NS_CLASS re_forward(RE *re)
{
	if (re->a)
		dlog(LOG_DEBUG, 0, __FUNCTION__, "forwarding RREQ to find %s",
			ip2str(re->target_addr));
	else
		dlog(LOG_DEBUG, 0, __FUNCTION__, "forwarding RREP to %s",
			ip2str(re->target_addr));
	__re_send(re);
}

void NS_CLASS re_forward_rreq_path_acc(RE *rreq, int blindex)
{
	int i;
	struct in_addr bcast_addr;

	dlog(LOG_DEBUG, 0, __FUNCTION__, "forwarding RREQ to find %s",
		ip2str(rreq->target_addr));

	bcast_addr.s_addr = DYMO_BROADCAST;

#ifdef OMNETPP

	if (numInterfacesActive==0)
	{
		delete rreq;
		rreq=NULL;
		return;
	}
	int numDup = numInterfacesActive-1;
		// Send RE over all enabled interfaces
	for (i = 0; i < DYMO_MAX_NR_INTERFACES; i++)
	{
		if (DEV_NR(i).enabled)
		{
			rreq->re_blocks[blindex].re_node_addr =
				DEV_NR(i).ipaddr.s_addr;
			if (numDup>0)
				dymo_socket_queue((DYMO_element *) rreq->dup());
			else
				dymo_socket_queue((DYMO_element *) rreq);
			numDup--;
			dymo_socket_send(bcast_addr, &DEV_NR(i));
		}
	}
	// delete rreq;
	rreq=NULL;
#else
	for (i = 0; i < DYMO_MAX_NR_INTERFACES; i++)
	{
		if (DEV_NR(i).enabled)
		{
			rreq->re_blocks[blindex].re_node_addr =
				(u_int32_t) DEV_NR(i).ipaddr.s_addr;

			dymo_socket_queue((DYMO_element *) rreq);
			dymo_socket_send(bcast_addr, &DEV_NR(i));
		}
	}
#endif
}

void NS_CLASS re_forward_rrep_path_acc(RE *rrep)
{
	dlog(LOG_DEBUG, 0, __FUNCTION__, "forwarding RREP to %s",
		ip2str(rrep->target_addr));
	__re_send(rrep);
}

void NS_CLASS route_discovery(struct in_addr dest_addr)
{
	u_int32_t	seqnum;
	u_int8_t	thopcnt;

	// If we are already doing a route discovery for dest_addr,
	// then simply return
	if (pending_rreq_find(dest_addr))
		return;

	// Get info from routing table (if there exists an entry)
	rtable_entry_t *rt_entry = rtable_find(dest_addr);
	if (rt_entry)
	{
		seqnum	= rt_entry->rt_seqnum;
		thopcnt	= rt_entry->rt_hopcnt;
	}
	else
	{
		seqnum	= 0;
		thopcnt	= 0;
	}

	if (RREQ_TRIES==1)
	{
		seqnum	= 0;
		thopcnt	= 0;
	}
	// Send a RREQ
	re_send_rreq(dest_addr, seqnum, thopcnt);

	// Record information for destination and set a timer
	pending_rreq_t *pend_rreq = pending_rreq_add(dest_addr, seqnum);
	timer_init(&pend_rreq->timer, &NS_CLASS route_discovery_timeout,
		pend_rreq);
	timer_set_timeout(&pend_rreq->timer, RREQ_WAIT_TIME);
	timer_add(&pend_rreq->timer);
}

void NS_CLASS re_intermediate_rrep (struct in_addr src_addr,struct in_addr dest_addr, rtable_entry_t *entry,int ifindex)
{
	/** create a new RREP and send it to given destination **/
#ifdef OMNETPP
	if (isInMacLayer())
		ev << "sending a reply to OrigNode " <<  src_addr.s_addr.getMACAddress() << endl;
	else
		ev << "sending a reply to OrigNode " <<  src_addr.s_addr.getIPAddress() << endl;
#endif
	rtable_entry_t *rev_rt	= rtable_find(src_addr);

	if (!rev_rt) error("no route to OrigNode found");

	// increment ownSeqNum.
	// TODO: The draft is unclear about when to increment ownSeqNum for intermediate DYMO router RREP creation
	INC_SEQNUM(this_host.seqnum);

	// create rrepToOrigNode

	int hopcnt=0;
	if (rev_rt->rt_hopcnt)
		hopcnt=rev_rt->rt_hopcnt;

	RE *rrep_src = re_create_rrep(
				src_addr,
				ntohl(rev_rt->rt_seqnum),
				dest_addr,
				entry->rt_seqnum,
				entry->rt_prefix,
				entry->rt_is_gw,
				NET_DIAMETER,
				hopcnt);
	if (entry->rt_hopcnt)
		rrep_src->re_blocks[0].re_hopcnt	= entry->rt_hopcnt;
	else
		rrep_src->re_blocks[0].re_hopcnt	= 0;

	if (!no_path_acc)
	{
#ifdef OMNETPP
			rrep_src->newBocks(1);
#endif
			rrep_src->re_blocks[1].g		= this_host.is_gw;
			rrep_src->re_blocks[1].prefix		= this_host.prefix;
			rrep_src->re_blocks[1].res		= 0;
			rrep_src->re_blocks[1].re_hopcnt	= 0;
			rrep_src->re_blocks[1].re_node_seqnum	= this_host.seqnum;
			rrep_src->re_blocks[1].re_node_addr	= DEV_NR(ifindex).ipaddr.s_addr;
			rrep_src->len += RE_BLOCK_SIZE;
	}
	re_send_rrep(rrep_src);


	if (entry->rt_hopcnt)
		hopcnt=entry->rt_hopcnt;
	else
		hopcnt=0;

	RE *rrep_dest = re_create_rrep(
		dest_addr,
		ntohl(entry->rt_seqnum),
		src_addr,
		rev_rt->rt_seqnum,
		rev_rt->rt_prefix,
		rev_rt->rt_is_gw,
		NET_DIAMETER,
		hopcnt);


		if (rev_rt->rt_hopcnt)
			rrep_dest->re_blocks[0].re_hopcnt	= rev_rt->rt_hopcnt;
		else
			rrep_dest->re_blocks[0].re_hopcnt	= 0;
		if (!no_path_acc)
		{
#ifdef OMNETPP
			rrep_dest->newBocks(1);
#endif
			rrep_dest->re_blocks[1].g		= this_host.is_gw;
			rrep_dest->re_blocks[1].prefix		= this_host.prefix;
			rrep_dest->re_blocks[1].res		= 0;
			rrep_dest->re_blocks[1].re_hopcnt	= 0;
			rrep_dest->re_blocks[1].re_node_seqnum	= this_host.seqnum;
			rrep_dest->re_blocks[1].re_node_addr	= DEV_NR(ifindex).ipaddr.s_addr;
			rrep_dest->len += RE_BLOCK_SIZE;
		}
		re_send_rrep(rrep_dest);
}


int NS_CLASS re_mustAnswer(RE *re,u_int32_t ifindex)
{
	int mustAnswer = 0;
	rtable_entry_t *entry;
	struct in_addr target_addr;

#ifdef OMNETPP
	bool haveRoute= false;
	if (isInMacLayer())
	{
		if (re->getControlInfo())
		{
			if (dynamic_cast<MeshControlInfo*>(re->getControlInfo()))
			{
				MeshControlInfo * controlInfo = dynamic_cast<MeshControlInfo*>(re->getControlInfo());
				if (controlInfo->getVectorAddressArraySize()>0)
				{
					haveRoute= true;
				}
			}
			else
			{
				delete re->removeControlInfo();
			}
		}
	}
	if (isLocalAddress (re->target_addr))  // If this node is the target, the RE must not be retransmitted
		mustAnswer = 1;
	else if (re->a && haveRoute)
		mustAnswer = 3;
	else if (re->a && intermediateRREP)
	{
		target_addr.s_addr	= re->target_addr;
		entry	= rtable_find(target_addr);
		u_int32_t target_seqnum	= ntohl(re->target_seqnum);
		if (entry && (entry->rt_state==RT_VALID) && (target_seqnum>0) && (target_seqnum < entry->rt_seqnum))
			mustAnswer = 2;
	}
#else
	if (re->target_addr == (u_int32_t) DEV_IFINDEX(ifindex).ipaddr.s_addr)  // If this node is the target, the RE must not be retransmitted
		mustAnswer = 1;
	else if (re->a && intermediateRREP)
	{
		u_int32_t target_seqnum;
		target_addr.s_addr	= re->target_addr;
		entry	= rtable_find(target_addr);
		target_seqnum	= ntohl(re->target_seqnum);
		if (entry && (entry->rt_state==VALID) && (target_seqnum>0) && (target_seqnum < entry->rt_seqnum))
			mustAnswer = 2;
	}
#endif
	return mustAnswer;
}

#ifdef OMNETPP
void NS_CLASS re_answer(RE *re,u_int32_t ifindex)
{
	struct in_addr node_addr;
	struct in_addr next_addr;
	struct in_addr target_addr;
	struct in_addr src_addr;
	rtable_entry_t *entry;
	rtable_entry_t *rev_rt;
	int i;
	MeshControlInfo * controlInfo = check_and_cast<MeshControlInfo*>(re->getControlInfo());
	// Process list of nodes
	int sizeVector =controlInfo->getVectorAddressArraySize();
	if (sizeVector>0)
	{
		next_addr.s_addr = controlInfo->getVectorAddress(0);
		for (i =0; i<sizeVector;i++)
		{
			node_addr.s_addr	= controlInfo->getVectorAddress(i);
			entry			= rtable_find(node_addr);
			if (entry)
			{
				if (entry->rt_hopcnt>i+1)
				{
					rtable_update(entry,node_addr,next_addr,ifindex,0,0,i+1,0);
				}
			}
			else
			{
				rtable_insert(
						node_addr,		// dest
						next_addr,		// nxt hop
					ifindex,	// iface
					0,		// seqnum
					0,		// prefix
					i+1,		// hop count
					0);		// is gw
			}
		}
	}

	INC_SEQNUM(this_host.seqnum);
	// create rrepToOrigNode
	int hopcnt=0;
	target_addr.s_addr	= re->target_addr;
	src_addr.s_addr = re->re_blocks[0].re_node_addr;

	rev_rt	= rtable_find(src_addr);
	entry = rtable_find(target_addr);

	if (rev_rt->rt_hopcnt)
		hopcnt=rev_rt->rt_hopcnt;

	RE *rrep_src = re_create_rrep(
			src_addr,
				ntohl(rev_rt->rt_seqnum),
				target_addr,
				entry->rt_seqnum,
				entry->rt_prefix,
				entry->rt_is_gw,
				NET_DIAMETER,
				hopcnt);

	if (entry->rt_hopcnt)
		rrep_src->re_blocks[0].re_hopcnt	= entry->rt_hopcnt;
	else
		rrep_src->re_blocks[0].re_hopcnt	= 0;
#if 1
	if (sizeVector>0 && !no_path_acc)
	{
		rrep_src->newBocks(sizeVector);
		for (int i = sizeVector-1;i>0;i--)
		{
			rrep_src->re_blocks[i].g		= 0;
			rrep_src->re_blocks[i].prefix		= 0;
			rrep_src->re_blocks[i].res		= 0;
			rrep_src->re_blocks[i].re_hopcnt	= i;
			rrep_src->re_blocks[i].re_node_seqnum	= 0;
			rrep_src->re_blocks[i].re_node_addr	=controlInfo->getVectorAddress(i);
			rrep_src->len += RE_BLOCK_SIZE;
		}
		rrep_src->re_blocks[sizeVector].g		= this_host.is_gw;
		rrep_src->re_blocks[sizeVector].prefix		= this_host.prefix;
		rrep_src->re_blocks[sizeVector].res		= 0;
		rrep_src->re_blocks[sizeVector].re_hopcnt	= 0;
		rrep_src->re_blocks[sizeVector].re_node_seqnum	= this_host.seqnum;
		rrep_src->re_blocks[sizeVector].re_node_addr	= DEV_NR(ifindex).ipaddr.s_addr;
		rrep_src->len += RE_BLOCK_SIZE;
	}
#else
	rrep_src->newBocks(1);
	rrep_src->re_blocks[1].g		= this_host.is_gw;
	rrep_src->re_blocks[1].prefix		= this_host.prefix;
	rrep_src->re_blocks[1].res		= 0;
	rrep_src->re_blocks[1].re_hopcnt	= 0;
	rrep_src->re_blocks[1].re_node_seqnum	= this_host.seqnum;
	rrep_src->re_blocks[1].re_node_addr	= DEV_NR(ifindex).ipaddr.s_addr;
	rrep_src->len += RE_BLOCK_SIZE;

	if (entry->rt_hopcnt)
		hopcnt=entry->rt_hopcnt;
	else
		hopcnt=0;

	RE *rrep_dest = re_create_rrep(
		target_addr,
		ntohl(entry->rt_seqnum),
		src_addr,
		rev_rt->rt_seqnum,
		rev_rt->rt_prefix,
		rev_rt->rt_is_gw,
		NET_DIAMETER,
		hopcnt);

	if (rev_rt->rt_hopcnt)
		rrep_dest->re_blocks[0].re_hopcnt	= rev_rt->rt_hopcnt;
	else
		rrep_dest->re_blocks[0].re_hopcnt	= 0;


	rrep_dest->newBocks(1);
	rrep_dest->re_blocks[1].g		= this_host.is_gw;
	rrep_dest->re_blocks[1].prefix		= this_host.prefix;
	rrep_dest->re_blocks[1].res		= 0;
	rrep_dest->re_blocks[1].re_hopcnt	= 0;
	rrep_dest->re_blocks[1].re_node_seqnum	= this_host.seqnum;
	rrep_dest->re_blocks[1].re_node_addr	= DEV_NR(ifindex).ipaddr.s_addr;
	rrep_dest->len += RE_BLOCK_SIZE;
	re_send_rrep(rrep_dest);
#endif
	re_send_rrep(rrep_src);
}
#endif
