/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "application.h"
#line 1 "/home/ceajog/particle/proyectos/Gust/meshHome/meshWaterLeakSensor/src/meshWaterLeakSensor.ino"
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

void setup();
void loop();
void publishData();
void sendUbidotsAlarm(char *alarm, float value);
void waterLeakOkEnterFunction();
void waterLeakOkUpdateFunction();
void waterLeakOkExitFunction();
void waterLeakTransitionEnterFunction();
void waterLeakTransitionUpdateFunction();
void waterLeakTransitionExitFunction();
void waterLeakAlarmEnterFunction();
void waterLeakAlarmUpdateFunction();
void waterLeakAlarmExitFunction();
void waterLeakSetState(String newState);
void debounceInputs();
void debounceWaterSensor();
#line 29 "/home/ceajog/particle/proyectos/Gust/meshHome/meshWaterLeakSensor/src/meshWaterLeakSensor.ino"
#define APP_NAME "waterLeakSensor-mesh"
#define APP_VERSION "Version 0.01"

/*******************************************************************************
 * changes in version 0.01:
       * first version
*******************************************************************************/

#include "FiniteStateMachine.h"
#include "Ubidots.h"
#include "elapsedMillis.h"

#define MESH_EVENT_WATER_LEAK_SENSOR "meshWaterLeakSensor"

/* The following parameters tune the algorithm to fit the particular
application.  The example numbers are for a case where a computer samples a
mechanical contact 10 times a second and a half-second integration time is
used to remove bounce.  Note: DEBOUNCE_TIME is in seconds and SAMPLE_FREQUENCY
is in Hertz */
// source: http://www.kennethkuhn.com/electronics/debounce.c
Timer debounceInputsTimer(20, debounceInputs);
#define DEBOUNCE_TIME 0.1
#define SAMPLE_FREQUENCY 50
#define MAXIMUM (DEBOUNCE_TIME * SAMPLE_FREQUENCY)

// comment out if you do NOT want serial logging
SerialLogHandler logHandler(LOG_LEVEL_ALL);
//SerialLogHandler logHandler(LOG_LEVEL_WARN);

const char *WEBHOOK_NAME = "Ubidots";
Ubidots ubidots("webhook", UBI_PARTICLE);

// enable the user code (our program below) to run in parallel with cloud connectivity code
// source: https://docs.particle.io/reference/firmware/photon/#system-thread
SYSTEM_THREAD(ENABLED);

#define WATER_LEAK_SENSOR D0
unsigned int integratorWaterLeakSensor = 0;
int waterLeakSensor = 0;

#define STATE_OK "Sensor OK"
#define STATE_IN_TRANSITION "Sensor in transition"
#define STATE_ALARM "Sensor Alarm"
#define ALARM_TRANSITION_PERIOD 30000 // min amount of time to before an alarm is sent out
#define ALARM_MIN 10000               // min amount of time to stay in alarm before coming back to normal

#define PUBLISH_INTERVAL 5000
elapsedMillis publishInterval;

// FSM declaration for water sensor
State waterLeakOkState = State(waterLeakOkEnterFunction, waterLeakOkUpdateFunction, waterLeakOkExitFunction);
State waterLeakTransitionState = State(waterLeakTransitionEnterFunction, waterLeakTransitionUpdateFunction, waterLeakTransitionExitFunction);
State waterLeakAlarmState = State(waterLeakAlarmEnterFunction, waterLeakAlarmUpdateFunction, waterLeakAlarmExitFunction);
FSM waterLeakStateMachine = FSM(waterLeakOkState);
String waterLeakState = STATE_OK;

#define BUFFER 623

/*******************************************************************************
 * Function Name  : setup
 * Description    : this function runs once at system boot
 *******************************************************************************/
void setup()
{

  // cloud variables
  // Up to 20 cloud variables may be registered and each variable name is limited to
  // a maximum of 12 characters (prior to 0.8.0), 64 characters (since 0.8.0).
  // https://docs.particle.io/reference/device-os/firmware/boron/#particle-variable-
  Particle.variable("waterLeakState", waterLeakState);

  pinMode(WATER_LEAK_SENSOR, INPUT_PULLDOWN);

  debounceInputsTimer.start();

  // publish start up message with firmware version
  Particle.publish(APP_NAME, APP_VERSION, PRIVATE);
  Log.info(String(APP_NAME) + " " + String(APP_VERSION));
}

/*******************************************************************************
 * Function Name  : loop
 * Description    : this function runs continuously while the project is running
 *******************************************************************************/
void loop()
{
  waterLeakStateMachine.update();

  publishData();
}

/*******************************************************************************
 * Function Name  : publishData
 * Description    : sends info to other devices and the cloud
 *******************************************************************************/
void publishData()
{
  //time is up? no, then come back later
  if (publishInterval < PUBLISH_INTERVAL)
  {
    return;
  }

  //time is up, reset timer
  publishInterval = 0;

  Particle.publish("water leak sensor", waterLeakState, PRIVATE | WITH_ACK);

#if PLATFORM_ID == PLATFORM_XENON
  Mesh.publish(MESH_EVENT_WATER_LEAK_SENSOR, waterLeakState);
#endif
}

/*******************************************************************************
 * Function Name  : sendUbidotsAlarm
 * Description    : sends an alarm to ubidots
                    there is a Particle webhook to create in the Particle Cloud
 *******************************************************************************/
