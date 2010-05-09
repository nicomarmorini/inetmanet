//
// Copyright (C) 2008 Irene Ruengeler
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
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//


#include "IPAddressResolver.h"
#include "SCTPAssociation.h"
#include "SCTPClient.h"

#define MSGKIND_CONNECT  0
#define MSGKIND_SEND     1
#define MSGKIND_ABORT    2
#define MSGKIND_PRIMARY  3
#define MSGKIND_STOP	 5


Define_Module(SCTPClient);

void SCTPClient::initialize()
{
	const char * address;
	char* token;
	AddressVector addresses;
	sctpEV3<<"initialize SCTP Client\n";	
	numSessions = numBroken = packetsSent = packetsRcvd = bytesSent = echoedBytesSent = bytesRcvd = 0;
	WATCH(numSessions);
	WATCH(numBroken);
	WATCH(packetsSent);
	WATCH(packetsRcvd);
	WATCH(bytesSent);
	WATCH(bytesRcvd);
	
	// parameters
	address=par("address");

	token = strtok((char*)address,",");
	while (token != NULL)
	{
		addresses.push_back(IPvXAddress(token));
		token = strtok(NULL, ",");
	}
	int32 port = par("port");
	echoFactor = par("echoFactor");
	if (!echoFactor) echoFactor = false;
	ordered = (bool)par("ordered");
	if (strcmp(address,"")==0)
	{
		socket.bind(port);
	}
	else
	{
		socket.bindx(addresses, port);
	} 
	
	socket.setCallbackObject(this);
	socket.setOutputGate(gate("sctpOut"));
	setStatusString("waiting");

	timeMsg = new cMessage("CliAppTimer");
	numRequestsToSend = 0;
	numPacketsToReceive = 0;
	queueSize = par("queueSize");
	WATCH(numRequestsToSend);
	numBytes = par("requestLength");
	recordScalar("ums", numBytes);
	timeMsg->setKind(MSGKIND_CONNECT);
	scheduleAt((simtime_t)par("startTime"), timeMsg);
	sendAllowed = true;
	if ((simtime_t)par("stopTime")!=0)
	{
		stopTimer = new cMessage("StopTimer");
		stopTimer->setKind(MSGKIND_STOP);
		scheduleAt((simtime_t)par("stopTime"), stopTimer);
		timer = true;
	}
	else
	{
		timer = false;
		stopTimer = NULL;
	}
	if ((simtime_t)par("primaryTime")!=0)
	{
		primaryChangeTimer = new cMessage("PrimaryTime");
		primaryChangeTimer->setKind(MSGKIND_PRIMARY);
		scheduleAt((simtime_t)par("primaryTime"), primaryChangeTimer);
	}
   else
   {
      primaryChangeTimer = NULL;
   }
}

void SCTPClient::handleMessage(cMessage *msg)
{
	if (msg->isSelfMessage())
		handleTimer(msg);
	else
	{
		socket.processMessage(PK(msg));
	}
}

void SCTPClient::connect()
{
	const char *connectAddress = par("connectAddress");
	int32 connectPort = par("connectPort");
	uint32 outStreams = par("outboundStreams");
	socket.setOutboundStreams(outStreams);
	ev << "issuing OPEN command\n";
	setStatusString("connecting");
	ev<<"connect to address "<<connectAddress<<"\n";
	socket.connect(IPAddressResolver().resolve(connectAddress, 1), connectPort, (uint32)par("numRequestsPerSession"));

numSessions++;
}

void SCTPClient::close()
{
	setStatusString("closing");
	socket.close();
}


void SCTPClient::setStatusString(const char *s)
{
	if (ev.isGUI()) getDisplayString().setTagArg("t", 0, s);
}

void SCTPClient::socketEstablished(int32, void *)
{
  	int32 count = 0;
 	ev<<"SCTPClient: connected\n";
	setStatusString("connected");
	// determine number of requests in this session
	numRequestsToSend = (long) par("numRequestsPerSession");
	numPacketsToReceive = (long) par("numPacketsToReceive");
	if (numRequestsToSend<1) 
		numRequestsToSend = 0;
	// perform first request (next one will be sent when reply arrives)
	if ((numRequestsToSend>0 && !timer) || timer)
	{
		if ((simtime_t)par("thinkTime") > 0)
		{
			if (sendAllowed)
			{
				sendRequest();
				if (!timer)
					numRequestsToSend--;
			}
			timeMsg->setKind(MSGKIND_SEND);
			scheduleAt(simulation.getSimTime()+(simtime_t)par("thinkTime"), timeMsg);
			
		}
		else
		{
			if (queueSize>0)
			{
				while (((!timer && numRequestsToSend > 0) || timer) && count++ < queueSize*2 && sendAllowed)
				{
					if (count == queueSize*2)
						sendRequest();
					else
						sendRequest(false);
					if (!timer)
					{
						if (--numRequestsToSend == 0)
							sendAllowed = false;
					}

				}
				if (((!timer && numRequestsToSend>0) || timer) && sendAllowed)
					sendQueueRequest();
			}
			else
			{
				while (((!timer && numRequestsToSend>0) || timer) && sendAllowed)
				{
					if (!timer && numRequestsToSend==1)
						sendRequest(true);
					else
						sendRequest(false);
					if (!timer)
						numRequestsToSend--;
				}
			}
			
			if ((!timer && numPacketsToReceive == 0) && (simtime_t)par("waitToClose")>0)
			{	
				timeMsg->setKind(MSGKIND_ABORT);
				scheduleAt(simulation.getSimTime()+(simtime_t)par("waitToClose"), timeMsg);
			}
			if ((!timer && numRequestsToSend == 0) && (simtime_t)par("waitToClose")==0)
			{
				sctpEV3<<"socketEstablished:no more packets to send, call shutdown\n";
				socket.shutdown();
				if (timeMsg->isScheduled())
					cancelEvent(timeMsg);
			}	
		}
	}
}

