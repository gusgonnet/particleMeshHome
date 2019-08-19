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

Ubidots ubidots(UBIDOTS_TOKEN, UBI_MESH);

/****************************************
 * Auxiliar Functions
 ****************************************/

// Put here your auxiliar functions

/****************************************
 * Main Functions
 ****************************************/

void setup() {
  Serial.begin(115200);
  ubidots.setDebug(true);  // Uncomment this line for printing debug messages
}

void loop() {
  float value = analogRead(A0);
  Serial.println("Adding value");
  ubidots.add("variable-label", value);
  ubidots.meshPublishToUbidots();
  Serial.println("finished");
  /* WARNING a very high sample rate may give you issues in your gateway routine
   */
  delay(20000);
}
