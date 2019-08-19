// Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)
// This is a human-readable summary of (and not a substitute for) the license.
// Disclaimer
//
// You are free to:
// Share — copy and redistribute the material in any medium or format
// Adapt — remix, transform, and build upon the material
// The licensor cannot revoke these freedoms as long as you follow the license terms.
//
// Under the following terms:
// Attribution — You must give appropriate credit, provide a link to the license, and indicate if changes were made. You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
// NonCommercial — You may not use the material for commercial purposes.
// ShareAlike — If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.
// No additional restrictions — You may not apply legal terms or technological measures that legally restrict others from doing anything the license permits.
//
// Notices:
// You do not have to comply with the license for elements of the material in the public domain or where your use is permitted by an applicable exception or limitation.
// No warranties are given. The license may not give you all of the permissions necessary for your intended use. For example, other rights such as publicity, privacy, or moral rights may limit how you use the material.
//
// Author: Gustavo Gonnet
// Date: August 12 2019
// github: https://github.com/gusgonnet/
// hackster: https://www.hackster.io/gusgonnet/
//
// FREE FOR PERSONAL USE.
//
// https://creativecommons.org/licenses/by-nc-sa/4.0/

#define APP_NAME "Thermostat3Gen"
#define APP_VERSION "Version 0.01"

/*******************************************************************************
 * changes in version 0.01:
       * first version, based on minimalist thermostat version 0.30 from here: https://www.hackster.io/gusgonnet/the-minimalist-thermostat-bb0410
                 
TODO:
  * set max time for heating or cooling in 5 hours (alarm) or 6 hours (auto-shut-off)
  * #define STATE_FAN_ON "Fan On" -> the fan status should show up in the status
  * the fan goes off few seconds after the cooling is on

*******************************************************************************/

/*******************************************************************************
 Here you decide if you want to use Blynk or not by 
 commenting this line "#define USE_BLYNK" (or not)
*******************************************************************************/
#define USE_BLYNK

#include "elapsedMillis.h"
#include "FiniteStateMachine.h"
#include "NCD4Relay.h"

#include "PietteTech_DHT.h"
#include "AnalogSmooth.h"

SerialLogHandler logHandler(LOG_LEVEL_ALL);

#ifdef USE_BLYNK
#include "blynk.h"
#include "blynkAuthToken.h"
#endif

/*******************************************************************************
 initialize FSM states with proper enter, update and exit functions
*******************************************************************************/
State initState = State(initEnterFunction, initUpdateFunction, initExitFunction);
State idleState = State(idleEnterFunction, idleUpdateFunction, idleExitFunction);
State heatingState = State(heatingEnterFunction, heatingUpdateFunction, heatingExitFunction);
State pulseState = State(pulseEnterFunction, pulseUpdateFunction, pulseExitFunction);
State coolingState = State(coolingEnterFunction, coolingUpdateFunction, coolingExitFunction);

//initialize state machine, start in state: Idle
FSM thermostatStateMachine = FSM(initState);

//milliseconds for the init cycle, so temperature samples get stabilized
//this should be in the order of the 5 minutes: 5*60*1000==300000
//for now, I will use 1 minute
#define INIT_TIMEOUT 60000
elapsedMillis initTimer;

//minimum number of milliseconds to leave the heating element on
// to protect on-off on the fan and the heating/cooling elements
#define MINIMUM_ON_TIMEOUT 60000
elapsedMillis minimumOnTimer;

//minimum number of milliseconds to leave the system in idle state
// to protect the fan and the heating/cooling elements
#define MINIMUM_IDLE_TIMEOUT 60000
elapsedMillis minimumIdleTimer;

//milliseconds to pulse on the heating = 600 seconds = 10 minutes
// turns the heating on for a certain time
// comes in handy when you want to warm up the house a little bit
#define PULSE_TIMEOUT 600000
elapsedMillis pulseTimer;

/*******************************************************************************
 IO mapping
*******************************************************************************/
// D1 : relay1: fan
// D2 : relay2: heat
// D3 : relay3: cool
// D4 : relay4: not used
// D5 : DHT22
int fan = D1;
int heat = D2;
int cool = D3;
//TESTING_HACK
int fanOutput;
int heatOutput;
int coolOutput;

/*******************************************************************************
 DHT sensor
*******************************************************************************/
#define DHTTYPE DHT22                    // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN 5                         // Digital pin for communications
#define TEMPERATURE_SAMPLE_INTERVAL 5000 // Sample room temperature every 5 seconds
void dht_wrapper();                      // must be declared before the lib initialization
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);
bool bDHTstarted; // flag to indicate we started acquisition
elapsedMillis temperatureSampleInterval;

/*******************************************************************************
 thermostat related declarations
*******************************************************************************/

/*******************************************************************************
 CELSIUS or FAHRENHEIT
*******************************************************************************/
bool useFahrenheit = false;

String statusDHTmain = "";

//temperature related variables - internal
float targetTemp = 19.0;
float currentTemp = 0.0;
float currentHumidity = 0.0;

//you can change this to your liking
// a smaller value will make your temperature more constant at the price of
//  starting the heat more times
// a larger value will reduce the number of times the HVAC comes on but will leave it on a longer time
float margin = 0.20;

//sensor difference with real temperature (if none set to zero)
//use this variable to align measurements with your existing thermostat
float temperatureCalibration = -1.35;

//temperature related variables - to be exposed in the cloud
String targetTempString = String(targetTemp);           //String to store the target temp so it can be exposed and set
String currentTempString = String(currentTemp);         //String to store the sensor's temp so it can be exposed
String currentHumidityString = String(currentHumidity); //String to store the sensor's humidity so it can be exposed

AnalogSmooth analogSmoothTemperature = AnalogSmooth(20); // get 20 samples
AnalogSmooth analogSmoothHumidity = AnalogSmooth(20);    // get 20 samples

#define DEBOUNCE_SETTINGS 2000
#define DEBOUNCE_SETTINGS_MODE 4000 //give more time to the MODE change
float newTargetTemp = 19.0;
elapsedMillis setNewTargetTempTimer;

bool externalFan = false;
bool internalFan = false;
bool fanButtonClick = false;
elapsedMillis fanButtonClickTimer;

bool externalPulse = false;
bool internalPulse = false;
bool pulseButtonClick = false;
elapsedMillis pulseButtonClickTimer;

//here are the possible modes the thermostat can be in: off/heat/cool
#define MODE_OFF "Off"
#define MODE_HEAT "Heat"
#define MODE_COOL "Cool"
String externalMode = MODE_OFF;
String internalMode = MODE_OFF;
bool modeButtonClick = false;
elapsedMillis modeButtonClickTimer;

//here are the possible states of the thermostat
#define STATE_INIT "Initializing"
#define STATE_IDLE "Idle"
#define STATE_HEATING "Heating"
#define STATE_COOLING "Cooling"
#define STATE_FAN_ON "Fan On"
#define STATE_OFF "Off"
#define STATE_PULSE_HEAT "Pulse Heat"
#define STATE_PULSE_COOL "Pulse Cool"
String state = STATE_INIT;

