/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "application.h"
#line 1 "/home/ceajog/particle/proyectos/Gust/meshHome/meshGarage/src/meshGarage.ino"
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
// github: https://github.com/gusgonnet
// hackster: https://www.hackster.io/gusgonnet
//
// Free for personal use.
//
// https://creativecommons.org/licenses/by-nc-sa/4.0/

#include "FiniteStateMachine.h"
#include "elapsedMillis.h"

void meshGarageCommandHandler(const char *event, const char *data);
void setup();
void loop();
void sendMeshUpdate();
void pulseGarage();
int cloudPulseGarage(String commandSentByUser);
void debounceInputs();
void debounceGarageOpen();
void debounceGarageClosed();
void closedEnterFunction();
void closedUpdateFunction();
void closedExitFunction();
void pulseBeforeOpeningEnterFunction();
void pulseBeforeOpeningUpdateFunction();
void pulseBeforeOpeningExitFunction();
void verifyOpeningEnterFunction();
void verifyOpeningUpdateFunction();
void verifyOpeningExitFunction();
void openingEnterFunction();
void openingUpdateFunction();
void openingExitFunction();
void openEnterFunction();
void openUpdateFunction();
void openExitFunction();
void pulseBeforeClosingEnterFunction();
void pulseBeforeClosingUpdateFunction();
void pulseBeforeClosingExitFunction();
void verifyClosingEnterFunction();
void verifyClosingUpdateFunction();
void verifyClosingExitFunction();
void closingEnterFunction();
void closingUpdateFunction();
void closingExitFunction();
void initEnterFunction();
void initUpdateFunction();
void initExitFunction();
void errorEnterFunction();
void errorUpdateFunction();
void errorExitFunction();
void unknownEnterFunction();
void unknownUpdateFunction();
void unknownExitFunction();
void pulseBeforeMovingEnterFunction();
void pulseBeforeMovingUpdateFunction();
void pulseBeforeMovingExitFunction();
void movingEnterFunction();
void movingUpdateFunction();
void movingExitFunction();
String getTime();
void defensiveClose();
void defensiveOpen();
bool garageIsClosed();
bool garageIsOpen();
void checkTimeout();
void setState(String newState, int newStateNumber);
void setAppState(String localState, int localStateNumber);
void myPublish(String description);
#line 30 "/home/ceajog/particle/proyectos/Gust/meshHome/meshGarage/src/meshGarage.ino"
#define APP_NAME "garageCommander3rdGen"
#define VERSION "2.01"

// SerialLogHandler logHandler(LOG_LEVEL_ALL);
SerialLogHandler logHandler(LOG_LEVEL_INFO);

/*******************************************************************************
 * changes in version 2.01:
       * this is version 2 of the project, building on v1 here: https://www.hackster.io/gusgonnet/garage-commander-30383a
*******************************************************************************/

// defines inputs and outputs
int GARAGE_OPEN = D1;
int GARAGE_CLOSED = D2;
int GARAGE_RELAY = D4;

// how much time to pulse the relay so the garage opens
#define GARAGE_RELAY_PULSE_MILLIS 1000

// how much time to wait in init so sensors get measured
#define INIT_DELAY 1000

// timers work on millis, so we adjust the value with this constant
#define MILLISECONDS_TO_MINUTES 60000
#define MILLISECONDS_TO_SECONDS 1000

// variable to read open garage sensor state
int garageOpen = 0;
// variable to read close garage sensor state
int garageClosed = 0;

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
unsigned int integratorGarageOpen = 0;   /* Will range from 0 to the specified MAXIMUM */
unsigned int integratorGarageClosed = 0; /* Will range from 0 to the specified MAXIMUM */

// states that imply that the garage door is moving have an extra 100
// in their state number to easily identify when the door is moving
// from the app
#define STATE_MOVING_OFFSET 100

