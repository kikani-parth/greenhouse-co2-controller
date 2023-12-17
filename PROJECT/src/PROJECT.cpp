/*
===============================================================================
 Name        : main.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
 */

#if defined (__USE_LPCOPEN)
#if defined(NO_BOARD_LIB)
#include "chip.h"
#else
#include "board.h"
#endif
#endif

#include <cr_section_macros.h>
#include "FreeRTOS.h"
#include "task.h"
#include "heap_lock_monitor.h"
#include "DigitalIoPin.h"
#include <mutex>
#include "Fmutex.h"
#include "modbus/ModbusMaster.h"
#include "modbus/ModbusRegister.h"
#include "mqtt_demo/PlaintextMQTTExample.h"
#include "LiquidCrystal.h"
#include "sensor/GMP252.h"
#include "sensor/HMP60.h"

#define QUEUE_LENGTH 				20
#define OK 							1
#define BACK 						-1
#define CLOCKWISE					10
#define COUNTER_CLOCKWISE			-10
#define CO2_SET_PT_EEPROM_OFFSET	0x00000100
#define SSID_EEPROM_OFFSET			0x00000200
#define PASS_EEPROM_OFFSET			0x00000300

using namespace std;


/* GLOBAL VARIABLES */
int co2SetPt = 0;
int co2Value = 0, tempValue = 0, rhValue = 0, valveValue = 0;
Fmutex guard;
QueueHandle_t handleMenuQueue;
struct EncoderEvent{
	int value;
	TickType_t timestamp;
};
EncoderEvent e;

// TODO: insert other definitions and declarations here
/* The following is required if runtime statistics are to be collected */
extern "C" {

void vConfigureTimerForRunTimeStats( void ) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
}

void PIN_INT1_IRQHandler(void) {
	// Clear interrupt status
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(1));

	BaseType_t xHigherPriorityWoken = pdFALSE;
	//EncoderEvent e;
	DigitalIoPin sw_a4(0, 6, DigitalIoPin::pullup, true);	// SIGB
	e.timestamp = xTaskGetTickCount();


	if(sw_a4.read()) {
		e.value = CLOCKWISE;				// Encoder turned clockwise
	}else {
		e.value = COUNTER_CLOCKWISE;			// Encoder turned counter-clockwise
	}


	xQueueSendToBackFromISR(handleMenuQueue, &e, &xHigherPriorityWoken);

	// Perform context switch if needed
	portEND_SWITCHING_ISR(xHigherPriorityWoken);
}

void PIN_INT2_IRQHandler(void) {
	// Clear interrupt status
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(2));

	BaseType_t xHigherPriorityWoken = pdFALSE;
	e.value = OK;											// Value to send in queue
	e.timestamp = xTaskGetTickCount();



	// Send value to queue
	xQueueSendToBackFromISR(handleMenuQueue, &e, &xHigherPriorityWoken);

	// Perform context switch if needed
	portEND_SWITCHING_ISR(xHigherPriorityWoken);
}

void PIN_INT3_IRQHandler(void) {
	// Clear interrupt status
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(3));

	BaseType_t xHigherPriorityWoken = pdFALSE;
	e.value = BACK;											// Value to send in queue
	e.timestamp = xTaskGetTickCount();

	// Send value to queue
	xQueueSendToBackFromISR(handleMenuQueue, &e, &xHigherPriorityWoken);

	// Perform context switch if needed
	portEND_SWITCHING_ISR(xHigherPriorityWoken);
}

}

/* Sets up system hardware */
static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();
}

void pinIntConfig(uint8_t port, uint8_t pin, IRQn_Type pinIntNVICName, uint32_t pinIndex) {
	// Set pin as a digital input with pull-up
	Chip_IOCON_PinMuxSet(LPC_IOCON, port, pin, (IOCON_MODE_PULLUP | IOCON_DIGMODE_EN));
	Chip_GPIO_SetPinDIRInput(LPC_GPIO, port, pin);

	Chip_INMUX_PinIntSel(pinIndex, port, pin);

	// Clear any pending interrupt
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(pinIndex));

	// Set pin mode to trigger on falling edge
	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH(pinIndex));

	// Enable interrupt on falling edge
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH(pinIndex));

	// Set interrupt priority
	NVIC_SetPriority(pinIntNVICName, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1);

	// Enable interrupt in NVIC
	NVIC_ClearPendingIRQ(pinIntNVICName);
	NVIC_EnableIRQ(pinIntNVICName);
}

