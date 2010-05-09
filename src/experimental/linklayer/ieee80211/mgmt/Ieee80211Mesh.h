//
// Copyright (C) 2006 Andras Varga
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

#ifndef IEEE80211_MESH_ADHOC_H
#define IEEE80211_MESH_ADHOC_H

#include <omnetpp.h>
#include "Ieee80211MgmtBase.h"
#include "NotificationBoard.h"
#include "IInterfaceTable.h"
#include "lwmpls_data.h"
#include "LWMPLSPacket_m.h"
#include "uint128.h"
#include "ManetRoutingBase.h"


/**
 * Used in 802.11 ligh wireless mpls  mode. See corresponding NED file for a detailed description.
 * This implementation ignores many details.
 *
 * @author Alfonso Ariza
 */

class INET_API Ieee80211Mesh : public Ieee80211MgmtBase
{


  private:
	cMessage WMPLSCHECKMAC;
	NotificationBoard *nb;
	bool proactiveFeedback;

	ManetRoutingBase *routingModuleProactive;
	ManetRoutingBase *routingModuleReactive;

	IInterfaceTable *ift;
	bool useLwmpls;

	LWMPLSDataStructure * mplsData;

	double multipler_active_break;
	simtime_t timer_active_refresh;
	bool active_mac_break;
	int macBaseGateId;  // id of the nicOut[0] gate


	cPacket * decapsulateMpls(LWMPLSPacket *frame);
	Ieee80211DataFrame *encapsulate(cPacket *msg,MACAddress dest);
	void mplsSendAck(int label);
	void mplsCreateNewPath(int label,LWMPLSPacket *mpls_pk_ptr,MACAddress sta_addr);
	void mplsBreakPath(int label,LWMPLSPacket *mpls_pk_ptr,MACAddress sta_addr);
	void mplsNotFoundPath(int label,LWMPLSPacket *mpls_pk_ptr,MACAddress sta_addr);
	void mplsForwardData(int label,LWMPLSPacket *mpls_pk_ptr,MACAddress sta_addr,LWmpls_Forwarding_Structure *forwarding_data);
	void mplsBasicSend (LWMPLSPacket *mpls_pk_ptr,MACAddress sta_addr);
	void mplsAckPath(int label,LWMPLSPacket *mpls_pk_ptr,MACAddress sta_addr);
	void mplsDataProcess(LWMPLSPacket * mpls_pk_ptr,MACAddress sta_addr);
	void mplsBreakMacLink (MACAddress mac_id);
	void mplsCheckRouteTime ();
	void mplsInitializeCheckMac();
	void mplsPurge (LWmpls_Forwarding_Structure *forwarding_ptr,bool purge_break);
	bool forwardMessage (Ieee80211DataFrame *);
	bool macLabelBasedSend (Ieee80211DataFrame *);



  public:
		Ieee80211Mesh();
  protected:
    virtual int numInitStages() const {return 5;}
    virtual void initialize(int);
    ~Ieee80211Mesh();


    virtual void handleMessage(cMessage*);

    /** Implements abstract to use routing protocols in the mac layer */
    virtual void handleRoutingMessage(cPacket*);

    /** Implements abstract Ieee80211MgmtBase method */
    virtual void handleTimer(cMessage *msg);

    /** Implements abstract Ieee80211MgmtBase method */
    virtual void handleUpperMessage(cPacket *msg);

    /** Implements abstract Ieee80211MgmtBase method -- throws an error (no commands supported) */
    virtual void handleCommand(int msgkind, cPolymorphic *ctrl);

    /** Utility function for handleUpperMessage() */
    virtual Ieee80211DataFrame *encapsulate(cPacket *msg);

    /** Called by the NotificationBoard whenever a change occurs we're interested in */
    virtual void receiveChangeNotification(int category, const cPolymorphic *details);

    /** @name Processing of different frame types */
    //@{
    virtual void handleDataFrame(Ieee80211DataFrame *frame);
    virtual void handleAuthenticationFrame(Ieee80211AuthenticationFrame *frame);
    virtual void handleDeauthenticationFrame(Ieee80211DeauthenticationFrame *frame);
    virtual void handleAssociationRequestFrame(Ieee80211AssociationRequestFrame *frame);
    virtual void handleAssociationResponseFrame(Ieee80211AssociationResponseFrame *frame);
    virtual void handleReassociationRequestFrame(Ieee80211ReassociationRequestFrame *frame);
    virtual void handleReassociationResponseFrame(Ieee80211ReassociationResponseFrame *frame);
    virtual void handleDisassociationFrame(Ieee80211DisassociationFrame *frame);
    virtual void handleBeaconFrame(Ieee80211BeaconFrame *frame);
    virtual void handleProbeRequestFrame(Ieee80211ProbeRequestFrame *frame);
    virtual void handleProbeResponseFrame(Ieee80211ProbeResponseFrame *frame);
    //@}
    /** Redefined from Ieee80211MgmtBase: send message to MAC */
    virtual void sendOut(cMessage *msg);
    /** Redefined from Ieee80211MgmtBase Utility method: sends the packet to the upper layer */
    virtual void sendUp(cMessage *msg);

    virtual bool isUpperLayer(cMessage *);
};

#endif