// timers work on millis, so we adjust the value with this constant
#define MILLISECONDS_TO_MINUTES 60000
#define MILLISECONDS_TO_SECONDS 1000

//TESTING_HACK
// this allows me to system test the project
bool testing = false;

#define BUFFER 623
#define LITTLE 50

/*******************************************************************************
 Your blynk token goes in another file to avoid sharing it by mistake
 The file containing your blynk auth token has to be named blynkAuthToken.h and it should 
 contain something like this:
  #define BLYNK_AUTH_TOKEN "1234567890123456789012345678901234567890"
 replace with your project auth token (the blynk app will give you one)
*******************************************************************************/
#ifdef USE_BLYNK
char auth[] = BLYNK_AUTH_TOKEN;

//definitions for the blynk interface
#define BLYNK_DISPLAY_CURRENT_TEMP V0
#define BLYNK_DISPLAY_HUMIDITY V1
#define BLYNK_DISPLAY_TARGET_TEMP V2
#define BLYNK_SLIDER_TEMP V10

#define BLYNK_BUTTON_FAN V11
#define BLYNK_LED_FAN V3
#define BLYNK_LED_HEAT V4
#define BLYNK_LED_COOL V5

#define BLYNK_DISPLAY_MODE V7
#define BLYNK_BUTTON_MODE V8
#define BLYNK_LED_PULSE V6
#define BLYNK_BUTTON_PULSE V12
#define BLYNK_DISPLAY_STATE V13

//this is one remote temperature sensor
#define BLYNK_DISPLAY_CURRENT_TEMP_REMOTE V9
#define BLYNK_DISPLAY_CURRENT_TEMP_REMOTE_LAST_HEARD_OF V14

//this is the pool
#define BLYNK_DISPLAY_CURRENT_TEMP_POOL V25
#define BLYNK_DISPLAY_CURRENT_TEMP_POOL_LAST_HEARD_OF V26

//this is the water leak sensor
#define BLYNK_DISPLAY_WATER_LEAK_SENSOR V20
#define BLYNK_DISPLAY_WATER_LEAK_SENSOR_LAST_HEARD_OF V21

//this is the sump pump
#define BLYNK_DISPLAY_SUMP_PUMP V22
#define BLYNK_DISPLAY_SUMP_PUMP_LAST_HEARD_OF V23

//this is the garage
#define BLYNK_DISPLAY_GARAGE_STATUS V31
#define BLYNK_DISPLAY_GARAGE_LAST_HEARD_OF V32

#define BLYNK_BUTTON_GARAGE_OPEN V33
#define BLYNK_BUTTON_GARAGE_MOVE V34
#define BLYNK_BUTTON_GARAGE_CLOSE V35

//this defines how often the readings are sent to the blynk cloud (millisecs)
#define BLYNK_STORE_INTERVAL 5000
elapsedMillis blynkStoreInterval;

WidgetLED fanLed(BLYNK_LED_FAN);     //register led to virtual pin 3
WidgetLED heatLed(BLYNK_LED_HEAT);   //register led to virtual pin 4
WidgetLED coolLed(BLYNK_LED_COOL);   //register led to virtual pin 5
WidgetLED pulseLed(BLYNK_LED_PULSE); //register led to virtual pin 6
#endif

//enable the user code (our program below) to run in parallel with cloud connectivity code
// source: https://docs.particle.io/reference/firmware/photon/#system-thread
SYSTEM_THREAD(ENABLED);

NCD4Relay relayController;

const int TIME_ZONE = -4;

/*******************************************************************************
 structure for writing thresholds in eeprom
 https://docs.particle.io/reference/firmware/photon/#eeprom
*******************************************************************************/
//randomly chosen value here. The only thing that matters is that it's not 255
// since 255 is the default value for uninitialized eeprom
// I used 137 and 138 in version 0.21 already
#define EEPROM_VERSION 139
#define EEPROM_ADDRESS 0

struct EepromMemoryStructure
{
  uint8_t version = EEPROM_VERSION;
  uint8_t targetTemp;
  uint8_t internalFan;
  uint8_t internalMode;
};
EepromMemoryStructure eepromMemory;

bool settingsHaveChanged = false;
elapsedMillis settingsHaveChanged_timer;
#define SAVE_SETTINGS_INTERVAL 10000

/*******************************************************************************
 mesh stuff - this HAS to be the SAME in the other devices' firmware
*******************************************************************************/
#define MESH_EVENT_DS18B20 "meshTempSensorDs18b20"

#define MESH_EVENT_POOL "meshPoolSensor"

#define MESH_EVENT_WATER_LEAK_SENSOR "meshWaterLeakSensor"

#define MESH_EVENT_SUMP_PUMP "meshSumpPump"

#define MESH_EVENT_GARAGE "garageStatus"
#define MESH_EVENT_GARAGE_COMMAND "garageCommand"
#define COMMAND_CLOSE_GARAGE "close"
#define COMMAND_OPEN_GARAGE "open"
#define COMMAND_MOVE_GARAGE "move"

/*******************************************************************************
 remote temperature mesh sensor
*******************************************************************************/
double meshTempSensorCurrentTemp = -999;
String meshTempSensorLastHeardOf = "Never";

// enable the remote temperature sensor if on an argon
#if PLATFORM_ID == PLATFORM_ARGON
void meshTempSensorHandler(const char *event, const char *data)
{
  char tempChar[BUFFER] = "";
  snprintf(tempChar, BUFFER, "event=%s data=%s", event, data ? data : "NULL");
  Log.info(tempChar);

  snprintf(tempChar, BUFFER, "%s", data ? data : "-999");
  meshTempSensorCurrentTemp = atof(tempChar);

  meshTempSensorLastHeardOf = Time.timeStr();

#ifdef USE_BLYNK
  Blynk.virtualWrite(BLYNK_DISPLAY_CURRENT_TEMP_REMOTE, meshTempSensorCurrentTemp);
  Blynk.virtualWrite(BLYNK_DISPLAY_CURRENT_TEMP_REMOTE_LAST_HEARD_OF, meshTempSensorLastHeardOf);
#endif
}
#endif

/*******************************************************************************
 pool mesh sensor
*******************************************************************************/
double meshTempSensorPoolCurrentTemp = -999;
String meshTempSensorPoolLastHeardOf = "Never";

// enable the remote temperature sensor for the pool if on an argon
#if PLATFORM_ID == PLATFORM_ARGON
void meshTempSensorPoolHandler(const char *event, const char *data)
{
  char tempChar[BUFFER] = "";
  snprintf(tempChar, BUFFER, "event=%s data=%s", event, data ? data : "NULL");
  Log.info(tempChar);

  snprintf(tempChar, BUFFER, "%s", data ? data : "-999");
  meshTempSensorPoolCurrentTemp = atof(tempChar);

  meshTempSensorPoolLastHeardOf = Time.timeStr();

#ifdef USE_BLYNK
  Blynk.virtualWrite(BLYNK_DISPLAY_CURRENT_TEMP_POOL, meshTempSensorPoolCurrentTemp);
  Blynk.virtualWrite(BLYNK_DISPLAY_CURRENT_TEMP_POOL_LAST_HEARD_OF, meshTempSensorPoolLastHeardOf);
#endif
}
#endif