/* Read from EEPROM */
string readFromEEPROM(uint32_t address){
	string str;

	// Suspend other tasks while accessing EEPROM
	vTaskSuspendAll();

	// read the EEPROM
	uint8_t buffer[64];
	Chip_EEPROM_Read(address, buffer, 64);

	xTaskResumeAll();	// Resume other tasks

	int i = 0;

	while(buffer[i] != '\0') {
		str += buffer[i];
		i++;
	}

	return str;
}


/* Write to EEPROM */
void writeToEEPROM(uint32_t address, string str){
	uint8_t bufferSize = str.length() + 1;
	uint8_t buffer[bufferSize];

	// Copy CO2 Set Point to buffer
	for(uint8_t i = 0; i < str.length(); i++){
		buffer[i] = str[i];
	}

	buffer[str.length()] = '\0';		// Add a terminating char

	// Suspend other tasks while writing to EEPROM
	vTaskSuspendAll();

	Chip_EEPROM_Write(address, buffer, bufferSize);		// Write to EEPROM

	xTaskResumeAll();

}

void navigateMENU(int value, int &CURRENT_SETTINGS, int &SET_CO2, int &SET_NETWORK_PARAMS, LiquidCrystal *lcd){
	if(value == 10)
	{
		if(CURRENT_SETTINGS){
			/* Update flags */
			CURRENT_SETTINGS = 0;
			SET_CO2 = 1;
			SET_NETWORK_PARAMS = 0;

			lcd->clear();
			lcd->setCursor(0, 0);
			lcd->print("Set CO2 Level");
		}else if(SET_CO2){
			/* Update flags */
			CURRENT_SETTINGS = 0;
			SET_CO2 = 0;
			SET_NETWORK_PARAMS = 1;

			lcd->clear();
			lcd->setCursor(2, 0);
			lcd->print("Set Network");
			lcd->setCursor(2, 1);
			lcd->print("Parameters");
		}else {
			/* Update flags */
			CURRENT_SETTINGS = 1;
			SET_CO2 = 0;
			SET_NETWORK_PARAMS = 0;

			lcd->clear();
			lcd->setCursor(0, 0);
			lcd->print("Current Settings");
		}

	}
	else{
		if(CURRENT_SETTINGS){
			/* Update flags */
			CURRENT_SETTINGS = 0;
			SET_CO2 = 0;
			SET_NETWORK_PARAMS = 1;

			lcd->clear();
			lcd->setCursor(2, 0);
			lcd->print("Set Network");
			lcd->setCursor(2, 1);
			lcd->print("Parameters");
		}else if(SET_CO2){
			/* Update flags */
			CURRENT_SETTINGS = 1;
			SET_CO2 = 0;
			SET_NETWORK_PARAMS = 0;

			lcd->clear();
			lcd->setCursor(0, 0);
			lcd->print("Current Settings");
		}else {
			/* Update flags */
			CURRENT_SETTINGS = 0;
			SET_CO2 = 1;
			SET_NETWORK_PARAMS = 0;

			lcd->clear();
			lcd->setCursor(0, 0);
			lcd->print("Set CO2 Level");
		}
	}

}

