/****************************************
 * Include Libraries
 ****************************************/

#include "Ubidots.h"

/****************************************
 * Define Instances and Constants
 ****************************************/

#ifndef UBIDOTS_TOKEN
#define UBIDOTS_TOKEN "BBFF-IONay56PteRbxIMQbk4ppZ81VFX7yHGbZe5CTfmgEwZyqhbWnVbcVC9"  // Put here your Ubidots TOKEN
#endif

Ubidots ubidots(UBIDOTS_TOKEN, UBI_INDUSTRIAL, UBI_MESH);

/****************************************
 * Auxiliar Functions
 ****************************************/

// Put here your auxiliar functions

/****************************************
 * Main Functions
 ****************************************/

void setup() {
  Serial.begin(115200);
  // Uncomment this line to choose a different cloud Protocol to send data
  // ubidots.setCloudProtocol(UBI_HTTP);

  // Uncomment this line for printing debug messages
  ubidots.setDebug(true);
}

void loop() { ubidots.meshLoop(); }
