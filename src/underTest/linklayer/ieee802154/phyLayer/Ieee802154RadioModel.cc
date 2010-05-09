
#include "Ieee802154RadioModel.h"
#include "Ieee802154Const.h"
#include "FWMath.h"


Register_Class(Ieee802154RadioModel);


void Ieee802154RadioModel::initializeFrom(cModule *radioModule)
{		
		// read from Ieee802154phy
    snirThreshold = dB2fraction(radioModule->par("snirThreshold"));
}

double Ieee802154RadioModel::calculateDuration(AirFrame *airframe)
{
    return (def_phyHeaderLength*8 + airframe->getBitLength())/airframe->getBitrate() ;
}


bool Ieee802154RadioModel::isReceivedCorrectly(AirFrame *airframe, const SnrList& receivedList)
{
    // calculate snirMin
    double snirMin = receivedList.begin()->snr;
    for (SnrList::const_iterator iter = receivedList.begin(); iter != receivedList.end(); iter++)
        if (iter->snr < snirMin)
            snirMin = iter->snr;

    cMessage *frame = airframe->getEncapsulatedMsg();
    EV << "packet (" << frame->getClassName() << ")" << frame->getName() << " (" << frame->info() << ") snrMin=" << snirMin << endl;

    if (snirMin <= snirThreshold)
    {
        // if snir is too low for the packet to be recognized
        EV << "COLLISION! Packet got lost\n";
        return false;
    }
    /*else if (packetOk(snirMin, airframe->getEncapsulatedMsg()->length(), airframe->getBitrate()))
    {
        EV << "packet was received correctly, it is now handed to upper layer...\n";
        return true;
    }
    else
    {
        EV << "Packet has BIT ERRORS! It is lost!\n";
        return false;
    }*/
    else 
    	return true;
}


/*bool Ieee802154RadioModel::packetOk(double snirMin, int lengthMPDU, double bitrate)
{
    double berHeader, berMPDU;

    berHeader = 0.5 * exp(-snirMin * BANDWIDTH / BITRATE_HEADER);

    // if PSK modulation
    if (bitrate == 1E+6 || bitrate == 2E+6)
        berMPDU = 0.5 * exp(-snirMin * BANDWIDTH / bitrate);
    // if CCK modulation (modeled with 16-QAM)
    else if (bitrate == 5.5E+6)
        berMPDU = 0.5 * (1 - 1 / sqrt(pow(2.0, 4))) * erfc(snirMin * BANDWIDTH / bitrate);
    else                        // CCK, modelled with 256-QAM
        berMPDU = 0.25 * (1 - 1 / sqrt(pow(2.0, 8))) * erfc(snirMin * BANDWIDTH / bitrate);

    // probability of no bit error in the PLCP header
    double headerNoError = pow(1.0 - berHeader, HEADER_WITHOUT_PREAMBLE);

    // probability of no bit error in the MPDU
    double MpduNoError = pow(1.0 - berMPDU, lengthMPDU);
    EV << "berHeader: " << berHeader << " berMPDU: " << berMPDU << endl;
    double rand = dblrand();

    if (rand > headerNoError)
        return false; // error in header
    else if (dblrand() > MpduNoError)
        return false;  // error in MPDU
    else
        return true; // no error
}
*/
double Ieee802154RadioModel::dB2fraction(double dB)
{
    return pow(10.0, (dB / 10));
}