/*******************************************************************************
 water leak sensor
*******************************************************************************/
String meshWaterLeakSensorState = "Unknown";
String meshWaterLeakSensorLastHeardOf = "Never";

// enable the remote temperature sensor for the pool if on an argon
#if PLATFORM_ID == PLATFORM_ARGON
void meshWaterLeakSensorHandler(const char *event, const char *data)
{
  char tempChar[BUFFER] = "";
  snprintf(tempChar, BUFFER, "event=%s data=%s", event, data ? data : "NULL");
  Log.info(tempChar);

  snprintf(tempChar, BUFFER, "%s", data ? data : "Unknown");
  meshWaterLeakSensorState = tempChar;

  meshWaterLeakSensorLastHeardOf = Time.timeStr();

#ifdef USE_BLYNK
  Blynk.virtualWrite(BLYNK_DISPLAY_WATER_LEAK_SENSOR, meshWaterLeakSensorState);
  Blynk.virtualWrite(BLYNK_DISPLAY_WATER_LEAK_SENSOR_LAST_HEARD_OF, meshWaterLeakSensorLastHeardOf);
#endif
}
#endif

/*******************************************************************************
sump pump sensors
*******************************************************************************/
String meshSumpPumpState = "Unknown";
String meshSumpPumpLastHeardOf = "Never";

// enable the remote temperature sensor for the pool if on an argon
#if PLATFORM_ID == PLATFORM_ARGON
void meshSumpPumpHandler(const char *event, const char *data)
{
  char tempChar[BUFFER] = "";
  snprintf(tempChar, BUFFER, "event=%s data=%s", event, data ? data : "NULL");
  Log.info(tempChar);

  snprintf(tempChar, BUFFER, "%s", data ? data : "Unknown");
  meshSumpPumpState = tempChar;

  meshSumpPumpLastHeardOf = Time.timeStr();

#ifdef USE_BLYNK
  Blynk.virtualWrite(BLYNK_DISPLAY_SUMP_PUMP, meshSumpPumpState);
  Blynk.virtualWrite(BLYNK_DISPLAY_SUMP_PUMP_LAST_HEARD_OF, meshSumpPumpLastHeardOf);
#endif
}
#endif

/*******************************************************************************
 garage mesh device
*******************************************************************************/
String meshGarageStatus = "Not connected";
String meshGarageLastHeardOf = "Never";

// enable the garage mesh device if on an argon
#if PLATFORM_ID == PLATFORM_ARGON
void garageStatusHandler(const char *event, const char *data)
{
  char tempChar[BUFFER] = "";
  snprintf(tempChar, BUFFER, "event=%s data=%s", event, data ? data : "NULL");
  Log.info(tempChar);

  snprintf(tempChar, BUFFER, "%s", data ? data : "Not connected");
  meshGarageStatus = tempChar;

  meshGarageLastHeardOf = Time.timeStr();

#ifdef USE_BLYNK
  Blynk.virtualWrite(BLYNK_DISPLAY_GARAGE_STATUS, meshGarageStatus);
  Blynk.virtualWrite(BLYNK_DISPLAY_GARAGE_LAST_HEARD_OF, meshGarageLastHeardOf);
#endif
}
#endif

/*******************************************************************************
 * Function Name  : setup
 * Description    : this function runs once at system boot
 *******************************************************************************/
void setup()
{

  // declare cloud variables
  Particle.variable("targetTemp", targetTempString);
  Particle.variable("currentTemp", currentTempString);
  Particle.variable("humidity", currentHumidityString);
  Particle.variable("mode", externalMode);
  Particle.variable("state", state);
  Particle.variable("statusDHTmain", statusDHTmain);

  Particle.variable("meshTempSensorCurrentTemp", meshTempSensorCurrentTemp);
  Particle.variable("meshTempSensorLastHeardOf", meshTempSensorLastHeardOf);

  Particle.variable("meshGarageStatus", meshGarageStatus);
  Particle.variable("meshGarageLastHeardOf", meshGarageLastHeardOf);

  // declare cloud functions
  Particle.function("setTargetTmp", setTargetTemp);
  Particle.function("setMode", setMode);
  Particle.function("setFan", setFan);

#if PLATFORM_ID == PLATFORM_ARGON
  // enable the remote temperature sensor if on an argon
  Mesh.subscribe(MESH_EVENT_DS18B20, meshTempSensorHandler);

  // enable the remote temperature sensor if on an argon
  Mesh.subscribe(MESH_EVENT_POOL, meshTempSensorPoolHandler);

  // enable the water leak sensor if on an argon
  Mesh.subscribe(MESH_EVENT_WATER_LEAK_SENSOR, meshWaterLeakSensorHandler);

  // enable the sump pump sensor if on an argon
  Mesh.subscribe(MESH_EVENT_SUMP_PUMP, meshSumpPumpHandler);

  // enable the garage mesh device if on an argon
  Mesh.subscribe(MESH_EVENT_GARAGE, garageStatusHandler);
#endif

  // Serial.begin(115200);
  relayController.setAddress(0, 0, 0);
  relayController.turnOffAllRelays();

  Time.zone(TIME_ZONE);

#ifdef USE_BLYNK
  Blynk.begin(auth);
#endif

  //restore settings from eeprom, if there were any saved before
  readFromEeprom();

  //publish startup message with firmware version
  Particle.publish(APP_NAME, APP_VERSION, 60, PRIVATE);
}

// This wrapper is in charge of calling the DHT sensor lib
void dht_wrapper() { DHT.isrCallback(); }

/*******************************************************************************
 * Function Name  : loop
 * Description    : this function runs continuously while the project is running
 *******************************************************************************/
void loop()
{

  //this function reads the temperature of the DHT sensor
  readTemperature();

#ifdef USE_BLYNK
  //all the Blynk magic happens here
  Blynk.run();

  //publish readings to the blynk server every minute so the History Graph gets updated
  // even when the blynk app is not on (running) in the users phone
  updateBlynkCloud();
#endif

  updateTargetTemp();
  updateFanStatus();
  updatePulseStatus();
  updateMode();

  //this function updates the FSM
  // the FSM is the heart of the thermostat - all actions are defined by its states
  thermostatStateMachine.update();

  //every now and then we save the settings
  saveSettings();
}

/*******************************************************************************/
/*******************************************************************************/
/*******************          CLOUD FUNCTIONS         *************************/
/*******************************************************************************/
/*******************************************************************************/

/*******************************************************************************
 * Function Name  : setFan
 * Description    : sets the target temperature of the thermostat
                    newTargetTemp has to be a valid float value, or no new target temp will be set
 * Return         : 0 if all is good, or -1 if the parameter cannot be converted to float or
                    is not in the accepted range (15<t<30 celsius)
 *******************************************************************************/
int setFan(String newFan)
{
  if (newFan.equalsIgnoreCase("on"))
  {
    internalFan = true;
    flagSettingsHaveChanged();
    return 0;
  }

  if (newFan.equalsIgnoreCase("off"))
  {
    internalFan = false;
    flagSettingsHaveChanged();
    return 0;
  }

  // else parameter was invalid
  return -1;
}

