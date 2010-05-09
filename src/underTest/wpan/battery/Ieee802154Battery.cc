#include "Ieee802154Battery.h"

Define_Module(Ieee802154Battery);

//#undef EV
//#define EV (ev.isDisabled()||!mDebug) ? std::cout : ev ==> EV is now part of <omnetpp.h>
/////////////////////////////// PUBLIC ///////////////////////////////////////

//============================= LIFECYCLE ===================================
/**
 * Initialization routine
 */
void Ieee802154Battery::initialize(int aStage)
{
	BasicBattery::initialize(aStage);
	 EV << getParentModule()->getFullName() << ": initializing Ieee802154Battery, stage=" << aStage << endl;
	if (0 == aStage)
	{
		// read parameters
		mUsageRadioIdle		= par("usage_radio_idle");
		mUsageRadioRecv		= par("usage_radio_recv");
		mUsageRadioSleep		= par("usage_radio_sleep");
		//mBatteryCapacityMAh	= par("battery_capacity");
		//mUsageCpuActive		= par("usage_cpu_active");
		//mUsageCpuSleep		= par("usage_cpu_sleep");
		mTransmitterPower		= par("transmitterPower");

		double trans_power_dbm = 10*log10(mTransmitterPower);
		// calculation of usage_radio_send
		// based on the values in Olaf Landsiedel's AEON paper
		/*if (trans_power_dbm <= -18)
			mUsageRadioSend = 8.8;
		else if (trans_power_dbm <= -13)
			mUsageRadioSend = 9.8;
		else if (trans_power_dbm <= -10)
			mUsageRadioSend = 10.4;
		else if (trans_power_dbm <= -6)
			mUsageRadioSend = 11.3;
		else if (trans_power_dbm <= -2)
			mUsageRadioSend = 15.6;
		else if (trans_power_dbm <= 0)
			mUsageRadioSend = 17;
		else if (trans_power_dbm <= 3)
			mUsageRadioSend = 20.2;
		else if (trans_power_dbm <= 4)
			mUsageRadioSend = 22.5;
		else if (trans_power_dbm <= 5)
			mUsageRadioSend = 26.9;
		else
			error("[Battery]: transmitter Power too high!");*/

		// based on the values for CC2420 in howitt paper
		if (trans_power_dbm <= -25)
			mUsageRadioSend = 8.53;
		else if (trans_power_dbm <= -15)
			mUsageRadioSend = 9.64;
		else if (trans_power_dbm <= -10)
			mUsageRadioSend = 10.68;
		else if (trans_power_dbm <= -7)
			mUsageRadioSend = 11.86;
		else if (trans_power_dbm <= -5)
			mUsageRadioSend = 13.11;
		else if (trans_power_dbm <= -3)
			mUsageRadioSend = 14.09;
		else if (trans_power_dbm <= -1)
			mUsageRadioSend = 15.07;
		else if (trans_power_dbm <= 0)
			mUsageRadioSend = 16.24;
		else
			error("[Battery]: transmitter Power too high!");

		// initialize statistical variables
		timeInRx		= 0;
		timeInTx		= 0;
		timeInIdle		= 0;
		timeInSleep	= 0;

		// initialize state variables
		lastRadioState = RadioState::IDLE;	// Phy layer will also initialize it in stage 1 via notificationboard
		mRadioLastStatechange = 0;
		mLastDisplayedPercent = 100;
		WATCH(mLastDisplayedPercent);
		mCurrEnergy = new cOutVector("currEnergy");
		//mTimeIntvTransmit = new cOutVector("intvTransmit");

		// subscribe to object states (radio, sensors, cpu)
		mpNb->subscribe(this, NF_RADIOSTATE_CHANGED);
		//mpNb->subscribe(this, NF_MAC_RADIOSTATE_CHANGED);
	}
	else if (1 == aStage)
	{
		mBatteryCapacityMAh = mRemainingEnergy.GetEnergy();
		UpdateEnergy();
	}
}

void Ieee802154Battery::finish()
{
	recordScalar("Energy consumed (mAh)", mBatteryCapacityMAh - mRemainingEnergy.GetEnergy());
	if (mRemainingEnergy.GetEnergy() < 0)
		recordScalar("Battery energy left (percentage)", 0);
	else
		recordScalar("Battery energy left (percentage)", mRemainingEnergy.GetEnergy()/mBatteryCapacityMAh);
	//recordScalar("Total time in Rx (s)",	timeInRx);
	//recordScalar("Total time in Tx (s)",	timeInTx);
	//recordScalar("Total time in Idle (s)",	timeInIdle);
	//recordScalar("Total time in Sleep (s)",	timeInSleep);
	recordScalar("Radio duty cycle", 1-timeInSleep/simTime());
}

