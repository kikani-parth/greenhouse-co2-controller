/*
 * GMP252.h
 *
 *  Created on: 5 Dec 2023
 *      Author: pkikani
 */
#if 1
#ifndef GMP252_H_
#define GMP252_H_

#include "../modbus/ModbusMaster.h"
#include "../modbus/ModbusRegister.h"

#define GMP252_MODBUS_ADDR		((uint8_t)240)
#define BAUD_RATE				9600
#define CO2_REG_ADDR			256
#define GMP252_STATUS_REG_ADDR	0x800

class GMP252 {
public:
	GMP252();
	~GMP252();
	int readCO2();
	int readStatus();
private:
	ModbusMaster gmp252_node;
	ModbusRegister co2_reg;
	ModbusRegister gmp252_status;
};

#endif /* GMP252_H_ */
#endif
