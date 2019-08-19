/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "application.h"
#line 1 "/home/ceajog/particle/proyectos/Gust/meshHome/meshSumpPump/src/meshSumpPump.ino"
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
void sumpPumpOkEnterFunction();
void sumpPumpOkUpdateFunction();
void sumpPumpOkExitFunction();
void sumpPumpTransitionEnterFunction();
void sumpPumpTransitionUpdateFunction();
void sumpPumpTransitionExitFunction();
void sumpPumpHighLevelEnterFunction();
void sumpPumpHighLevelUpdateFunction();
void sumpPumpHighLevelExitFunction();
void sumpPumpVeryHighLevelEnterFunction();
void sumpPumpVeryHighLevelUpdateFunction();
void sumpPumpVeryHighLevelExitFunction();
void sumpPumpSetState(String newState);
void debounceInputs();
void debounceSumpPumpSensorHigh();
void debounceSumpPumpSensorVeryHigh();
#line 29 "/home/ceajog/particle/proyectos/Gust/meshHome/meshSumpPump/src/meshSumpPump.ino"
#define APP_NAME "waterLeakSensor-mesh"
#define APP_VERSION "Version 0.01"

/*******************************************************************************
 * changes in version 0.01:
       * first version
*******************************************************************************/

#include "FiniteStateMachine.h"
#include "Ubidots.h"
#include "elapsedMillis.h"

#define MESH_EVENT_SUMP_PUMP "meshSumpPump"

// enable the user code (our program below) to run in parallel with cloud connectivity code
// source: https://docs.particle.io/reference/firmware/photon/#system-thread
SYSTEM_THREAD(ENABLED);

#define SUMP_PUMP_SENSOR_HIGH D1
unsigned int integratorSumpPumpSensorHigh = 0;
int sumpPumpSensorHigh = 0;

#define SUMP_PUMP_SENSOR_VERY_HIGH D2
unsigned int integratorSumpPumpSensorVeryHigh = 0;
int sumpPumpSensorVeryHigh = 0;

#define STATE_OK "Sensor OK"
#define STATE_IN_TRANSITION "Sensor in transition"
#define STATE_ALARM "Sensor Alarm"
#define ALARM_TRANSITION_PERIOD 30000 // min amount of time to before an alarm is sent out
#define ALARM_MIN 10000               // min amount of time to stay in alarm before coming back to normal

#define STATE_HIGH_LEVEL "Sensor High Level"
#define STATE_VERY_HIGH_LEVEL "Sensor Very High Level"
// FSM declaration for sump pump sensors
State sumpPumpOkState = State(sumpPumpOkEnterFunction, sumpPumpOkUpdateFunction, sumpPumpOkExitFunction);
State sumpPumpTransitionState = State(sumpPumpTransitionEnterFunction, sumpPumpTransitionUpdateFunction, sumpPumpTransitionExitFunction);
State sumpPumpHighLevelState = State(sumpPumpHighLevelEnterFunction, sumpPumpHighLevelUpdateFunction, sumpPumpHighLevelExitFunction);
State sumpPumpVeryHighLevelState = State(sumpPumpVeryHighLevelEnterFunction, sumpPumpVeryHighLevelUpdateFunction, sumpPumpVeryHighLevelExitFunction);
FSM sumpPumpStateMachine = FSM(sumpPumpOkState);
String sumpPumpState = STATE_OK;

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

#define PUBLISH_INTERVAL 5000
elapsedMillis publishInterval;

/*******************************************************************************
 * Function Name  : setup
 * Description    : this function runs once at system boot
 *******************************************************************************/