/*******************************************************************************
 * Function Name  : setMode
 * Description    : sets the target temperature of the thermostat
                    newTargetTemp has to be a valid float value, or no new target temp will be set
 * Return         : 0 if all is good, or -1 if the parameter cannot be converted to float or
                    is not in the accepted range (15<t<30 celsius)
 *******************************************************************************/
int setMode(String newMode)
{
  //mode: cycle through off->heating->cooling
  // do this only when blynk sends a 1
  // background: in a BLYNK push button, blynk sends 0 then 1 when user taps on it
  // source: http://docs.blynk.cc/#widgets-controllers-button
  if ((newMode != MODE_OFF) && (newMode != MODE_HEAT) && (newMode != MODE_COOL))
  {
    return -1;
  }

  externalMode = newMode;
  flagSettingsHaveChanged();
  return 0;
}

/*******************************************************************************
 * Function Name  : setTargetTemp
 * Description    : sets the target temperature of the thermostat
                    newTargetTemp has to be a valid float value, or no new target temp will be set
 * Return         : 0 if all is good, or -1 if the parameter cannot be converted to float or
                    is not in the accepted range (15<t<30 celsius)
 *******************************************************************************/
int setTargetTemp(String temp)
{
  float tmpFloat = temp.toFloat();
  //update the target temp only in the case the conversion to float works
  // (toFloat returns 0 if there is a problem in the conversion)
  // sorry, if you wanted to set 0 as the target temp, you can't :)
  if ((tmpFloat > 0) && (tmpFloat > 14.9) && (tmpFloat < 31))
  {
    //newTargetTemp will be copied to targetTemp moments after in function updateTargetTemp()
    // this is to 1-debounce the blynk slider I use and 2-debounce the user changing his/her mind quickly
    targetTemp = tmpFloat;
    targetTempString = float2string(targetTemp);
    flagSettingsHaveChanged();
    return 0;
  }

  //show only 2 decimals in notifications
  // Example: show 19.00 instead of 19.000000
  temp = temp.substring(0, temp.length() - 4);

  //if the execution reaches here then the value was invalid
  //Particle.publish(APP_NAME, "ERROR: Failed to set new target temp to " + temp, 60, PRIVATE);
  String tempStatus = "ERROR: Failed to set new target temp to " + temp + getTime();
  publishEvent(tempStatus);
  return -1;
}

/*******************************************************************************
 * Function Name  : setTargetTempInternal
 * Description    : sets the target temperature of the thermostat
                    newTargetTemp has to be a valid float value, or no new target temp will be set
 * Behavior       : the new setting will not take place right away, but moments after
                    since a timer is triggered. This is to debounce the setting and
                    allow the users to change their mind
* Return         : 0 if all is good, or -1 if the parameter cannot be converted to float or
                    is not in the accepted range (15<t<30 celsius)
 *******************************************************************************/
int setTargetTempInternal(String temp)
{
  float tmpFloat = temp.toFloat();
  //update the target temp only in the case the conversion to float works
  // (toFloat returns 0 if there is a problem in the conversion)
  // sorry, if you wanted to set 0 as the target temp, you can't :)
  if ((tmpFloat > 0) && (tmpFloat > 14.9) && (tmpFloat < 31))
  {
    //newTargetTemp will be copied to targetTemp moments after in function updateTargetTemp()
    // this is to 1-debounce the blynk slider I use and 2-debounce the user changing his/her mind quickly
    newTargetTemp = tmpFloat;
    //start timer to debounce this new setting
    setNewTargetTempTimer = 0;
    return 0;
  }

  //show only 2 decimals in notifications
  // Example: show 19.00 instead of 19.000000
  temp = temp.substring(0, temp.length() - 4);

  //if the execution reaches here then the value was invalid
  String tempStatus = "ERROR: Failed to set new target temp to " + temp + getTime();
  publishEvent(tempStatus);
  return -1;
}

/*******************************************************************************
 * Function Name  : updateTargetTemp
 * Description    : updates the value of target temperature of the thermostat
                    moments after it was set with setTargetTempInternal
 * Return         : none
 *******************************************************************************/
void updateTargetTemp()
{
  //debounce the new setting
  if (setNewTargetTempTimer < DEBOUNCE_SETTINGS)
  {
    return;
  }
  //is there anything to update?
  if (targetTemp == newTargetTemp)
  {
    return;
  }

  targetTemp = newTargetTemp;
  targetTempString = float2string(targetTemp);

  String tempStatus = "New target temp: " + targetTempString + "°C" + getTime();
  publishEvent(tempStatus);
}

/*******************************************************************************
 * Function Name  : float2string
 * Description    : return the string representation of the float number
                     passed as parameter with 2 decimals
 * Return         : the string
 *******************************************************************************/
String float2string(float floatNumber)
{
  String stringNumber = String(floatNumber);

  //return only 2 decimals
  // Example: show 19.00 instead of 19.000000
  stringNumber = stringNumber.substring(0, stringNumber.length() - 4);

  return stringNumber;
}

/*******************************************************************************
 * Function Name  : updateFanStatus
 * Description    : updates the status of the fan moments after it was set
 * Return         : none
 *******************************************************************************/
void updateFanStatus()
{
  //if the button was not pressed, get out
  if (not fanButtonClick)
  {
    return;
  }

  //debounce the new setting
  if (fanButtonClickTimer < DEBOUNCE_SETTINGS)
  {
    return;
  }

  //reset flag of button pressed
  fanButtonClick = false;

  //is there anything to update?
  // this code here takes care of the users having cycled the mode to the same original value
  if (internalFan == externalFan)
  {
    return;
  }

  //update the new setting from the external to the internal variable
  internalFan = externalFan;

  if (internalFan)
  {
    String tempStatus = "Fan on" + getTime();
    publishEvent(tempStatus);
  }
  else
  {
    String tempStatus = "Fan off" + getTime();
    publishEvent(tempStatus);
  }
}

/*******************************************************************************
 * Function Name  : updatePulseStatus
 * Description    : updates the status of the pulse of the thermostat
                    moments after it was set
 * Return         : none
 *******************************************************************************/
void updatePulseStatus()
{
  //if the button was not pressed, get out
  if (not pulseButtonClick)
  {
    return;
  }

  //debounce the new setting
  if (pulseButtonClickTimer < DEBOUNCE_SETTINGS)
  {
    return;
  }

  //reset flag of button pressed
  pulseButtonClick = false;

  //is there anything to update?
  // this code here takes care of the users having cycled the mode to the same original value
  if (internalPulse == externalPulse)
  {
    return;
  }

  //update only in the case the FSM state is idleState (the thermostat is doing nothing)
  // or pulseState (a pulse is already running and the user wants to abort it)
  if (not(thermostatStateMachine.isInState(idleState) or thermostatStateMachine.isInState(pulseState)))
  {
    Particle.publish("ALARM", "ERROR: You can only start a pulse in idle state" + getTime(), 60, PRIVATE);
#ifdef USE_BLYNK
    pulseLed.off();
#endif
    return;
  }

  //update the new setting from the external to the internal variable
  internalPulse = externalPulse;
}

