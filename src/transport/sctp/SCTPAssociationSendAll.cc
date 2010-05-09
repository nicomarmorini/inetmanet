// $Id: SCTPAssociationSendAll.cc,v 1.29 2009/03/26 02:44:13 dreibh Exp $
// Copyright (C) 2007-2009 Irene Ruengeler
// Copyright (C) 2009 Thomas Dreibholz
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

#include "SCTPAssociation.h"
#include "SCTPAlgorithm.h"
#include "SCTPCommand_m.h"

#include <algorithm>


int32 SCTPAssociation::calculateBytesToSendOnPath(const SCTPPathVariables* pathVar)
{
   int32              bytesToSend;
   const SCTPDataMsg* datMsg = peekOutboundDataMsg();
   if(datMsg != NULL) {
      const uint32 ums = datMsg->getBooksize();   // Get user message size
         const uint32 num = (uint32)floor((pathVar->pmtu - 32) / (ums + SCTP_DATA_CHUNK_LENGTH));
         if (num * ums > state->peerRwnd) {
            // Receiver cannot handle data yet
            bytesToSend = 0;
         }
         else {
            // Receiver will accept data
            bytesToSend = num * ums;
         }
   }
   else {
      bytesToSend = 0;
   }
   return(bytesToSend);
}


void SCTPAssociation::sendAll(IPvXAddress pathId)
{
   // ====== Variables ======================================================
   SCTPPathVariables*       pathVar;
   SCTPMessage*             sctpMsg      = NULL;
   SCTPSackChunk*           sackChunk    = NULL;
   SCTPDataChunk*           chunkPtr     = NULL;
   SCTPDataVariables*       chunk        = NULL;

   IPvXAddress dpi, newDpi, pd;

   uint16 chunksAdded      = 0;
   uint16 dataChunksAdded  = 0;
   uint32 totalChunksSent  = 0;
   uint32 totalPacketsSent = 0;
   uint32 nextSSN          = 0;
   uint32 outstandingBytes = 0;

   int32 tcount            = 0;
   int32 rtcount           = 0;
   int32 scount            = 0;
   int32 bcount            = 0;
   int32 bytesToSend       = 0;

   bool headerCreated      = false;
   bool firstTime          = false;
   bool sendBeforeRtx      = false;
   bool rtxActive          = false;
   bool packetFull         = false;
   bool probing            = false;
   bool sendOneMorePacket  = false;

   // ====== Obtain path ====================================================
   sctpEV3 << "\n\n--------------------------\nsendAll: pathId=" << pathId << "\n";
   printSctpPathMap();
   if (pathId == IPvXAddress("0.0.0.0")) {
      pd = state->primaryPathIndex;
   }
   else {
      pd = pathId;
   }
   newDpi = pd;
   sctpEV3 << "sendAll: newDpi set to " << newDpi << ", dpi="  << dpi << "\n";


   // ====== Initialize counters ============================================
   // ------ Queued bytes for path pd ---------------------
   CounterMap::iterator tq = qCounter.roomTransQ.find(pd);
   if (tq != qCounter.roomTransQ.end()) {
      tcount = tq->second;
   }
   else {
      tcount = 0;
   }
   // ------ Queued retransmission bytes for path pd ------
   CounterMap::iterator rtq = qCounter.roomRetransQ.find(pd);
   if (rtq != qCounter.roomRetransQ.end()) {
      rtcount = rtq->second;
   }
   else {
      rtcount = 0;
   }
   scount = qCounter.roomSumSendStreams;   // includes header and padding
   bcount = qCounter.bookedSumSendStreams; // sums of bytes in send streams, counting the booksize; depends on whether header is included or not
   sctpEV3 << "\nsendAll: on " << pd << ":"
           << "  tcount="  << tcount
           << "  scount="  << scount
           << "  rtcount=" << rtcount
           << "  nextTSN=" << state->nextTSN << "\n";


   if (!state->stopSending)  // for M3UA
   {
      if (tcount > 0)
      {
         rtxActive = true;
         dpi = pd;
         sctpEV3 << "dpi=pd=" << dpi << "\n";
      }

     if (tcount == 0 && scount == 0) {
         // ====== No DATA chunks to send ==========================================
         sctpEV3 << "No DATA chunk available!\n";
         bytesToSend       = 0;
         bytes.bytesToSend = 0;
         bytes.packet      = false;
         bytes.chunk       = false;
         dpi = pathId;
         sctpEV3 << "sendAll: dpi=pathId=" << dpi << "\n";
      }
      else {
         // ====== There is something to send ===============================
            // ------ scount > 0 --------------------------------------------
            if (tcount == 0) {
                  dpi = state->primaryPathIndex;
                  sctpEV3 << "sendAll: dpi=primary=" << dpi <<"\n";
               firstTime = true;
            }
            if (!(state->resetPending && firstTime)) {
               bytesAllowedToSend(dpi);
            }
         newDpi = dpi;
         sctpEV3 << "sendAll: newDpi set to "<< newDpi <<" dpi=" << dpi << "\n";
      }

      if (newDpi==IPvXAddress("0.0.0.0"))
         opp_error("newDpi unspecified");

      bytesToSend = bytes.bytesToSend;

      if (bytesToSend < 0)
         bytesToSend=0;

      pathVar = getPath(dpi);

      sctpEV3 << "sendAll: after bytesAllowedToSend:"
              << "  dpi="         << dpi
              << "  bytesToSend=" << bytesToSend
              << "  packet="      << bytes.packet
              << "  chunk="       << bytes.chunk << "\n";

      if ((bytes.chunk) &&
          ((rtcount == 0) || (tcount > 0))) {
         // There is a chunk to send, it is not a retransmission.
         probing = true;   // ??? Bedeutung der Var. "probing"?
      }
      if (((state->numGaps > 0) || (state->dupList.size() > 0)) &&
          (state->sackAllowed)) {
         // Schedule sending of SACKs at once, when we have fragments to report
         sctpEV3 << "sendAll: gaps present ==> ackState = " << sackFrequency << "\n";
         state->ackState = sackFrequency;
      }
      if ((state->ackState < sackFrequency) &&
           (bytesToSend == 0) &&
           (bytes.chunk == false) &&
           (bytes.packet == false)) 
	{
         sctpEV3 << "sendAll: nothing to send... chunk pointer is NULL... BYE...\n";
         return;
      }

      sctpEV3 << "sendAll:"
              << "  bytesToSend=" << bytesToSend
              << "  outstandingBytes="         << pathVar->outstandingBytes
              << "  cwnd="        << pathVar->cwnd
              << "  arwnd="       << state->peerRwnd << "\n";

      outstandingBytes = pathVar->outstandingBytes;
      if ((int32)pathVar->outstandingBytes < 0) {
         opp_error("util %d ,tsna=%d", __LINE__, state->cTsnAck);
      }
   }
   else {   // state->stopSending
      dpi = pd;
   }


   // #######################################################################
   // #### Acknowledgements Handling                                     ####
   // #######################################################################

   sctpEV3 << "sendAll:"
           << "  ackState="  << state->ackState
           << "  SackTimer=" << SackTimer->isScheduled() << "\n";

   // ====== SACK ===========================================================
   if ((state->ackState >= sackFrequency) ||
       (SackTimer->isScheduled() && (state->alwaysBundleSack == true || pathId == dpi)) ) {
      // ------ Create SACK chunk -------------------------------------------
      sctpMsg = new SCTPMessage();
      sctpMsg->setBitLength(SCTP_COMMON_HEADER * 8);

      sctpEV3 << "sendAll: localPort=" << localPort
              << "  remotePort=" << remotePort << "\n";

      /* build a SACK chunk... */
      headerCreated = true;
      sackChunk = createSack();

      dpi = remoteAddr;

      sctpEV3 << "sendAll:  SACK: dpi=" << dpi << "\n";

      /* ...and add a SACK to the packet... */
      chunksAdded++;
      totalChunksSent++;

      // ------ Create AUTH chunk, if necessary -----------------------------

      // ------ Add SACK chunk ----------------------------------------------
      sctpMsg->addChunk(sackChunk);

      if ((tcount == 0) &&
          (bytesToSend > 0) &&
          (state->nagleEnabled) &&
          (state->peerRwnd < pathVar->pmtu) &&
          (pathVar->outstandingBytes > 0)) {
         // Test whether a full packet can be filled
         bytesToSend = calculateBytesToSendOnPath(pathVar);
         sctpEV3 << "sendAll:  peerRwnd=" << state->peerRwnd
                 << "  bytesToSend=" << bytesToSend
                 << "  nextTSN="     << state->nextTSN << "\n";
      }
      sctpEV3<<"sendAll: appended " << sackChunk->getBitLength()/8 << " bytes SACK chunk, msg length now " << sctpMsg->getBitLength()/8 << "\n";

      state->ackState = 0;
      // Stop SACK timer if it is running...
      stopTimer(SackTimer);
      sctpAlgorithm->sackSent();
      state->sackAllowed = false;
   }

   else
   {
      if ((tcount == 0) &&
          (bytesToSend > 0) &&
          (state->nagleEnabled) &&
          (state->peerRwnd < pathVar->pmtu) &&
          (pathVar->outstandingBytes > 0)) {
         // Test whether a full packet can be filled
         bytesToSend = calculateBytesToSendOnPath(pathVar);
      }

      if (bytesToSend > 0 || bytes.chunk || bytes.packet || (bytes.chunk && probing)) {
         // There is data to send ...

         if (tcount>0 || (scount>0 && (!(state->nagleEnabled && outstandingBytes>0)||(uint32)scount>=pathVar->pmtu-32 ))) {
            // ------ Create SCTP message -----------------------------------
            sctpMsg = new SCTPMessage();
            headerCreated = true;
            sctpMsg->setBitLength(SCTP_COMMON_HEADER * 8);

         }
      }
      else {
         sctpEV3 << "sendAll: no header created" << endl;
      }
   }


   // #######################################################################
   // #### Data Transmission                                             ####
   // #######################################################################

   while ((bytesToSend > 0 || bytes.chunk || bytes.packet ) &&
          (!(state->resetPending && firstTime)) &&
          (headerCreated) &&
          (!state->stopSending)) {
      sctpEV3 << "sendAll: transmission loop, bytesToSend=" << bytesToSend
              << ", bytes.chunk=" << bytes.chunk << ", bytes.packet=" << bytes.packet << endl;
      SCTPDataMsg*       datMsg = NULL;
      SCTPDataVariables* datVar = NULL;

      // ====== There is data in transmission queue => prepare retransmit ===
      if (tq->second > 0) {
         sctpEV3 << tq->second << " bytes are in the transQ\n";
         datVar = getOutboundDataChunk(dpi, pathVar->pmtu-sctpMsg->getBitLength() / 8 - 20);
         sctpEV3 << "sendAll:"
                 << "  sctpMsg->length=" << sctpMsg->getBitLength() / 8
                 << "  length datVar="   << pathVar->pmtu-sctpMsg->getBitLength() / 8 - 20 << "\n";
         if (datVar != NULL) {
            datVar->numberOfRetransmissions++;
            if (datVar->hasBeenAcked == false) {
               datVar->countsAsOutstanding = true;
               datVar->hasBeenRemoved      = false;
               getPath(datVar->nextDestination)->outstandingBytes += (datVar->booksize);
               sctpEV3 << "sendAll: outstandingBytes="
                       << getPath(datVar->nextDestination)->outstandingBytes << "\n";
               CounterMap::iterator iterator = qCounter.roomRetransQ.find(getPath(datVar->lastDestination)->remoteAddress);
                  iterator->second += ADD_PADDING(datVar->booksize + SCTP_DATA_CHUNK_LENGTH);
            }
         }
         else
         {
            sctpEV3 << "sendAll: datVar=NULL -> packetFull\n";
            packetFull = true;
         }
      }

      // ====== Prepare regular message transmission ========================
      else if ((scount > 0) && (dpi == state->primaryPathIndex)) {
         // ------ Check whether a whole packet can be filled ---------------
         sctpEV3 << "sendAll:  scount="   << scount
                 << "  nagle="            << state->nagleEnabled
                 << "  outstandingBytes=" << outstandingBytes
                 << "  mtu="              << pathVar->pmtu - 32 <<"\n";
         if ((state->nagleEnabled) &&
             (outstandingBytes > 0) &&
             ((uint32)scount < pathVar->pmtu - 32)) {
            // Cannot fill a whole path MTU => try to wait until there is more data ...
            scount = 0;
            break;
         }

         // ------ Dequeue data message -------------------------------------
         sctpEV3 << "sendAll:  sctpMsg->length=" << sctpMsg->getBitLength() / 8
                 << "   length datMsg=" << pathVar->pmtu-sctpMsg->getBitLength() / 8 - 20 <<"\n";
		   datMsg = dequeueOutboundDataMsg(pathVar->pmtu-sctpMsg->getBitLength() / 8 - 20);

         // ------ Handle data message --------------------------------------
         if (datMsg) {
            state->queuedMessages--;
            if ((state->queueLimit > 0) &&
                (state->queuedMessages < state->queueLimit) &&
                (state->queueUpdate == false)) {
               // Tell upper layer readiness to accept more data
               sendIndicationToApp(SCTP_I_SEND_MSG);
               state->queueUpdate = true;
               sctpEV3 << "sendAll: queue has to be filled! Queued Messages = "
                       << state->queuedMessages << "\n";
			   }
            datVar = new SCTPDataVariables();
			datVar->bbit          = datMsg->getBBit();
			datVar->ebit          = datMsg->getEBit();
            datVar->enqueuingTime = datMsg->getEnqueuingTime();
            datVar->expiryTime    = datMsg->getExpiryTime();
            datVar->ppid          = datMsg->getPpid();
            // datMsg->setInitialDestination(state->primaryPathIndex); // I.R. always send on primaryPath
            datMsg->setInitialDestination(dpi);   // T.D. 17.03.09  dpi == state->primaryPathIndex when CMT is off
            datVar->initialDestination = datMsg->getInitialDestination();
            dpi = datVar->initialDestination;  // I.R. 23.10.07
            sctpEV3 << "sendAll: initialDestination=" << datVar->initialDestination
                    << ": data will be sent to " << dpi << "\n";
            // datVar->len is just data
            datVar->len                      = datMsg->getBitLength();
            datVar->sid                      = datMsg->getSid();
            datVar->allowedNoRetransmissions = datMsg->getRtx();
            datVar->booksize                 = datMsg->getBooksize();

            // ------ Stream handling ---------------------------------------
            SCTPSendStreamMap::iterator iterator = sendStreams.find(datMsg->getSid());
            SCTPSendStream*             stream   = iterator->second;
            nextSSN = stream->getNextStreamSeqNum();
            datVar->userData = datMsg->decapsulate();
            if (datMsg->getOrdered()) {
               // ------ Ordered mode: assign SSN ---------
               if (datMsg->getEBit())
			   {
				   datVar->ssn     = nextSSN++;
			   }
			   else
				   datVar->ssn     = nextSSN;

               datVar->ordered = true;
               if (nextSSN > 65535) {   // ??? Wäre uint16 als nextSSN nicht einfacher?
                  stream->setNextStreamSeqNum(0);
               }
               else {
                  stream->setNextStreamSeqNum(nextSSN);
               }
            }
            else {
               // ------ Ordered mode: no SSN needed ------
               datVar->ssn     = 0;
               datVar->ordered = false;
            }
            firstTime = true;
            delete datMsg;

            sctpEV3 << "sendAll: chunk " << datVar << " dequeued from StreamQ "
                    << datVar->sid << ": tsn=" << datVar->tsn
                    << ", bytes now " << qCounter.roomSumSendStreams << "\n";
         }

         // ------ No data message has been dequeued ------------------------
         else {   // i.e. there may only be control chunls
            if (headerCreated == true) {
               // ------ Are there any chunks to send? ----
               if (chunksAdded == 0) {
                  // No -> nothing more to do.
                  delete sctpMsg;
                  return;
               }
               else {
                  // Yes.
                  packetFull = true;
                  sctpEV3 << "sendAll: packetFull: msg length = " << sctpMsg->getBitLength() / 8 + 20 << "\n";
                  datVar = NULL;
               }
            }
         }
      }


      // ====== Chunk handling ==============================================
      chunk = datVar;
      // ------ Handle DATA chunk -------------------------------------------
      if (chunk != NULL) {
         // ------ Assign TSN -----------------------------------------------
         if (chunk->tsn == 0) {
            chunk->tsn = state->nextTSN;
            sctpEV3 << "sendAll: set TSN=" << chunk->tsn
                    << " sid=" << chunk->sid << ", ssn=" << chunk->ssn << "\n";
            state->nextTSN++;
         }
         pathVar->pathTSN->record(chunk->tsn);

         SCTP::AssocStatMap::iterator iterator = sctpMain->assocStatMap.find(assocId);
         iterator->second.transmittedBytes += chunk->len/8;
         /* only if it is a first time send event, store the chunk also in the retransmission queue */
         chunk->countsAsOutstanding = true;
         chunk->hasBeenRemoved = false;
         chunk->sendTime = simulation.getSimTime(); //I.R. to send Fast RTX just once a RTT

         // ------ Chunk has not been transmitted yet (first transmission) --
         if (chunk->numberOfTransmissions == 0) {

               chunk->lastDestination = state->primaryPathIndex;
            sctpEV3 << "sendAll: " << simulation.getSimTime() << " firstTime, TSN "
                    << chunk->tsn  << ": lastDestination set to  "<< chunk->lastDestination << "\n";
            if (chunk->lastDestination==IPvXAddress("0.0.0.0")) {
               opp_error("primaryPathIndex unspecified");
            }
            if (!state->firstDataSent) {
               state->firstDataSent = true;
            }
            sctpEV3 << "sendAll: insert in retransmissionQ tsn=" << chunk->tsn << "\n";
            if(!retransmissionQ->checkAndInsertVar(chunk->tsn, chunk)) {
               opp_error("Cannot add chunk to retransmissionQ!");

               // Falls es hier aufschlaegt, muss ueberlegt werden, ob es OK ist, dass Chunks nicht eingefuegt werden koennen.
            }
            else {
               sctpEV3 << "sendAll: size of retransmissionQ=" << retransmissionQ->getQueueSize() << "\n";
               chunk->hasBeenAcked = false;
               pathVar->outstandingBytes += chunk->booksize;
               sctpEV3 << "sendAll: outstandingBytes=" << pathVar->outstandingBytes << "\n";
               CounterMap::iterator iterator = qCounter.roomRetransQ.find(dpi);
               iterator->second += ADD_PADDING(chunk->len / 8 + SCTP_DATA_CHUNK_LENGTH);
            }
         }
         // ------ Chunk has been transmitted before (retransmission) -------
         else {
            sctpEV3 << "sendAll: numberOfTransmissions=" << chunk->numberOfTransmissions << "\n";
         }

         /* chunk is already in the retransmissionQ */
         chunk->numberOfTransmissions++;
         chunk->gapReports               = 0;
         chunk->hasBeenFastRetransmitted = false;
         sctpEV3<<"sendAll(): adding new outbound data chunk to packet (tsn="<<chunk->tsn<<")...!!!\n";


         chunkPtr = transformDataChunk(chunk);

         /* update counters */
         totalChunksSent++;
         chunksAdded++;
         dataChunksAdded++;
         sctpMsg->addChunk(chunkPtr);


         // ------ Peer will not accept chunk (receiver window to small) ----
         if (chunk->booksize > state->peerRwnd)
         {
            state->peerRwnd     = 0;
            bytesToSend         = 0;
            bytes.packet        = false;
            packetFull          = true;
         }
         // ------ Peer will accept chunk (receiver window okay) ------------
         else  {
            state->peerRwnd -= chunk->booksize;
            if ((bytes.chunk == false) && (bytes.packet == false)) {
               bytesToSend -= chunk->booksize;
            }
            else if (bytes.chunk) {
               bytes.chunk = false;
            }
            else if ((bytes.packet) && (packetFull)) {
               bytes.packet = false;
            }
         }

         if (bytesToSend <= 0) {
            if ((!packetFull) && (qCounter.roomSumSendStreams > pathVar->pmtu - 32)) {
               sendOneMorePacket = true;
               bytes.packet      = true;
               sctpEV3 << "sendAll: one more packet allowed\n";
            }
            else {
               packetFull          = true;
            }
            bytesToSend = 0;
         }
         else if ((qCounter.roomSumSendStreams == 0) && (tq->second==0)) {
            packetFull          = true;
            sctpEV3 << "sendAll: no data in send and transQ: packet full\n";
         }
         sctpEV3 << "sendAll: bytesToSend after reduction: " << bytesToSend << "\n";

         chunk->lastDestination = newDpi;
         sctpEV3 << "sendAll: " << simulation.getSimTime() <<"  TSN " <<chunk->tsn
                 << ": lastDestination set to " << chunk->lastDestination << "\n";
         if (chunk->lastDestination == IPvXAddress("0.0.0.0")) {
            opp_error("newDpi unspecified");
         }
      }
      // ------ Handle non-DATA chunk ---------------------------------------
      else if (headerCreated) {   // No DATA chunk, but control chunk(s)
         if (chunksAdded == 0) {   // Nothing to do -> return
            delete sctpMsg;
            return;
         }
         else {
            packetFull = true;
            sctpEV3 << "sendAll: packetFull: msg length = " << sctpMsg->getBitLength() / 8 + 20 << "\n";
            datVar = NULL;
         }
      }


      // ====== Send packet =================================================
      if ((packetFull) || (dpi != newDpi) || (sendBeforeRtx)) {
         sctpEV3 << "sendAll: " << simulation.getSimTime() << "  packet full:"
                 << "  totalLength=" << sctpMsg->getBitLength() / 8 + 20
                 << ",  dpi="        << dpi
                 << "   newDpi="     << newDpi
                 << "   "            << dataChunksAdded << " chunks added, outstandingBytes now "
                 << getPath(dpi)->outstandingBytes << "\n";

         // ------ There are already a header + one or more chunks ----------
         if ((headerCreated == true) && (chunksAdded > 0)) {
            /* new chunks would exceed MTU, so we send old packet and build a new one */
            /* this implies that at least one data chunk is send here */
            if (dataChunksAdded > 0) {
               if (!pathVar->T3_RtxTimer->isScheduled()) {
                  // Start retransmission timer, if not scheduled before
                  startTimer(pathVar->T3_RtxTimer, pathVar->pathRto);
               }
               else {
                  sctpEV3 << "sendAll: RTX Timer already scheduled -> no need to schedule it\n";
               }
            }
            if (sendOneMorePacket) {
               sendOneMorePacket = false;
               bytesToSend       = 0;
               bytes.packet      = false;
            }
            sendToIP(sctpMsg, dpi);
            pmDataIsSentOn(dpi);
            totalPacketsSent++;

            // ------ Reset status ------------------------------------------
            headerCreated   = false;
            chunksAdded     = 0;
            dataChunksAdded = 0;
            firstTime       = false;
            sendBeforeRtx   = false;
            packetFull      = false;

            sctpEV3 << "sendAll: sending Packet to path " << dpi
                    << "  scount=" << scount
                    << "  tcount=" << tcount
                    << "  bytesToSend=" << bytesToSend << "\n";
            if ((tcount == 0) &&
                (bytesToSend > 0) &&
                (state->nagleEnabled) &&
                (state->peerRwnd < pathVar->pmtu) &&
                (pathVar->outstandingBytes > 0)) {
               // Test whether a full packet can be filled
               bytesToSend = calculateBytesToSendOnPath(pathVar);
               sctpEV3 << "sendAll:  peerRwnd=" << state->peerRwnd
                       << "  bytesToSend=" << bytesToSend
                       << "  nextTSN="     << state->nextTSN << "\n";
            }
         }
         else {
            opp_error("Logic Error: packet size + new chunk len exceed path mtu, but chunksAdded==0");
         }
      }
      sctpEV3 << "sendAll: still " << bytesToSend << " bytes to send, headerCreated=" << headerCreated << "\n";

      // ------ There is no header yet -> create SCTP message first ---------
      if ((!headerCreated) && (bytesToSend > 0)) {
         sctpEV3 << "sendAll:  headerCreated=" << headerCreated
                 << "  bytesToSend="    << bytesToSend
                 << "  sumSendStreams=" << qCounter.roomSumSendStreams << "\n";
         if (((state->nagleEnabled == false) && ((qCounter.roomSumSendStreams > 0) || (tq->second > 0))) ||
             ((state->nagleEnabled == true)  && ((qCounter.roomSumSendStreams >= (pathVar->pmtu - 32)) || (tq->second > 0)))) {
            sctpMsg = new SCTPMessage();
            headerCreated = true;
            sctpMsg->setBitLength(SCTP_COMMON_HEADER * 8);
            tcount  = tq->second;
            rtcount = rtq->second;
            scount  = qCounter.roomSumSendStreams;
            bcount  = qCounter.bookedSumSendStreams;
         }
         else {
            bytesToSend = 0;
         }
      }
   }


   // #######################################################################
   // #### Transmission of remaining chunks                              ####
   // #######################################################################

   if ((chunksAdded > 0) && (headerCreated == true)) {
      sctpEV3 << "sendAll: out of while:"
              << "  headerCreated="   << headerCreated
              << "  chunksAdded="     << chunksAdded
              << "  dataChunksAdded=" << dataChunksAdded << "\n";
      if ((state->nagleEnabled == false) ||
          (outstandingBytes == 0) ||
          (firstTime == false) ||
          ((state->nagleEnabled) && (qCounter.roomSumSendStreams >= (pathVar->pmtu - 32))) ||
          (rtxActive == true)) {
         /* this destination may have to be changed (e.g. to last fromAddress) */
         if (dpi == IPvXAddress("0.0.0.0")) {
            dpi = state->primaryPathIndex;
         }
         /* start T3 timer, if it is not already running, and turn off probing */
         if (dataChunksAdded > 0) {
            if (!getPath(dpi)->T3_RtxTimer->isScheduled()) {
               startTimer(getPath(dpi)->T3_RtxTimer, getPath(dpi)->pathRto);
            }
         }
      }
      sctpEV3 << "sendAll: send all queued chunks to " << dpi
              << " and get out. outstandingBytes now " << getPath(dpi)->outstandingBytes << "\n";
      sendToIP(sctpMsg, dpi);
      pmDataIsSentOn(dpi);
      totalPacketsSent++;
      rtxActive = false;
      probing   = false;
   }
   else {
      sctpEV3 << "sendAll: nothing more to send... BYE!\n";
   }

}