// FSM declaration
#define STATE_CLOSED "Closed"
#define STATE_CLOSED_NUMBER 1
State closedState = State(closedEnterFunction, closedUpdateFunction, closedExitFunction);

// I will leave this at "Opening" so the app does not show "Pulse before opening"
// which might be something that the user won't understand and they
// don't really need to know
// #define STATE_PULSE_BEFORE_OPENING "Pulse before Opening"
#define STATE_PULSE_BEFORE_OPENING "Opening"
#define STATE_PULSE_BEFORE_OPENING_NUMBER 2 + STATE_MOVING_OFFSET
State pulseBeforeOpeningState = State(pulseBeforeOpeningEnterFunction, pulseBeforeOpeningUpdateFunction, pulseBeforeOpeningExitFunction);

#define STATE_VERIFY_OPENING "Opening"
#define STATE_VERIFY_OPENING_NUMBER 3 + STATE_MOVING_OFFSET
State verifyOpeningState = State(verifyOpeningEnterFunction, verifyOpeningUpdateFunction, verifyOpeningExitFunction);

#define STATE_OPENING "Opening"
#define STATE_OPENING_NUMBER 4 + STATE_MOVING_OFFSET
State openingState = State(openingEnterFunction, openingUpdateFunction, openingExitFunction);

#define STATE_OPEN "Open"
#define STATE_OPEN_NUMBER 5
State openState = State(openEnterFunction, openUpdateFunction, openExitFunction);

// #define STATE_PULSE_BEFORE_CLOSING "Pulse before Closing"
#define STATE_PULSE_BEFORE_CLOSING "Closing"
#define STATE_PULSE_BEFORE_CLOSING_NUMBER 6 + STATE_MOVING_OFFSET
State pulseBeforeClosingState = State(pulseBeforeClosingEnterFunction, pulseBeforeClosingUpdateFunction, pulseBeforeClosingExitFunction);

#define STATE_VERIFY_CLOSING "Closing"
#define STATE_VERIFY_CLOSING_NUMBER 7 + STATE_MOVING_OFFSET
State verifyClosingState = State(verifyClosingEnterFunction, verifyClosingUpdateFunction, verifyClosingExitFunction);

#define STATE_CLOSING "Closing"
#define STATE_CLOSING_NUMBER 8 + STATE_MOVING_OFFSET
State closingState = State(closingEnterFunction, closingUpdateFunction, closingExitFunction);

#define STATE_INIT "Initializing"
#define STATE_INIT_NUMBER 50
State initState = State(initEnterFunction, initUpdateFunction, initExitFunction);

#define STATE_ERROR "Error"
#define STATE_ERROR_NUMBER 51
State errorState = State(errorEnterFunction, errorUpdateFunction, errorExitFunction);

#define STATE_TIMEOUT 20000
#define STATE_UNKNOWN "Unknown"
#define STATE_UNKNOWN_NUMBER 52
State unknownState = State(unknownEnterFunction, unknownUpdateFunction, unknownExitFunction);

// #define STATE_PULSE_BEFORE_MOVING "Pulse before Moving"
#define STATE_PULSE_BEFORE_MOVING "Moving"
#define STATE_PULSE_BEFORE_MOVING_NUMBER 53 + STATE_MOVING_OFFSET
State pulseBeforeMovingState = State(pulseBeforeMovingEnterFunction, pulseBeforeMovingUpdateFunction, pulseBeforeMovingExitFunction);

#define STATE_MOVING "Moving"
#define STATE_MOVING_NUMBER 54 + STATE_MOVING_OFFSET
State movingState = State(movingEnterFunction, movingUpdateFunction, movingExitFunction);

// we use this timeout to detect if the door moved out of its current state
#define STATE_SHORT_TIMEOUT 2000

String state = STATE_INIT;
String appState = String(STATE_INIT_NUMBER) + "|" + STATE_INIT;
int stateNumber = STATE_INIT_NUMBER;

String lastError = "";
bool debugMessages = true;

