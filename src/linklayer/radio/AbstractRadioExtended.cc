//
// Copyright (C) 2006 Andras Varga, Levente Meszaros
// Based on the Mobility Framework's SnrEval by Marc Loebbers
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//


#include "AbstractRadioExtended.h"
#include "FWMath.h"
#include "PhyControlInfo_m.h"
#include "Ieee80211Consts.h"  //XXX for the COLLISION and BITERROR msg kind constants
#include "Radio80211aControlInfo_m.h"



#define MK_TRANSMISSION_OVER  1
#define MK_RECEPTION_COMPLETE 2


AbstractRadioExtended::AbstractRadioExtended() : rs(this->getId())
{
    radioModel = NULL;
    receptionModel = NULL;
    transceiverConnect = true;
}

void AbstractRadioExtended::initialize(int stage)
{
    ChannelAccessExtended::initialize(stage);

    EV << "Initializing AbstractRadio, stage=" << stage << endl;

    if (stage == 0)
    {
        gate("radioIn")->setDeliverOnReceptionStart(true);

        uppergateIn = findGate("uppergateIn");
        uppergateOut = findGate("uppergateOut");

        // read parameters
        transmitterPower = par("transmitterPower");
        if (transmitterPower > (double) (cc->par("pMax")))
            error("transmitterPower cannot be bigger than pMax in ChannelControl!");
        rs.setBitrate(par("bitrate"));
        rs.setChannelNumber(par("channelNumber"));
        rs.setRadioId(this->getId());
        thermalNoise = FWMath::dBm2mW(par("thermalNoise"));
        // carrierFrequency = cc->par("carrierFrequency");  // taken from ChannelControl
        sensitivity = FWMath::dBm2mW(par("sensitivity"));

        carrierFrequency = par("carrierFrequency");

        // initialize noiseLevel
        noiseLevel = thermalNoise;

        EV << "Initialized channel with noise: " << noiseLevel << " sensitivity: " << sensitivity <<
            endl;

        // initialize the pointer of the snrInfo with NULL to indicate
        // that currently no message is received
        snrInfo.ptr = NULL;

        // no channel switch pending
        newChannel = -1;

        // statistics
        numGivenUp = 0;
        numReceivedCorrectly = 0;
        lostVector.setName("MAC loss rate");

        // Initialize radio state. If thermal noise is already to high, radio
        // state has to be initialized as RECV
        rs.setState(RadioState::IDLE);
        if (noiseLevel >= sensitivity)
            rs.setState(RadioState::RECV);

        WATCH(noiseLevel);
        WATCH(rs);

        if (par("attenuationModel").stdstringValue ()=="tworay")
        	receptionModel = createReceptionModelTwoRay();
        if (par("attenuationModel").stdstringValue ()=="pathlost")
        	receptionModel = createReceptionModelPathLost();
        else
        	receptionModel = createReceptionModel();
        receptionModel->initializeFrom(this);

        radioModel = createRadioModel();
        radioModel->initializeFrom(this);
    }
    else if (stage == 1)
    {
        // tell initial values to MAC; must be done in stage 1, because they
        // subscribe in stage 0
        nb->fireChangeNotification(NF_RADIOSTATE_CHANGED, &rs);
        nb->fireChangeNotification(NF_RADIO_CHANNEL_CHANGED, &rs);
    }
    else if (stage == 2)
    {
        // tell initial channel number to ChannelControl; should be done in
        // stage==2 or later, because base class initializes myHostRef in that stage
    	// JcM Fix: Register radio considering multiples radio hosts
    	if (ccExt)
    	{
    		((ChannelControlExtended::HostRefExtended)myHostRef)->registerRadio(this);
    		ccExt->updateHostChannel(myHostRef, rs.getChannelNumber(),this,carrierFrequency);
    	}
    	else
    		cc->updateHostChannel(myHostRef, rs.getChannelNumber());
    }
}

void AbstractRadioExtended::finish()
{
}

AbstractRadioExtended::~AbstractRadioExtended()
{
    delete radioModel;
    delete receptionModel;

    // delete messages being received
    for (RecvBuff::iterator it = recvBuff.begin(); it!=recvBuff.end(); ++it)
        delete it->first;
}


