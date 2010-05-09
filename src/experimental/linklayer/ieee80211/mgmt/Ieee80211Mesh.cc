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


#include "Ieee80211Mesh.h"
#include "MeshControlInfo_m.h"
#include "lwmpls_data.h"
#include "ControlInfoBreakLink_m.h"
#include "ControlManetRouting_m.h"
#include "OLSRpkt_m.h"
#include "dymo_msg_struct.h"
#include "aodv_msg_struct.h"
#include "InterfaceTableAccess.h"
#include "IPDatagram.h"
#include <string.h>


/* WMPLS */

#define WLAN_MPLS_TIME_DELETE  6

#define WLAN_MPLS_TIME_REFRESH 3

#define _WLAN_BAD_PKT_TIME_ 30


#if !defined (UINT32_MAX)
#   define UINT32_MAX  4294967295UL
#endif


Define_Module(Ieee80211Mesh);

uint64_t MacToUint64(const MACAddress &add)
{
	uint64_t aux;
	uint64_t lo=0;
	for (int i=0;i<MAC_ADDRESS_BYTES;i++)
	{
		aux  = add.getAddressByte(MAC_ADDRESS_BYTES-i-1);
		aux <<= 8*i;
		lo  |= aux ;
	}
	return lo;
}

MACAddress Uint64ToMac(uint64_t lo)
{
	MACAddress add;
	add.setAddressByte(0, (lo>>40)&0xff);
	add.setAddressByte(1, (lo>>32)&0xff);
	add.setAddressByte(2, (lo>>24)&0xff);
	add.setAddressByte(3, (lo>>16)&0xff);
	add.setAddressByte(4, (lo>>8)&0xff);
	add.setAddressByte(5, lo&0xff);
	return add;
}

void Ieee80211Mesh::initialize(int stage)
{
	EV << "Init mesh proccess \n";
    Ieee80211MgmtBase::initialize(stage);

    if (stage==1)
    {
		mplsData = new LWMPLSDataStructure;
		cModuleType *moduleType;
		cModule *module;
		routingModuleProactive = NULL;
		routingModuleReactive = NULL;
		useLwmpls = par("UseLwMpls");
		bool useReactive = par("useReactive");
		bool useProactive = par("useProactive");
		proactiveFeedback  = par("ProactiveFeedback");

		// Proactive protocol
		if (useProactive)
		{

			//if (isEtx)
			//	moduleType = cModuleType::find("inet.networklayer.manetrouting.OLSR_ETX");
			//else
			moduleType = cModuleType::find("inet.networklayer.manetrouting.OLSR");
			module = moduleType->create("ManetRoutingProtocolProactive", this);
			routingModuleProactive = dynamic_cast <ManetRoutingBase*> (module);
			routingModuleProactive->gate("to_ip")->connectTo(gate("routingInProactive"));
			gate("routingOutProactive")->connectTo(routingModuleProactive->gate("from_ip"));
			routingModuleProactive->buildInside();
			routingModuleProactive->scheduleStart(simTime());
		}

		// Reactive protocol
		if (useReactive)
		{
			moduleType = cModuleType::find("inet.networklayer.manetrouting.DYMOUM");
			module = moduleType->create("ManetRoutingProtocolReactive", this);
			routingModuleReactive = dynamic_cast <ManetRoutingBase*> (module);

			routingModuleReactive->gate("to_ip")->connectTo(gate("routingInReactive"));
			gate("routingOutReactive")->connectTo(routingModuleReactive->gate("from_ip"));
			routingModuleReactive->buildInside();
			routingModuleReactive->scheduleStart(simTime());
		}

		if (routingModuleProactive==NULL && routingModuleReactive ==NULL)
			error("Ieee80211Mesh doesn't have active routing protocol");

		mplsData->mplsMaxTime()=35;
		active_mac_break=false;
	}
	if (stage==4)
	{
		macBaseGateId = gateSize("macOut")==0 ? -1 : gate("macOut",0)->getId();
		EV << "macBaseGateId :" << macBaseGateId << "\n";
		ift = InterfaceTableAccess ().get();
		nb = NotificationBoardAccess().get();
		nb->subscribe(this, NF_LINK_BREAK);
		nb->subscribe(this,NF_LINK_REFRESH);
    }
}


void Ieee80211Mesh::handleMessage(cMessage *msg)
{

    if (msg->isSelfMessage())
    {
        // process timers
        EV << "Timer expired: " << msg << "\n";
        handleTimer(msg);
        return;
    }
    cGate * msggate = msg->getArrivalGate();
    char gateName [40];
    memset(gateName,0,40);
    strcpy(gateName,msggate->getBaseName());
    //if (msg->arrivedOn("macIn"))
	if (strstr(gateName,"macIn")!=NULL)
    {
        // process incoming frame
        EV << "Frame arrived from MAC: " << msg << "\n";
        Ieee80211DataOrMgmtFrame *frame = check_and_cast<Ieee80211DataOrMgmtFrame *>(msg);
        processFrame(frame);
    }
	//else if (msg->arrivedOn("agentIn"))
    else if (strstr(gateName,"agentIn")!=NULL)
    {
        // process command from agent
        EV << "Command arrived from agent: " << msg << "\n";
        int msgkind = msg->getKind();
        cPolymorphic *ctrl = msg->removeControlInfo();
        delete msg;
        handleCommand(msgkind, ctrl);
    }
    //else if (msg->arrivedOn("routingIn"))
    else if (strstr(gateName,"routingIn")!=NULL)
    {
        handleRoutingMessage(PK(msg));
    }
    else
    {
        cPacket *pk = PK(msg);
        // packet from upper layers, to be sent out
        EV << "Packet arrived from upper layers: " << pk << "\n";
        if (pk->getByteLength() > 2312)
            error("message from higher layer (%s)%s is too long for 802.11b, %d bytes (fragmentation is not supported yet)",
                  pk->getClassName(), pk->getName(), pk->getByteLength());
        handleUpperMessage(pk);
    }
}

void Ieee80211Mesh::handleTimer(cMessage *msg)
{
    //ASSERT(false);
    mplsData->lwmpls_interface_delete_old_path();
    mplsCheckRouteTime();
}


void Ieee80211Mesh::handleRoutingMessage(cPacket *msg)
{
    cObject *temp  = msg->removeControlInfo();
    Ieee802Ctrl * ctrl = dynamic_cast<Ieee802Ctrl*> (temp);
    if (!ctrl)
    {
    	char name[50];
        strcpy(name,msg->getName());
    	error ("Message error");
    }
    Ieee80211DataFrame * frame = encapsulate(msg,ctrl->getDest());
    frame->setKind(ctrl->getInputPort());
    delete ctrl;
    if (frame)
    	sendOrEnqueue(frame);
}

void Ieee80211Mesh::handleUpperMessage(cPacket *msg)
{
    Ieee80211DataFrame *frame = encapsulate(msg);
    if (frame)
    	sendOrEnqueue(frame);
}

void Ieee80211Mesh::handleCommand(int msgkind, cPolymorphic *ctrl)
{
    error("handleCommand(): no commands supported");
}