static void vMqttTask( void * pvParameters )
{
	NetworkContext_t xNetworkContext = { 0 };
	PlaintextTransportParams_t xPlaintextTransportParams = { 0 };
	MQTTContext_t xMQTTContext;
	PlaintextTransportStatus_t xNetworkStatus;

	/* Remove compiler warnings about unused parameters. */
	( void ) pvParameters;

	/* Set the pParams member of the network context with desired transport. */
	xNetworkContext.pParams = &xPlaintextTransportParams;

	ulGlobalEntryTimeMs = prvGetTimeMs();
	char msg[64];

	while(1){
		/****************************** Connect. ******************************/

		/* Attempt to connect to the MQTT broker. If connection fails, retry after
		 * a timeout. The timeout value will exponentially increase until the
		 * maximum number of attempts are reached or the maximum timeout value is
		 * reached. The function below returns a failure status if the TCP connection
		 * cannot be established to the broker after the configured number of attempts. */
		xNetworkStatus = prvConnectToServerWithBackoffRetries( &xNetworkContext );
		configASSERT( xNetworkStatus == PLAINTEXT_TRANSPORT_SUCCESS );

		/* Sends an MQTT Connect packet over the already connected TCP socket,
		 * and waits for a connection acknowledgment (CONNACK) packet. */
		LogInfo( ( "Creating an MQTT connection to %s.", democonfigMQTT_BROKER_ENDPOINT ) );
		prvCreateMQTTConnectionWithBroker( &xMQTTContext, &xNetworkContext );

		/******************* Publish and Keep Alive Loop. *********************/

		/* Publish messages with QoS0, then send and process Keep Alive messages. */
		while(1){
			sprintf(msg, "field1=%d&field2=%d&field3=%d&field4=%d&field5=%d", co2Value, rhValue, tempValue, valveValue, co2SetPt);
			prvMQTTPublishToTopic(msg, &xMQTTContext);
			vTaskDelay(300000);	// 5 min interval between messages
		}
	}
}