void setup()
{

  // cloud variables
  Particle.variable("sumpPumpState", sumpPumpState);

  pinMode(SUMP_PUMP_SENSOR_HIGH, INPUT_PULLDOWN);
  pinMode(SUMP_PUMP_SENSOR_VERY_HIGH, INPUT_PULLDOWN);

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
  sumpPumpStateMachine.update();

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

  Particle.publish("sump pump state", sumpPumpState, PRIVATE | WITH_ACK);

#if PLATFORM_ID == PLATFORM_XENON
  Mesh.publish(MESH_EVENT_SUMP_PUMP, sumpPumpState);
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
 SUMP PUMP sensor
*******************************************************************************/
void sumpPumpOkEnterFunction()
{
  Particle.publish("OK", "SUMP PUMP sensors OK", PRIVATE | WITH_ACK);
  sendUbidotsAlarm("sumpPump", 0);
  sumpPumpSetState(STATE_OK);
}
void sumpPumpOkUpdateFunction()
{
  if (sumpPumpSensorHigh == HIGH)
  {
    sumpPumpStateMachine.transitionTo(sumpPumpTransitionState);
    Log.info("sumpPumpSensorHigh is HIGH, transition to sumpPumpTransitionState");
  }
  if (sumpPumpSensorVeryHigh == HIGH)
  {
    sumpPumpStateMachine.transitionTo(sumpPumpTransitionState);
    Log.info("sumpPumpSensorVeryHigh is HIGH, transition to sumpPumpTransitionState");
  }
}
void sumpPumpOkExitFunction()
{
}

void sumpPumpTransitionEnterFunction()
{
  Particle.publish("IN TRANSITION", "SUMP PUMP sensors IN TRANSITION", PRIVATE | WITH_ACK);
  sumpPumpSetState(STATE_IN_TRANSITION);
}
void sumpPumpTransitionUpdateFunction()
{
  // stay here the soak period before sending any alarm
  if (sumpPumpStateMachine.timeInCurrentState() < ALARM_TRANSITION_PERIOD)
  {
    return;
  }

  if (sumpPumpSensorHigh == HIGH)
  {
    sumpPumpStateMachine.transitionTo(sumpPumpHighLevelState);
    Log.info("sumpPumpSensorHigh is HIGH, transition to sumpPumpHighLevelState");
  }
  if (sumpPumpSensorVeryHigh == HIGH)
  {
    sumpPumpStateMachine.transitionTo(sumpPumpVeryHighLevelState);
    Log.info("sumpPumpSensorVeryHigh is HIGH, transition to sumpPumpVeryHighLevelState");
  }

  if ((sumpPumpSensorHigh == LOW) && (sumpPumpSensorVeryHigh == LOW))
  {
    sumpPumpStateMachine.transitionTo(sumpPumpOkState);
    Log.info("sumpPumpSensorHigh and sumpPumpSensorVeryHigh are LOW, transition to sumpPumpOkState");
  }
}
void sumpPumpTransitionExitFunction()
{
}

void sumpPumpHighLevelEnterFunction()
{
  Particle.publish("ALARM", "Alarm on SUMP PUMP sensor: HIGH level reached", PRIVATE | WITH_ACK);
  sendUbidotsAlarm("sumpPump", 1);
  sumpPumpSetState(STATE_HIGH_LEVEL);
}
void sumpPumpHighLevelUpdateFunction()
{
  // stay here a minimum time
  if (sumpPumpStateMachine.timeInCurrentState() < ALARM_MIN)
  {
    return;
  }
  // sensors went back to normal
  if ((sumpPumpSensorHigh == LOW) && (sumpPumpSensorVeryHigh == LOW))
  {
    sumpPumpStateMachine.transitionTo(sumpPumpOkState);
    Log.info("sumpPumpSensorHigh and sumpPumpSensorVeryHigh are LOW, transition to sumpPumpOkState");
  }
  // sensor very high got triggered
  if (sumpPumpSensorVeryHigh == HIGH)
  {
    sumpPumpStateMachine.transitionTo(sumpPumpVeryHighLevelState);
    Log.info("sumpPumpSensorVeryHigh is HIGH, transition to sumpPumpVeryHighLevelState");
  }
}
void sumpPumpHighLevelExitFunction()
{
}

void sumpPumpVeryHighLevelEnterFunction()
{
  Particle.publish("ALARM", "Alarm on SUMP PUMP sensor: VERY HIGH level reached", PRIVATE | WITH_ACK);
  sendUbidotsAlarm("sumpPump", 2);
  sumpPumpSetState(STATE_VERY_HIGH_LEVEL);
}
void sumpPumpVeryHighLevelUpdateFunction()
{
  // stay here a minimum time
  if (sumpPumpStateMachine.timeInCurrentState() < ALARM_MIN)
  {
    return;
  }
  // both sensors went back to normal
  if ((sumpPumpSensorHigh == LOW) && (sumpPumpSensorVeryHigh == LOW))
  {
    sumpPumpStateMachine.transitionTo(sumpPumpOkState);
    Log.info("sumpPumpSensorHigh and sumpPumpSensorVeryHigh are LOW, transition to sumpPumpOkState");
  }
  // sensor very high went normal
  if ((sumpPumpSensorHigh == HIGH) && (sumpPumpSensorVeryHigh == LOW))
  {
    sumpPumpStateMachine.transitionTo(sumpPumpHighLevelState);
    Log.info("sumpPumpSensorHigh is HIGH and sumpPumpSensorVeryHigh is LOW, transition to sumpPumpHighLevelState");
  }
}
void sumpPumpVeryHighLevelExitFunction()
{
}

/*******************************************************************************
 * Function Name  : sumpPumpSetState
 * Description    : sets the state of an FSM
 * Return         : none
 *******************************************************************************/
void sumpPumpSetState(String newState)
{
  sumpPumpState = newState;
  Particle.publish("FSM", "SUMP PUMP fsm entering " + newState + " state", PRIVATE | WITH_ACK);
  Log.info("SUMP PUMP fsm entering " + newState + " state");
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
  debounceSumpPumpSensorHigh();
  debounceSumpPumpSensorVeryHigh();
}

/*******************************************************************************
 * Function Name  :     debounceSumpPumpSensorHigh
 * Description    : debounces the sump pump sensor high level
 * Return         : nothing
 * Source: http://www.ganssle.com/debouncing-pt2.htm
 * Source: http://www.kennethkuhn.com/electronics/debounce.c
 *******************************************************************************/
void debounceSumpPumpSensorHigh()
{
  // Step 1: Update the integrator based on the input signal.  Note that the
  // integrator follows the input, decreasing or increasing towards the limits as
  // determined by the input state (0 or 1).
  if (digitalRead(SUMP_PUMP_SENSOR_HIGH) == LOW)
  {
    if (integratorSumpPumpSensorHigh > 0)
      integratorSumpPumpSensorHigh--;
  }
  else if (integratorSumpPumpSensorHigh < MAXIMUM)
    integratorSumpPumpSensorHigh++;

  // Step 2: Update the output state based on the integrator.  Note that the
  // output will only change states if the integrator has reached a limit, either
  // 0 or MAXIMUM.
  if (integratorSumpPumpSensorHigh == 0)
    sumpPumpSensorHigh = 0;
  else if (integratorSumpPumpSensorHigh >= MAXIMUM)
  {
    sumpPumpSensorHigh = 1;
    integratorSumpPumpSensorHigh = MAXIMUM; /* defensive code if integrator got corrupted */
  }
}

/*******************************************************************************
 * Function Name  :     debounceSumpPumpSensorVeryHigh
 * Description    : debounces the sump pump sensor very high level
 * Return         : nothing
 * Source: http://www.ganssle.com/debouncing-pt2.htm
 * Source: http://www.kennethkuhn.com/electronics/debounce.c
 *******************************************************************************/
void debounceSumpPumpSensorVeryHigh()
{
  // Step 1: Update the integrator based on the input signal.  Note that the
  // integrator follows the input, decreasing or increasing towards the limits as
  // determined by the input state (0 or 1).
  if (digitalRead(SUMP_PUMP_SENSOR_VERY_HIGH) == LOW)
  {
    if (integratorSumpPumpSensorVeryHigh > 0)
      integratorSumpPumpSensorVeryHigh--;
  }
  else if (integratorSumpPumpSensorVeryHigh < MAXIMUM)
    integratorSumpPumpSensorVeryHigh++;

  // Step 2: Update the output state based on the integrator.  Note that the
  // output will only change states if the integrator has reached a limit, either
  // 0 or MAXIMUM.
  if (integratorSumpPumpSensorVeryHigh == 0)
    sumpPumpSensorVeryHigh = 0;
  else if (integratorSumpPumpSensorVeryHigh >= MAXIMUM)
  {
    sumpPumpSensorVeryHigh = 1;
    integratorSumpPumpSensorVeryHigh = MAXIMUM; /* defensive code if integrator got corrupted */
  }
}
