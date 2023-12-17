/*
 * HMP60.cpp
 *
 *  Created on: 5 Dec 2023
 *      Author: pkikani
 */
#if 1
#include "HMP60.h"

HMP60::HMP60() : // Initialize the member variables
				hmp60_node(HMP60_MODBUS_ADDR),
				rh_reg(&hmp60_node, RH_REG_ADDR, true),
				temp_reg(&hmp60_node, TEMP_REG_ADDR, true),
				hmp60_status(&hmp60_node, HMP60_STATUS_REG_ADDR, true)
{
	hmp60_node.begin(BAUD_RATE);
}

HMP60::~HMP60() {
	// TODO Auto-generated destructor stub
}

int HMP60::readStatus(){
	return hmp60_status.read();
}

int HMP60::readTemp(){
	return temp_reg.read() / 10;
}

int HMP60::readRH(){
	return rh_reg.read() / 10;
}
#endif