/*******************************************************************************
 * Function Name  : updateMode
 * Description    : check if the mode has changed
 * Behavior       : the new setting will not take place right away, but moments after
                    since a timer is triggered. This is to debounce the setting and
                    allow the users to change their mind
 * Return         : none
 *******************************************************************************/
void updateMode()
{
  //if the mode button was not pressed, get out
  if (not modeButtonClick)
  {
    return;
  }

  //debounce the new setting
  if (modeButtonClickTimer < DEBOUNCE_SETTINGS_MODE)
  {
    return;
  }

  //reset flag of button pressed
  modeButtonClick = false;

  //is there anything to update?
  // this code here takes care of the users having cycled the mode to the same original value
  if (internalMode == externalMode)
  {
    return;
  }

  //update the new mode from the external to the internal variable
  internalMode = externalMode;
  String tempStatus = "Mode set to " + internalMode + getTime();
  publishEvent(tempStatus);
}

/*******************************************************************************
 * Function Name  : readTemperature
 * Description    : reads the temperature of the DHT22 sensor at every TEMPERATURE_SAMPLE_INTERVAL
 * Return         : 0
 *******************************************************************************/
void readTemperature()
{

  //time is up? no, then come back later
  if (temperatureSampleInterval < TEMPERATURE_SAMPLE_INTERVAL)
  {
    return;
  }

  //time is up, reset timer
  temperatureSampleInterval = 0;

  // start the sample
  if (!bDHTstarted)
  {
    DHT.acquireAndWait(5);
    bDHTstarted = true;
  }

  //still acquiring sample? go away and come back later
  if (DHT.acquiring())
  {
    return;
  }

  //I observed my dht22 measuring below 0 from time to time, so let's discard that sample
  if (DHT.getCelsius() < 0)
  {
    //reset the sample flag so we can take another
    bDHTstarted = false;
    return;
  }

  //valid sample acquired
  float temp = (float)DHT.getCelsius();

  char tempChar[BUFFER] = "";
  snprintf(tempChar, BUFFER, "raw temp: %.2f °C", temp);
  Log.info(tempChar);

  if (useFahrenheit)
  {
    temp = (temp * 1.8) + 32;
  }

  currentTemp = analogSmoothTemperature.smooth(temp);
  snprintf(tempChar, BUFFER, "current temperature: %.2f °C", currentTemp);
  Log.info(tempChar);

  currentHumidity = analogSmoothHumidity.smooth((float)DHT.getHumidity());
  snprintf(tempChar, BUFFER, "current humidity: %.2f %%", currentHumidity);
  Log.info(tempChar);

  //sample acquired and averaged - publish it
  publishTemperatureToParticleCloud(currentTemp, currentHumidity);

  //reset the sample flag so we can take another sample
  bDHTstarted = false;

  return;
}

/*******************************************************************************
 * Function Name  : publishTemperatureToParticleCloud
 * Description    : publish the temperature/humidity passed as parameters to Particle
 *******************************************************************************/
void publishTemperatureToParticleCloud(float temperature, float humidity)
{

  char pubChar[BUFFER] = "";
  char tempChar[LITTLE] = "";

  if (useFahrenheit)
  {
    snprintf(tempChar, LITTLE, "%.2f °F", temperature);
  }
  else
  {
    snprintf(tempChar, LITTLE, "%.2f °C", temperature);
  }
  strcat(pubChar, tempChar);

  snprintf(tempChar, LITTLE, ", %.2f %%", humidity);
  strcat(pubChar, tempChar);

  Log.info(pubChar);
  Particle.publish("STATUS", pubChar, PRIVATE | WITH_ACK);

  statusDHTmain = pubChar;
}

/*******************************************************************************
********************************************************************************
********************************************************************************
 FINITE STATE MACHINE FUNCTIONS
********************************************************************************
********************************************************************************
*******************************************************************************/
void initEnterFunction()
{
  //start the timer of this cycle
  initTimer = 0;
  //set the state
  setState(STATE_INIT);
}
void initUpdateFunction()
{
  //time is up?
  if (initTimer > INIT_TIMEOUT)
  {
    thermostatStateMachine.transitionTo(idleState);
  }
}
void initExitFunction()
{
  Particle.publish(APP_NAME, "Initialization done", 60, PRIVATE);
}

void idleEnterFunction()
{
  //set the state
  setState(STATE_IDLE);

  //turn off the fan only if fan was not set on manually by the user
  if (internalFan == false)
  {
    myDigitalWrite(fan, LOW);
  }
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, LOW);

  //start the minimum timer of this cycle
  minimumIdleTimer = 0;
}
void idleUpdateFunction()
{
  //set the fan output to the internalFan ONLY in this state of the FSM
  // since other states might need the fan on
  //set it off only if it was on and internalFan changed to false
  if (internalFan == false and fanOutput == HIGH)
  {
    myDigitalWrite(fan, LOW);
  }
  //set it on only if it was off and internalFan changed to true
  if (internalFan == true and fanOutput == LOW)
  {
    myDigitalWrite(fan, HIGH);
  }

  //is minimum time up? not yet, so get out of here
  if (minimumIdleTimer < MINIMUM_IDLE_TIMEOUT)
  {
    return;
  }

  //if the thermostat is OFF, there is not much to do
  if (internalMode == MODE_OFF)
  {
    if (internalPulse)
    {
      Particle.publish("ALARM", "ERROR: You cannot start a pulse when the system is OFF" + getTime(), 60, PRIVATE);
      internalPulse = false;
    }
    return;
  }

  //are we heating?
  if (internalMode == MODE_HEAT)
  {
    //if the temperature is lower than the target, transition to heatingState
    if (currentTemp <= (targetTemp - margin))
    {
      thermostatStateMachine.transitionTo(heatingState);
    }
    if (internalPulse)
    {
      thermostatStateMachine.transitionTo(pulseState);
    }
  }

  //are we cooling?
  if (internalMode == MODE_COOL)
  {
    //if the temperature is higher than the target, transition to coolingState
    if (currentTemp > (targetTemp + margin))
    {
      thermostatStateMachine.transitionTo(coolingState);
    }
    if (internalPulse)
    {
      thermostatStateMachine.transitionTo(pulseState);
    }
  }
}
void idleExitFunction()
{
}

void heatingEnterFunction()
{
  //set the state
  setState(STATE_HEATING);

  String tempStatus = "Heat on" + getTime();
  publishEvent(tempStatus);
  myDigitalWrite(fan, HIGH);
  myDigitalWrite(heat, HIGH);
  myDigitalWrite(cool, LOW);

  //start the minimum timer of this cycle
  minimumOnTimer = 0;
}
void heatingUpdateFunction()
{
  //is minimum time up?
  if (minimumOnTimer < MINIMUM_ON_TIMEOUT)
  {
    //not yet, so get out of here
    return;
  }

  if (currentTemp >= (targetTemp + margin))
  {
    String tempStatus = "Desired temperature reached: " + targetTempString + "°C" + getTime();
    publishEvent(tempStatus);
    thermostatStateMachine.transitionTo(idleState);
  }

  //was the mode changed by the user? if so, go back to idleState
  if (internalMode != MODE_HEAT)
  {
    thermostatStateMachine.transitionTo(idleState);
  }
}
void heatingExitFunction()
{
  String tempStatus = "Heat off" + getTime();
  publishEvent(tempStatus);
  myDigitalWrite(fan, LOW);
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, LOW);
}