bool AbstractRadioExtended::processAirFrame(AirFrame *airframe)
{

	int chnum = airframe->getChannelNumber();
	AirFrameExtended *airframeext = dynamic_cast<AirFrameExtended *>(airframe);
	if (ccExt && airframeext)
	{
		double perc = ccExt->getPercentage();
		double fqFrame = airframeext->getCarrierFrequency();
		if (fqFrame > 0.0 && carrierFrequency>0.0)
		{
			if (chnum == getChannelNumber() && (fabs((fqFrame - carrierFrequency)/carrierFrequency)<=perc))
				return true;
			else
				return false;
		}
		else
			return (chnum == getChannelNumber());
	}
	else
	{
		return (chnum == getChannelNumber());
	}
}
/**
 * The basic handle message function.
 *
 * Depending on the gate a message arrives handleMessage just calls
 * different handle*Msg functions to further process the message.
 *
 * Messages from the channel are also buffered here in order to
 * simulate a transmission delay
 *
 * You should not make any changes in this function but implement all
 * your functionality into the handle*Msg functions called from here.
 *
 * @sa handleUpperMsg, handleLowerMsgStart, handleLowerMsgEnd,
 * handleSelfMsg
 */
void AbstractRadioExtended::handleMessage(cMessage *msg)
{
    // handle commands
    if (msg->getArrivalGateId()==uppergateIn && !msg->isPacket() /*FIXME XXX ENSURE REALLY PLAIN cMessage ARE SENT AS COMMANDS!!! && msg->getBitLength()==0*/)
    {
        cPolymorphic *ctrl = msg->removeControlInfo();
        if (msg->getKind()==0)
            error("Message '%s' with length==0 is supposed to be a command, but msg kind is also zero", msg->getName());
        handleCommand(msg->getKind(), ctrl);
        delete msg;
        return;
    }

    if (msg->getArrivalGateId() == uppergateIn)
    {
        AirFrameExtended *airframe = encapsulatePacket(PK(msg));
        handleUpperMsg(airframe);
    }
    else if (msg->isSelfMessage())
    {
        handleSelfMsg(msg);
    }
    else if (processAirFrame (check_and_cast<AirFrame*>(msg)))
    {
        // must be an AirFrame
    	AirFrame *airframe = (AirFrame *) msg;
        handleLowerMsgStart(airframe);
        bufferMsg(airframe);
    }
    else
    {
        EV << "listening to different channel when receiving message -- dropping it\n";
        delete msg;
    }
}

/**
 * The packet is put in a buffer for the time the transmission would
 * last in reality. A timer indicates when the transmission is
 * complete. So, look at unbufferMsg to see what happens when the
 * transmission is complete..
 */
void AbstractRadioExtended::bufferMsg(AirFrame *airframe) //FIXME: add explicit simtime_t atTime arg?
{
    // set timer to indicate transmission is complete
    cMessage *endRxTimer = new cMessage("endRx", MK_RECEPTION_COMPLETE);
    endRxTimer->setContextPointer(airframe);
    airframe->setContextPointer(endRxTimer);

    // NOTE: use arrivalTime instead of simTime, because we might be calling this
    // function during a channel change, when we're picking up ongoing transmissions
    // on the channel -- and then the message's arrival time is in the past!
    scheduleAt(airframe->getArrivalTime() + airframe->getDuration(), endRxTimer);
}

AirFrameExtended *AbstractRadioExtended::encapsulatePacket(cPacket *frame)
{
    PhyControlInfo *ctrl = dynamic_cast<PhyControlInfo *>(frame->removeControlInfo());
    ASSERT(!ctrl || ctrl->getChannelNumber()==-1); // per-packet channel switching not supported

    // Note: we don't set length() of the AirFrameExtended, because duration will be used everywhere instead
    if (ctrl && ctrl->getAdativeSensitivity()) updateSensitivity(ctrl->getBitrate());
    AirFrameExtended *airframe = createAirFrame();
    airframe->setName(frame->getName());
    airframe->setPSend(transmitterPower);
    airframe->setChannelNumber(getChannelNumber());
    airframe->encapsulate(frame);
    airframe->setBitrate(ctrl ? ctrl->getBitrate() : rs.getBitrate());
    airframe->setDuration(radioModel->calculateDuration(airframe));
    airframe->setSenderPos(getMyPosition());
	airframe->setCarrierFrequency(carrierFrequency);
    delete ctrl;

    EV << "Frame (" << frame->getClassName() << ")" << frame->getName()
       << " will be transmitted at " << (airframe->getBitrate()/1e6) << "Mbps\n";
    return airframe;
}