String command = "";
#define COMMAND_CLOSE "close"
#define COMMAND_OPEN "open"
#define COMMAND_MOVE "move"

//initialize the state machine, start in state: init
FSM garageStateMachine = FSM(initState);

#define BUFFER 623
#define LITTLE 50

const int TIME_ZONE = -4;

// interval to send updates to mesh gateway (which relays the info to blynk)
#define UPDATE_MESH_INTERVAL 1000
elapsedMillis updateMeshInterval;

/*******************************************************************************
 garage mesh sensor
*******************************************************************************/
String meshGatewayLastHeardOf = "Never";
#define MESH_EVENT_GARAGE "garageStatus"
#define MESH_EVENT_GARAGE_COMMAND "garageCommand"

// enable the remote temperature sensor if on an argon
#if PLATFORM_ID == PLATFORM_XENON
void meshGarageCommandHandler(const char *event, const char *data)
{
    char tempChar[BUFFER] = "";
    snprintf(tempChar, BUFFER, "event=%s data=%s", event, data ? data : "NULL");
    Log.info(tempChar);

    snprintf(tempChar, BUFFER, "%s", data ? data : "");

    // if there is no ongoing command, accept the incoming command
    if (command == "")
    {
        command = tempChar;
    }

    meshGatewayLastHeardOf = Time.timeStr();
}
#endif

/*******************************************************************************
 * Function Name  : setup
 * Description    : this function runs once at system boot
 *******************************************************************************/
void setup()
{

    // declare cloud variables
    // https://docs.particle.io/reference/firmware/photon/#particle-variable-
    Particle.variable("state", state);
    Particle.variable("stateNumber", stateNumber);
    Particle.variable("appState", appState);

    Particle.variable("meshGatewayLastHeardOf", meshGatewayLastHeardOf);

    // declare cloud functions
    // https://docs.particle.io/reference/firmware/photon/#particle-function-
    Particle.function("pulseGarage", cloudPulseGarage);
    // Particle.function("openGarage", cloudOpenGarage);
    // Particle.function("closeGarage", cloudCloseGarage);

// enable the remote temperature sensor if on an argon
#if PLATFORM_ID == PLATFORM_XENON
    Mesh.subscribe(MESH_EVENT_GARAGE_COMMAND, meshGarageCommandHandler);
#endif

    pinMode(GARAGE_OPEN, INPUT_PULLUP);
    pinMode(GARAGE_CLOSED, INPUT_PULLUP);
    pinMode(GARAGE_RELAY, OUTPUT);

    debounceInputsTimer.start();

    Time.zone(TIME_ZONE);

    // this code prints the firmware version
    Particle.publish(APP_NAME, VERSION, PRIVATE);
}

/*******************************************************************************
 * Function Name  : loop
 * Description    : this function runs continuously while the project is running
 *******************************************************************************/
void loop()
{

    // call the FSM
    garageStateMachine.update();

    // command takes in an asynchronous call from a particle cloud function
    // the FSM addressed the command with the garageStateMachine.update() call, so we can flush it here
    command = "";

    sendMeshUpdate();
}

/*******************************************************************************
 * Function Name  : sendMeshUpdate
 * Description    : updates the mesh gateway with the status of the garage
 *******************************************************************************/
void sendMeshUpdate()
{

    //time is up? no, then come back later
    if (updateMeshInterval < UPDATE_MESH_INTERVAL)
    {
        return;
    }

    //time is up, reset timer
    updateMeshInterval = 0;

#if PLATFORM_ID == PLATFORM_XENON
    Mesh.publish(MESH_EVENT_GARAGE, state);

    Log.info(state);
#endif
}

/*******************************************************************************
 * Function Name  : pulseGarage
 * Description    : this function pulses the garage relay
 *******************************************************************************/
void pulseGarage()
{
    Particle.publish(APP_NAME, "pulsing garage relay", PRIVATE);
    digitalWrite(GARAGE_RELAY, HIGH);

    delay(GARAGE_RELAY_PULSE_MILLIS);
    digitalWrite(GARAGE_RELAY, LOW);
}