//////////////////////////////// PRIVATE  ////////////////////////////////////

//============================= OPERATIONS ===================================

void Ieee802154Battery::handleSelfMsg(cMessage* apMsg)
{
    // no self messages to handle
    delete apMsg;
}

void Ieee802154Battery::receiveChangeNotification (int aCategory, cPolymorphic* aDetails)
{
	//EV << "[Battery]: receiveChangeNotification" << endl;
	if (aCategory == NF_RADIOSTATE_CHANGED)
	{
		simtime_t duration = simTime() - mRadioLastStatechange;

		switch(lastRadioState)
		{
			case RadioState::IDLE:
				timeInIdle += duration;
				mRemainingEnergy.SubtractEnergy(mUsageRadioIdle*SIMTIME_DBL(duration)/3600.0);
				//mRemainingEnergy.SubtractEnergy(mUsageCpuSleep*(simTime() - mRadioLastStatechange)/3600.0);
				break;

			case RadioState::TRANSMIT:
				timeInTx += duration;
				mRemainingEnergy.SubtractEnergy(mUsageRadioSend*SIMTIME_DBL(duration)/3600.0);
				//mRemainingEnergy.SubtractEnergy(mUsageCpuActive*(simTime() - mRadioLastStatechange)/3600.0);
				//mTimeIntvTransmit->record((simTime() - mRadioLastStatechange));
				break;

			case RadioState::RECV:
				timeInRx += duration;
				mRemainingEnergy.SubtractEnergy(mUsageRadioRecv*SIMTIME_DBL(duration)/3600.0);
				//mRemainingEnergy.SubtractEnergy(mUsageCpuActive*(simTime() - mRadioLastStatechange)/3600.0);
				break;

			case RadioState::SLEEP:
				timeInSleep += duration;
				mRemainingEnergy.SubtractEnergy(mUsageRadioSleep*SIMTIME_DBL(duration)/3600.0);
				//mRemainingEnergy.SubtractEnergy(mUsageCpuSleep*(simTime() - mRadioLastStatechange)/3600.0);
				break;

			default:
				error("[Battery]: undefined radio state!");
		}

		if (mRemainingEnergy.GetEnergy() < 0)
		{
			cDisplayString* display_string = &getParentModule()->getDisplayString();
			display_string->setTagArg("i", 1, "#ff0000");
			EV << "[BATTERY]: " << getParentModule()->getFullName() <<" 's battery exhausted, stop simulation" << endl;
			endSimulation();
		}

		// update the local copy of the radio state
		lastRadioState = check_and_cast<RadioState *>(aDetails)->getState();

		UpdateEnergy();
		Display();

		mRadioLastStatechange = simTime();
		mCurrEnergy->record(mRemainingEnergy.GetEnergy());
	}
}

/**
 *  Function to update the display string with the remaining energy
 */
void Ieee802154Battery::Display()
{
    // calculate percentage of used battery capacity
    double energy_level = mRemainingEnergy.GetEnergy() / mBatteryCapacityMAh;
    double percent = (1 - energy_level) * 100;
    if (percent > 100) percent = 100;

    if (100-floor(percent) < mLastDisplayedPercent)
    {
        mLastDisplayedPercent = 100-(int)floor(percent);
        cDisplayString* display_string = &getParentModule()->getDisplayString();
    	if (mRemainingEnergy.GetEnergy() > 0)
        {
    		display_string->setTagArg("i", 1, "#000000"); // black coloring
		EV << "[BATTERY]: " << getParentModule()->getFullName() << " 's battery energy left percentage: " << energy_level*100 << "%" << endl;
    		//char buf[3];
    		//sprintf(buf, "%f", percent);
    		//display_string->setTagArg("i", 2, buf); // percentage based on energy
		display_string->setTagArg("i", 2, 100-mLastDisplayedPercent); // percentage based on energy
    	}
        else // red coloring if node dead
        {
		EV << "[BATTERY]: " << getParentModule()->getFullName() <<" 's battery exhausted, stop simulation" << endl;
    		display_string->setTagArg("i", 1, "#ff0000");
		endSimulation();
    	}
    }
}