Ieee80211DataFrame *Ieee80211Mesh::encapsulate(cPacket *msg)
{
	Ieee80211DataFrame *frame = new Ieee80211DataFrame(msg->getName());
	LWMPLSPacket *lwmplspk = NULL;
	LWmpls_Forwarding_Structure *forwarding_ptr=NULL;

    // copy receiver address from the control info (sender address will be set in MAC)
	Ieee802Ctrl *ctrl = check_and_cast<Ieee802Ctrl *>(msg->removeControlInfo());
	MACAddress dest = ctrl->getDest();
	MACAddress next = ctrl->getDest();
	delete ctrl;

	frame->setAddress4(dest);
	frame->setAddress3(myAddress);


	if (dest.isBroadcast())
	{
		frame->setReceiverAddress(dest);
		uint32_t cont;

		mplsData->getBroadCastCounter(cont);

		lwmplspk = new LWMPLSPacket(msg->getName());
		cont++;
		mplsData->setBroadCastCounter(cont);
		lwmplspk->setCounter(cont);
		lwmplspk->setSource(myAddress);
		lwmplspk->setDest(dest);
		lwmplspk->setType(WMPLS_BROADCAST);
		lwmplspk->encapsulate(msg);
		frame->encapsulate(lwmplspk);
		return frame;
	}
	//
	// Search in the data base
	//
	int label = mplsData->getRegisterRoute(MacToUint64(dest));

	if (label!=-1)
	{
		forwarding_ptr = mplsData->lwmpls_forwarding_data(label,-1,0);
		if (!forwarding_ptr)
			 mplsData->deleteRegisterRoute(MacToUint64(dest));

	}

	if (forwarding_ptr)
	{
		lwmplspk = new LWMPLSPacket(msg->getName());
		lwmplspk->setSource(myAddress);
		lwmplspk->setDest(dest);

		if (forwarding_ptr->order==LWMPLS_EXTRACT)
		{
// Source or destination?
			if (forwarding_ptr->output_label>0 || forwarding_ptr->return_label_output>0)
			{
				lwmplspk->setType(WMPLS_NORMAL);
				if (forwarding_ptr->return_label_input==label && forwarding_ptr->output_label>0)
				{
					next = Uint64ToMac(forwarding_ptr->mac_address);
					lwmplspk->setLabel(forwarding_ptr->output_label);
				}
				else if (forwarding_ptr->input_label==label && forwarding_ptr->return_label_output>0)
				{
					next = Uint64ToMac(forwarding_ptr->input_mac_address);
					lwmplspk->setLabel(forwarding_ptr->return_label_output);
				}
				else
				{
					opp_error("lwmpls data base error");
				}
			}
			else
			{
				lwmplspk->setType(WMPLS_BEGIN_W_ROUTE);

				int dist = forwarding_ptr->path.size()-2;
				lwmplspk->setVectorAddressArraySize(dist);
				//lwmplspk->setDist(dist);
				next=Uint64ToMac(forwarding_ptr->path[1]);

				for (int i=0;i<dist;i++)
					lwmplspk->setVectorAddress(i,Uint64ToMac(forwarding_ptr->path[i+1]));
				lwmplspk->setLabel (forwarding_ptr->return_label_input);
			}
		}
		else
		{
			lwmplspk->setType(WMPLS_NORMAL);
			if (forwarding_ptr->input_label==label && forwarding_ptr->output_label>0)
			{
				next = Uint64ToMac(forwarding_ptr->mac_address);
				lwmplspk->setLabel(forwarding_ptr->output_label);
			}
			else if (forwarding_ptr->return_label_input==label && forwarding_ptr->return_label_output>0)
			{
				next = Uint64ToMac(forwarding_ptr->input_mac_address);
				lwmplspk->setLabel(forwarding_ptr->return_label_output);
			}
			else
			{
				opp_error("lwmpls data base error");
			}
		}
		forwarding_ptr->last_use=simTime();
	}
	else
	{
		Uint128 add[20];
		int dist = 0;
		bool noRoute;

		if (routingModuleProactive)
		{
			dist = routingModuleProactive->getRoute(dest,add);
			noRoute = false;
		}

		if (dist==0)
		{
		// Search in the reactive routing protocol
		// Destination unreachable
			if (routingModuleReactive)
			{
				int iface;
				noRoute = true;
				if (!routingModuleReactive->getNextHop(dest,add[0],iface)) //send the packet to the routingModuleReactive
				{
					ControlManetRouting *ctrlmanet = new ControlManetRouting();
					ctrlmanet->setOptionCode(MANET_ROUTE_NOROUTE);
					ctrlmanet->setDestAddress(dest);
					ctrlmanet->setSrcAddress(myAddress);
					ctrlmanet->encapsulate(msg);

					send(ctrlmanet,"routingOutReactive");
					delete frame;
					return NULL;
				}
				else
				{
					if (add[0].getMACAddress() == dest)
						dist=1;
					else
						dist = 2;
				}
			}
			else
			{
				delete frame;
				delete msg;
				return NULL;
			}
		}

		next=add[0];


		if (dist >1 && useLwmpls)
		{
			lwmplspk = new LWMPLSPacket(msg->getName());
			if (!noRoute)
				lwmplspk->setType(WMPLS_BEGIN_W_ROUTE);
			else
				lwmplspk->setType(WMPLS_BEGIN);

			lwmplspk->setSource(myAddress);
			lwmplspk->setDest(dest);
			if (!noRoute)
			{
				next=add[0];
				lwmplspk->setVectorAddressArraySize(dist-1);
				//lwmplspk->setDist(dist-1);
				for (int i=0;i<dist-1;i++)
					lwmplspk->setVectorAddress(i,add[i]);
				lwmplspk->setByteLength(lwmplspk->getByteLength()+((dist-1)*6));
			}

			int label_in =mplsData->getLWMPLSLabel();

			/* es necesario introducir el nuevo path en la lista de enlace */
	  		//lwmpls_initialize_interface(lwmpls_data_ptr,&interface_str_ptr,label_in,sta_addr, ip_address,LWMPLS_INPUT_LABEL);
			/* es necesario ahora introducir los datos en la tabla */
			forwarding_ptr = new LWmpls_Forwarding_Structure();
			forwarding_ptr->input_label=-1;
			forwarding_ptr->return_label_input=label_in;
			forwarding_ptr->return_label_output=-1;
			forwarding_ptr->order=LWMPLS_EXTRACT;
			forwarding_ptr->mac_address=MacToUint64(next);
			forwarding_ptr->label_life_limit=mplsData->mplsMaxTime();
			forwarding_ptr->last_use=simTime();

			forwarding_ptr->path.push_back(MacToUint64(myAddress));
			for (int i=0;i<dist-1;i++)
				forwarding_ptr->path.push_back(MacToUint64(add[i]));
			forwarding_ptr->path.push_back(MacToUint64(dest));

			mplsData->lwmpls_forwarding_input_data_add(label_in,forwarding_ptr);
			// lwmpls_forwarding_output_data_add(label_out,sta_addr,forwarding_ptr,true);
			/*lwmpls_label_fw_relations (lwmpls_data_ptr,label_in,forwarding_ptr);*/
			lwmplspk->setLabel (label_in);
			mplsData->registerRoute(MacToUint64(dest),label_in);
		}
	}
   	frame->setReceiverAddress(next);
	if (lwmplspk)
	{
		lwmplspk->encapsulate(msg);
		frame->encapsulate(lwmplspk);
	}
	else
		frame->encapsulate(msg);

    if (frame->getReceiverAddress().isUnspecified())
           ASSERT(!frame->getReceiverAddress().isUnspecified());
	return frame;
}

Ieee80211DataFrame *Ieee80211Mesh::encapsulate(cPacket *msg,MACAddress dest)
{
   Ieee80211DataFrame *frame = new Ieee80211DataFrame(msg->getName());

   if (msg->getControlInfo())
   	delete msg->removeControlInfo();
   frame->setReceiverAddress(dest);
   frame->encapsulate(msg);

   if (frame->getReceiverAddress().isUnspecified())
     opp_error ("Ieee80211Mesh::encapsulate Bad Address");
   return frame;
}


void Ieee80211Mesh::receiveChangeNotification(int category, const cPolymorphic *details)
{
    Enter_Method_Silent();
    printNotificationBanner(category, details);

	if (details==NULL)
	return;

	if (category == NF_LINK_BREAK)
	{
		cMessage *msg  = check_and_cast<cMessage*>(details);
		ControlInfoBreakLink *info = (ControlInfoBreakLink*) msg->getControlInfo();
		MACAddress add = info->getDest();
		mplsBreakMacLink(add);
	}
	else if (category == NF_LINK_REFRESH)
	{
		Ieee80211TwoAddressFrame *frame  = check_and_cast<Ieee80211TwoAddressFrame *>(details);
		if (frame)
			mplsData->lwmpls_refresh_mac (MacToUint64(frame->getTransmitterAddress()),simTime());
	}
}