/*******************************************************************************
 * FSM state Name : pulseState
 * Description    : turns the HVAC on for a certain time
                    comes in handy when you want to warm up/cool down the house a little bit
 *******************************************************************************/
void pulseEnterFunction()
{
  String tempStatus = "Pulse on" + getTime();
  publishEvent(tempStatus);
  if (internalMode == MODE_HEAT)
  {
    myDigitalWrite(fan, HIGH);
    myDigitalWrite(heat, HIGH);
    myDigitalWrite(cool, LOW);
    //set the state
    setState(STATE_PULSE_HEAT);
  }
  else if (internalMode == MODE_COOL)
  {
    myDigitalWrite(fan, HIGH);
    myDigitalWrite(heat, LOW);
    myDigitalWrite(cool, HIGH);
    //set the state
    setState(STATE_PULSE_COOL);
  }
  //start the timer of this cycle
  pulseTimer = 0;

  //start the minimum timer of this cycle
  minimumOnTimer = 0;
}
void pulseUpdateFunction()
{
  //is minimum time up? if not, get out of here
  if (minimumOnTimer < MINIMUM_ON_TIMEOUT)
  {
    return;
  }

  //if the pulse was canceled by the user, transition to idleState
  if (not internalPulse)
  {
    thermostatStateMachine.transitionTo(idleState);
  }

  //is the time up for the pulse? if not, get out of here
  if (pulseTimer < PULSE_TIMEOUT)
  {
    return;
  }

  thermostatStateMachine.transitionTo(idleState);
}
void pulseExitFunction()
{
  internalPulse = false;
  String tempStatus = "Pulse off" + getTime();
  publishEvent(tempStatus);
  myDigitalWrite(fan, LOW);
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, LOW);

#ifdef USE_BLYNK
  pulseLed.off();
#endif
}

/*******************************************************************************
 * FSM state Name : coolingState
 * Description    : turns the cooling element on until the desired temperature is reached
 *******************************************************************************/
void coolingEnterFunction()
{
  //set the state
  setState(STATE_COOLING);

  String tempStatus = "Cool on" + getTime();
  publishEvent(tempStatus);
  myDigitalWrite(fan, HIGH);
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, HIGH);

  //start the minimum timer of this cycle
  minimumOnTimer = 0;
}
void coolingUpdateFunction()
{
  //is minimum time up?
  if (minimumOnTimer < MINIMUM_ON_TIMEOUT)
  {
    //not yet, so get out of here
    return;
  }

  if (currentTemp <= (targetTemp - margin))
  {
    String tempStatus = "Desired temperature reached: " + targetTempString + "°C" + getTime();
    publishEvent(tempStatus);
    thermostatStateMachine.transitionTo(idleState);
  }

  //was the mode changed by the user? if so, go back to idleState
  if (internalMode != MODE_COOL)
  {
    thermostatStateMachine.transitionTo(idleState);
  }
}
void coolingExitFunction()
{
  String tempStatus = "Cool off" + getTime();
  publishEvent(tempStatus);
  myDigitalWrite(fan, LOW);
  myDigitalWrite(heat, LOW);
  myDigitalWrite(cool, LOW);
}

/*******************************************************************************
 * Function Name  : myDigitalWrite
 * Description    : writes to the pin or the relayController and sets a variable to keep track
                    this is a hack that allows me to system test the project
                    and know what is the status of the outputs
 * Return         : void
 *******************************************************************************/
void myDigitalWrite(int input, int status)
{

  if (status == LOW)
  {
    relayController.turnOffRelay(input);
  }
  else
  {
    relayController.turnOnRelay(input);
  }

  if (input == fan)
  {
    fanOutput = status;

    Particle.publish("DEBUG fan", String(status), 60, PRIVATE);

#ifdef USE_BLYNK
    BLYNK_setFanLed(status);
#endif
  }

  if (input == heat)
  {
    heatOutput = status;
#ifdef USE_BLYNK
    BLYNK_setHeatLed(status);
#endif
  }

  if (input == cool)
  {
    coolOutput = status;
#ifdef USE_BLYNK
    BLYNK_setCoolLed(status);
#endif
  }
}

/*******************************************************************************
 * Function Name  : getTime
 * Description    : returns the time in the following format: 14:42:31
 TIME_FORMAT_ISO8601_FULL example: 2016-03-23T14:42:31-04:00
 * Return         : the time
 *******************************************************************************/
String getTime()
{
  String timeNow = Time.format(Time.now(), TIME_FORMAT_ISO8601_FULL);
  timeNow = timeNow.substring(11, timeNow.length() - 6);
  return " " + timeNow;
}

/*******************************************************************************
 * Function Name  : setState
 * Description    : sets the state of the system
 * Return         : none
 *******************************************************************************/
void setState(String newState)
{
  state = newState;
#ifdef USE_BLYNK
  Blynk.virtualWrite(BLYNK_DISPLAY_STATE, state);
#endif
}

/*******************************************************************************
 * Function Name  : publishEvent
 * Description    : publishes an event, to the particle console or google sheets
 * Return         : none
 *******************************************************************************/
void publishEvent(String event)
{
  Particle.publish("event", event, 60, PRIVATE);
}

/*******************************************************************************/
/*******************************************************************************/
/*******************          BLYNK FUNCTIONS         **************************/
/*******************************************************************************/
/*******************************************************************************/

// only include blynk related code if you are using blynk
#ifdef USE_BLYNK

/*******************************************************************************
 * Function Name  : BLYNK_READ
 * Description    : these functions are called by blynk when the blynk app wants
 to read values from the photon
 source: http://docs.blynk.cc/#blynk-main-operations-get-data-from-hardware
 *******************************************************************************/
BLYNK_READ(BLYNK_DISPLAY_GARAGE_STATUS)
{
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_GARAGE_STATUS, meshGarageStatus);
}
BLYNK_READ(BLYNK_DISPLAY_GARAGE_LAST_HEARD_OF)
{
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_GARAGE_LAST_HEARD_OF, meshGarageLastHeardOf);
}

BLYNK_READ(BLYNK_DISPLAY_CURRENT_TEMP_REMOTE)
{
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_CURRENT_TEMP_REMOTE, meshTempSensorCurrentTemp);
}
BLYNK_READ(BLYNK_DISPLAY_CURRENT_TEMP_REMOTE_LAST_HEARD_OF)
{
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_CURRENT_TEMP_REMOTE_LAST_HEARD_OF, meshTempSensorLastHeardOf);
}

BLYNK_READ(BLYNK_DISPLAY_CURRENT_TEMP_POOL)
{
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_CURRENT_TEMP_POOL, meshTempSensorPoolCurrentTemp);
}
BLYNK_READ(BLYNK_DISPLAY_CURRENT_TEMP_POOL_LAST_HEARD_OF)
{
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_CURRENT_TEMP_POOL_LAST_HEARD_OF, meshTempSensorPoolLastHeardOf);
}