void AbstractRadioExtended::sendUp(AirFrameExtended *airframe)
{
    cPacket *frame = airframe->decapsulate();
    Radio80211aControlInfo * cinfo = new Radio80211aControlInfo;
    cinfo->setSnr(airframe->getSnr());
    cinfo->setLossRate(airframe->getLossRate());
    cinfo->setRecPow(airframe->getPowRec());
    frame->setControlInfo(cinfo);

    delete airframe;
    EV << "sending up frame " << frame->getName() << endl;
    send(frame, uppergateOut);
}

void AbstractRadioExtended::sendDown(AirFrameExtended *airframe)
{
	if (transceiverConnect)
		sendToChannel(airframe);
	else
		delete airframe;
}

/**
 * Get the context pointer to the now completely received AirFrameExtended and
 * delete the self message
 */
AirFrameExtended *AbstractRadioExtended::unbufferMsg(cMessage *msg)
{
	AirFrameExtended *airframe = (AirFrameExtended *) msg->getContextPointer();
    //delete the self message
    delete msg;

    return airframe;
}

/**
 * If a message is already being transmitted, an error is raised.
 *
 * Otherwise the RadioState is set to TRANSMIT and a timer is
 * started. When this timer expires the RadioState will be set back to RECV
 * (or IDLE respectively) again.
 *
 * If the host is receiving a packet this packet is from now on only
 * considered as noise.
 */
void AbstractRadioExtended::handleUpperMsg(AirFrameExtended *airframe)
{
    if (rs.getState() == RadioState::TRANSMIT)
        error("Trying to send a message while already transmitting -- MAC should "
              "take care this does not happen");

    // if a packet was being received, it is corrupted now as should be treated as noise
    if (snrInfo.ptr != NULL)
    {
        EV << "Sending a message while receiving another. The received one is now corrupted.\n";

        // remove the snr information stored for the message currently being
        // received. This message is treated as noise now and the
        // receive power has to be added to the noiseLevel

        // delete the pointer to indicate that no message is being received
        snrInfo.ptr = NULL;
        // clear the snr list
        snrInfo.sList.clear();
        // add the receive power to the noise level
        noiseLevel += snrInfo.rcvdPower;
    }

    // now we are done with all the exception handling and can take care
    // about the "real" stuff

    // change radio status
    EV << "sending, changing RadioState to TRANSMIT\n";
    setRadioState(RadioState::TRANSMIT);

    cMessage *timer = new cMessage(NULL, MK_TRANSMISSION_OVER);
    scheduleAt(simTime() + airframe->getDuration(), timer);
    sendDown(airframe);
}

void AbstractRadioExtended::handleCommand(int msgkind, cPolymorphic *ctrl)
{
    if (msgkind==PHY_C_CONFIGURERADIO)
    {
        // extract new channel number
        PhyControlInfo *phyCtrl = check_and_cast<PhyControlInfo *>(ctrl);
        int newChannel = phyCtrl->getChannelNumber();
        double newBitrate = phyCtrl->getBitrate();
        delete ctrl;

        if (newChannel!=-1)
        {
            EV << "Command received: change to channel #" << newChannel << "\n";

            // do it
            if (rs.getChannelNumber()==newChannel)
                EV << "Right on that channel, nothing to do\n"; // fine, nothing to do
            else if (rs.getState()==RadioState::TRANSMIT) {
                EV << "We're transmitting right now, remembering to change after it's completed\n";
                this->newChannel = newChannel;
            } else
                changeChannel(newChannel); // change channel right now
        }
        if (newBitrate!=-1)
        {
            EV << "Command received: change bitrate to " << (newBitrate/1e6) << "Mbps\n";

            // do it
            if (rs.getBitrate()==newBitrate)
                EV << "Right at that bitrate, nothing to do\n"; // fine, nothing to do
            else if (rs.getState()==RadioState::TRANSMIT) {
                EV << "We're transmitting right now, remembering to change after it's completed\n";
                this->newBitrate = newBitrate;
            } else
                setBitrate(newBitrate); // change bitrate right now
        }
    }
    else if (msgkind==PHY_C_CHANGETRANSMITTERPOWER)
    {
    	 PhyControlInfo *phyCtrl = check_and_cast<PhyControlInfo *>(ctrl);
    	 double newTransmitterPower = phyCtrl->getTransmitterPower();
    	 if (newTransmitterPower!=-1)
    	 {
    	      if (newTransmitterPower > (double) (cc->par("pMax")))
    	    	  transmitterPower = (double) (cc->par("pMax"));
    	      else
    	    	  transmitterPower = newTransmitterPower;
    	 }
    }
    else
    {
        error("unknown command (msgkind=%d)", msgkind);
    }
}