/*******************************************************************************
 * Function Name  : cloudPulseGarage
 * Description    : this function receives a cloud command and 
                       sets a flag to pulse the garage relay
 *******************************************************************************/
int cloudPulseGarage(String commandSentByUser)
{
    // if there is a command ON already, return -1
    // this is to avoid double commands
    if (command != "")
    {
        myPublish("ERROR: there is an ongoing garage command already");
        return -1;
    }

    // validate commandSentByUser
    if (!(
            (commandSentByUser.equalsIgnoreCase(COMMAND_CLOSE)) or (commandSentByUser.equalsIgnoreCase(COMMAND_OPEN)) or (commandSentByUser.equalsIgnoreCase(COMMAND_MOVE))))
    {
        myPublish("ERROR: Invalid value for commandSentByUser: " + commandSentByUser);
        return -1;
    }

    if (garageStateMachine.isInState(closedState) and (commandSentByUser.equalsIgnoreCase(COMMAND_CLOSE)))
    {
        myPublish("ERROR: garage is already closed");
        return -1;
    }

    if (garageStateMachine.isInState(openState) and (commandSentByUser.equalsIgnoreCase(COMMAND_OPEN)))
    {
        myPublish("ERROR: garage is already open");
        return -1;
    }

    if (garageStateMachine.isInState(movingState) and (commandSentByUser.equalsIgnoreCase(COMMAND_MOVE)))
    {
        myPublish("ERROR: garage is already moving");
        return -1;
    }

    command = commandSentByUser;
    return 0;
}

/*******************************************************************************
 * Function Name  : debounceInputs
 * Description    : debounces the contact sensors, triggered by a timer with similar name
 * Return         : nothing
 * Source: http://www.ganssle.com/debouncing-pt2.htm
 * Source: http://www.kennethkuhn.com/electronics/debounce.c
 *******************************************************************************/
void debounceInputs()
{
    debounceGarageOpen();
    debounceGarageClosed();
}

/*******************************************************************************
 * Function Name  : debounceGarageOpen
 * Description    : debounces the garage_open sensor
 * Return         : nothing
 * Source: http://www.ganssle.com/debouncing-pt2.htm
 * Source: http://www.kennethkuhn.com/electronics/debounce.c
 *******************************************************************************/
void debounceGarageOpen()
{

    int localInput = digitalRead(GARAGE_OPEN);

    // Step 1: Update the integrator based on the input signal.  Note that the
    // integrator follows the input, decreasing or increasing towards the limits as
    // determined by the input state (0 or 1).
    if (localInput == LOW)
    {
        if (integratorGarageOpen > 0)
            integratorGarageOpen--;
    }
    else if (integratorGarageOpen < MAXIMUM)
        integratorGarageOpen++;

    // Step 2: Update the output state based on the integrator.  Note that the
    // output will only change states if the integrator has reached a limit, either
    // 0 or MAXIMUM.
    if (integratorGarageOpen == 0)
        garageOpen = 0;
    else if (integratorGarageOpen >= MAXIMUM)
    {
        garageOpen = 1;
        integratorGarageOpen = MAXIMUM; /* defensive code if integrator got corrupted */
    }
}

/*******************************************************************************
 * Function Name  : debounceGarageClosed
 * Description    : debounces the garage_closed sensor
 * Return         : nothing
 * Source: http://www.ganssle.com/debouncing-pt2.htm
 * Source: http://www.kennethkuhn.com/electronics/debounce.c
 *******************************************************************************/
