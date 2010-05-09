/* Copyright (C) Uppsala University
 *
 * This file is distributed under the terms of the GNU general Public
 * License (GPL), see the file LICENSE
 *
 * Author: Erik Nordström, <erikn@it.uu.se>
 */

#define OMNETPP
#ifdef __KERNEL__
#include <linux/proc_fs.h>
#include "dsr-dev.h"
#endif

#ifndef OMNETPP
#ifdef NS2
#include "ns-agent.h"
#endif
#else
#include "dsr-uu-omnetpp.h"
#endif

#include "tbl.h"
#include "debug_dsr.h"
#include "dsr-opt.h"
#include "dsr-ack.h"
#include "link-cache.h"
#include "neigh.h"
#include "maint-buf.h"

struct dsr_ack_opt *dsr_ack_opt_add(char *buf, int len, struct in_addr src,
				    struct in_addr dst, unsigned short id)
{
	struct dsr_ack_opt *ack = (struct dsr_ack_opt *)buf;

	if (len < (int)DSR_ACK_HDR_LEN)
		return NULL;

	ack->type = DSR_OPT_ACK;
	ack->length = DSR_ACK_OPT_LEN;
	ack->id = htons(id);
	ack->dst = dst.s_addr;
	ack->src = src.s_addr;

	return ack;
}

int NSCLASS dsr_ack_send(struct in_addr dst, unsigned short id)
{
	struct dsr_pkt *dp;
	struct dsr_ack_opt *ack_opt;
	int len;
	char *buf;

	/* srt = dsr_rtc_find(my_addr(), dst); */

/* 	if (!srt) { */
/* 		DEBUG("No source route to %s\n", print_ip(dst.s_addr)); */
/* 		return -1; */
/* 	} */

	len = DSR_OPT_HDR_LEN + /* DSR_SRT_OPT_LEN(srt) +  */ DSR_ACK_HDR_LEN;

	dp = dsr_pkt_alloc(NULL);

	dp->dst = dst;
	/* dp->srt = srt; */
	dp->nxt_hop = dst;	//dsr_srt_next_hop(dp->srt, 0);
	dp->src = my_addr();

	buf = dsr_pkt_alloc_opts(dp, len);

	if (!buf)
		goto out_err;

	dp->nh.iph = dsr_build_ip(dp, dp->src, dp->dst, IP_HDR_LEN,
				  IP_HDR_LEN + len, IPPROTO_DSR, IPDEFTTL);

	if (!dp->nh.iph) {
		DEBUG("Could not create IP header\n");
		goto out_err;
	}

	dp->dh.opth = dsr_opt_hdr_add(buf, len, DSR_NO_NEXT_HDR_TYPE);

	if (!dp->dh.opth) {
		DEBUG("Could not create DSR opt header\n");
		goto out_err;
	}

	buf += DSR_OPT_HDR_LEN;
	len -= DSR_OPT_HDR_LEN;

	/* dp->srt_opt = dsr_srt_opt_add(buf, len, dp->srt); */

/* 	if (!dp->srt_opt) { */
/* 		DEBUG("Could not create Source Route option header\n"); */
/* 		goto out_err; */
/* 	} */

/* 	buf += DSR_SRT_OPT_LEN(dp->srt); */
/* 	len -= DSR_SRT_OPT_LEN(dp->srt); */

	ack_opt = dsr_ack_opt_add(buf, len, dp->src, dp->dst, id);

	if (!ack_opt) {
		DEBUG("Could not create DSR ACK opt header\n");
		goto out_err;
	}

	DEBUG("Sending ACK to %s id=%u\n", print_ip(dst), id);

	dp->flags |= PKT_XMIT_JITTER;

	XMIT(dp);

	return 1;

      out_err:
	dsr_pkt_free(dp);
	return -1;
}

