/**
 * @short Implementation of a simple battery model
 *		"real" battery models should subclass this!
 *		The basic class publishes the remaining energy on the notification board,
 *		but does not decrement the energy!
 *		It can therefore be used for hosts having an infinite energy supply
 *		i.e. a power plug
 * @author Isabel Dietrich
*/

#ifndef BASIC_BATTERY_H
#define BASIC_BATTERY_H

// SYSTEM INCLUDES
#include <omnetpp.h>

#include "Energy.h"

// INCLUDES for access to the Notification board (publish energy)
#include "NotifierConsts.h"
#include "NotificationBoard.h"

// provides possibility to take energy snapshots
#include "AnalysisEnergy.h"

class BasicBattery : public cSimpleModule, public INotifiable
{
public:
    // LIFECYCLE

	virtual void initialize(int);
	virtual void finish();

    // OPERATIONS
	void    UpdateEnergy();
    double  GetEnergy();
	simtime_t 	GetLifetime();

protected:
    // OPERATIONS
	void HandleSelfMsg(cMessage*);


	// MEMBER VARIABLES

	// holds the current amount of energy of a node
    Energy              mRemainingEnergy;

	// pointer to the notification board
    NotificationBoard*  mpNb;

	// debugging enabled for this node? Used in the definition of EV
    bool                mDebug;

	// gate which absorbs all messages from the network when the node is dead
	int					mEmptyIn;

	// holds the time the node died (because of energy depletion or node failure)
	simtime_t			mNodeDied;

	// holds the mean time to failure. If <= 0: no failure
	double				mMeanTimeToFailure;

	// timer message to schedule node failure
	cMessage*			mFailure;


private:
    // OPERATIONS
    void         handleMessage(cMessage *msg);

    virtual void receiveChangeNotification (int, const cPolymorphic*);

};

#endif