void AbstractRadioExtended::handleSelfMsg(cMessage *msg)
{
    EV<<"AbstractRadio::handleSelfMsg"<<msg->getKind()<<endl;
    if (msg->getKind()==MK_RECEPTION_COMPLETE)
    {
        EV << "frame is completely received now\n";

        // unbuffer the message
        AirFrameExtended *airframe = unbufferMsg(msg);

        handleLowerMsgEnd(airframe);
    }
    else if (msg->getKind() == MK_TRANSMISSION_OVER)
    {
        // Transmission has completed. The RadioState has to be changed
        // to IDLE or RECV, based on the noise level on the channel.
        // If the noise level is bigger than the sensitivity switch to receive mode,
        // otherwise to idle mode.
        if (noiseLevel < sensitivity)
        {
            // set the RadioState to IDLE
            EV << "transmission over, switch to idle mode (state:IDLE)\n";
            setRadioState(RadioState::IDLE);
        }
        else
        {
            // set the RadioState to RECV
            EV << "transmission over but noise level too high, switch to recv mode (state:RECV)\n";
            setRadioState(RadioState::RECV);
        }

        // delete the timer
        delete msg;

        // switch channel if it needs be
        if (newChannel!=-1)
        {
            changeChannel(newChannel);
            newChannel = -1;
        }
    }
    else
    {
        error("Internal error: unknown self-message `%s'", msg->getName());
    }
    EV<<"AbstractRadio::handleSelfMsg END"<<endl;
}


/**
 * This function is called right after a packet arrived, i.e. right
 * before it is buffered for 'transmission time'.
 *
 * First the receive power of the packet has to be calculated and is
 * stored in the recvBuff. Afterwards it has to be decided whether the
 * packet is just noise or a "real" packet that needs to be received.
 *
 * The message is not treated as noise if all of the following
 * conditions apply:
 *
 * -# the power of the received signal is higher than the sensitivity.
 * -# the host is currently not sending a message
 * -# no other packet is already being received
 *
 * If all conditions apply a new SnrList is created and the RadioState
 * is changed to RECV.
 *
 * If the packet is just noise the receive power is added to the noise
 * Level of the channel. Additionally the snr information of the
 * currently being received message (if any) has to be updated as
 * well as the RadioState.
 */