void Ieee80211Mesh::handleDataFrame(Ieee80211DataFrame *frame)
{

	// The message is forward
	if (forwardMessage (frame))
			return;

	MACAddress source= frame->getTransmitterAddress();
	cPacket *msg = decapsulate(frame);
	LWMPLSPacket *lwmplspk = dynamic_cast<LWMPLSPacket*> (msg);
	mplsData->lwmpls_refresh_mac(MacToUint64(source),simTime());

	if (!lwmplspk)
	{
		//cGate * msggate = msg->getArrivalGate();
		//int baseId = gateBaseId("macIn");
		//int index = baseId - msggate->getId();
		msg->setKind(0);
		if ((routingModuleProactive != NULL) && (routingModuleProactive->isOurType(msg)))
		{
			//sendDirect(msg,0, routingModule, "from_ip");
			send(msg,"routingOutProactive");
		}
		// else if (dynamic_cast<AODV_msg  *>(msg) || dynamic_cast<DYMO_element  *>(msg))
		else if ((routingModuleReactive != NULL) && routingModuleReactive->isOurType(msg))
		{

			Ieee802Ctrl *ctrl = check_and_cast<Ieee802Ctrl*>(msg->removeControlInfo());
			MeshControlInfo *controlInfo = new MeshControlInfo;
			Ieee802Ctrl *ctrlAux = controlInfo;
			*ctrlAux=*ctrl;
			delete ctrl;
			Uint128 dest;
			msg->setControlInfo(controlInfo);
			if (routingModuleReactive->getDestAddress(msg,dest))
			{
				Uint128 add[20];
				Uint128 src = controlInfo->getSrc();
				int dist = 0;
				if (routingModuleProactive)
				{
					// int neig = routingModuleProactive))->getRoute(src,add);
					controlInfo->setPreviousFix(true); // This node is fix
					dist = routingModuleProactive->getRoute(dest,add);
				}
				else
					controlInfo->setPreviousFix(false); // This node is not fix

				if (dist!=0 && proactiveFeedback)
				{
					controlInfo->setVectorAddressArraySize(dist);
					for (int i=0;i<dist;i++)
						controlInfo->setVectorAddress(i,add[i]);
				}
			}
			send(msg,"routingOutReactive");
		}
		else if (dynamic_cast<OLSR_pkt*>(msg) || dynamic_cast <DYMO_element *>(msg) || dynamic_cast <AODV_msg *>(msg))
		{
			delete msg;
		}
		else // Normal frame test if upper layer frame in other case delete
			sendUp(msg);
		return;
	}
	mplsDataProcess(lwmplspk,source);
}

void Ieee80211Mesh::handleAuthenticationFrame(Ieee80211AuthenticationFrame *frame)
{
    dropManagementFrame(frame);
}

void Ieee80211Mesh::handleDeauthenticationFrame(Ieee80211DeauthenticationFrame *frame)
{
    dropManagementFrame(frame);
}

void Ieee80211Mesh::handleAssociationRequestFrame(Ieee80211AssociationRequestFrame *frame)
{
    dropManagementFrame(frame);
}

void Ieee80211Mesh::handleAssociationResponseFrame(Ieee80211AssociationResponseFrame *frame)
{
    dropManagementFrame(frame);
}

void Ieee80211Mesh::handleReassociationRequestFrame(Ieee80211ReassociationRequestFrame *frame)
{
    dropManagementFrame(frame);
}

void Ieee80211Mesh::handleReassociationResponseFrame(Ieee80211ReassociationResponseFrame *frame)
{
    dropManagementFrame(frame);
}

void Ieee80211Mesh::handleDisassociationFrame(Ieee80211DisassociationFrame *frame)
{
    dropManagementFrame(frame);
}

void Ieee80211Mesh::handleBeaconFrame(Ieee80211BeaconFrame *frame)
{
    dropManagementFrame(frame);
}

void Ieee80211Mesh::handleProbeRequestFrame(Ieee80211ProbeRequestFrame *frame)
{
    dropManagementFrame(frame);
}

void Ieee80211Mesh::handleProbeResponseFrame(Ieee80211ProbeResponseFrame *frame)
{
    dropManagementFrame(frame);
}

cPacket *Ieee80211Mesh::decapsulateMpls(LWMPLSPacket *frame)
{
    cPacket *payload = frame->decapsulate();
    // ctrl->setSrc(frame->getAddress3());
    Ieee802Ctrl *ctrl =(Ieee802Ctrl*) frame->removeControlInfo();
    payload->setControlInfo(ctrl);
    delete frame;
    return payload;
}

void Ieee80211Mesh::mplsSendAck(int label)
{
	if (label >= LWMPLS_MAX_LABEL || label <= 0)
		opp_error("mplsSendAck error in label %i", label);
	LWMPLSPacket *mpls_pk_aux_ptr = new LWMPLSPacket();
	mpls_pk_aux_ptr->setLabelReturn (label);
	LWmpls_Forwarding_Structure * forwarding_ptr = mplsData->lwmpls_forwarding_data(label,0,0);

	MACAddress sta_addr;
	int return_label;
	if (forwarding_ptr->input_label==label)
	{
		sta_addr = Uint64ToMac(forwarding_ptr->input_mac_address);
		return_label = forwarding_ptr->return_label_output;
	}
	else if (forwarding_ptr->return_label_input==label)
	{
		sta_addr = Uint64ToMac(forwarding_ptr->mac_address);
		return_label = forwarding_ptr->output_label;
	}
	mpls_pk_aux_ptr->setType(WMPLS_ACK);
	mpls_pk_aux_ptr->setLabel(return_label);
	mpls_pk_aux_ptr->setDest(sta_addr);
	mpls_pk_aux_ptr->setSource(myAddress);
//	sendOrEnqueue(encapsulate (mpls_pk_aux_ptr, sta_addr));
	sendOrEnqueue(encapsulate (mpls_pk_aux_ptr, MACAddress::BROADCAST_ADDRESS));
   			/* initialize the mac timer */
	mplsInitializeCheckMac ();
}

