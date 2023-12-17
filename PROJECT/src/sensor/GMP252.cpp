/*
 * GMP252.cpp
 *
 *  Created on: 5 Dec 2023
 *      Author: pkikani
 */
#if 1
#include "GMP252.h"

GMP252::GMP252() :	// Initialize member variables
					gmp252_node(GMP252_MODBUS_ADDR),
					co2_reg(&gmp252_node, CO2_REG_ADDR, true),
					gmp252_status(&gmp252_node, GMP252_STATUS_REG_ADDR, true)
{
	gmp252_node.begin(BAUD_RATE);
}

GMP252::~GMP252() {
	// TODO Auto-generated destructor stub
}

int GMP252::readStatus(){
	if(gmp252_status.read() == 0){
		return 1;
	}
}

int GMP252::readCO2(){
	return co2_reg.read();
}
#endif