void AbstractRadioExtended::handleLowerMsgStart(AirFrame* airframe)
{
    // Calculate the receive power of the message

    // calculate distance
    const Coord& myPos = getMyPosition();
    const Coord& framePos = airframe->getSenderPos();
    double distance = myPos.distance(framePos);

    // calculate receive power
    AirFrameExtended * airframeExt = dynamic_cast<AirFrameExtended * >(airframe);

    double frequency = carrierFrequency;
    if (airframeExt)
    {
    	if (airframeExt->getCarrierFrequency()>0.0)
    		frequency = airframeExt->getCarrierFrequency();

    }

    double rcvdPower = receptionModel->calculateReceivedPower(airframe->getPSend(), frequency, distance);
    airframe->setPowRec(rcvdPower);
    // store the receive power in the recvBuff
    recvBuff[airframe] = rcvdPower;

    // if receive power is bigger than sensitivity and if not sending
    // and currently not receiving another message and the message has
    // arrived in time
    // NOTE: a message may have arrival time in the past here when we are
    // processing ongoing transmissions during a channel change
    if (airframe->getArrivalTime() == simTime() && rcvdPower >= sensitivity && rs.getState() != RadioState::TRANSMIT && snrInfo.ptr == NULL)
    {
        EV << "receiving frame " << airframe->getName() << endl;

        // Put frame and related SnrList in receive buffer
        SnrList snrList;
        snrInfo.ptr = airframe;
        snrInfo.rcvdPower = rcvdPower;
        snrInfo.sList = snrList;

        // add initial snr value
        addNewSnr();

        if (rs.getState() != RadioState::RECV)
        {
            // publish new RadioState
            EV << "publish new RadioState:RECV\n";
            setRadioState(RadioState::RECV);
        }
    }
    // receive power is too low or another message is being sent or received
    else
    {
        EV << "frame " << airframe->getName() << " is just noise\n";
        //add receive power to the noise level
        noiseLevel += rcvdPower;

        // if a message is being received add a new snr value
        if (snrInfo.ptr != NULL)
        {
            // update snr info for currently being received message
            EV << "adding new snr value to snr list of message being received\n";
            addNewSnr();
        }

        // update the RadioState if the noiseLevel exceeded the threshold
        // and the radio is currently not in receive or in send mode
        if (noiseLevel >= sensitivity && rs.getState() == RadioState::IDLE)
        {
            EV << "setting radio state to RECV\n";
            setRadioState(RadioState::RECV);
        }
    }
}


/**
 * This function is called right after the transmission is over,
 * i.e. right after unbuffering.  The noise level of the channel and
 * the snr information of the buffered messages have to be updated.
 *
 * Additionally the RadioState has to be updated.
 *
 * If the corresponding AirFrame was not only noise the corresponding
 * SnrList and the AirFrame are sent to the decider.
 */
void AbstractRadioExtended::handleLowerMsgEnd(AirFrameExtended * airframe)
{
    // check if message has to be send to the decider
    if (snrInfo.ptr == airframe)
    {
        EV << "reception of frame over, preparing to send packet to upper layer\n";
        // get Packet and list out of the receive buffer:
        SnrList list;
        list = snrInfo.sList;

        // delete the pointer to indicate that no message is currently
        // being received and clear the list
        snrInfo.ptr = NULL;
        snrInfo.sList.clear();

        airframe->setSnr(10*log10(recvBuff[airframe]/ noiseLevel));//ahmed
        airframe->setLossRate(lossRate);
        // delete the frame from the recvBuff
        recvBuff.erase(airframe);

        //XXX send up the frame:
        //if (radioModel->isReceivedCorrectly(airframe, list))
        //    sendUp(airframe);
        //else
        //    delete airframe;
        if (!radioModel->isReceivedCorrectly(airframe, list))
        {
            airframe->getEncapsulatedMsg()->setKind(list.size()>1 ? COLLISION : BITERROR);
            airframe->setName(list.size()>1 ? "COLLISION" : "BITERROR");

            numGivenUp++;
        }
        else
            numReceivedCorrectly++;

        if ( (numReceivedCorrectly+numGivenUp)%50 == 0)
        {
            lossRate = (double)numGivenUp/((double)numReceivedCorrectly+(double)numGivenUp);
            lostVector.record(lossRate);
            numReceivedCorrectly = 0;
            numGivenUp = 0;
        }
        sendUp(airframe);
    }
    // all other messages are noise
    else
    {
        EV << "reception of noise message over, removing recvdPower from noiseLevel....\n";
        // get the rcvdPower and subtract it from the noiseLevel
        noiseLevel -= recvBuff[airframe];

        // delete message from the recvBuff
        recvBuff.erase(airframe);

        // update snr info for message currently being received if any
        if (snrInfo.ptr != NULL)
        {
            addNewSnr();
        }

        // message should be deleted
        delete airframe;
        EV << "message deleted\n";
    }

    // check the RadioState and update if necessary
    // change to idle if noiseLevel smaller than threshold and state was
    // not idle before
    // do not change state if currently sending or receiving a message!!!
    if (noiseLevel < sensitivity && rs.getState() == RadioState::RECV && snrInfo.ptr == NULL)
    {
        // publish the new RadioState:
        EV << "new RadioState is IDLE\n";
        setRadioState(RadioState::IDLE);
    }
}

