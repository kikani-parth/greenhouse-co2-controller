# greenhouse-co2-controller

#Introduction

The project's objective is to develop an automated system for managing the greenhouse's CO2 level. Modbus connections are used to connect sensors to obtain readings of the many environmental parameters, such as temperature, humidity, and CO2 level, that impact crop growth. This project makes use of the LPC 1549 microcontroller. After that, the readings are sent to the local user interface for display. The rotary encoder and its button are used to control the Co2 set point. The user-set CO2 Setpoint determines when the solenoid valve opens and closes. Co2 gas is injected once the valve is opened and continues until the setpoint is reached.The readings of Co2, Temperature, Humidity, and valve state along with the Co2 set point are also sent to the ThingSpeak Cloud service via MQTT.

#Components
1. LPC1549 - Microcontroller used in this system.
2. CO2 Sensor: **GMP252** - Used to read CO2 levels within the environment.
3. Temperature and Humidity Sensor: **HMP60** - Provides temperature and humidity values.
4. Valve - Opens to inject CO2 gas into the greenhouse when the CO2 value is below the CO2 set point. 
5. LCD Display - Shows all gathered data such as CO2 set point, CO2 level, valve state,  temperature, and humidity.
6. Rotary Encoder - Enables user control and adjustment of the CO2 set point and Network Parameters through the local UI.