void debounceGarageClosed()
{

    int localInput = digitalRead(GARAGE_CLOSED);

    // Step 1: Update the integrator based on the input signal.  Note that the
    // integrator follows the input, decreasing or increasing towards the limits as
    // determined by the input state (0 or 1).
    if (localInput == LOW)
    {
        if (integratorGarageClosed > 0)
            integratorGarageClosed--;
    }
    else if (integratorGarageClosed < MAXIMUM)
        integratorGarageClosed++;

    // Step 2: Update the output state based on the integrator.  Note that the
    // output will only change states if the integrator has reached a limit, either
    // 0 or MAXIMUM.
    if (integratorGarageClosed == 0)
        garageClosed = 0;
    else if (integratorGarageClosed >= MAXIMUM)
    {
        garageClosed = 1;
        integratorGarageClosed = MAXIMUM; /* defensive code if integrator got corrupted */
    }
}

/*******************************************************************************
********************************************************************************
********************************************************************************
 FINITE STATE MACHINE FUNCTIONS
********************************************************************************
********************************************************************************
*******************************************************************************/

// state 1
void closedEnterFunction()
{
    setState(STATE_CLOSED, STATE_CLOSED_NUMBER);
}
void closedUpdateFunction()
{
    // open command received?
    if (command == COMMAND_OPEN)
    {
        garageStateMachine.transitionTo(pulseBeforeOpeningState); // transition to state 2
        command = "";
    }

    // if the garage is not sensed closed anymore, transition to opening state
    if (!garageIsClosed())
    {
        garageStateMachine.transitionTo(openingState); // transition to state 4
    }

    // defensive code in case FSM gets corrupted
    defensiveOpen();
}
void closedExitFunction()
{
}

// state 2
void pulseBeforeOpeningEnterFunction()
{
    setState(STATE_PULSE_BEFORE_OPENING, STATE_PULSE_BEFORE_OPENING_NUMBER);
    pulseGarage();
}
void pulseBeforeOpeningUpdateFunction()
{
    garageStateMachine.transitionTo(verifyOpeningState); // transition to state 3
}
void pulseBeforeOpeningExitFunction()
{
}

// state 3
void verifyOpeningEnterFunction()
{
    setState(STATE_VERIFY_OPENING, STATE_VERIFY_OPENING_NUMBER);
}
void verifyOpeningUpdateFunction()
{
    // did the garage start to open?
    if (!garageIsClosed())
    {
        garageStateMachine.transitionTo(openingState); // transition to state 4
    }

    // check if it was in this state for too long
    // meaning something may be wrong since the door is not opening
    if (garageStateMachine.timeInCurrentState() > STATE_SHORT_TIMEOUT)
    {
        garageStateMachine.transitionTo(closedState); // transition to state closed 1
        myPublish("ERROR: garage did not open");
    }
}
void verifyOpeningExitFunction()
{
}

// state 4
void openingEnterFunction()
{
    setState(STATE_OPENING, STATE_OPENING_NUMBER);
}
void openingUpdateFunction()
{
    // did the garage fully open?
    if (garageIsOpen())
    {
        garageStateMachine.transitionTo(openState); // transition to state 3
    }

    // check if it was in this state for too long
    checkTimeout();
}
void openingExitFunction()
{
}

// state 5
void openEnterFunction()
{
    setState(STATE_OPEN, STATE_OPEN_NUMBER);
}
void openUpdateFunction()
{
    // close command received?
    if (command == COMMAND_CLOSE)
    {
        garageStateMachine.transitionTo(pulseBeforeClosingState); // transition to state 6
        command = "";
    }

    // if the garage is not sensed open anymore, transition to closing state
    if (!garageIsOpen())
    {
        garageStateMachine.transitionTo(closingState); // transition to state 8
    }

    // defensive code in case FSM gets corrupted
    defensiveClose();
}
void openExitFunction()
{
}

// state 6
void pulseBeforeClosingEnterFunction()
{
    setState(STATE_PULSE_BEFORE_CLOSING, STATE_PULSE_BEFORE_CLOSING_NUMBER);
    pulseGarage();
}
void pulseBeforeClosingUpdateFunction()
{
    garageStateMachine.transitionTo(verifyClosingState); // transition to state 7
}
void pulseBeforeClosingExitFunction()
{
}