BLYNK_READ(BLYNK_DISPLAY_WATER_LEAK_SENSOR)
{
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_WATER_LEAK_SENSOR, meshWaterLeakSensorState);
}
BLYNK_READ(BLYNK_DISPLAY_WATER_LEAK_SENSOR_LAST_HEARD_OF)
{
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_WATER_LEAK_SENSOR_LAST_HEARD_OF, meshWaterLeakSensorLastHeardOf);
}

BLYNK_READ(BLYNK_DISPLAY_SUMP_PUMP)
{
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_SUMP_PUMP, meshSumpPumpState);
}
BLYNK_READ(BLYNK_DISPLAY_SUMP_PUMP_LAST_HEARD_OF)
{
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_SUMP_PUMP_LAST_HEARD_OF, meshSumpPumpLastHeardOf);
}

BLYNK_READ(BLYNK_DISPLAY_CURRENT_TEMP)
{
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_CURRENT_TEMP, currentTemp);
}
BLYNK_READ(BLYNK_DISPLAY_HUMIDITY)
{
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_HUMIDITY, currentHumidity);
}
BLYNK_READ(BLYNK_DISPLAY_TARGET_TEMP)
{
  //this is a blynk value display
  // source: http://docs.blynk.cc/#widgets-displays-value-display
  Blynk.virtualWrite(BLYNK_DISPLAY_TARGET_TEMP, targetTemp);
}
BLYNK_READ(BLYNK_LED_FAN)
{
  //this is a blynk led
  // source: http://docs.blynk.cc/#widgets-displays-led
  if (externalFan)
  {
    fanLed.on();
  }
  else
  {
    fanLed.off();
  }
}
BLYNK_READ(BLYNK_LED_PULSE)
{
  //this is a blynk led
  // source: http://docs.blynk.cc/#widgets-displays-led
  if (externalPulse)
  {
    pulseLed.on();
  }
  else
  {
    pulseLed.off();
  }
}
BLYNK_READ(BLYNK_LED_HEAT)
{
  //this is a blynk led
  // source: http://docs.blynk.cc/#widgets-displays-led
  if (heatOutput)
  {
    heatLed.on();
  }
  else
  {
    heatLed.off();
  }
}
BLYNK_READ(BLYNK_LED_COOL)
{
  //this is a blynk led
  // source: http://docs.blynk.cc/#widgets-displays-led
  if (coolOutput)
  {
    coolLed.on();
  }
  else
  {
    coolLed.off();
  }
}
BLYNK_READ(BLYNK_DISPLAY_MODE)
{
  Blynk.virtualWrite(BLYNK_DISPLAY_MODE, externalMode);
}
BLYNK_READ(BLYNK_DISPLAY_STATE)
{
  Blynk.virtualWrite(BLYNK_DISPLAY_STATE, state);
}

/*******************************************************************************
 * Function Name  : BLYNK_WRITE
 * Description    : these functions are called by blynk when the blynk app wants
                     to write values to the photon
                    source: http://docs.blynk.cc/#blynk-main-operations-send-data-from-app-to-hardware
 *******************************************************************************/
BLYNK_WRITE(BLYNK_SLIDER_TEMP)
{
  //this is the blynk slider
  // source: http://docs.blynk.cc/#widgets-controllers-slider
  setTargetTempInternal(param.asStr());
  flagSettingsHaveChanged();
}

BLYNK_WRITE(BLYNK_BUTTON_FAN)
{
  //flip fan status, if it's on switch it off and viceversa
  // do this only when blynk sends a 1
  // background: in a BLYNK push button, blynk sends 0 then 1 when user taps on it
  // source: http://docs.blynk.cc/#widgets-controllers-button
  if (param.asInt() == 1)
  {
    externalFan = not externalFan;
    //start timer to debounce this new setting
    fanButtonClickTimer = 0;
    //flag that the button was clicked
    fanButtonClick = true;
    //update the led
    if (externalFan)
    {
      fanLed.on();
    }
    else
    {
      fanLed.off();
    }

    flagSettingsHaveChanged();
  }
}

BLYNK_WRITE(BLYNK_BUTTON_PULSE)
{
  //flip pulse status, if it's on switch it off and viceversa
  // do this only when blynk sends a 1
  // background: in a BLYNK push button, blynk sends 0 then 1 when user taps on it
  // source: http://docs.blynk.cc/#widgets-controllers-button
  if (param.asInt() == 1)
  {
    externalPulse = not externalPulse;
    //start timer to debounce this new setting
    pulseButtonClickTimer = 0;
    //flag that the button was clicked
    pulseButtonClick = true;
    //update the pulse led
    if (externalPulse)
    {
      pulseLed.on();
    }
    else
    {
      pulseLed.off();
    }
  }
}

BLYNK_WRITE(BLYNK_BUTTON_MODE)
{
  //mode: cycle through off->heating->cooling
  // do this only when blynk sends a 1
  // background: in a BLYNK push button, blynk sends 0 then 1 when user taps on it
  // source: http://docs.blynk.cc/#widgets-controllers-button
  if (param.asInt() == 1)
  {
    if (externalMode == MODE_OFF)
    {
      externalMode = MODE_HEAT;
    }
    else if (externalMode == MODE_HEAT)
    {
      externalMode = MODE_COOL;
    }
    else if (externalMode == MODE_COOL)
    {
      externalMode = MODE_OFF;
    }
    else
    {
      externalMode = MODE_OFF;
    }

    //start timer to debounce this new setting
    modeButtonClickTimer = 0;
    //flag that the button was clicked
    modeButtonClick = true;
    //update the mode indicator
    Blynk.virtualWrite(BLYNK_DISPLAY_MODE, externalMode);

    flagSettingsHaveChanged();
  }
}

/*******************************************************************************
 * Function Name  : BLYNK_WRITE
 * Description    : these functions are called by blynk to send a mesh command to the mesh gate device
*******************************************************************************/
BLYNK_WRITE(BLYNK_BUTTON_GARAGE_OPEN)
{
  if (param.asInt() == 1)
  {
#if PLATFORM_ID == PLATFORM_ARGON
    Mesh.publish(MESH_EVENT_GARAGE_COMMAND, COMMAND_OPEN_GARAGE);
#endif
  }
}
BLYNK_WRITE(BLYNK_BUTTON_GARAGE_CLOSE)
{
  if (param.asInt() == 1)
  {
#if PLATFORM_ID == PLATFORM_ARGON
    Mesh.publish(MESH_EVENT_GARAGE_COMMAND, COMMAND_CLOSE_GARAGE);
#endif
  }
}
BLYNK_WRITE(BLYNK_BUTTON_GARAGE_MOVE)
{
  if (param.asInt() == 1)
  {
#if PLATFORM_ID == PLATFORM_ARGON
    Mesh.publish(MESH_EVENT_GARAGE_COMMAND, COMMAND_MOVE_GARAGE);
#endif
  }
}

/*******************************************************************************
 * Function Name  : BLYNK_setXxxLed
 * Description    : these functions are called by our program to update the status
                    of the leds in the blynk cloud and the blynk app
                    source: http://docs.blynk.cc/#blynk-main-operations-send-data-from-app-to-hardware
*******************************************************************************/
void BLYNK_setFanLed(int status)
{
  if (status)
  {
    fanLed.on();
  }
  else
  {
    fanLed.off();
  }
}