/* Task for handling local UI (Menu) */
static void vHandleMenuTask(void *pvParameters){
	// Initialize PININT driver
	Chip_PININT_Init(LPC_GPIO_PIN_INT);

	// Enable clock for PININT
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_PININT);

	// Reset the PININT Block
	Chip_SYSCTL_PeriphReset(RESET_PININT);

	// Configure SIGA (SW_A3) pin for interrupt
	pinIntConfig(0, 5, PIN_INT1_IRQn, 1);

	// Configure encoder button (SW_A2) for interrupt
	pinIntConfig(1, 8, PIN_INT2_IRQn, 2);

	// Configure SW_A5 (back btn) for interrupt
	pinIntConfig(0, 7, PIN_INT3_IRQn, 3);

	// Configure pins for lcd
	DigitalIoPin *rs = new DigitalIoPin(0, 29, DigitalIoPin::output);
	DigitalIoPin *en = new DigitalIoPin(0, 9, DigitalIoPin::output);
	DigitalIoPin *d4 = new DigitalIoPin(0, 10, DigitalIoPin::output);
	DigitalIoPin *d5 = new DigitalIoPin(0, 16, DigitalIoPin::output);
	DigitalIoPin *d6 = new DigitalIoPin(1, 3, DigitalIoPin::output);
	DigitalIoPin *d7 = new DigitalIoPin(0, 0, DigitalIoPin::output);

	// Create lcd
	LiquidCrystal *lcd = new LiquidCrystal(rs, en, d4, d5, d6, d7);

	// configure display geometry
	lcd->begin(16, 2);

	TickType_t elapsedTime = 0, lastTimestamp = 0, filterTime = 350;
	string co2Value_str, temp_str, rh_str, co2SetPt_str, valve_str;
	int co2SetPt = 0;
	int CURRENT_SETTINGS = 1, SET_CO2 = 0, SET_NETWORK_PARAMS = 0;		// Flags for determining menu context
	int viewing_settings = 0, setting_co2_level = 0, setting_ssid = 0, setting_password = 0, selecting_network_params = 0;
	int network_params_set = 0;
	int cursorIndex = 0;  // Initial cursor index for character selection
	const char characters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.!@#$-_?";
	char ssid_char[2] = {'\0', '\0'};
	char pass_char[2] = {'\0', '\0'};
	string user_input_ssid, user_input_password;
	uint8_t cursor_column = 0;

	lcd->setCursor(0, 0);
	lcd->print("Current Settings");

	while(1){
		if (xQueueReceive(handleMenuQueue, &e, 3500) == pdPASS){
			elapsedTime = e.timestamp - lastTimestamp;

			if(elapsedTime >= filterTime){
				lastTimestamp = e.timestamp;	// Update last time stamp

				if(CURRENT_SETTINGS)	// Show current settings
				{
					switch (e.value) {
					case 1:		// OK button pressed
						viewing_settings = 1;

						// Read CO2 Set Point from EEPROM
						co2SetPt_str = readFromEEPROM(CO2_SET_PT_EEPROM_OFFSET);

						co2Value_str = to_string(co2Value);
						temp_str = to_string(tempValue);
						rh_str = to_string(rhValue);
						valve_str = to_string(valveValue);

						lcd->setCursor(0, 0);
						lcd->print("setPT=" + co2SetPt_str + " " + "rh="+ rh_str + "            ");

						lcd->setCursor(0, 1);
						lcd->print("C="+co2Value_str + " " +"t=" + temp_str + " " + "V=" + valve_str + "               " );
						break;

					case -1:		// BACK button pressed
						viewing_settings = 0;

						lcd->clear();
						lcd->setCursor(0, 0);
						lcd->print("Current Settings");
						break;

					case 10:		// Encoder turned clockwise
						if(!viewing_settings){
							navigateMENU(e.value, CURRENT_SETTINGS, SET_CO2, SET_NETWORK_PARAMS, lcd);
						}
						break;

					case -10:		// Encoder turned counter-clockwise
						if(!viewing_settings){
							navigateMENU(e.value, CURRENT_SETTINGS, SET_CO2, SET_NETWORK_PARAMS, lcd);
						}
						break;

					default:
						break;
					}
				}
				else if(SET_CO2)	// Set Co2 Level
				{
					// Read CO2 Set Point from EEPROM
					co2SetPt_str = readFromEEPROM(CO2_SET_PT_EEPROM_OFFSET);

					switch (e.value) {
					case 1:		// OK button pressed
						setting_co2_level = 1;

						lcd->clear();
						lcd->setCursor(0, 0);
						co2SetPt_str = to_string(co2SetPt);
						lcd->print("co2setPT=" + co2SetPt_str);

						writeToEEPROM(CO2_SET_PT_EEPROM_OFFSET, co2SetPt_str);   	// Write the value to EEPROM

						break;

					case -1:		// BACK button pressed
						setting_co2_level = 0;

						lcd->clear();
						lcd->setCursor(0, 0);
						lcd->print("Set CO2 Level");
						break;

					case 10:		// Encoder turned clockwise
						if(setting_co2_level){
							co2SetPt += e.value;

							lcd->clear();
							lcd->setCursor(0, 0);
							lcd->print("co2setPT=" + to_string(co2SetPt));
						}else{						// Navigate through MENU
							navigateMENU(e.value, CURRENT_SETTINGS, SET_CO2, SET_NETWORK_PARAMS, lcd);
						}

						break;

					case -10:		// Encoder turned counter-clockwise
						if(setting_co2_level){
							co2SetPt += e.value;

							// Set point cannot be less than 0
							if(co2SetPt < 0){
								co2SetPt = 0;
							}

							lcd->setCursor(0, 0);
							lcd->print("co2setPT=" + to_string(co2SetPt));
						}else{
							navigateMENU(e.value, CURRENT_SETTINGS, SET_CO2, SET_NETWORK_PARAMS, lcd);
						}

						break;

					default:
						break;
					}

				}
				else{						// Set Network Params
					switch (e.value) {
					case 1:		// OK button pressed
						selecting_network_params = 1;
						if(setting_ssid || setting_password){
							selecting_network_params = 0;
							if(setting_ssid){
								lcd->clear();
								lcd->setCursor(0, 0);
								lcd->print("Enter SSID:        ");
							}else{
								lcd->clear();
								lcd->setCursor(0, 0);
								lcd->print("Enter Password:      ");
							}
							while(!network_params_set){
								if (xQueueReceive(handleMenuQueue, &e, portMAX_DELAY) == pdPASS){
									elapsedTime = e.timestamp - lastTimestamp;

									if(elapsedTime >= filterTime){
										lastTimestamp = e.timestamp;	// Update last time stamp

										switch(e.value){
										case 1:
											if(setting_ssid){
												lcd->setCursor(0, 0);
												lcd->print("Enter SSID:     ");
												if(!(ssid_char[0] == '\0')){						// If not empty
													user_input_ssid += ssid_char;				// Store the char and form a string
													lcd->setCursor(0, 1);
													lcd->print(user_input_ssid);					// Print the user inputed SSID (if any)
												}
											}else{	// Setting password
												lcd->setCursor(0, 0);
												lcd->print("Enter Password: ");
												if(!(pass_char[0] == '\0')){						// If not empty
													user_input_password += pass_char;				// Store the char and form a string
													lcd->setCursor(0, 1);
													lcd->print(user_input_password);					// Print the user inputed Password (if any)
												}
											}
											break;
										case -1:
											// If both fields are entered already, exit
											if(!(user_input_ssid.empty()) && !(user_input_password.empty())){
												network_params_set = 1; 	// Set Flag to break out of loop
												setting_ssid = 0;
												setting_password = 0;

												// Write Network Params to EEPROM
												writeToEEPROM(SSID_EEPROM_OFFSET, user_input_ssid);   	// Write the ssid to EEPROM
												writeToEEPROM(PASS_EEPROM_OFFSET, user_input_password);   	// Write the password to EEPROM

												//Empty the ssid and password strings
												user_input_ssid.clear();
												user_input_password.clear();

												break;
											}

											if(!(user_input_ssid.empty())){
												// Switch to Password
												setting_ssid = 0;
												setting_password = 1;

												lcd->clear();
												lcd->setCursor(0, 0);
												lcd->print("Enter Password:   ");

											}else if(!(user_input_password.empty())){
												// Switch to SSID
												setting_ssid = 1;
												setting_password = 0;

												lcd->clear();
												lcd->setCursor(0, 0);
												lcd->print("Enter SSID:      ");

											}

											break;
										case 10:
											if(setting_ssid){
												lcd->setCursor(0, 0);
												lcd->print("Enter SSID:     ");
												cursorIndex = (cursorIndex + 1) % strlen(characters);

												sprintf(ssid_char, "%c", characters[cursorIndex]);

												// Determine the cursor based on the length of the string
												cursor_column = user_input_ssid.length();

												lcd->setCursor(cursor_column, 1);
												lcd->print(ssid_char);
											}else{	// Setting password
												lcd->setCursor(0, 0);
												lcd->print("Enter Password:   ");
												cursorIndex = (cursorIndex + 1) % strlen(characters);

												sprintf(pass_char, "%c", characters[cursorIndex]);

												// Determine the cursor based on the length of the string
												cursor_column = user_input_password.length();

												lcd->setCursor(cursor_column, 1);
												lcd->print(pass_char);
											}
											break;
										case -10:
											if(setting_ssid){
												lcd->setCursor(0, 0);
												lcd->print("Enter SSID:      ");
												cursorIndex = (cursorIndex - 1 + strlen(characters)) % strlen(characters);

												sprintf(ssid_char, "%c", characters[cursorIndex]);

												// Determine the cursor based on the length of the string
												cursor_column = user_input_ssid.length();

												lcd->setCursor(cursor_column, 1);
												lcd->print(ssid_char);
											}else{	// setting password
												lcd->setCursor(0, 0);
												lcd->print("Enter Password:   ");
												cursorIndex = (cursorIndex - 1 + strlen(characters)) % strlen(characters);

												sprintf(pass_char, "%c", characters[cursorIndex]);

												// Determine the cursor based on the length of the string
												cursor_column = user_input_password.length();

												lcd->setCursor(cursor_column, 1);
												lcd->print(pass_char);
											}
											break;
										default:
											break;
										}

									}


								}

							}

						}else if(selecting_network_params){
							setting_ssid = 1;

							lcd->clear();
							lcd->setCursor(4, 0);
							lcd->print("Set SSID");
						}

						if(network_params_set){
							navigateMENU(e.value, CURRENT_SETTINGS, SET_CO2, SET_NETWORK_PARAMS, lcd);
							network_params_set = 0;
						}
						break;

					case -1:		// BACK button pressed
						selecting_network_params = 0;

						lcd->clear();
						lcd->setCursor(2, 0);
						lcd->print("Set Network");
						lcd->setCursor(2, 1);
						lcd->print("Parameters");
						break;

					case 10:		// Encoder turned clockwise
						if(selecting_network_params){
							lcd->clear();
							lcd->setCursor(2, 0);
							lcd->print("Set Password");

							setting_ssid = 0;
							setting_password = 1;
						}else{
							navigateMENU(e.value, CURRENT_SETTINGS, SET_CO2, SET_NETWORK_PARAMS, lcd);
						}
						break;

					case -10:		// Encoder turned counter-clockwise
						if(selecting_network_params){
							lcd->clear();
							lcd->setCursor(4, 0);
							lcd->print("Set SSID");

							setting_ssid = 1;
							setting_password = 0;
						}else{
							navigateMENU(e.value, CURRENT_SETTINGS, SET_CO2, SET_NETWORK_PARAMS, lcd);
						}

						break;

					default:
						break;
					}
				}

			}
		}
		else{
			lastTimestamp = e.timestamp;	// Update last time stamp

			// Read CO2 Set Point from EEPROM
			co2SetPt_str = readFromEEPROM(CO2_SET_PT_EEPROM_OFFSET);

			co2Value_str = to_string(co2Value);
			temp_str = to_string(tempValue);
			rh_str = to_string(rhValue);
			valve_str = to_string(valveValue);
			lcd->clear();
			lcd->setCursor(0, 0);
			lcd->print("setPT=" + co2SetPt_str + " " + "rh="+ rh_str + "            ");

			lcd->setCursor(0, 1);
			lcd->print("C="+co2Value_str + " " +"t=" + temp_str + " " + "V=" + valve_str + "               " );
		}
	}
}