// state 7
void verifyClosingEnterFunction()
{
    setState(STATE_VERIFY_CLOSING, STATE_VERIFY_CLOSING_NUMBER);
}
void verifyClosingUpdateFunction()
{
    // did the garage start to open?
    if (!garageIsOpen())
    {
        garageStateMachine.transitionTo(closingState); // transition to state 8
    }

    // check if it was in this state for too long
    // meaning something may be wrong since the door is not opening
    if (garageStateMachine.timeInCurrentState() > STATE_SHORT_TIMEOUT)
    {
        garageStateMachine.transitionTo(closedState); // transition to state closed 1
        myPublish("ERROR: garage did not close");
    }
}
void verifyClosingExitFunction()
{
}

// state 8
void closingEnterFunction()
{
    setState(STATE_CLOSING, STATE_CLOSING_NUMBER);
}
void closingUpdateFunction()
{
    if (garageIsClosed())
    {
        garageStateMachine.transitionTo(closedState); // transition to state 1
    }

    // check if it was in this state for too long
    checkTimeout();
}
void closingExitFunction()
{
}

// state 50
void initEnterFunction()
{
    setState(STATE_INIT, STATE_INIT_NUMBER);
    delay(INIT_DELAY);
}
void initUpdateFunction()
{

    // decide to which state to transition based on the garage sensors states
    // if both sensors are closed, then something is wrong
    if ((garageIsClosed()) and (garageIsOpen()))
    {
        lastError = getTime() + " - both open/closed sensors are active";
        myPublish("ERROR: both open/closed sensors are active");
        garageStateMachine.transitionTo(errorState);
        return;
    }

    // if the closed sensor is on, then the garage is closed
    if (garageIsClosed())
    {
        garageStateMachine.transitionTo(closedState); // transition to state 1
        return;
    }

    // if the open sensor is on, then the garage is open
    if (garageIsOpen())
    {
        garageStateMachine.transitionTo(openState); // transition to state 5
        return;
    }

    // if the code reached here, it means the sensors are not sensing close nor open so
    // the FSM transitions to the unknown state
    garageStateMachine.transitionTo(unknownState);
}
void initExitFunction()
{
}

// state 51
void errorEnterFunction()
{
    setState(STATE_ERROR, STATE_ERROR_NUMBER);
}
void errorUpdateFunction()
{
    // if both sensors are closed, then something is still wrong
    if ((garageIsClosed()) and (garageIsOpen()))
    {
        return;
    }

    // decide to which state to transition based on the garage sensors states
    if (garageIsClosed())
    {
        garageStateMachine.transitionTo(closedState); // transition to state 1
        return;
    }

    // decide to which state to transition based on the garage sensors states
    if (garageIsOpen())
    {
        garageStateMachine.transitionTo(openState); // transition to state 5
        return;
    }
}
void errorExitFunction()
{
}

// state 52
void unknownEnterFunction()
{
    setState(STATE_UNKNOWN, STATE_UNKNOWN_NUMBER);
}
void unknownUpdateFunction()
{
    // decide to which state to transition based on the garage sensors states
    if (garageIsClosed())
    {
        garageStateMachine.transitionTo(closedState); // transition to state 1
        return;
    }

    // decide to which state to transition based on the garage sensors states
    if (garageIsOpen())
    {
        garageStateMachine.transitionTo(openState); // transition to state 5
        return;
    }

    // any command received?
    if ((command == COMMAND_CLOSE) or (command == COMMAND_OPEN) or (command == COMMAND_MOVE))
    {
        garageStateMachine.transitionTo(pulseBeforeMovingState); // transition to state 53
    }
}
void unknownExitFunction()
{
}