//
// Crea las estructuras para enviar los paquetes por mpls e inicializa los registros del mac
//
void Ieee80211Mesh::mplsCreateNewPath(int label,LWMPLSPacket *mpls_pk_ptr,MACAddress sta_addr)
{
	int label_out = label;
// Alwais send a ACK
	int label_in;

	LWmpls_Interface_Structure * interface=NULL;

	LWmpls_Forwarding_Structure * forwarding_ptr = mplsData->lwmpls_forwarding_data(0,label_out,MacToUint64(sta_addr));
	if (forwarding_ptr!=NULL)
	{
		mplsData->lwmpls_check_label (forwarding_ptr->input_label,"begin");
		mplsData->lwmpls_check_label (forwarding_ptr->return_label_input,"begin");
		forwarding_ptr->last_use=simTime();

		mplsData->lwmpls_init_interface(&interface,forwarding_ptr->input_label,MacToUint64(sta_addr),LWMPLS_INPUT_LABEL);
// Is the destination?
		if (mpls_pk_ptr->getDest()==myAddress)
		{
			sendUp(decapsulateMpls(mpls_pk_ptr));
			forwarding_ptr->order = LWMPLS_EXTRACT;
			forwarding_ptr->output_label=0;
			if (Uint64ToMac(forwarding_ptr->input_mac_address) == sta_addr)
				mplsSendAck(forwarding_ptr->input_label);
			else if (Uint64ToMac(forwarding_ptr->mac_address) == sta_addr)
				mplsSendAck(forwarding_ptr->return_label_input);
			return;
		}
		int usedOutLabel;
		int usedIntLabel;
		MACAddress nextMacAddress;
		if (sta_addr == Uint64ToMac(forwarding_ptr->input_mac_address)) // forward path
		{
			usedOutLabel = forwarding_ptr->output_label;
                        usedIntLabel = forwarding_ptr->input_label;
			nextMacAddress = Uint64ToMac(forwarding_ptr->mac_address);
		}
		else if (sta_addr== Uint64ToMac(forwarding_ptr->mac_address)) // reverse path
		{
			usedOutLabel = forwarding_ptr->return_label_output;
                        usedIntLabel = forwarding_ptr->return_label_input;
			nextMacAddress = Uint64ToMac(forwarding_ptr->input_mac_address);
		}
		else
			opp_error("mplsCreateNewPath mac address incorrect");
		label_in = usedIntLabel;

		if (usedOutLabel>0)
		{
			/* path already exist */
			/* change to normal */
			mpls_pk_ptr->setType(WMPLS_NORMAL);
			cPacket * pk = mpls_pk_ptr->decapsulate();
			mpls_pk_ptr->setVectorAddressArraySize(0);
			mpls_pk_ptr->setByteLength(4);
			if (pk)
				mpls_pk_ptr->encapsulate(pk);

			//int dist = mpls_pk_ptr->getDist();
			//mpls_pk_ptr->setDist(0);
			/*op_pk_nfd_set_int32 (mpls_pk_ptr, "label",forwarding_ptr->output_label);*/
			Ieee80211DataFrame *frame = new Ieee80211DataFrame(mpls_pk_ptr->getName());
			if (usedOutLabel<=0 || usedIntLabel<=0)
				opp_error("mplsCreateNewPath Error in label");

			mpls_pk_ptr->setLabel(usedOutLabel);
			frame->setReceiverAddress(nextMacAddress);
			label_in = usedIntLabel;

			if (mpls_pk_ptr->getControlInfo())
				delete mpls_pk_ptr->removeControlInfo();
			frame->encapsulate(mpls_pk_ptr);
			if (frame->getReceiverAddress().isUnspecified())
				ASSERT(!frame->getReceiverAddress().isUnspecified());
			sendOrEnqueue(frame);
		}
		else
		{

			if (Uint64ToMac(forwarding_ptr->mac_address).isUnspecified())
			{
				forwarding_ptr->output_label=0;
				if (mpls_pk_ptr->getType()==WMPLS_BEGIN ||
					mpls_pk_ptr->getVectorAddressArraySize()==0 )
					//mpls_pk_ptr->getDist()==0 )
				{
					Uint128 add[20];
					int dist = 0;

					if (routingModuleProactive)
						dist = routingModuleProactive->getRoute(mpls_pk_ptr->getDest(),add);

					if (dist==0 && routingModuleReactive)
					{
						int iface;
						if (routingModuleReactive->getNextHop(mpls_pk_ptr->getDest(),add[0],iface))
								dist = 1;
					}

					if (dist==0)
					{
		// Destination unreachable
						mplsData->deleteForwarding(forwarding_ptr);
						delete mpls_pk_ptr;
						return;
					}
					forwarding_ptr->mac_address=MacToUint64(add[0]);
					if (routingModuleReactive)
					{
						Uint128 src(mpls_pk_ptr->getSource());
						Uint128 dst(mpls_pk_ptr->getDest());
						Uint128 prev(sta_addr);

						routingModuleReactive->setRefreshRoute(src,dst,add[0],prev);
					}
				}
				else
				{
					int position = -1;
					int arraySize = mpls_pk_ptr->getVectorAddressArraySize();
					//int arraySize = mpls_pk_ptr->getDist();
					for (int i=0;i<arraySize;i++)
						if (mpls_pk_ptr->getVectorAddress(i)==myAddress)
							position = i;
					if (position==(arraySize-1))
						forwarding_ptr->mac_address= MacToUint64 (mpls_pk_ptr->getDest());
					else if (position>=0)
					{
// Check if neigbourd?
						forwarding_ptr->mac_address=MacToUint64(mpls_pk_ptr->getVectorAddress(position+1));
					}
					else
					{
// Local route
						Uint128 add[20];
						int dist = 0;
						if (routingModuleProactive)
							dist = routingModuleProactive->getRoute(mpls_pk_ptr->getDest(),add);

						if (dist==0 && routingModuleReactive)
						{
							int iface;
							if (routingModuleReactive->getNextHop(mpls_pk_ptr->getDest(),add[0],iface))
								dist = 1;
						}

						if (dist==0)
						{
						// Destination unreachable
							mplsData->deleteForwarding(forwarding_ptr);
							delete mpls_pk_ptr;
							return;
						}
						if (routingModuleReactive)
						{
							Uint128 src(mpls_pk_ptr->getSource());
							Uint128 dst(mpls_pk_ptr->getDest());
							Uint128 prev(sta_addr);
							routingModuleReactive->setRefreshRoute(src,dst,add[0],prev);
						}
						forwarding_ptr->mac_address=MacToUint64(add[0]);
						mpls_pk_ptr->setVectorAddressArraySize(0);
						//mpls_pk_ptr->setDist(0);
					}
				}
			}

			if (forwarding_ptr->return_label_input<=0)
				opp_error("Error in label");

			mpls_pk_ptr->setLabel(forwarding_ptr->return_label_input);
			mpls_pk_ptr->setLabelReturn(0);
			Ieee80211DataFrame *frame = new Ieee80211DataFrame(mpls_pk_ptr->getName());
			frame->setReceiverAddress(Uint64ToMac(forwarding_ptr->mac_address));

			if (routingModuleReactive)
			{
				Uint128 src(mpls_pk_ptr->getSource());
				Uint128 dst(mpls_pk_ptr->getDest());
				Uint128 next(forwarding_ptr->mac_address);
				Uint128 prev(sta_addr);
				routingModuleReactive->setRefreshRoute(src,dst,next,prev);
			}


			if (mpls_pk_ptr->getControlInfo())
				delete mpls_pk_ptr->removeControlInfo();
			frame->encapsulate(mpls_pk_ptr);
			if (frame->getReceiverAddress().isUnspecified())
				ASSERT(!frame->getReceiverAddress().isUnspecified());
			sendOrEnqueue(frame);
		}
	}
	else
	{
// New structure
		/* Obtain a label */
		label_in =mplsData->getLWMPLSLabel();
		mplsData->lwmpls_init_interface(&interface,label_in,MacToUint64 (sta_addr),LWMPLS_INPUT_LABEL);
		/* es necesario introducir el nuevo path en la lista de enlace */
		//lwmpls_initialize_interface(lwmpls_data_ptr,&interface_str_ptr,label_in,sta_addr, ip_address,LWMPLS_INPUT_LABEL);
			/* es necesario ahora introducir los datos en la tabla */
		forwarding_ptr = new LWmpls_Forwarding_Structure();
		forwarding_ptr->output_label=0;
		forwarding_ptr->input_label=label_in;
		forwarding_ptr->return_label_input=0;
		forwarding_ptr->return_label_output=label_out;
		forwarding_ptr->order=LWMPLS_EXTRACT;
		forwarding_ptr->input_mac_address=MacToUint64(sta_addr);
		forwarding_ptr->label_life_limit =mplsData->mplsMaxTime();
		forwarding_ptr->last_use=simTime();

		forwarding_ptr->path.push_back((Uint128)mpls_pk_ptr->getSource());
		for (unsigned int i=0 ;i<mpls_pk_ptr->getVectorAddressArraySize();i++)
		//for (int i=0 ;i<mpls_pk_ptr->getDist();i++)
				forwarding_ptr->path.push_back((Uint128)mpls_pk_ptr->getVectorAddress(i));
		forwarding_ptr->path.push_back((Uint128)mpls_pk_ptr->getDest());

		// Add structure
		mplsData->lwmpls_forwarding_input_data_add(label_in,forwarding_ptr);
		if (!mplsData->lwmpls_forwarding_output_data_add(label_out,MacToUint64(sta_addr),forwarding_ptr,true))
		{
			mplsBasicSend (mpls_pk_ptr,sta_addr);
			return;
		}

		if (mpls_pk_ptr->getDest()==myAddress)
		{
				mplsSendAck(label_in);
				mplsData->registerRoute(MacToUint64(mpls_pk_ptr->getSource()),label_in);
				sendUp(decapsulateMpls(mpls_pk_ptr));
				// Register route
				return;
		}

		if (mpls_pk_ptr->getType()==WMPLS_BEGIN ||
			mpls_pk_ptr->getVectorAddressArraySize()==0 )
			//mpls_pk_ptr->getDist()==0 )
		{
			Uint128 add[20];
			int dist = 0;
			if (routingModuleProactive)
				dist = routingModuleProactive->getRoute(mpls_pk_ptr->getDest(),add);
			if (dist==0 && routingModuleReactive)
			{
				int iface;
				if (routingModuleReactive->getNextHop(mpls_pk_ptr->getDest(),add[0],iface))
					dist = 1;
			}


			if (dist==0)
			{
			// Destination unreachable
				mplsData->deleteForwarding(forwarding_ptr);
				delete mpls_pk_ptr;
				return;
			}
			forwarding_ptr->mac_address=MacToUint64(add[0]);
		}
		else
		{
			int position = -1;
			int arraySize = mpls_pk_ptr->getVectorAddressArraySize();
			//int arraySize = mpls_pk_ptr->getDist();
			for (int i=0;i<arraySize;i++)
				if (mpls_pk_ptr->getVectorAddress(i)==myAddress)
				{
					position = i;
					break;
				}
			if (position==(arraySize-1) && (position>=0))
				forwarding_ptr->mac_address=MacToUint64(mpls_pk_ptr->getDest());
			else if (position>=0)
			{
// Check if neigbourd?
				forwarding_ptr->mac_address=MacToUint64(mpls_pk_ptr->getVectorAddress(position+1));
			}
			else
			{
// Local route o discard?
				// delete mpls_pk_ptr
				// return;
				Uint128 add[20];
				int dist = 0;
				if (routingModuleProactive)
					dist = routingModuleProactive->getRoute(mpls_pk_ptr->getDest(),add);
				if (dist==0 && routingModuleReactive)
				{
					int iface;
					if (routingModuleReactive->getNextHop(mpls_pk_ptr->getDest(),add[0],iface))
						dist = 1;
				}


				if (dist==0)
				{
				// Destination unreachable
					mplsData->deleteForwarding(forwarding_ptr);
					delete mpls_pk_ptr;
					return;
				}
				forwarding_ptr->mac_address=MacToUint64 (add[0]);
				mpls_pk_ptr->setVectorAddressArraySize(0);
				//mpls_pk_ptr->setDist(0);
			}
		}

		if (routingModuleReactive)
		{
			Uint128 src(mpls_pk_ptr->getSource());
			Uint128 dst(mpls_pk_ptr->getDest());
			Uint128 next(forwarding_ptr->mac_address);
			Uint128 prev(sta_addr);
			routingModuleReactive->setRefreshRoute(src,dst,next,prev);
		}

// Send to next node
		Ieee80211DataFrame *frame = new Ieee80211DataFrame(mpls_pk_ptr->getName());
		frame->setReceiverAddress(Uint64ToMac(forwarding_ptr->mac_address));

// The reverse path label
		forwarding_ptr->return_label_input = mplsData->getLWMPLSLabel();
// Initialize the next interface
		interface = NULL;
		mplsData->lwmpls_init_interface(&interface,forwarding_ptr->return_label_input,forwarding_ptr->mac_address,LWMPLS_INPUT_LABEL_RETURN);
// Store the reverse path label
		mplsData->lwmpls_forwarding_input_data_add(forwarding_ptr->return_label_input,forwarding_ptr);

		mpls_pk_ptr->setLabel(forwarding_ptr->return_label_input);
		mpls_pk_ptr->setLabelReturn(0);

		if (mpls_pk_ptr->getControlInfo())
			delete mpls_pk_ptr->removeControlInfo();
		frame->encapsulate(mpls_pk_ptr);
		if (frame->getReceiverAddress().isUnspecified())
			ASSERT(!frame->getReceiverAddress().isUnspecified());
		sendOrEnqueue(frame);
	}
	mplsSendAck(label_in);
}