/* Read sensors on the system */
void vSensorTask(void *pvParameters){
	int counter = 0;

	HMP60 hmp60;
	GMP252 gmp252;

	DigitalIoPin relay(0, 27, DigitalIoPin::output);

	string co2SetPt_str;
	int co2_setPt = 0;
	char buf[64];
	int valve_close_counter = 10000;

	while(1){
		// Close the valve if open for 5 seconds
		if(valveValue == 1 && counter >= 5000){
			relay.write(false);		// Close valve
			valveValue = 0;
			counter = 0;
			/* Sleep for 10 seconds and then proceed to read the sensor data again.
			 * This will allow the co2 gas to be dispersed in the greenhouse.
			 */
			vTaskDelay(10000);
		}

		// Read CO2 Set Point from EEPROM
		co2SetPt_str = readFromEEPROM(CO2_SET_PT_EEPROM_OFFSET);

		// Convert str to int
		co2_setPt = stoi(co2SetPt_str);

		// Read co2 sensor status
		if(gmp252.readStatus()){

			// Read co2 value
			vTaskDelay(10);
			co2Value = gmp252.readCO2();

			if(co2Value < co2_setPt){
				relay.write(true);		// Open valve
				valveValue = 1;
				counter += 50;
			}else if(co2Value > co2_setPt){
				relay.write(false);		// Close valve
				valveValue = 0;
				counter = 0;
			}

		}

		// Read Temperature and Humidity sensor status
		if(hmp60.readStatus()){

			// Read temperature
			vTaskDelay(10);
			tempValue = hmp60.readTemp();
			vTaskDelay(10);

			// Read humidity
			vTaskDelay(10);
			rhValue = hmp60.readRH();
			vTaskDelay(10);
		}

	}
}


int main(void) {
	prvSetupHardware();

	// Enable EEPROM
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_EEPROM);
	Chip_SYSCTL_PeriphReset(RESET_EEPROM);

	handleMenuQueue = xQueueCreate(QUEUE_LENGTH, sizeof(int));
	vQueueAddToRegistry(handleMenuQueue, "Handle Menu Events Queue");

	heap_monitor_setup();

	xTaskCreate(vSensorTask, "SensorTask",
			configMINIMAL_STACK_SIZE * 8, NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t*) NULL);

	xTaskCreate(vHandleMenuTask, "HandleMenuTask",
			configMINIMAL_STACK_SIZE * 8, NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t*) NULL);

	xTaskCreate(vMqttTask, "MQTT_Task",
			configMINIMAL_STACK_SIZE * 8, NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t*) NULL);

	// Start the scheduler
	vTaskStartScheduler();

	// Should never reach here
	return 1;
}

