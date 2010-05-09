/**
 *  Battery module implemented for IEEE 802.15.4
 *  The model accounts for energy usage due to radio transmissions and
 *  receptions
*/

#ifndef IEEE_802154_BATTERY_H
#define IEEE_802154_BATTERY_H

#include "BasicBattery.h"
#include "RadioState.h"

class Ieee802154Battery: public BasicBattery
{
public:
	virtual void	initialize(int);
	virtual int	numInitStages   () const {return 2;}
	virtual void	finish();

private:
	virtual void    handleSelfMsg(cMessage*);
	void            receiveChangeNotification(int, cPolymorphic*);

	void            Display();

	// MEMBER VARIABLES
	// power consumption parameters in mW
	double      mUsageRadioIdle;
	double      mUsageRadioRecv;
	double      mUsageRadioSend;
	double      mUsageRadioSleep;
	double      mBatteryCapacityMAh;
	double      mTransmitterPower;
	//double      mUsageCpuActive;
	//double      mUsageCpuSleep;


	int         mLastDisplayedPercent;

	cOutVector* mCurrEnergy;
	//cOutVector* mTimeIntvTransmit;

	RadioState::State	lastRadioState;
	simtime_t		timeInRx;
	simtime_t		timeInTx;
	simtime_t		timeInIdle;
	simtime_t		timeInSleep;
	simtime_t		mRadioLastStatechange;
};

#endif