void Ieee80211Mesh::mplsBasicSend (LWMPLSPacket *mpls_pk_ptr,MACAddress sta_addr)
{
	if (mpls_pk_ptr->getDest()==myAddress)
	{
		if (routingModuleReactive)
		{
			Uint128 src(mpls_pk_ptr->getSource());
			Uint128 dst(mpls_pk_ptr->getDest());
			Uint128 prev(sta_addr);
			routingModuleReactive->setRefreshRoute(src,dst,0,prev);
		}
		sendUp(decapsulateMpls(mpls_pk_ptr));

	}
	else
	{
		Uint128 add[20];
		int dist=0;

		if (routingModuleProactive)
			dist = routingModuleProactive->getRoute(mpls_pk_ptr->getDest(),add);
		if (dist==0 && routingModuleReactive)
		{
			int iface;
			if (routingModuleReactive->getNextHop(mpls_pk_ptr->getDest(),add[0],iface))
				dist = 1;
		}


		if (dist==0)
		{
		// Destination unreachable
			delete mpls_pk_ptr;
			return;
		}

		if (routingModuleReactive)
		{
			Uint128 src(mpls_pk_ptr->getSource());
			Uint128 dst(mpls_pk_ptr->getDest());
			Uint128 prev(sta_addr);
			routingModuleReactive->setRefreshRoute(src,dst,add[0],prev);
		}

		mpls_pk_ptr->setType(WMPLS_SEND);
		cPacket * pk = mpls_pk_ptr->decapsulate();
		mpls_pk_ptr->setVectorAddressArraySize(0);
		mpls_pk_ptr->setByteLength(0);
		if (pk)
			mpls_pk_ptr->encapsulate(pk);
   		Ieee80211DataFrame *frame = new Ieee80211DataFrame(mpls_pk_ptr->getName());
		if (dist>1)
			frame->setReceiverAddress(add[0]);
		else
			frame->setReceiverAddress(mpls_pk_ptr->getDest());

		if (mpls_pk_ptr->getControlInfo())
			delete mpls_pk_ptr->removeControlInfo();
		frame->encapsulate(mpls_pk_ptr);
		if (frame->getReceiverAddress().isUnspecified())
			ASSERT(!frame->getReceiverAddress().isUnspecified());
		sendOrEnqueue(frame);
	}
}

void Ieee80211Mesh::mplsBreakPath(int label,LWMPLSPacket *mpls_pk_ptr,MACAddress sta_addr)
{
		// printf("break %f\n",time);
		// printf("code %i my_address %d org %d lin %d \n",code,my_address,sta_addr,label);
		// liberar todos los path, dos pasos quien detecta la ruptura y quien la propaga.
		// Es mecesario tambien liberar los caminos de retorno.
		/*	forwarding_ptr= lwmpls_forwarding_data(lwmpls_data_ptr,0,label,sta_addr);*/
	MACAddress send_mac_addr;
	LWmpls_Forwarding_Structure * forwarding_ptr=mplsData->lwmpls_interface_delete_label(label);
	if (forwarding_ptr == NULL)
	{
		delete mpls_pk_ptr;
		return;
	}

	if (label == forwarding_ptr->input_label)
	{
		mpls_pk_ptr->setLabel(forwarding_ptr->output_label);
		send_mac_addr = Uint64ToMac(forwarding_ptr->mac_address);
	}
	else
	{
		mpls_pk_ptr->setLabel(forwarding_ptr->return_label_output);
		send_mac_addr = Uint64ToMac(forwarding_ptr->input_mac_address);
	}

	mplsPurge (forwarding_ptr,true);
	// Must clean the routing tables?

	if ((forwarding_ptr->order==LWMPLS_CHANGE) && (!send_mac_addr.isUnspecified()))
	{
		Ieee80211DataFrame *frame = new Ieee80211DataFrame(mpls_pk_ptr->getName());
		frame->setReceiverAddress(send_mac_addr);
		if (mpls_pk_ptr->getControlInfo())
			delete mpls_pk_ptr->removeControlInfo();
		frame->encapsulate(mpls_pk_ptr);
		if (frame->getReceiverAddress().isUnspecified())
			ASSERT(!frame->getReceiverAddress().isUnspecified());
		sendOrEnqueue(frame);
	}
	else
	{
		// Firts or last node
		delete mpls_pk_ptr;
		//mplsData->deleteRegisterLabel(forwarding_ptr->input_label);
		//mplsData->deleteRegisterLabel(forwarding_ptr->return_label_input);
	}

	mplsData->deleteForwarding(forwarding_ptr);
}


