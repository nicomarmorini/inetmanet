cplusplus {{
#include "Ieee802154Const.h"
#include "Ieee802154Enum.h"
}}


enum PHYenum;

enum PHYPIBenum;


packet Ieee802154MacPhyPrimitives
{
    byteLength = 0;

    short status enum(PHYenum);
    short attribute enum(PHYPIBenum);

    unsigned char energyLevel;

    int channelNumber; 		// the current radio channel
		//unsigned int	phyChannelsSupported; 
		//double		phyTransmitPower;
		//unsigned char	phyCCAMode;
}
