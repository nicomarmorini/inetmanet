

#ifndef IEEE80216RADIO_H
#define IEEE80216RADIO_H

#include "AbstractRadio.h"
#include "AbstractRadioExtended.h"
#include "Ieee80216Consts.h"//XXX for the COLLISION and BITERROR msg kind constants
#include "Ieee80216MacHeader_m.h"
#include "WiMAXPathLossReceptionModel.h"
/**
 * Radio for the IEEE 802.11 model. Just a AbstractRadio with PathLossReceptionModel
 * and Ieee80211RadioModel.
 */

class INET_API Ieee80216Radio : public AbstractRadioExtended
{
private:
	bool isCollision;

	int collisions;

public:
	void handleLowerMsgStart(AirFrame * airframe);
	void handleLowerMsgEnd(AirFrame * airframe);
	void sendUp(AirFrame *airframe, SnrList list, double rcvdPower);

	SnrList getSNRlist();

protected:
  virtual void initialize(int);
  virtual void finish();
  virtual IReceptionModel *createReceptionModel() {return (IReceptionModel *)createOne("WiMAXPathLossReceptionModel");}
  virtual IRadioModel *createRadioModel() {return (IRadioModel *)createOne("Ieee80216RadioModel");}

};

#endif

