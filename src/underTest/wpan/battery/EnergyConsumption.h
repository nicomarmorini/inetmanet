/**
 * @short Class to hold the energy consumption of a node. 
 *  Used to notify the Battery of a change in energy consumption when the sensor
 *  program changes.
 * @author Isabel Dietrich
*/

#ifndef ENERGY_CONSUMPTION_H
#define ENERGY_CONSUMPTION_H

class EnergyConsumption : public cPolymorphic
{
	public:
        // LIFECYCLE
		EnergyConsumption(double e) : cPolymorphic(), consumption(e) {};
    
        // MEMBER VARIABLES
		double consumption;	// holds the current energy consumption
};

#endif
