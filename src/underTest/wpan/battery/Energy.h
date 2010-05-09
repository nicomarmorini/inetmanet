/**
 * @short Class to hold the current energy level of the node.
 * @author Isabel Dietrich
*/

#ifndef ENERGY_H
#define ENERGY_H

// SYSTEM INCLUDES
#include <omnetpp.h>

class Energy : public cPolymorphic
{
public:
    // LIFECYCLE
    Energy(double e=250) : cPolymorphic(), mEnergy(e) {};

    // OPERATIONS
    double  GetEnergy() const        { return mEnergy; }
	void    SetEnergy(double e)      { mEnergy = e; }
	void    SubtractEnergy(double e) { mEnergy -= e; }


private:
    // MEMBER VARIABLES
	double mEnergy;

};

#endif