void AbstractRadioExtended::addNewSnr()
{
    SnrListEntry listEntry;     // create a new entry
    listEntry.time = simTime();
    listEntry.snr = snrInfo.rcvdPower / noiseLevel;
    snrInfo.sList.push_back(listEntry);
}

void AbstractRadioExtended::changeChannel(int channel)
{
    if (channel == rs.getChannelNumber())
        return;
    if (channel < 0 || channel >= cc->getNumChannels())
        error("changeChannel(): channel number %d is out of range (hint: numChannels is a parameter of ChannelControl)", channel);
    if (rs.getState() == RadioState::TRANSMIT)
        error("changing channel while transmitting is not allowed");

    // if we are currently receiving, must clean that up before moving to different channel
    if (rs.getState() == RadioState::RECV)
    {
        // delete messages being received, and cancel associated self-messages
        for (RecvBuff::iterator it = recvBuff.begin(); it!=recvBuff.end(); ++it)
        {
        	AirFrame *airframe = it->first;
            cMessage *endRxTimer = (cMessage *)airframe->getContextPointer();
            delete airframe;
            delete cancelEvent(endRxTimer);
        }
        recvBuff.clear();
    }

    // clear snr info
    snrInfo.ptr = NULL;
    snrInfo.sList.clear();

    // do channel switch
    EV << "Changing to channel #" << channel << "\n";

    rs.setChannelNumber(channel);

    //cc->updateHostChannel(myHostRef, channel);
    if (ccExt)
    {
    	ccExt->updateHostChannel(myHostRef, rs.getChannelNumber(),this,carrierFrequency);
    }
    else
    {
    	cc->updateHostChannel(myHostRef, rs.getChannelNumber());
    }

    cModule *myHost = findHost();
    //cGate *radioGate = myHost->gate("radioIn");
    cGate* radioGate = this->gate("radioIn")->getPathStartGate();

    // pick up ongoing transmissions on the new channel
    EV << "Picking up ongoing transmissions on new channel:\n";
    if (ccExt)
    {
    	ChannelControlExtended::TransmissionList tlAux = ccExt->getOngoingTransmissions(channel);
    	for (ChannelControlExtended::TransmissionList::const_iterator it = tlAux.begin(); it != tlAux.end(); ++it)
    	{
    		AirFrameExtended *airframe = check_and_cast<AirFrameExtended *> (*it);
        // time for the message to reach us
    		double distance = myHostRef->pos.distance(airframe->getSenderPos());
    		simtime_t propagationDelay = distance / LIGHT_SPEED;

        // if this transmission is on our new channel and it would reach us in the future, then schedule it
    		if (channel == airframe->getChannelNumber())
    		{
    			EV << " - (" << airframe->getClassName() << ")" << airframe->getName() << ": ";
    		}

            // if there is a message on the air which will reach us in the future
            if (airframe->getTimestamp() + propagationDelay >= simTime())
            {
                 EV << "will arrive in the future, scheduling it\n";

                 // we need to send to each radioIn[] gate of this host
                 for (int i = 0; i < radioGate->size(); i++)
                     sendDirect(airframe->dup(), airframe->getTimestamp() + propagationDelay - simTime(), airframe->getDuration(), myHost, radioGate->getId() + i);
            }
            // if we hear some part of the message
            else if (airframe->getTimestamp() + airframe->getDuration() + propagationDelay > simTime())
            {
                 EV << "missed beginning of frame, processing it as noise\n";

                 AirFrameExtended *frameDup = airframe->dup();
                 frameDup->setArrivalTime(airframe->getTimestamp() + propagationDelay);
                 handleLowerMsgStart(frameDup);
                 bufferMsg(frameDup);
            }
            else
            {
                EV << "in the past\n";
            }
        }
    }
    else
    {
    	ChannelControl::TransmissionList tlAux = cc->getOngoingTransmissions(channel);
     	for (ChannelControl::TransmissionList::const_iterator it = tlAux.begin(); it != tlAux.end(); ++it)
    	{
     	   AirFrame *airframe = *it;
     	   // time for the message to reach us
     	   double distance = myHostRef->pos.distance(airframe->getSenderPos());
     	   simtime_t propagationDelay = distance / LIGHT_SPEED;

     	   // if this transmission is on our new channel and it would reach us in the future, then schedule it
     	   if (channel == airframe->getChannelNumber())
     	   {
     		   EV << " - (" << airframe->getClassName() << ")" << airframe->getName() << ": ";
     		   // if there is a message on the air which will reach us in the future
     		   if (airframe->getTimestamp() + propagationDelay >= simTime())
     		   {
     			   EV << "will arrive in the future, scheduling it\n";
     			   // we need to send to each radioIn[] gate of this host
     			   for (int i = 0; i < radioGate->size(); i++)
     				   sendDirect(airframe->dup(), airframe->getTimestamp() + propagationDelay - simTime(), airframe->getDuration(), myHost, radioGate->getId() + i);
     			}
     		   // if we hear some part of the message
     		    else if (airframe->getTimestamp() + airframe->getDuration() + propagationDelay > simTime())
     	        {
     			   EV << "missed beginning of frame, processing it as noise\n";

     			   AirFrame *frameDup = airframe->dup();
     			   frameDup->setArrivalTime(airframe->getTimestamp() + propagationDelay);
     			   handleLowerMsgStart(frameDup);
     			   bufferMsg((AirFrameExtended*) frameDup);
     			 }
     	         else
     	         {
     	           EV << "in the past\n";
     	         }
     	   }
    	}
    }

    // notify other modules about the channel switch; and actually, radio state has changed too
    nb->fireChangeNotification(NF_RADIO_CHANNEL_CHANGED, &rs);
    nb->fireChangeNotification(NF_RADIOSTATE_CHANGED, &rs);
}