static struct dsr_ack_req_opt *dsr_ack_req_opt_create(char *buf, int len,
						      unsigned short id)
{
	struct dsr_ack_req_opt *ack_req = (struct dsr_ack_req_opt *)buf;

	if (len < (int)DSR_ACK_REQ_HDR_LEN)
		return NULL;

	/* Build option */
	ack_req->type = DSR_OPT_ACK_REQ;
	ack_req->length = DSR_ACK_REQ_OPT_LEN;
	ack_req->id = htons(id);

	return ack_req;
}



int NSCLASS dsr_ack_req_send(struct dsr_pkt *dp)
{
	DSRPkt *p=NULL;

	/* srt = dsr_rtc_find(my_addr(), dst); */

/* 	if (!srt) { */
/* 		DEBUG("No source route to %s\n", print_ip(dst.s_addr)); */
/* 		return -1; */
/* 	} */
	dp->numRetries++;

	if (dp->payload)
	{
		p = new DSRPkt();
		IPDatagram *dgram;
		dgram = p;

#ifndef MobilityFramework
		IPAddress destAddress_var((uint32_t)dp->dst.s_addr);
		dgram->setDestAddress(destAddress_var);
		IPAddress srcAddress_var((uint32_t)dp->src.s_addr);
		dgram->setSrcAddress(srcAddress_var);
		dgram->setHeaderLength(dp->nh.iph->ihl); // Header length
		dgram->setVersion(dp->nh.iph->version); // Ip version
		dgram->setDiffServCodePoint(dp->nh.iph->tos); // ToS
		dgram->setIdentification(dp->nh.iph->id); // Identification
		dgram->setMoreFragments(dp->nh.iph->tos & 0x2000);
		dgram->setDontFragment (dp->nh.iph->frag_off & 0x4000);
		dgram->setTimeToLive (dp->nh.iph->ttl); // TTL
#else
		dgram->setDestAddr(dp->dst.s_addr);
		dgram->setSrcAddr(dp->src.s_addr);
		dgram->setTtl (dp->nh.iph->ttl); // TTL
#endif
		if (dp->nh.iph->protocol == IP_PROT_DSR)
		{
			dgram->setTransportProtocol(IP_PROT_DSR);
			struct dsr_opt_hdr *opth;
			struct dsr_opt_hdr * options;
			opth = dp->dh.opth;
			int dsr_opts_len = opth->p_len + DSR_OPT_HDR_LEN;
			options = (dsr_opt_hdr *)malloc (dsr_opts_len);
			memcpy((char*)options,(char*)opth,dsr_pkt_opts_len(dp));
			p->setOptions(options);
			p->setBitLength (p->getBitLength()+((DSR_OPT_HDR_LEN+options->p_len)*8));
			p->setEncapProtocol((IPProtocolId)dp->encapsulate_protocol);
		}
		else
			opp_error("error in Packet, is not DSR ");

		dgram->encapsulate(dp->payload->dup());
	}


	if (!p) {
		DEBUG("Could not create packet\n");
		return -1;
	}


	/* If TTL = 0, drop packet */
	if (p->getTimeToLive() <= 0) {
		DEBUG("Dropping packet with TTL = 0.");
		drop(p, ICMP_TIME_EXCEEDED);
		return -1;
	}

	DEBUG("xmitting pkt src=%d dst=%d nxt_hop=%d\n",
	      (uint32_t)dp->src.s_addr, (uint32_t)dp->dst.s_addr, (uint32_t)dp->nxt_hop.s_addr);

	/* Set packet fields depending on packet type */
	double jitter;
	if (dp->flags & PKT_XMIT_JITTER) {
		/* Broadcast packet */
		jitter = uniform(0, ((double) ConfVal(BroadCastJitter))/1000);
		DEBUG("xmit jitter=%f s\n", jitter);
	}

	if (dp->dst.s_addr != DSR_BROADCAST)
	{
		/* Get hardware destination address */
#ifdef MobilityFramework
		int macAddr = arp->getMacAddr(dp->nxt_hop.s_addr);
		p->setNextAddress(dp->nxt_hop.s_addr);
		p->setControlInfo(new MacControlInfo(macAddr));
#else
		IPControlInfo *controlInfo=NULL;
		if (p->getControlInfo())
		{
			if (dynamic_cast<IPControlInfo*>(p->getControlInfo()))
				controlInfo = check_and_cast<IPControlInfo*>(p->getControlInfo());
			else
				delete p->removeControlInfo();
		}

		if (controlInfo==NULL)
		{
			controlInfo = new IPControlInfo();
			p->setControlInfo(controlInfo);
		}

		IPAddress nextIp((uint32_t)dp->nxt_hop.s_addr);
		controlInfo->setNextHopAddr(nextIp);

		controlInfo->setProtocol(IP_PROT_DSR);
		controlInfo->setInterfaceId(interfaceId); // If broadcast packet send to interface
		controlInfo->setTimeToLive(dp->nh.iph->ttl);
		controlInfo->setSrcAddr(p->getSrcAddress());
		controlInfo->setDestAddr(p->getDestAddress());

		p->setNextAddress(nextIp);
#endif
	}
#ifdef MobilityFramework
	else
	{
		p->setDestAddr(L3BROADCAST);
		int macAddr = L2BROADCAST;
		p->setControlInfo(new MacControlInfo(macAddr));
	}
#endif
	/*
	if (!ConfVal(UseNetworkLayerAck)) {
		cmh->xmit_failure_ = xmit_failure;
		cmh->xmit_failure_data_ = (void *) this;
	}
	*/
#ifdef MobilityFramework
	int prev = myaddr_.s_addr;
#else
	IPAddress prev((uint32_t)myaddr_.s_addr);
#endif
	p->setPrevAddress(prev);
	if (jitter)
		sendDelayed(p,jitter, "toIp");
	else
		if (dp->dst.s_addr != DSR_BROADCAST)
			send(p,"toIp");
		else
			sendDelayed(p,par ("broadCastDelay"), "toIp");
		return 1;
}


