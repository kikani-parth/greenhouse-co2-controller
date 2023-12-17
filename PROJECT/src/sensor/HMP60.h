/*
 * HMP60.h
 *
 *  Created on: 5 Dec 2023
 *      Author: pkikani
 */
#if 1
#ifndef HMP60_H_
#define HMP60_H_

#include "../modbus/ModbusMaster.h"
#include "../modbus/ModbusRegister.h"

#define HMP60_MODBUS_ADDR		((uint8_t)241)
#define BAUD_RATE				9600
#define RH_REG_ADDR      		256
#define TEMP_REG_ADDR   		257
#define HMP60_STATUS_REG_ADDR	0x200


class HMP60 {
public:
	HMP60();
	~HMP60();
	int readTemp();
	int readRH();
	int readStatus();
private:
	ModbusMaster hmp60_node;
	ModbusRegister rh_reg;
	ModbusRegister temp_reg;
	ModbusRegister hmp60_status;
};

#endif /* HMP60_H_ */
#endif