void Ieee80211Mesh::mplsNotFoundPath(int label,LWMPLSPacket *mpls_pk_ptr,MACAddress sta_addr)
{
	LWmpls_Forwarding_Structure * forwarding_ptr= mplsData->lwmpls_forwarding_data(0,label,MacToUint64 (sta_addr));
	MACAddress send_mac_addr;
	if (forwarding_ptr == NULL)
		delete mpls_pk_ptr;
	else
	{
		mplsData->lwmpls_interface_delete_label(forwarding_ptr->input_label);
		mplsData->lwmpls_interface_delete_label(forwarding_ptr->return_label_input);
		if (label == forwarding_ptr->output_label)
		{
			mpls_pk_ptr->setLabel (forwarding_ptr->input_label);
			send_mac_addr = Uint64ToMac (forwarding_ptr->input_mac_address);
		}
		else
		{
			mpls_pk_ptr->setLabel(forwarding_ptr->return_label_input);
			send_mac_addr = Uint64ToMac (forwarding_ptr->mac_address);
		}
		mplsPurge (forwarding_ptr,false);

		if ((forwarding_ptr->order==LWMPLS_CHANGE)&&(!send_mac_addr.isUnspecified()))
		{
			Ieee80211DataFrame *frame = new Ieee80211DataFrame(mpls_pk_ptr->getName());
			frame->setReceiverAddress(send_mac_addr);
			if (mpls_pk_ptr->getControlInfo())
				delete mpls_pk_ptr->removeControlInfo();
			frame->encapsulate(mpls_pk_ptr);
			if (frame->getReceiverAddress().isUnspecified())
				ASSERT(!frame->getReceiverAddress().isUnspecified());
			sendOrEnqueue(frame);
		}
		else
		{
			delete mpls_pk_ptr;
			//deleteRegisterLabel(forwarding_ptr->input_label);
			//deleteRegisterLabel(forwarding_ptr->return_label_input);
		}
		/* */
		mplsData->deleteForwarding(forwarding_ptr);
	}
}

void Ieee80211Mesh::mplsForwardData(int label,LWMPLSPacket *mpls_pk_ptr,MACAddress sta_addr,LWmpls_Forwarding_Structure *forwarding_data)
{
		/* Extraer la etiqueta y la direcci�n de enlace del siguiente salto */
	LWmpls_Forwarding_Structure * forwarding_ptr = forwarding_data;
	if (forwarding_ptr==NULL)
		forwarding_ptr =  mplsData->lwmpls_forwarding_data(label,0,0);
	forwarding_ptr->last_use=simTime();
	bool is_source=false;
	int output_label,input_label_aux;
	MACAddress send_mac_addr;

	if (forwarding_ptr->order==LWMPLS_CHANGE || is_source)
	{
		if ((label == forwarding_ptr->input_label) || is_source)
		{
			output_label=forwarding_ptr->output_label;
			input_label_aux=forwarding_ptr->return_label_input;
			send_mac_addr = Uint64ToMac (forwarding_ptr->mac_address);
		}
		else
		{
			output_label=forwarding_ptr->return_label_output;
			input_label_aux=forwarding_ptr->input_label;
			send_mac_addr = Uint64ToMac(forwarding_ptr->input_mac_address);
		}
		if (output_label > 0)
		{
			mpls_pk_ptr->setLabel(output_label);
		}
		else
		{
			mpls_pk_ptr->setType (WMPLS_BEGIN);
			mpls_pk_ptr->setLabel(input_label_aux);
		}
		// Enviar al mac
				// polling = wlan_poll_list_member_find (send_mac_addr);
				// wlan_hlpk_enqueue (mpls_pk_ptr, send_mac_addr, polling,false);

		if (routingModuleReactive)
		{
			Uint128 src(mpls_pk_ptr->getSource());
			Uint128 dst(mpls_pk_ptr->getDest());
			Uint128 nextHop(send_mac_addr);
			Uint128 prev(sta_addr);
			routingModuleReactive->setRefreshRoute(src,dst,nextHop,prev);
		}

		sendOrEnqueue(encapsulate(mpls_pk_ptr,send_mac_addr));
		return;
	}
	else if (forwarding_ptr->order==LWMPLS_EXTRACT)
	{

		if (mpls_pk_ptr->getDest()==myAddress)
		{
			sendUp(decapsulateMpls(mpls_pk_ptr));
			return;
		}
		mplsBasicSend(mpls_pk_ptr,sta_addr);
		return;


		if (!(dynamic_cast<LWMPLSPacket*> (mpls_pk_ptr->getEncapsulatedMsg())))
		{
// Source or destination?

			if (sta_addr!= Uint64ToMac (forwarding_ptr->input_mac_address) || forwarding_ptr->mac_address ==0)
			{
				mplsBasicSend(mpls_pk_ptr,sta_addr);
				return;
			}

			output_label = forwarding_ptr->output_label;
			send_mac_addr = Uint64ToMac(forwarding_ptr->mac_address);

			if (output_label>0)
			{
				forwarding_ptr->order=LWMPLS_CHANGE;
				mpls_pk_ptr->setLabel(output_label);
				sendOrEnqueue(encapsulate(mpls_pk_ptr,send_mac_addr));
			}
			else
			{
				mpls_pk_ptr->setLabel (forwarding_ptr->return_label_input);
				if (forwarding_ptr->path.size()>0)
				{
					mpls_pk_ptr->setType(WMPLS_BEGIN_W_ROUTE);
					int dist = forwarding_ptr->path.size()-2;
					mpls_pk_ptr->setVectorAddressArraySize(dist);
					for (int i =0;i<dist;i++)
						mpls_pk_ptr->setVectorAddress(i,Uint64ToMac(forwarding_ptr->path[i+1]));
				}
				else
					mpls_pk_ptr->setType(WMPLS_BEGIN);

				sendOrEnqueue(encapsulate(mpls_pk_ptr,send_mac_addr));
			}
		}
		else
		{
			cPacket *seg_pkptr =  mpls_pk_ptr->decapsulate();
			delete mpls_pk_ptr;
			mplsDataProcess((LWMPLSPacket*)seg_pkptr,sta_addr);
		}
				// printf("To application %d normal %f \n",time);
	}
}

void Ieee80211Mesh::mplsAckPath(int label,LWMPLSPacket *mpls_pk_ptr,MACAddress sta_addr)
{
		//   printf("ack %f\n",time);
	int label_out = mpls_pk_ptr->getLabelReturn ();

	/* es necesario ahora introducir los datos en la tabla */
	LWmpls_Forwarding_Structure * forwarding_ptr = mplsData->lwmpls_forwarding_data(label,0,0);

// Intermediate node


	int *labelOutPtr;
	int *labelInPtr;


	if (Uint64ToMac(forwarding_ptr->mac_address)==sta_addr)
	{
		labelOutPtr = &forwarding_ptr->output_label;
		labelInPtr = &forwarding_ptr->return_label_input;
	}
	else if (Uint64ToMac(forwarding_ptr->input_mac_address)==sta_addr)
	{
		labelOutPtr = &forwarding_ptr->return_label_output;
		labelInPtr = &forwarding_ptr->input_label;
	}
	else
	{
		delete mpls_pk_ptr;
		return;
	}

	if (*labelOutPtr==0)
	{
		*labelOutPtr=label_out;
		mplsData->lwmpls_forwarding_output_data_add(label_out,MacToUint64(sta_addr),forwarding_ptr,false);
	}
	else
	{
		if (*labelOutPtr!=label_out)
		{
			/* change of label */
			// prg_string_hash_table_item_remove (lwmpls_data_ptr->forwarding_table_output,forwarding_ptr->key_output);
			*labelOutPtr=label_out;
			mplsData->lwmpls_forwarding_output_data_add(label_out,MacToUint64(sta_addr),forwarding_ptr,false);
		}
	}

	forwarding_ptr->last_use=simTime();
	/* initialize the mac timer */
// init the
	LWmpls_Interface_Structure *interface=NULL;
	mplsData->lwmpls_init_interface(&interface,*labelInPtr,MacToUint64(sta_addr),LWMPLS_INPUT_LABEL_RETURN);
	mplsInitializeCheckMac ();

	if (forwarding_ptr->return_label_output>0 && forwarding_ptr->output_label>0)
		forwarding_ptr->order=LWMPLS_CHANGE;

	delete mpls_pk_ptr;
}

