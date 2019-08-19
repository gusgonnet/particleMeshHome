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

#define APP_NAME "TempSensor-mesh"
#define APP_VERSION "Version 0.01"

/*******************************************************************************
 * changes in version 0.01:
       * first version
*******************************************************************************/

/*******************************************************************************
 HERE YOU DEFINE THE TYPE OF SENSOR THIS CODE READS
 comment out this line to select a thermistor or a ds18b20
 tip: you comment out a line with a double slash at the beginning of the line

 A line like this enables the thermistor:
 #define SENSOR_POOL // thermistor

 like this you disable the thermistor
 // #define SENSOR POOL // thermistor
*******************************************************************************/
// #define SENSOR_POOL // thermistor
// #define SENSOR_DS18B20
#define SENSOR_DHT22

#define MESH_EVENT_DS18B20 "meshTempSensorDs18b20"
#define MESH_EVENT_POOL "meshPoolSensor"

#include <DS18B20.h>
#include <math.h>
#include "elapsedMillis.h"
#include "AnalogSmooth.h"

#ifdef SENSOR_DHT22
#include "PietteTech_DHT.h"

/*******************************************************************************
 DHT sensor
*******************************************************************************/
#define DHTTYPE DHT22                    // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN 5                         // Digital pin for communications
#define TEMPERATURE_SAMPLE_INTERVAL 5000 // Sample room temperature every 5 seconds
void dht_wrapper();                      // must be declared before the lib initialization
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);
bool bDHTstarted; // flag to indicate we started acquisition

// This wrapper is in charge of calling the DHT sensor lib
void dht_wrapper() { DHT.isrCallback(); }

double humidity;

#endif

SerialLogHandler traceLog(LOG_LEVEL_ALL);

#define TEMP_SAMPLE_INTERVAL 5000
elapsedMillis tempSampleInterval;

/*******************************************************************************
 CELSIUS or FAHRENHEIT
*******************************************************************************/
bool useFahrenheit = false;

/*******************************************************************************
 thermistor sensor
*******************************************************************************/
#define POOL_THERMISTOR A0

// this is the thermistor used
// https://www.adafruit.com/products/372
// resistance at 25 degrees C
#define THERMISTORNOMINAL 10000
// temp. for nominal resistance (almost always 25 C)
#define TEMPERATURENOMINAL 25
// The beta coefficient of the thermistor (usually 3000-4000)
#define BCOEFFICIENT 3950
// the value of the 'other' resistor
#define SERIESRESISTOR 10000

/*******************************************************************************
 DS18B20 sensor
*******************************************************************************/
const int MAXRETRY = 20;

// sets pin for temp sensor and states that this is the only sensor on the bus
DS18B20 ds18b20(D9, true);

double celsius;
double fahrenheit;
AnalogSmooth analogSmoothTempCelsius = AnalogSmooth(20);    // get 20 samples
AnalogSmooth analogSmoothTempFahrenheit = AnalogSmooth(20); // get 20 samples
AnalogSmooth analogSmoothHumidity = AnalogSmooth(20);       // get 20 samples

#define BUFFER 623
#define LITTLE 50

/*******************************************************************************
 the firmware
*******************************************************************************/
void setup()
{
#ifdef SENSOR_POOL
  pinMode(POOL_THERMISTOR, INPUT);
#endif
}

void loop()
{
  getTemp();
}

/*******************************************************************************
 * Function Name  : publishData
 * Description    : sends the reading to other devices and the cloud
 *******************************************************************************/
void publishData()
{
  char tempChar[BUFFER] = "";
  snprintf(tempChar, BUFFER, "%.2f", celsius);

  if (useFahrenheit)
  {
    snprintf(tempChar, BUFFER, "%.2f", fahrenheit);
  }

  Particle.publish("remoteTemp", tempChar, PRIVATE | WITH_ACK);

#if PLATFORM_ID == PLATFORM_XENON

#ifdef SENSOR_DS18B20
  Mesh.publish(MESH_EVENT_DS18B20, tempChar);
#endif

#ifdef SENSOR_POOL
  Mesh.publish(MESH_EVENT_POOL, tempChar);
#endif

#endif
}

/*******************************************************************************
 * Function Name  : getTemp
 * Description    : gets a reading from the sensor
 *******************************************************************************/
void getTemp()
{

  //time is up? no, then come back later
  if (tempSampleInterval < TEMP_SAMPLE_INTERVAL)
  {
    return;
  }

  //time is up, reset timer
  tempSampleInterval = 0;

#ifdef SENSOR_DS18B20
  getTempDs18b20();
#endif

#ifdef SENSOR_POOL
  getTempThermistor();
#endif

#ifdef SENSOR_DHT22
  getTempDHT22();
#endif

  char tempChar[BUFFER] = "";
  snprintf(tempChar, BUFFER, "current temperature: %.2f °C", celsius);
  Log.info(tempChar);

  snprintf(tempChar, BUFFER, "current temperature: %.2f °F", fahrenheit);
  Log.info(tempChar);

#ifdef SENSOR_DHT22
  snprintf(tempChar, BUFFER, "current humidity: %.2f %%", humidity);
  Log.info(tempChar);
#endif

  // then let the other devices know the reading
  publishData();
}

/*******************************************************************************
 * Function Name  : getTempDs18b20
 * Description    : gets a reading from the sensor ds18b20
 *******************************************************************************/
void getTempDs18b20()
{

  float _temp;
  int i = 0;

  do
  {
    _temp = ds18b20.getTemperature();
  } while (!ds18b20.crcCheck() && MAXRETRY > i++);

  if (i > MAXRETRY)
  {
    celsius = fahrenheit = NAN;
    Log.info("Invalid reading");
    return;
  }

  // if code reaches here, the reading is good
  celsius = analogSmoothTempCelsius.smooth(_temp);
  fahrenheit = analogSmoothTempFahrenheit.smooth(ds18b20.convertToFahrenheit(_temp));
}

/*******************************************************************************
 * Function Name  : getTempThermistor
 * Description    : gets a reading from the sensor of the pool
 *******************************************************************************/
void getTempThermistor()
{

  float sample = analogRead(POOL_THERMISTOR);

  //convert the value to resistance
  sample = (4095 / sample) - 1;
  sample = SERIESRESISTOR / sample;

  float steinhart;
  steinhart = sample / THERMISTORNOMINAL;           // (R/Ro)
  steinhart = log(steinhart);                       // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                        // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                      // Invert
  steinhart -= 273.15;                              // convert to C

  // now smooth it
  celsius = analogSmoothTempCelsius.smooth(steinhart);

  //Convert Celsius to Fahrenheit
  fahrenheit = (celsius * 9.0) / 5.0 + 32.0;
}

/*******************************************************************************
 * Function Name  : getTempDHT22
 * Description    : reads the temperature of the DHT22 sensor at every TEMP_SAMPLE_INTERVAL
 *******************************************************************************/
void getTempDHT22()
{

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

  // now smooth it
  celsius = analogSmoothTempCelsius.smooth(DHT.getCelsius());

  //Convert Celsius to Fahrenheit
  fahrenheit = (celsius * 9.0) / 5.0 + 32.0;

  humidity = analogSmoothHumidity.smooth(DHT.getHumidity());

  //reset the sample flag so we can take another sample
  bDHTstarted = false;

  return;
}