void BLYNK_setHeatLed(int status)
{
  if (status)
  {
    heatLed.on();
  }
  else
  {
    heatLed.off();
  }
}

void BLYNK_setCoolLed(int status)
{
  if (status)
  {
    coolLed.on();
  }
  else
  {
    coolLed.off();
  }
}

// BLYNK_CONNECTED() {
//   Blynk.syncVirtual(BLYNK_DISPLAY_CURRENT_TEMP);
//   Blynk.syncVirtual(BLYNK_DISPLAY_HUMIDITY);
//   Blynk.syncVirtual(BLYNK_DISPLAY_TARGET_TEMP);
//   Blynk.syncVirtual(BLYNK_LED_FAN);
//   Blynk.syncVirtual(BLYNK_LED_HEAT);
//   Blynk.syncVirtual(BLYNK_LED_COOL);
//   Blynk.syncVirtual(BLYNK_LED_PULSE);
//   Blynk.syncVirtual(BLYNK_DISPLAY_MODE);
//   // BLYNK_setFanLed(fan);
//   // BLYNK_setHeatLed(heat);
//   // BLYNK_setCoolLed(cool);
//
//   //update the mode and state indicator
//   Blynk.virtualWrite(BLYNK_DISPLAY_MODE, externalMode);
//   Blynk.virtualWrite(BLYNK_DISPLAY_STATE, state);
// }

/*******************************************************************************
 * Function Name  : updateBlynkCloud
 * Description    : publish readings to the blynk server every minute so the
                    History Graph gets updated even when
                    the blynk app is not on (running) in the users phone
 * Return         : none
 *******************************************************************************/
void updateBlynkCloud()
{

  //is it time to store in the blynk cloud? if so, do it
  if (blynkStoreInterval > BLYNK_STORE_INTERVAL)
  {

    //reset timer
    blynkStoreInterval = 0;

    //do not write the temp while the thermostat is initializing
    if (not thermostatStateMachine.isInState(initState))
    {
      Blynk.virtualWrite(BLYNK_DISPLAY_CURRENT_TEMP, currentTemp);
      Blynk.virtualWrite(BLYNK_DISPLAY_HUMIDITY, currentHumidity);
    }

    Blynk.virtualWrite(BLYNK_DISPLAY_TARGET_TEMP, targetTemp);

    if (externalPulse)
    {
      pulseLed.on();
    }
    else
    {
      pulseLed.off();
    }

    BLYNK_setFanLed(externalFan);
    BLYNK_setHeatLed(heatOutput);
    BLYNK_setCoolLed(coolOutput);

    //update the mode and state indicator
    Blynk.virtualWrite(BLYNK_DISPLAY_MODE, externalMode);
    Blynk.virtualWrite(BLYNK_DISPLAY_STATE, state);
  }
}
#endif

/*******************************************************************************/
/*******************************************************************************/
/*******************          EEPROM FUNCTIONS         *************************/
/********  https://docs.particle.io/reference/firmware/photon/#eeprom  *********/
/*******************************************************************************/
/*******************************************************************************/

/*******************************************************************************
 * Function Name  : flagSettingsHaveChanged
 * Description    : this function gets called when the user of the blynk app
                    changes a setting. The blynk app calls the blynk cloud and in turn
                    it calls the functions BLYNK_WRITE()
 * Return         : none
 *******************************************************************************/
void flagSettingsHaveChanged()
{
  settingsHaveChanged = true;
  settingsHaveChanged_timer = 0;
}

/*******************************************************************************
 * Function Name  : readFromEeprom
 * Description    : retrieves the settings from the EEPROM memory
 * Return         : none
 *******************************************************************************/
void readFromEeprom()
{

  EepromMemoryStructure myObj;
  EEPROM.get(EEPROM_ADDRESS, myObj);

  //verify this eeprom was written before
  // if version is 255 it means the eeprom was never written in the first place, hence the
  // data just read with the previous EEPROM.get() is invalid and we will ignore it
  if (myObj.version == EEPROM_VERSION)
  {

    targetTemp = float(myObj.targetTemp);
    newTargetTemp = targetTemp;
    targetTempString = float2string(targetTemp);

    internalMode = convertIntToMode(myObj.internalMode);
    externalMode = internalMode;

    //these variables are false at boot
    if (myObj.internalFan == 1)
    {
      internalFan = true;
      externalFan = true;
    }

    // Particle.publish(APP_NAME, "DEBUG: read settings from EEPROM: " + String(myObj.targetTemp)
    Particle.publish(APP_NAME, "read:" + internalMode + "-" + String(internalFan) + "-" + String(targetTemp), 60, PRIVATE);
  }
}

/*******************************************************************************
 * Function Name  : saveSettings
 * Description    : in this function we wait a bit to give the user time
                    to adjust the right value for them and in this way we try not
                    to save in EEPROM at every little change.
                    Remember that each eeprom writing cycle is a precious and finite resource
 * Return         : none
 *******************************************************************************/
void saveSettings()
{

  //if the thermostat is initializing, get out of here
  if (thermostatStateMachine.isInState(initState))
  {
    return;
  }

  //if no settings were changed, get out of here
  if (not settingsHaveChanged)
  {
    return;
  }

  //if settings have changed, is it time to store them?
  if (settingsHaveChanged_timer < SAVE_SETTINGS_INTERVAL)
  {
    return;
  }

  //reset timer
  settingsHaveChanged_timer = 0;
  settingsHaveChanged = false;

  //store thresholds in the struct type that will be saved in the eeprom
  eepromMemory.version = EEPROM_VERSION;
  eepromMemory.targetTemp = uint8_t(targetTemp);
  eepromMemory.internalMode = convertModeToInt(internalMode);

  eepromMemory.internalFan = 0;
  if (internalFan)
  {
    eepromMemory.internalFan = 1;
  }

  //then save
  EEPROM.put(EEPROM_ADDRESS, eepromMemory);

  // Particle.publish(APP_NAME, "stored:" + eepromMemory.internalMode + "-" + String(eepromMemory.internalFan) + "-" + String(eepromMemory.targetTemp) , 60, PRIVATE);
  Particle.publish(APP_NAME, "stored:" + internalMode + "-" + String(internalFan) + "-" + String(targetTemp), 60, PRIVATE);
}

/*******************************************************************************
 * Function Name  : convertIntToMode
 * Description    : converts the int mode (saved in the eeprom) into the String mode
 * Return         : String
 *******************************************************************************/
String convertIntToMode(uint8_t mode)
{
  if (mode == 1)
  {
    return MODE_HEAT;
  }
  if (mode == 2)
  {
    return MODE_COOL;
  }

  //in all other cases
  return MODE_OFF;
}

/*******************************************************************************
 * Function Name  : convertModeToInt
 * Description    : converts the String mode into the int mode (to be saved in the eeprom)
 * Return         : String
 *******************************************************************************/
uint8_t convertModeToInt(String mode)
{
  if (mode == MODE_HEAT)
  {
    return 1;
  }
  if (mode == MODE_COOL)
  {
    return 2;
  }

  //in all other cases
  return 0;
}