struct dsr_ack_req_opt *NSCLASS
dsr_ack_req_opt_add(struct dsr_pkt *dp, unsigned short id)
{
	char *buf = NULL;
	int prot = 0, tot_len = 0, ttl = IPDEFTTL;

	if (!dp)
		return NULL;

	/* If we are forwarding a packet and there is already an ACK REQ option,
	 * we just overwrite the old one. */
	if (dp->ack_req_opt) {
		buf = (char *)dp->ack_req_opt;
		goto end;
	}
#ifdef NS2
	if (dp->p) {
		hdr_cmn *cmh = HDR_CMN(dp->p);
		prot = cmh->ptype();
	} else
		prot = DSR_NO_NEXT_HDR_TYPE;

	ttl = dp->nh.iph->ttl();
#else
	if (dp->nh.raw) {
		tot_len = ntohs(dp->nh.iph->tot_len);
		prot = dp->nh.iph->protocol;
		ttl = dp->nh.iph->ttl;
	}
#endif
	if (!dsr_pkt_opts_len(dp)) {

		buf =
		    dsr_pkt_alloc_opts(dp,
				       DSR_OPT_HDR_LEN + DSR_ACK_REQ_HDR_LEN);
		DEBUG("Allocating options for ACK REQ\n");
		if (!buf)
			return NULL;

		dsr_build_ip(dp, dp->src, dp->dst, IP_HDR_LEN,
			     tot_len + DSR_OPT_HDR_LEN + DSR_ACK_REQ_HDR_LEN,
			     IPPROTO_DSR, ttl);

		dp->dh.opth =
		    dsr_opt_hdr_add(buf, DSR_OPT_HDR_LEN + DSR_ACK_REQ_HDR_LEN,
				    prot);

		if (!dp->dh.opth) {
			return NULL;
		}

		buf += DSR_OPT_HDR_LEN;

	} else {
		buf = dsr_pkt_alloc_opts_expand(dp, DSR_ACK_REQ_HDR_LEN);

		DEBUG("Expanding options for ACK REQ p_len=%d\n",
		      ntohs(dp->dh.opth->p_len));
		if (!buf)
			return NULL;

		dsr_build_ip(dp, dp->src, dp->dst, IP_HDR_LEN,
			     tot_len + DSR_ACK_REQ_HDR_LEN, IPPROTO_DSR, ttl);

		dp->dh.opth =
		    dsr_opt_hdr_add(dp->dh.raw,
				    DSR_OPT_HDR_LEN +
				    ntohs(dp->dh.opth->p_len) +
				    DSR_ACK_REQ_HDR_LEN, dp->dh.opth->nh);
	}
	DEBUG("Added ACK REQ option id=%u\n", id, ntohs(dp->dh.opth->p_len));
      end:
	return dsr_ack_req_opt_create(buf, DSR_ACK_REQ_HDR_LEN, id);
}

