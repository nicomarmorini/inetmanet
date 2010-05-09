//
// @short Implementation of a simple battery model
//		"real" battery models should subclass this! 
//		The basic class publishes the remaining energy on the notification board,
//		but does not decrement the energy!
//		It can therefore be used for hosts having an infinite energy supply
//		i.e. a power plug
// @author Isabel Dietrich
//

package inet.underTest.wpan.battery;
simple BasicBattery
{
    parameters:
        // debug switch
        bool debug; 	 // debug switch
        // mean time to failure of a node
        double meanTimeToFailure @unit("s"); // mean time to failure of a node
									 // <= 0 means no failure
        // capacity of the used battery in mAh
        double batteryCapacity; // capacity of the used battery in mAh
    gates:
        input emptyIn[];
}