void SCTPClient::sendQueueRequest()
{
	cPacket* cmsg = new cPacket("Queue");
	SCTPInfo* qinfo = new SCTPInfo();
	qinfo->setText(queueSize);
	cmsg->setKind(SCTP_C_QUEUE);
	qinfo->setAssocId(socket.getConnectionId());
	cmsg->setControlInfo(qinfo);  
	socket.sendRequest(cmsg);
	
}

void SCTPClient::sendRequestArrived()
{
int32 count = 0;

	sctpEV3<<"sendRequestArrived numRequestsToSend="<<numRequestsToSend<<"\n";
	while (((!timer && numRequestsToSend > 0) || timer) && count++ < queueSize && sendAllowed)
	{
		if (count == queueSize)
			sendRequest();
		else
			sendRequest(false);

		if (!timer)
			numRequestsToSend--;
		
		if ((!timer && numRequestsToSend == 0))
		{
			sctpEV3<<"no more packets to send, call shutdown\n";
			socket.shutdown();
			if (timeMsg->isScheduled())
				cancelEvent(timeMsg);
		}	
	}
	
	
}

void SCTPClient::socketDataArrived(int32, void *, cPacket *msg, bool)
{
	packetsRcvd++;
	 
	sctpEV3<<"Client received packet Nr "<<packetsRcvd<<" from SCTP\n";
	SCTPCommand* ind = check_and_cast<SCTPCommand*>(msg->removeControlInfo());
	bytesRcvd+=msg->getByteLength();
	if (echoFactor > 0)
	{
		SCTPSimpleMessage *smsg=check_and_cast<SCTPSimpleMessage*>(msg->dup());
		cPacket* cmsg = new cPacket("SVData");	
		echoedBytesSent+=smsg->getBitLength()/8;
		cmsg->encapsulate(smsg);
		if (ind->getSendUnordered())
			cmsg->setKind(SCTP_C_SEND_UNORDERED);
		else
			cmsg->setKind(SCTP_C_SEND_ORDERED);
		packetsSent++;
		delete msg;
		socket.send(cmsg, 1);
	}
	if ((long)par("numPacketsToReceive")>0)
	{
		numPacketsToReceive--;
		if (numPacketsToReceive == 0)
		{
			close();
		}
	}
	delete ind;
}


void SCTPClient::sendRequest(bool last)
{
	uint32 i;
	
	if (numBytes < 1)
		numBytes=1;
	 
	cPacket* cmsg = new cPacket("AppData");
	SCTPSimpleMessage* msg=new SCTPSimpleMessage("data");

	msg->setDataArraySize(numBytes);
	for (i=0; i<numBytes; i++)
	{
		msg->setData(i, 'a');
	}
	msg->setDataLen(numBytes);
	msg->setBitLength(numBytes * 8);
	msg->setCreationTime(simulation.getSimTime());
	cmsg->encapsulate(msg);
	if (ordered)
		cmsg->setKind(SCTP_C_SEND_ORDERED);
	else
		cmsg->setKind(SCTP_C_SEND_UNORDERED);
	// send SCTPMessage with SCTPSimpleMessage enclosed
	socket.send(cmsg, last);
	bytesSent+=numBytes;
}

void SCTPClient::handleTimer(cMessage *msg)
{
	
	switch (msg->getKind())
	{
		case MSGKIND_CONNECT:
			ev << "starting session call connect\n";
			connect();
			break;
	
		case MSGKIND_SEND:
			if (((!timer && numRequestsToSend>0) || timer))
			{
				if (sendAllowed)
				{
					sendRequest();
					if (!timer)
						numRequestsToSend--;
				}
				if ((simtime_t)par("thinkTime") > 0)
					scheduleAt(simulation.getSimTime()+(simtime_t)par("thinkTime"), timeMsg);
				
				if ((!timer && numRequestsToSend == 0) && (simtime_t)par("waitToClose")==0)
				{
					socket.shutdown();
					if (timeMsg->isScheduled())
						cancelEvent(timeMsg);
				}	
			}
			else if ((!timer && numRequestsToSend == 0) && (simtime_t)par("waitToClose")==0)
			{
					socket.shutdown();
					if (timeMsg->isScheduled())
						cancelEvent(timeMsg);
			}	
			break;
		case MSGKIND_ABORT:
			close();
			break;
		case MSGKIND_PRIMARY:
			setPrimaryPath((const char*)par("newPrimary"));
			break;
		case MSGKIND_STOP:
			numRequestsToSend=0;
			sendAllowed = false;
			socket.shutdown();
			if (timeMsg->isScheduled())
				cancelEvent(timeMsg);
			//socket.close();
			break;
		default:
			ev<<"MsgKind ="<<msg->getKind()<<" unknown\n";
			break;
	}
}