void AbstractRadioExtended::setBitrate(double bitrate)
{
    if (rs.getBitrate() == bitrate)
        return;
    if (bitrate < 0)
        error("setBitrate(): bitrate cannot be negative (%g)", bitrate);
    if (rs.getState() == RadioState::TRANSMIT)
        error("changing the bitrate while transmitting is not allowed");

    EV << "Setting bitrate to " << (bitrate/1e6) << "Mbps\n";
    rs.setBitrate(bitrate);

    //XXX fire some notification?
}

void AbstractRadioExtended::setRadioState(RadioState::State newState)
{
    rs.setState(newState);
    nb->fireChangeNotification(NF_RADIOSTATE_CHANGED, &rs);
}

void AbstractRadioExtended::updateSensitivity(double rate)
{
    EV<<"bitrate = "<<rate<<endl;
    EV <<" sensitivity: "<<sensitivity<<endl;
    if (rate == 6E+6)
    {
        sensitivity = FWMath::dBm2mW (-82);
    }
    else if (rate == 9E+6)
    {
        sensitivity = FWMath::dBm2mW (-81);
    }
    else if (rate == 12E+6)
    {
        sensitivity = FWMath::dBm2mW (-79);
    }
    else if (rate == 18E+6)
    {
        sensitivity = FWMath::dBm2mW (-77);
    }
    else if (rate == 24E+6)
    {
        sensitivity = FWMath::dBm2mW (-74);
    }
    else if (rate == 36E+6)
    {
        sensitivity = FWMath::dBm2mW (-70);
    }
    else if (rate == 48E+6)
    {
        sensitivity = FWMath::dBm2mW (-66);
    }
    else if (rate == 54E+6)
    {
        sensitivity = FWMath::dBm2mW (-65);
    }
    EV <<" sensitivity after updateSensitivity: "<<sensitivity<<endl;
}

void AbstractRadioExtended::disconnectReceiver()
{
	if (ccExt)
   	{
   		((ChannelControlExtended::HostRefExtended)myHostRef)->unregisterRadio(this);
   	}
}

void AbstractRadioExtended::connectReceiver()
{
  	if (ccExt)
    {
  		((ChannelControlExtended::HostRefExtended)myHostRef)->registerRadio(this);
  		ccExt->updateHostChannel(myHostRef, rs.getChannelNumber(),this,carrierFrequency);
    }
}