void Ieee80211Mesh::mplsDataProcess(LWMPLSPacket * mpls_pk_ptr,MACAddress sta_addr)
{
	int label;
	LWmpls_Forwarding_Structure *forwarding_ptr=NULL;
	bool         label_found;
	int code;
	simtime_t     time;

	/* First check for the case where the received segment contains the	*/
	/* entire data packet, i.e. the data is transmitted as a single		*/
	/* fragment.*/
	time = simTime();
	code = mpls_pk_ptr->getType();
	label =	mpls_pk_ptr->getLabel();
	bool is_source = false;
	label_found = true;

	if (code==WMPLS_ACK && mpls_pk_ptr->getDest()!=myAddress)
	{
		delete mpls_pk_ptr;
		return;
	}
   // printf("code %i my_address %d org %d lin %d %f \n",code,my_address,sta_addr,label,op_sim_time());
	bool testMplsData = (code!=WMPLS_BEGIN) && (code!=WMPLS_NOTFOUND) && (code!= WMPLS_BEGIN_W_ROUTE) &&
		(code!=WMPLS_SEND) && (code!=WMPLS_BROADCAST);

	if (testMplsData)
	{
		if ((code ==WMPLS_REFRES) && (label==0))
		{
			/* In this case the refresh message is used for refresh the mac connections */
			delete mpls_pk_ptr;
			// printf("refresh %f\n",time);
			// printf("fin 1 %i \n",code);
			return;
		}
		if (label>0)
		{
			if ((forwarding_ptr = mplsData->lwmpls_forwarding_data(label,0,0))!=NULL)
			{
				if  (code == WMPLS_NORMAL)
				{
					if (!is_source)
					{
						if (forwarding_ptr->input_label ==label && forwarding_ptr->input_mac_address!=sta_addr)
							forwarding_ptr = NULL;
						else if (forwarding_ptr->return_label_input ==label && forwarding_ptr->mac_address!=sta_addr)
							forwarding_ptr = NULL;
					}
				}
			}
			//printf (" %p \n",forwarding_ptr);
			if (forwarding_ptr == NULL)
				label_found = false;
		}

		if (!label_found)
		{
			if ((code ==WMPLS_NORMAL))
				mplsBasicSend ((LWMPLSPacket*)mpls_pk_ptr->dup(),sta_addr);
			if (code !=WMPLS_ACK)
				delete mpls_pk_ptr->decapsulate();

			// � es necesario destruir label_msg_ptr? mirar la memoria
			//op_pk_nfd_set_ptr (seg_pkptr, "pointer", label_msg_ptr);
			mpls_pk_ptr->setType(WMPLS_NOTFOUND);
			// Enviar el mensaje al mac
			//polling = wlan_poll_list_member_find (sta_addr);
			// wlan_hlpk_enqueue (mpls_pk_ptr, sta_addr, polling,true);
			sendOrEnqueue(encapsulate(mpls_pk_ptr,sta_addr));
			return;
		}
	}

	switch (code)
	{

		case WMPLS_NORMAL:
			mplsForwardData(label,mpls_pk_ptr,sta_addr,forwarding_ptr);
			break;

		case WMPLS_BEGIN:
		case WMPLS_BEGIN_W_ROUTE:
				mplsCreateNewPath(label,mpls_pk_ptr,sta_addr);
			break;

		case WMPLS_REFRES:
		  // printf("refresh %f\n",time);
			forwarding_ptr->last_use=simTime();
			if (forwarding_ptr->order==LWMPLS_CHANGE)
			{
				if (!(Uint64ToMac(forwarding_ptr->mac_address).isUnspecified()))
				{
					mpls_pk_ptr->setLabel(forwarding_ptr->output_label);
					sendOrEnqueue(encapsulate(mpls_pk_ptr,Uint64ToMac(forwarding_ptr->mac_address)));
				}
				else
					delete mpls_pk_ptr;
			}
			else if (forwarding_ptr->order==LWMPLS_EXTRACT)
			{
				delete mpls_pk_ptr;
			}
			break;

		case WMPLS_END:
			break;

		case WMPLS_BREAK:
			mplsBreakPath (label,mpls_pk_ptr,sta_addr);
			break;

		case WMPLS_NOTFOUND:
			mplsNotFoundPath(label,mpls_pk_ptr,sta_addr);
			break;

		case WMPLS_ACK:
			mplsAckPath(label,mpls_pk_ptr,sta_addr);
			break;
		case WMPLS_SEND:
			mplsBasicSend (mpls_pk_ptr,sta_addr);
			break;
		case WMPLS_ADITIONAL:
			break;
		case WMPLS_BROADCAST:

			uint32_t cont;
			uint32_t newCounter = mpls_pk_ptr->getCounter();

			if (mplsData->getBroadCastCounter(MacToUint64 (mpls_pk_ptr->getSource()),cont))
			{
				if (newCounter==cont)
				{
					delete mpls_pk_ptr;
					return;
				}
				else if (newCounter < cont) //
				{
					if (!(cont > UINT32_MAX-100 && newCounter<100)) // Dado la vuelta
					{
						delete mpls_pk_ptr;
						return;
					}
				}
			}
			mplsData->setBroadCastCounter(MacToUint64(mpls_pk_ptr->getSource()),newCounter);
			// send up and Resend
			sendUp(mpls_pk_ptr->getEncapsulatedMsg()->dup());
			sendOrEnqueue(encapsulate(mpls_pk_ptr,MACAddress::BROADCAST_ADDRESS));
			break;
   	}
}


/* clean the path and create the message WMPLS_BREAK and send */
void Ieee80211Mesh::mplsBreakMacLink (MACAddress macAddress)
{
	LWmpls_Forwarding_Structure *forwarding_ptr;

	uint64_t des_add;
	int out_label;
	uint64_t mac_id;
	mac_id = MacToUint64(macAddress);


	LWmpls_Interface_Structure * mac_ptr = mplsData->lwmpls_interface_structure(mac_id);
	if (!mac_ptr)
		return;

// Test para evitar falsos positivos por colisiones
	if ((simTime()-mac_ptr->lastUse())<mplsData->mplsMacLimit())
			return;

	int numRtr= mac_ptr->numRtr();

	if (numRtr<mplsData->mplsMaxMacRetry ())
	{
		mac_ptr->numRtr()=numRtr+1;
		return;
	}

	LWmplsInterfaceMap::iterator it = mplsData->interfaceMap->find(mac_id);
	if (it!= mplsData->interfaceMap->end())
		if (!it->second->numLabels())
		{
			delete it->second;
			mplsData->interfaceMap->erase(it);
		}

	for (unsigned int i = 1; i< mplsData->label_list.size();i++)
	{
		forwarding_ptr = mplsData->lwmpls_forwarding_data (i,0,0);
		if (forwarding_ptr!=NULL)
		{
			if ((forwarding_ptr->mac_address == mac_id) || (forwarding_ptr->input_mac_address == mac_id))
			{
				mplsPurge (forwarding_ptr,true);
				/* prepare and send break message */
				if (forwarding_ptr->input_mac_address == mac_id)
				{
					des_add	= forwarding_ptr->mac_address;
					out_label = forwarding_ptr->output_label;
				}
				else
				{
					des_add	= forwarding_ptr->input_mac_address;
					out_label = forwarding_ptr->return_label_output;
				}
				if (des_add!=0)
				{
					LWMPLSPacket *lwmplspk = new LWMPLSPacket;
					lwmplspk->setType(WMPLS_BREAK);
					lwmplspk->setLabel(out_label);
					sendOrEnqueue(encapsulate(lwmplspk,Uint64ToMac(des_add)));
				}
				mplsData->deleteForwarding(forwarding_ptr);
				forwarding_ptr=NULL;
			}
		}
	}
}


