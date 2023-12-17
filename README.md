# Greenhouse CO2 Controller

## Introduction

The project's objective is to develop an automated system for managing the greenhouse's CO2 level. Modbus connections are used to connect sensors to obtain readings of the many environmental parameters, such as temperature, humidity, and CO2 level, that impact crop growth. This project makes use of the LPC 1549 microcontroller. After that, the readings are sent to the local user interface for display. The rotary encoder and its button are used to control the Co2 set point. The user-set CO2 Setpoint determines when the solenoid valve opens and closes. Co2 gas is injected once the valve is opened and continues until the setpoint is reached.The readings of Co2, Temperature, Humidity, and valve state along with the Co2 set point are also sent to the ThingSpeak Cloud service via MQTT.

## Components
1. LPC1549 - Microcontroller used in this system.
2. CO2 Sensor: **GMP252** - Used to read CO2 levels within the environment.
3. Temperature and Humidity Sensor: **HMP60** - Provides temperature and humidity values.
4. Valve - Opens to inject CO2 gas into the greenhouse when the CO2 value is below the CO2 set point. 
5. LCD Display - Shows all gathered data such as CO2 set point, CO2 level, valve state,  temperature, and humidity.
6. Rotary Encoder - Enables user control and adjustment of the CO2 set point and Network Parameters through the local UI.

## User Manual

### Controlling the UI

![image](https://github.com/kikani-parth/greenhouse-co2-controller/assets/99806873/e8b2796f-4ec1-43d3-ba2b-bf0874c897f0)

  **Fig 1. Rotary Encoder**
  
The Rotary encoder is used to navigate through the UI. There is also a button on the tip of the shaft which acts like an ‘OK’ button to select options displayed on the LCD.

![image](https://github.com/kikani-parth/greenhouse-co2-controller/assets/99806873/b8bf11dd-88be-49fd-b204-260febaa117a)

**Fig 2. BACK button (Button in red circle)**

The user can exit a specific view by pressing the ‘BACK’ button (SW_A5).

### Current Settings View

As the system boots up, the LCD displays ‘Current Settings’ view (see Fig 3.)

![image](https://github.com/kikani-parth/greenhouse-co2-controller/assets/99806873/709ba3e4-9e17-4dd9-b85c-b36013f0223a)

**Fig 3. Current Settings View**

The user can select this by pressing the ‘OK’ button to view the current settings (see Fig 4.)

![image](https://github.com/kikani-parth/greenhouse-co2-controller/assets/99806873/71f9676d-c72a-4d7b-b047-dc86dd7a4048)

**Fig 4. Current Co2 set point, sensor and valve readings**

The LCD screen has five parameters:

**setPT** is the co2 set point that can be adjusted based on requirements.

**rh** is the relative humidity measurement.

**C** is the current co2 level in the greenhouse.

**t** is the temperature measurement.

**V** stands for the solenoid valve's status: V=0 (closed valve) and V=1 (open valve)

_Note: If there is 3.5 seconds of inactivity (i.e. no button is pressed or the encoder is left unturned), the system will display this view automatically to allow the user to monitor the sensor and valve state readings._

### Set CO2 Level View

The user can turn the rotary encoder and get the ‘Set CO2 Level’ view. (see Fig 5.)

![image](https://github.com/kikani-parth/greenhouse-co2-controller/assets/99806873/e9585c79-880d-4831-9787-5caea1934caf)

**Fig 5. Set CO2 Level View**

The user can select this by pressing the ‘OK’ button to set the required CO2 level in the greenhouse. Turn the encoder clockwise/counter-clockwise to increase/decrease the set point. Press the ‘OK’ button to set the co2 level.

![image](https://github.com/kikani-parth/greenhouse-co2-controller/assets/99806873/522ecabc-6c7c-4006-ad14-8e1e3a05330a)

**Fig 6. Setting co2 level**

### Set Network Parameters View

The user can turn the encoder to get the ‘Set Network Parameters’ view (see Fig 7.)

![image](https://github.com/kikani-parth/greenhouse-co2-controller/assets/99806873/fccd9716-7f89-4e8a-85c8-9fc5ba640cec)

**Fig 7. Set Network Parameters View**

The user can press the ‘OK’ button which will allow them to enter the SSID. Rotate the encoder clockwise/counter-clockwise to start navigating through the characters. Once you get your desired character, press the ‘OK’ button to enter it. Then, move onto selecting the second character and press the ‘OK’ button to enter the second character. Similarly, follow this pattern until the required SSID is entered. Then, the user can press the ‘BACK’ button and the system will prompt the user to enter the PASSWORD. Enter the password in a similar fashion as the SSID and lastly press the ‘BACK’ button when done.

![image](https://github.com/kikani-parth/greenhouse-co2-controller/assets/99806873/3f9d739a-d139-40c9-a67f-ae5a527b4ac0)

**Fig 8. Entering SSID**

![image](https://github.com/kikani-parth/greenhouse-co2-controller/assets/99806873/17bce1bc-c0f2-4922-8fb2-e2363701e5a6)

**Fig 9. Entering Password**
