/***************************************************************************
 * file:        ChannelAccessExtended.cc
 *
 * author:      Marc Loebbers
 *
 * copyright:   (C) 2004 Telecommunication Networks Group (TKN) at
 *              Technische Universitaet Berlin, Germany.
 * copyright:   (C) 2009 Juan-Carlos Maureira
 * copyright:   (C) 2009 Alfonso Ariza
 *
 *              This program is free software; you can redistribute it
 *              and/or modify it under the terms of the GNU General Public
 *              License as published by the Free Software Foundation; either
 *              version 2 of the License, or (at your option) any later
 *              version.
 *              For further information see file COPYING
 *              in the top level directory
 *
 *
 *				ChangeLog
 *				-- Added Multiple radios support (Juan-Carlos Maureira / Paula Uribe. INRIA 2009)
 *
 ***************************************************************************
 * part of:     framework implementation developed by tkn
 **************************************************************************/


#include "ChannelAccessExtended.h"


#define coreEV (ev.isDisabled()||!coreDebug) ? ev : ev << logName() << "::ChannelAccess: "

/**
 * Upon initialization ChannelAccess registers the nic parent module
 * to have all its connections handled by ChannelControl
 */
void ChannelAccessExtended::initialize(int stage)
{
    BasicModule::initialize(stage);

    if (stage == 0)
    {
        ccExt = ChannelControlExtended::get();
        if (ccExt==NULL)
        	cc = ChannelControl::get();
        else
        	cc = ccExt;

        // register to get a notification when position changes
        nb->subscribe(this, NF_HOSTPOSITION_UPDATED);
    }
    else if (stage == 2)
    {
        cModule *hostModule = findHost();
        if (ccExt)
        	myHostRef = ccExt->lookupHost(hostModule);
        else
        	myHostRef = cc->lookupHost(hostModule);
        if (myHostRef==0)
            error("host not registered yet in ChannelControl (this should be done by "
                  "the Mobility module -- maybe this host doesn't have one?)");
    }
}


/**
 * This function has to be called whenever a packet is supposed to be
 * sent to the channel.
 *
 * This function really sends the message away, so if you still want
 * to work with it you should send a duplicate!
 */
void ChannelAccessExtended::sendToChannel(AirFrame *msg)
{

	if (!ccExt)
	{
		ChannelAccess::sendToChannel(msg);
		return;
	}

    const  ChannelControl::ModuleList &neighbors= ccExt->getNeighbors(myHostRef);
    coreEV << "sendToChannel: sending to gates\n";

    // loop through all hosts in range
    ChannelControlExtended::ModuleList::const_iterator it;
    for (it = neighbors.begin(); it != neighbors.end(); ++it)
    {
        cModule *mod = *it;

        // we need to send to each radioIn[] gate
        //cGate *radioGate = mod->gate("radioIn");
        //if (radioGate == NULL)
        //    error("module %s must have a gate called radioIn", mod->getFullPath().c_str());

        // Check if the host is registered
        ChannelControlExtended::HostRefExtended h;
       	h = dynamic_cast<ChannelControlExtended::HostRefExtended> (ccExt->lookupHost(mod));
        if (h == NULL)
            error("cannot find module in channel control");

        // Get the radioIn gates connected to a radio on the given channel
        AirFrameExtended * msgAux = dynamic_cast<AirFrameExtended*>(msg);
        ChannelControlExtended::radioGatesList theRadioList;
        if (msgAux)
        	theRadioList = h->getHostGatesOnChannel(msg->getChannelNumber(),msgAux->getCarrierFrequency());
        else
        	theRadioList = h->getHostGatesOnChannel(msg->getChannelNumber(),0.0);
        // if there are some radio on the channel.
        if (theRadioList.size()>0) {

			//for (int i = 0; i < radioGate->size(); i++)
			//{
        	for(ChannelControlExtended::radioGatesList::iterator rit=theRadioList.begin();rit != theRadioList.end();rit++) {
				// ChannelControl::HostRef h = cc->lookupHost(mod);
				//if (h == NULL)
				//    error("cannot find module in channel control");

        		cGate* radioGate = (*rit);

        		//if (h->channel == msg->getChannelNumber())
				//{
					coreEV << "sending message to host listening on the same channel\n";
					// account for propagation delay, based on distance in meters
					// Over 300m, dt=1us=10 bit times @ 10Mbps
					sendDirect((cMessage *)msg->dup(), myHostRef->pos.distance(h->pos) / LIGHT_SPEED, msg->getDuration(), mod, radioGate->getId());
				//}
			}
        } else {
			coreEV << "skipping host listening on a different channel\n";
        }
    }
    // register transmission in ChannelControl
   	ccExt->addOngoingTransmission(myHostRef, msg);
}