void Ieee80211Mesh::mplsCheckRouteTime ()
{
	simtime_t actual_time;
	bool active = false;
	LWmpls_Forwarding_Structure *forwarding_ptr;
	int out_label;
	uint64_t mac_id;
	uint64_t des_add;

	actual_time = simTime();

	LWmplsInterfaceMap::iterator it;

	for ( it=mplsData->interfaceMap->begin() ; it != mplsData->interfaceMap->end();)
	{
		if ((actual_time - it->second->lastUse()) < (multipler_active_break*timer_active_refresh))
		{
			it++;
			continue;
		}

		mac_id = it->second->macAddress();
		delete it->second;
		mplsData->interfaceMap->erase(it);
		it = mplsData->interfaceMap->begin();
		if (mac_id==0)
			continue;

		for (unsigned int i = 1; i< mplsData->label_list.size();i++)
		{
			forwarding_ptr = mplsData->lwmpls_forwarding_data (i,0,0);
			if (forwarding_ptr && (mac_id == forwarding_ptr->mac_address || mac_id == forwarding_ptr->input_mac_address))
			{
				mplsPurge (forwarding_ptr,true);
				/* prepare and send break message */
				if (forwarding_ptr->input_mac_address == mac_id)
				{
					des_add	= forwarding_ptr->mac_address;
					out_label = forwarding_ptr->output_label;
				}
				else
				{
					des_add	= forwarding_ptr->input_mac_address;
					out_label = forwarding_ptr->return_label_output;
				}
				if (des_add!=0)
				{
					LWMPLSPacket *lwmplspk = new LWMPLSPacket;
					lwmplspk->setType(WMPLS_BREAK);
					lwmplspk->setLabel(out_label);
					sendOrEnqueue(encapsulate(lwmplspk,Uint64ToMac(des_add)));
				}
				mplsData->deleteForwarding (forwarding_ptr);
			}
		}


	}

	if (mplsData->lwmpls_nun_labels_in_use ()>0)
		active=true;

	if (active_mac_break &&  active)
	{
		if (!WMPLSCHECKMAC.isScheduled())
			scheduleAt (actual_time+(multipler_active_break*timer_active_refresh),&WMPLSCHECKMAC);
	}
}


void Ieee80211Mesh::mplsInitializeCheckMac ()
{
	int list_size;
	bool active = false;

	if (active_mac_break == false)
	{
		return ;
	}

	list_size = mplsData->lwmpls_nun_labels_in_use ();

	if (list_size>0)
		active=true;

	if (active ==true)
	{
		if (!WMPLSCHECKMAC.isScheduled())
			scheduleAt (simTime()+(multipler_active_break*timer_active_refresh),&WMPLSCHECKMAC);
	}
	return;
}


void Ieee80211Mesh::mplsPurge (LWmpls_Forwarding_Structure *forwarding_ptr,bool purge_break)
{
// �Como? las colas estan en otra parte.
	bool purge;

	if (forwarding_ptr==NULL)
		return;

	for( cQueue::Iterator iter(dataQueue,1); !iter.end();)
	{
		cMessage *msg = (cMessage *) iter();
		purge=false;
		Ieee80211DataFrame *frame =  dynamic_cast<Ieee80211DataFrame*> (msg);
                if (frame==NULL)
		{
                    iter++;
                    continue;
		}
		LWMPLSPacket* mplsmsg = dynamic_cast<LWMPLSPacket*>(frame->getEncapsulatedMsg());
		if (mplsmsg!=NULL)
		{
			int label = mplsmsg->getLabel();
			int code = mplsmsg->getType();
			if (label ==0)
			{
                            iter++;
	                    continue;
			}
			if (code==WMPLS_NORMAL)
			{
				if ((forwarding_ptr->output_label==label &&  frame->getReceiverAddress() ==
					Uint64ToMac(forwarding_ptr->mac_address)) ||
					(forwarding_ptr->return_label_output==label && frame->getReceiverAddress() ==
					Uint64ToMac(forwarding_ptr->input_mac_address)))
					purge = true;
			}
			else if ((code==WMPLS_BEGIN) &&(purge_break==true))
			{
				if (forwarding_ptr->return_label_input==label &&  frame->getReceiverAddress() ==
				Uint64ToMac(forwarding_ptr->mac_address))
					purge = true;
			}
			else if ((code==WMPLS_BEGIN) &&(purge_break==false))
			{
				if (forwarding_ptr->output_label>0)
					if (forwarding_ptr->return_label_input==label &&  frame->getReceiverAddress() ==
					Uint64ToMac(forwarding_ptr->mac_address))
						purge = true;
			}
			if (purge == true)
			{
				dataQueue.remove(msg);
				mplsmsg = dynamic_cast<LWMPLSPacket*>(decapsulate(frame));
				delete msg;
				if (mplsmsg)
				{
					MACAddress prev;
					mplsBasicSend(mplsmsg,prev);
				}
				else
					delete mplsmsg;
				iter.init(dataQueue,1);
				continue;
			}
			else
			{
				iter++;
			}

		}
		else
			iter++;
	}
}


// Cada ver que se envia un mensaje sirve para generar mensajes de permanencia. usa los propios hellos para garantizar que se env�an mensajes

Ieee80211Mesh::~Ieee80211Mesh()
{
	if (mplsData)
	{
		delete mplsData;
		mplsData = NULL;
	}
}

Ieee80211Mesh::Ieee80211Mesh()
{
		mplsData = NULL;
		routingModuleProactive = NULL;
		routingModuleReactive = NULL;
		macBaseGateId = -1;
}

void Ieee80211Mesh::sendOut(cMessage *msg)
{
	//InterfaceEntry *ie = ift->getInterfaceById(msg->getKind());
	msg->setKind(0);
	//send(msg, macBaseGateId + ie->getNetworkLayerGateIndex());
	send(msg, "macOut",0);
}


//
// mac label address method
// Equivalent to the 802.11s forwarding mechanism
//

bool Ieee80211Mesh::forwardMessage (Ieee80211DataFrame *frame)
{
	cPacket *msg = frame->getEncapsulatedMsg();
	LWMPLSPacket *lwmplspk = dynamic_cast<LWMPLSPacket*> (msg);

	if (lwmplspk)
		return false;
	if ((routingModuleProactive != NULL) && (routingModuleProactive->isOurType(msg)))
		return false;
	else if ((routingModuleReactive != NULL) && routingModuleReactive->isOurType(msg))
		return false;
	else // Normal frame test if use the mac label address method
		return macLabelBasedSend(frame);

}

bool Ieee80211Mesh::macLabelBasedSend (Ieee80211DataFrame *frame)
{

	if (!frame)
		return false;

	if (frame->getAddress4()==myAddress || frame->getAddress4().isUnspecified())
		return false;

	uint64_t dest = MacToUint64(frame->getAddress4());
	uint64_t src = MacToUint64(frame->getAddress3());
	uint64_t prev = MacToUint64(frame->getTransmitterAddress());
	uint64_t next = mplsData->getForwardingMacKey(src,dest,prev);

	if (next)
	{
		frame->setReceiverAddress(Uint64ToMac(next));
	}
	else
	{
		Uint128 add[20];
		int dist=0;
		if (routingModuleProactive)
			dist = routingModuleProactive->getRoute(dest,add);

		if (dist==0 && routingModuleReactive)
		{
			int iface;
			if (routingModuleReactive->getNextHop(dest,add[0],iface))
					dist = 1;
		}

		if (dist==0)
		{
// Destination unreachable
			if (routingModuleReactive)
			{
				ControlManetRouting *ctrlmanet = new ControlManetRouting();
				ctrlmanet->setOptionCode(MANET_ROUTE_NOROUTE);
				ctrlmanet->setDestAddress(dest);
				ctrlmanet->setSrcAddress(myAddress);
				ctrlmanet->encapsulate(frame->decapsulate());
				delete frame;
				frame = NULL;
				send(ctrlmanet,"routingOutReactive");
			}
			else
			{
				delete frame;
				frame=NULL;
			}
		}
		else
		{
			if (routingModuleReactive)
			{
				routingModuleReactive->setRefreshRoute(src,dest,add[0],prev);
			}
			frame->setReceiverAddress(add[0].getMACAddress());
		}

	}
	//send(msg, macBaseGateId + ie->getNetworkLayerGateIndex());
	if (frame)
		sendOrEnqueue(frame);
	return true;
}

void Ieee80211Mesh::sendUp(cMessage *msg)
{
	if (isUpperLayer(msg))
		send(msg, "uppergateOut");
	else
		delete msg;
}

bool Ieee80211Mesh::isUpperLayer(cMessage *msg)
{
	if (dynamic_cast<IPDatagram*>(msg))
		return true;
	return false;
}