int NSCLASS dsr_ack_req_send(struct in_addr neigh_addr, unsigned short id)
{
	struct dsr_pkt *dp;
	struct dsr_ack_req_opt *ack_req;
	int len = DSR_OPT_HDR_LEN + DSR_ACK_REQ_HDR_LEN;
	char *buf;

	dp = dsr_pkt_alloc(NULL);

	dp->dst = neigh_addr;
	dp->nxt_hop = neigh_addr;
	dp->src = my_addr();

	buf = dsr_pkt_alloc_opts(dp, len);

	if (!buf)
		goto out_err;

	dp->nh.iph = dsr_build_ip(dp, dp->src, dp->dst, IP_HDR_LEN,
				  IP_HDR_LEN + len, IPPROTO_DSR, 1);

	if (!dp->nh.iph) {
		DEBUG("Could not create IP header\n");
		goto out_err;
	}

	dp->dh.opth = dsr_opt_hdr_add(buf, len, DSR_NO_NEXT_HDR_TYPE);

	if (!dp->dh.opth) {
		DEBUG("Could not create DSR opt header\n");
		goto out_err;
	}

	buf += DSR_OPT_HDR_LEN;
	len -= DSR_OPT_HDR_LEN;

	ack_req = dsr_ack_req_opt_create(buf, len, id);

	if (!ack_req) {
		DEBUG("Could not create ACK REQ opt\n");
		goto out_err;
	}

	DEBUG("Sending ACK REQ for %s id=%u\n", print_ip(neigh_addr), id);

	XMIT(dp);

	return 1;

      out_err:
	dsr_pkt_free(dp);
	return -1;
}

int NSCLASS dsr_ack_req_opt_recv(struct dsr_pkt *dp, struct dsr_ack_req_opt *ack_req_opt)
{
	unsigned short id;

	if (!ack_req_opt || !dp || dp->flags & PKT_PROMISC_RECV)
		return DSR_PKT_ERROR;

	dp->ack_req_opt = ack_req_opt;

	id = ntohs(ack_req_opt->id);

	if (!dp->srt_opt)
		dp->prv_hop = dp->src;

	DEBUG("src=%s prv=%s id=%u\n",
	      print_ip(dp->src), print_ip(dp->prv_hop), id);

	dsr_ack_send(dp->prv_hop, id);

	return DSR_PKT_NONE;
}

int NSCLASS dsr_ack_opt_recv(struct dsr_ack_opt *ack)
{
	unsigned short id;
	struct in_addr dst, src, myaddr;
	int n;

	if (!ack)
		return DSR_PKT_ERROR;

	myaddr = my_addr();

	dst.s_addr = ack->dst;
	src.s_addr = ack->src;
	id = ntohs(ack->id);

	DEBUG("ACK dst=%s src=%s id=%u\n", print_ip(dst), print_ip(src), id);

	if (dst.s_addr != myaddr.s_addr)
		return DSR_PKT_ERROR;

	/* Purge packets buffered for this next hop */
	n = maint_buf_del_all_id(src, id);

	DEBUG("Removed %d packets from maint buf\n", n);

	return DSR_PKT_NONE;
}
