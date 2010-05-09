/***************************************************************************
                          RTPPayloadSender.cc  -  description
                             -------------------
    (C) 2007 Ahmed Ayadi  <ahmed.ayadi@sophia.inria.fr>
    (C) 2001 Matthias Oppitz, Arndt Buschmann <Matthias.Oppitz@gmx.de> <a.buschmann@gmx.de>

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/** \file RTPPayloadSender.cc
 * This file contains the implementation of member functions of the class
 * RTPPayloadSender.
 */

#include "RTPPayloadSender.h"


Define_Module(RTPPayloadSender);


RTPPayloadSender::~RTPPayloadSender() {
    closeSourceFile();
}


void RTPPayloadSender::initialize() {
    cSimpleModule::initialize();
    _mtu = 0;
    _ssrc = 0;
    _payloadType = 0;
    _clockRate = 0;
    _timeStampBase = intrand(65535);
    _timeStamp = _timeStampBase;
    _sequenceNumberBase = intrand(0x7fffffff);
    _sequenceNumber = _sequenceNumberBase;
};


void RTPPayloadSender::activity() {
    const char *command;
    while (true) {
        cMessage *msg = receive();
        if (msg->getArrivalGateId() == findGate("fromProfile")) {
            RTPInnerPacket *rinpIn = (RTPInnerPacket *)msg;
            if (rinpIn->getType() == RTPInnerPacket::RTP_INP_INITIALIZE_SENDER_MODULE) {
                initializeSenderModule(rinpIn);
            }
            else if (rinpIn->getType() == RTPInnerPacket::RTP_INP_SENDER_MODULE_CONTROL) {
                RTPSenderControlMessage *rscm = (RTPSenderControlMessage *)(rinpIn->decapsulate());
                delete rinpIn;
                command = rscm->getCommand();
                if (!opp_strcmp(command, "PLAY")) {
                    play();
                }
                else if (!opp_strcmp(command, "PLAY_UNTIL_TIME")) {
                    playUntilTime(rscm->getCommandParameter1());
                }
                else if (!opp_strcmp(command, "PLAY_UNTIL_BYTE")) {
                    playUntilByte(rscm->getCommandParameter1());
                }
                else if (!opp_strcmp(command, "PAUSE")) {
                    pause();
                }
                else if (!opp_strcmp(command, "STOP")) {
                    stop();
                }
                else if (!opp_strcmp(command, "SEEK_TIME")) {
                    seekTime(rscm->getCommandParameter1());
                }
                else if (!opp_strcmp(command, "SEEK_BYTE")) {
                    seekByte(rscm->getCommandParameter1());
                }
                else {
                    error("unknown sender control message");
                };
                delete rscm;
            }
        }
        else {
            if (!sendPacket()) {
                endOfFile();
            }
            delete msg;
        }
    }
};


void RTPPayloadSender::initializeSenderModule(RTPInnerPacket *rinpIn) {
    ev << "initializeSenderModule Enter" << endl;
    _mtu = rinpIn->getMTU();
    _ssrc = rinpIn->getSSRC();
    const char *fileName = rinpIn->getFileName();
    openSourceFile(fileName);
    delete rinpIn;
    RTPInnerPacket *rinpOut = new RTPInnerPacket("senderModuleInitialized()");
    rinpOut->senderModuleInitialized(_ssrc, _payloadType, _clockRate, _timeStampBase, _sequenceNumberBase);
    send(rinpOut, "toProfile");
    _status = STOPPED;
    ev << "initializeSenderModule Exit" << endl;
};


void RTPPayloadSender::openSourceFile(const char *fileName) {
    _inputFileStream.open(fileName);
    if (!_inputFileStream) {
        opp_error("sender module: error open data file");
    }
};


void RTPPayloadSender::closeSourceFile() {
    _inputFileStream.close();
};


void RTPPayloadSender::play() {
    _status = PLAYING;
    RTPSenderStatusMessage *rssm = new RTPSenderStatusMessage("PLAYING");
    rssm->setStatus("PLAYING");
    rssm->setTimeStamp(_timeStamp);
    RTPInnerPacket *rinpOut = new RTPInnerPacket("senderModuleStatus(PLAYING)");
    rinpOut->senderModuleStatus(_ssrc, rssm);
    send(rinpOut, "toProfile");

    if (!sendPacket()) {
        endOfFile();
    }
};


void RTPPayloadSender::playUntilTime(simtime_t moment) {
    error("playUntilTime() not implemented");
};


void RTPPayloadSender::playUntilByte(int position) {
    error("playUntilByte() not implemented");
};


void RTPPayloadSender::pause() {
    cancelEvent(_reminderMessage);
    _status = STOPPED;
    RTPInnerPacket *rinpOut = new RTPInnerPacket("senderModuleStatus(PAUSED)");
    RTPSenderStatusMessage *rsim = new RTPSenderStatusMessage();
    rsim->setStatus("PAUSED");
    rinpOut->senderModuleStatus(_ssrc, rsim);
    send(rinpOut, "toProfile");
};


void RTPPayloadSender::seekTime(simtime_t moment) {
    error("seekTime() not implemented");
};


void RTPPayloadSender::seekByte(int position) {
    error("seekByte() not implemented");
};


void RTPPayloadSender::stop() {
    cancelEvent(_reminderMessage);
    _status = STOPPED;
    RTPSenderStatusMessage *rssm = new RTPSenderStatusMessage("STOPPED");
    rssm->setStatus("STOPPED");
    RTPInnerPacket *rinp = new RTPInnerPacket("senderModuleStatus(STOPPED)");
    rinp->senderModuleStatus(_ssrc, rssm);
    send(rinp, "toProfile");
};


void RTPPayloadSender::endOfFile() {
    _status = STOPPED;
    RTPSenderStatusMessage *rssm = new RTPSenderStatusMessage();
    rssm->setStatus("FINISHED");
    RTPInnerPacket *rinpOut = new RTPInnerPacket("senderModuleStatus(FINISHED)");
    rinpOut->senderModuleStatus(_ssrc, rssm);
    send(rinpOut, "toProfile");
};


bool RTPPayloadSender::sendPacket() {
    error("sendPacket() not implemented");
    return false;
};