void SCTPClient::socketDataNotificationArrived(int32 connId, void *ptr, cPacket *msg)
{
	SCTPCommand *ind = check_and_cast<SCTPCommand *>(msg->removeControlInfo());
	cPacket* cmsg = new cPacket("CMSG");
	SCTPSendCommand *cmd = new SCTPSendCommand();
	cmd->setAssocId(ind->getAssocId());
	cmd->setSid(ind->getSid());
	cmd->setNumMsgs(ind->getNumMsgs());
	cmsg->setKind(SCTP_C_RECEIVE);
	cmsg->setControlInfo(cmd);  
	delete ind;
	socket.sendNotification(cmsg);
}

void SCTPClient::shutdownReceivedArrived(int32 connId)
{
	if (numRequestsToSend==0)
	{
		cPacket* cmsg = new cPacket("Request");
		SCTPInfo* qinfo = new SCTPInfo();
		cmsg->setKind(SCTP_C_NO_OUTSTANDING);
		qinfo->setAssocId(connId);
		cmsg->setControlInfo(qinfo);  
		socket.sendNotification(cmsg);
	}
}

void SCTPClient::socketPeerClosed(int32, void *)
{
	// close the connection (if not already closed)
	if (socket.getState()==SCTPSocket::PEER_CLOSED)
	{
		ev << "remote SCTP closed, closing here as well\n";
		close();
	}
}

void SCTPClient::socketClosed(int32, void *)
{
	// *redefine* to start another session etc.
	ev << "connection closed\n";
	setStatusString("closed");
   if (primaryChangeTimer)
   {
      cancelEvent(primaryChangeTimer);
      delete primaryChangeTimer;
      primaryChangeTimer = NULL;
   }
}

void SCTPClient::socketFailure(int32, void *, int32 code)
{
	// subclasses may override this function, and add code try to reconnect after a delay.
	ev << "connection broken\n";
	setStatusString("broken");
	
	numBroken++;
	
	// reconnect after a delay
	timeMsg->setKind(MSGKIND_CONNECT);
	scheduleAt(simulation.getSimTime()+(simtime_t)par("reconnectInterval"), timeMsg);
}

void SCTPClient::socketStatusArrived(int32 assocId, void *yourPtr, SCTPStatusInfo *status)
{
struct pathStatus ps;
	SCTPPathStatus::iterator i=sctpPathStatus.find(status->getPathId());
	if (i!=sctpPathStatus.end())
	{
		ps = i->second;
		ps.active=status->getActive();
	}
	else
	{
		ps.active = status->getActive();
		ps.pid = status->getPathId();
		ps.primaryPath = false;
		sctpPathStatus[ps.pid]=ps;
	}
}

void SCTPClient::setPrimaryPath (const char* str)
{

	cPacket* cmsg = new cPacket("CMSG");
	SCTPPathInfo *pinfo = new SCTPPathInfo();
	if (strcmp(str,"")!=0)
	{
		pinfo->setRemoteAddress(IPvXAddress(str));
	}
	else
	{
		str = (const char*)par("newPrimary");
		if (strcmp(str, "")!=0)
			pinfo->setRemoteAddress(IPvXAddress(str));
		else
		{
			str = (const char*)par("connectAddress");
			pinfo->setRemoteAddress(IPvXAddress(str));
		}
	}

	pinfo->setAssocId(socket.getConnectionId());
	cmsg->setKind(SCTP_C_PRIMARY);
	cmsg->setControlInfo(pinfo);  
	socket.sendNotification(cmsg);
}




void SCTPClient::sendqueueFullArrived(int32 assocId)
{
	sendAllowed = false;
}

void SCTPClient::addressAddedArrived(int32 assocId, IPvXAddress remoteAddr)
{
}

void SCTPClient::finish()
{
	if (timeMsg->isScheduled())
		cancelEvent(timeMsg);
	delete timeMsg;
	if (stopTimer)
	{
		cancelEvent(stopTimer);
		delete stopTimer;
	}
   if (primaryChangeTimer)
   {
      cancelEvent(primaryChangeTimer);
      delete primaryChangeTimer;
      primaryChangeTimer = NULL;
   }
	ev << getFullPath() << ": opened " << numSessions << " sessions\n";
	ev << getFullPath() << ": sent " << bytesSent << " bytes in " << packetsSent << " packets\n";
	ev << getFullPath() << ": received " << bytesRcvd << " bytes in " << packetsRcvd << " packets\n";
	sctpEV3<<"Client finished\n";
}

