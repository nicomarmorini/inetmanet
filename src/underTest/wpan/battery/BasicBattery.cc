/**
 * @short Implementation of a simple battery model
 *		"real" battery models should subclass this!
 *		The basic class publishes the remaining energy on the notification board,
 *		but does not decrement the energy!
 *		It can therefore be used for hosts having an infinite energy supply
 *		i.e. a power plug
 * @author Isabel Dietrich
*/

#include "BasicBattery.h"

//#undef EV
//#define EV (ev.isDisabled()||!mDebug) ? std::cout : ev ==> EV is now part of <omnetpp.h>

Define_Module( BasicBattery );

/////////////////////////////// PUBLIC ///////////////////////////////////////

//============================= LIFECYCLE ===================================
/**
 * Initialization routine
 */
void BasicBattery::initialize(int aStage)
{
	cSimpleModule::initialize(aStage); //DO NOT DELETE!!

	if (0 == aStage)
    {
        mDebug = par("debug");
		mEmptyIn  = findGate("emptyIn");
		mMeanTimeToFailure = par("meanTimeToFailure");
		if (mMeanTimeToFailure > 0)
		{
			mFailure = new cMessage("mFailure");
			scheduleAt(mMeanTimeToFailure, mFailure);
		}

		// read parameters
		double e = par("batteryCapacity");
		if (!e) e = 0;
		mRemainingEnergy.SetEnergy(e);

		EV << "ENERGY: start capacity: " << mRemainingEnergy.GetEnergy() << "\n";

        // get a pointer to the NotificationBoard module
        mpNb = NotificationBoardAccess().get();

        mNodeDied       = -1;

	}
	else if (1 == aStage)
    {
		UpdateEnergy();
	}
}

void BasicBattery::finish()
{

}

//============================= OPERATIONS ===================================

/**
 * This function updates the energy currently available on this host
 * on the notification board
 */
void BasicBattery::UpdateEnergy()
{
	Energy* p_ene = new Energy(mRemainingEnergy.GetEnergy());
	mpNb->fireChangeNotification(NF_BATTERY_CHANGED, p_ene);
    delete p_ene;

	// if no energy is left AND the analysisEnergy module is present:
	// call SnapshotEnergies()
	if (mRemainingEnergy.GetEnergy() <= 0)
	{
		if (simulation.getModuleByPath("analysisEnergy") != NULL)
		{
			check_and_cast<AnalysisEnergy *>(simulation.getModuleByPath("analysisEnergy"))->Snapshot();
		}
	}
}

/**
 * @return the current remaining energy of the node
 */
double BasicBattery::GetEnergy()
{
    return mRemainingEnergy.GetEnergy();
}

/**
 *  @return the time when the node died, -1 if it still lives
 */
simtime_t BasicBattery::GetLifetime()
{
	return mNodeDied;
}

/**
 * Dispatches self-messages to handleSelfMsg()
 */
void BasicBattery::handleMessage(cMessage* apMsg)
{
	// This handles messages sent to the battery after the battery is depleted
	// The host should not see any of these messages as the battery is empty
	// So, the messages are deleted here
	if (mEmptyIn == apMsg->getArrivalGateId())
	{
		delete apMsg;
	}
    else if (!apMsg->isSelfMessage())
    {
        error("battery modules can only receive self messages");
    }
    HandleSelfMsg(apMsg);
}

//////////////////////////////// PRIVATE  ////////////////////////////////////

//============================= OPERATIONS ===================================

// You have sent yourself a message -- probably a timer -- take care of it
void BasicBattery::HandleSelfMsg(cMessage* apMsg)
{
	// disconnect the node, color it blue, set energy to zero
	// and record time of node death
    if (apMsg == mFailure)
	{
		if (getParentModule()->gate("radioIn") != NULL)
		{
			int size = getParentModule()->gateSize("radioIn");
			this->setGateSize("emptyIn",size);
			for (int i=0;i<size;i++)
			{
				getParentModule()->gate("radioIn",i)->disconnect();
				getParentModule()->gate("radioIn",i)->connectTo(this->gate("emptyIn",i));
			}
		}
		else
		{
			error("gate not found");
		}
		cDisplayString* display_string = &getParentModule()->getDisplayString();
		display_string->setTagArg("i", 1, "#0000ff");

		mRemainingEnergy.SetEnergy(0);
		UpdateEnergy();

		if (mNodeDied == -1)
		{
			mNodeDied = simTime();
		}

		ev << "node failure" << endl;
	}
}

void BasicBattery::receiveChangeNotification (
    int aCategory,
    const cPolymorphic* aDetails)
{
	ev << "this text should not appear. error in BasicBattery.cc" << endl;
}



