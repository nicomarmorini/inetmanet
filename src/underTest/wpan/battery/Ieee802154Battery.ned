package inet.underTest.wpan.battery;

simple Ieee802154Battery
{
    parameters:
        // debug switch
        bool debug; 				// debug switch
        // capacity of the used battery in mAh
        double batteryCapacity; 				// capacity of the used battery in mAh
        //battery_capacity:			numeric,				// capacity of the used battery in mAh
        // mean time to failure of a node, <= 0 means no failure
        double meanTimeToFailure @unit("s"); 				// mean time to failure of a node, <= 0 means no failure

        // parameters for energy usage
        double usage_radio_idle @unit("mA");
        double usage_radio_recv @unit("mA");
        double usage_radio_sleep @unit("mA");
        double transmitterPower @unit("mW");

	//update_avg_consumption:	numeric,
	//avg_usage_cpu:			numeric,
	//usage_cpu_active:			numeric,
	//usage_cpu_sleep:			numeric;

}