void sendUbidotsAlarm(char *alarm, float value)
{
  ubidots.add(alarm, value);

  bool bufferSent = false;
  bufferSent = ubidots.send(WEBHOOK_NAME, PRIVATE | WITH_ACK); // Will use particle webhooks to send data

  if (bufferSent)
  {
    // Do something if values were sent properly
    Log.info(String::format("Alarm sent to Ubidots: %s, value: %i", alarm, value));
  }
}

/*******************************************************************************
********************************************************************************
********************************************************************************
 FINITE STATE MACHINE FUNCTIONS
********************************************************************************
********************************************************************************
*******************************************************************************/

/*******************************************************************************
 WATER LEAK sensor
*******************************************************************************/
void waterLeakOkEnterFunction()
{
  Particle.publish("OK", "WATER LEAK sensor OK", PRIVATE | WITH_ACK);
  sendUbidotsAlarm("waterLeak", 0);
  waterLeakSetState(STATE_OK);
}
void waterLeakOkUpdateFunction()
{
  if (waterLeakSensor == HIGH)
  {
    waterLeakStateMachine.transitionTo(waterLeakTransitionState);
    Log.info("waterLeakSensor is HIGH, transition to waterLeakTransitionState");
  }
}
void waterLeakOkExitFunction()
{
}

void waterLeakTransitionEnterFunction()
{
  Particle.publish("IN TRANSITION", "WATER LEAK sensor IN TRANSITION", PRIVATE | WITH_ACK);
  waterLeakSetState(STATE_IN_TRANSITION);
}
void waterLeakTransitionUpdateFunction()
{
  // stay here the soak period before sending any alarm
  if (waterLeakStateMachine.timeInCurrentState() < ALARM_TRANSITION_PERIOD)
  {
    return;
  }
  if (waterLeakSensor == HIGH)
  {
    waterLeakStateMachine.transitionTo(waterLeakAlarmState);
    Log.info("waterLeakSensor is HIGH, transition to waterLeakAlarmState");
  }
  if (waterLeakSensor == LOW)
  {
    waterLeakStateMachine.transitionTo(waterLeakOkState);
    Log.info("waterLeakSensor is LOW, transition to waterLeakOkState");
  }
}
void waterLeakTransitionExitFunction()
{
}

void waterLeakAlarmEnterFunction()
{
  Particle.publish("ALARM", "Alarm on WATER LEAK sensor", PRIVATE | WITH_ACK);
  sendUbidotsAlarm("waterLeak", 1);
  waterLeakSetState(STATE_ALARM);
}
void waterLeakAlarmUpdateFunction()
{
  // stay here a minimum time
  if (waterLeakStateMachine.timeInCurrentState() < ALARM_MIN)
  {
    return;
  }
  // sensor went back to normal
  if (waterLeakSensor == LOW)
  {
    waterLeakStateMachine.transitionTo(waterLeakOkState);
    Log.info("waterLeakSensor is LOW, transition to waterLeakOkState");
  }
}
void waterLeakAlarmExitFunction()
{
}

/*******************************************************************************
 * Function Name  : waterLeakSetState
 * Description    : sets the state of an FSM
 * Return         : none
 *******************************************************************************/
void waterLeakSetState(String newState)
{
  waterLeakState = newState;
  Particle.publish("FSM", "WATER LEAK fsm entering " + newState + " state", PRIVATE | WITH_ACK);
  Log.info("WATER LEAK fsm entering " + newState + " state");
}

/*******************************************************************************
********************************************************************************
********************************************************************************
 HELPER FUNCTIONS
********************************************************************************
********************************************************************************
*******************************************************************************/

/*******************************************************************************
 * Function Name  : debounceInputs
 * Description    : debounces the contact sensors, triggered by a timer with similar name
 * Return         : nothing
 * Source: http://www.ganssle.com/debouncing-pt2.htm
 * Source: http://www.kennethkuhn.com/electronics/debounce.c
 *******************************************************************************/
void debounceInputs()
{
  debounceWaterSensor();
}

/*******************************************************************************
 * Function Name  :     debounceWaterSensor
 * Description    : debounces the water leak sensor
 * Return         : nothing
 * Source: http://www.ganssle.com/debouncing-pt2.htm
 * Source: http://www.kennethkuhn.com/electronics/debounce.c
 *******************************************************************************/
void debounceWaterSensor()
{
  // Step 1: Update the integrator based on the input signal.  Note that the
  // integrator follows the input, decreasing or increasing towards the limits as
  // determined by the input state (0 or 1).
  if (digitalRead(WATER_LEAK_SENSOR) == LOW)
  {
    if (integratorWaterLeakSensor > 0)
      integratorWaterLeakSensor--;
  }
  else if (integratorWaterLeakSensor < MAXIMUM)
    integratorWaterLeakSensor++;

  // Step 2: Update the output state based on the integrator.  Note that the
  // output will only change states if the integrator has reached a limit, either
  // 0 or MAXIMUM.
  if (integratorWaterLeakSensor == 0)
    waterLeakSensor = 0;
  else if (integratorWaterLeakSensor >= MAXIMUM)
  {
    waterLeakSensor = 1;
    integratorWaterLeakSensor = MAXIMUM; /* defensive code if integrator got corrupted */
  }
}