// state 53
void pulseBeforeMovingEnterFunction()
{
    setState(STATE_PULSE_BEFORE_MOVING, STATE_PULSE_BEFORE_MOVING_NUMBER);
    pulseGarage();
}
void pulseBeforeMovingUpdateFunction()
{
    garageStateMachine.transitionTo(movingState); // transition to state 54
}
void pulseBeforeMovingExitFunction()
{
}

// state 54
void movingEnterFunction()
{
    setState(STATE_MOVING, STATE_MOVING_NUMBER);
}
void movingUpdateFunction()
{
    // decide to which state to transition based on the garage sensors states
    if (garageIsClosed())
    {
        garageStateMachine.transitionTo(closedState); // transition to state 1
        return;
    }

    // decide to which state to transition based on the garage sensors states
    if (garageIsOpen())
    {
        garageStateMachine.transitionTo(openState); // transition to state 5
        return;
    }

    // check if it was in this state for too long
    checkTimeout();
}
void movingExitFunction()
{
}

/*******************************************************************************
********************************************************************************
********************************************************************************
 HELPER FUNCTIONS
********************************************************************************
********************************************************************************
*******************************************************************************/

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
 * Function Name  : transition FSM to closedState
 * Description    : used in case the FSM gets corrupted to bring the system back to a known state
 * Return         : nothing
 *******************************************************************************/
void defensiveClose()
{
    if (garageIsClosed())
    {
        lastError = getTime() + " - garage closed while on " + state;
        myPublish("ERROR: garage closed while on " + state);
        garageStateMachine.transitionTo(closedState); // transition to state 1
    }
}

/*******************************************************************************
 * Function Name  : transition FSM to openState
 * Description    : used in case the FSM gets corrupted to bring the system back to a known state
 * Return         : nothing
 *******************************************************************************/
void defensiveOpen()
{
    if (garageIsOpen())
    {
        lastError = getTime() + " - garage opened while on " + state;
        myPublish("ERROR: garage opened while on " + state);
        garageStateMachine.transitionTo(openState); // transition to state 3
    }
}

/*******************************************************************************
 * Function Name  : garageIsClosed
 * Description    : returns true or false depending on detection of garage closed
 * Return         : boolean
 *******************************************************************************/
bool garageIsClosed()
{
    if (garageClosed == LOW)
    {
        return true;
    }
    return false;
}

/*******************************************************************************
 * Function Name  : garageIsOpen
 * Description    : returns true or false depending on detection of garage open
 * Return         : boolean
 *******************************************************************************/
bool garageIsOpen()
{
    if (garageOpen == LOW)
    {
        return true;
    }
    return false;
}

/*******************************************************************************
 * Function Name  : checkTimeout
 * Description    : if it takes too long to close or open, something might be wrong
 * Return         : none
 *******************************************************************************/
void checkTimeout()
{
    if (garageStateMachine.timeInCurrentState() > STATE_TIMEOUT)
    {
        garageStateMachine.transitionTo(unknownState);
    }
}

/*******************************************************************************
 * Function Name  : setState
 * Description    : sets the state of the system and publishes the change
 * Return         : none
 *******************************************************************************/
void setState(String newState, int newStateNumber)
{
    state = newState;
    stateNumber = newStateNumber;

    setAppState(newState, newStateNumber);

    myPublish("Entering " + String(stateNumber) + " | " + state + " state");
}

/*******************************************************************************
 * Function Name  : setAppState
 * Description    : sets the appState variable required for the app
 * Return         : none
 *******************************************************************************/
void setAppState(String localState, int localStateNumber)
{
    appState = String(localStateNumber) + "|" + localState;
}

/*******************************************************************************
 * Function Name  : myPublish
 * Description    : Particle.publish filtered by debug status
 * Return         : nothing
 *******************************************************************************/
void myPublish(String description)
{
    if (debugMessages)
    {
        Particle.publish(APP_NAME, description, PRIVATE);
    }
}

/*******************************************************************************
********************************************************************************
********************************************************************************
 GARAGE FUNCTIONS
********************************************************************************
********************************************************************************
*******************************************************************************/
