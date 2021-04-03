/*
**  Program  : output_ext.ino
**  Version  : v0.8.2-beta
**
**  Copyright (c) 2021 Robert van den Breemen
**  Contributed by Sjorsjuhmaniac
**
**  TERMS OF USE: MIT License. See bottom of file.   
** 
** most code shamelessly copied from Miles Burton's - Arduino Dallas library
** example 'Multiple'   
*/

#include <OneWire.h>
#include <DallasTemperature.h>

//prototype
char* getDallasAddress(DeviceAddress deviceAddress);

// GPIO where the DS18B20 is connected to
// Data wire is plugged TO GPIO 10
// #define ONE_WIRE_BUS 10

// Number of temperature devices found
int numberOfDevices;

// We'll use this variable to store a found device address
DeviceAddress tempDeviceAddress;

// Setup a oneWire instance to communicate with any OneWire devices
// needs a PIN to init correctly, pin is changed when we initSensors()
// this still may cause problems though because we this configures the pin already
// BUT, we're an OTGW, on a NODO print, so we are pretty sure nothing
// else is connected on these pins unlease somebody tweaked something.
// still don't like this too much:
OneWire oneWire(settingGPIOSENSORSpin);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);


void initSensors() {
  if (!settingGPIOSENSORSenabled) return;

  DebugTf("init GPIO Sensors on GPIO%d...\r\n", settingGPIOSENSORSpin);

  oneWire.begin(settingGPIOSENSORSpin);

  // Start the DS18B20 sensor
  sensors.begin();

  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();

  DebugTf("Found %d device(s)\r\n", numberOfDevices);
  int realDeviceCount = 0;
  // Loop through each device, print out address
  for (int i = 0; i < numberOfDevices; i++)
  {
    // Search the wire for address
    if (sensors.getAddress(tempDeviceAddress, i))
    {
      //TODO: get real device address, push data to mqtt topic.
      DebugTf("Device address %d device(s)\r\n", tempDeviceAddress);
      DebugFlush();
      realDeviceCount++;
    }
    else
    {
      DebugTf("Found ghost device %d but could not detect address. Check power and cabling\r\n", i);
    }
  }

  if (numberOfDevices < 1 or realDeviceCount < 1)
  {
    DebugTln("No Sensors Found, disabled GPIO Sensors! Reboot node to search again.");
    settingGPIOSENSORSenabled = false;
    return;
  }
}

int pollSensors()
{
  if (!settingGPIOSENSORSenabled) return 1;
  // Setup a oneWire instance to communicate with any OneWire devices
  // oneWire.begin(settingGPIOSENSORSpin);

  // Pass our oneWire reference to Dallas Temperature sensor 
  DallasTemperature sensors(&oneWire);

  if (numberOfDevices < 1)
  {
    DebugTln("No Sensors Found, please reboot the node to search for sensors");
    return 1;
  }
  // DebugTln("start polling sensors");
  sensors.requestTemperatures(); // Send the command to get temperatures

  // Loop through each device, print out temperature data
  for (int i = 0; i < numberOfDevices; i++)
  {
    // Search the wire for address
    if (sensors.getAddress(tempDeviceAddress, i))
    {
      // Output the device ID
      // Print the data

      const char * strDeviceAddress = getDallasAddress(tempDeviceAddress);

      float tempC = sensors.getTempC(tempDeviceAddress);
      DebugTf("Device: %s, TempC: %f\r\n", strDeviceAddress, tempC);

      //Build string for MQTT
      char _msg[15]{0};
      char _topic[50]{0};
      snprintf(_topic, sizeof _topic, "otgw-firmware/sensors/%s", strDeviceAddress);
      snprintf(_msg, sizeof _msg, "%f", tempC);

      // DebugTf("Topic: %s -- Payload: %s\r\n", _topic, _msg);
      DebugFlush();

      sendMQTTData(_topic, _msg);
      // Serial.println(DallasTemperature::toFahrenheit(tempC)); // Converts tempC to Fahrenheit
    }
  }
  delay(100);
  // DebugTln("end polling sensors");
  DebugFlush();
  return 0;
}

// function to print a device address
char* getDallasAddress(DeviceAddress deviceAddress)
{
  // DebugTf("\r\n");
  static char dest[10];
  dest[0] = '\0';
  
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16)
    {
      // Serial.print("0");
      strcat(dest, "0");
    }
    // Serial.print(deviceAddress[i], HEX);
    sprintf(dest+i, "%X", deviceAddress[i]);
    // DebugTf("blah: %s\r\n", dest);
    // DebugFlush();
  }
  return dest;
}

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
****************************************************************************
*/
