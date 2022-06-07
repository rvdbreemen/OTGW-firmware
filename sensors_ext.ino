/*
**  Program  : output_ext.ino
**  Version  : v0.9.5
**
**  Copyright (c) 2021-2022 Robert van den Breemen
**  Contributed by Sjorsjuhmaniac
**  Modified by Rob Roos to enable MQ autoconfigure and cleanup
**
**  TERMS OF USE: MIT License. See bottom of file.   
** 
** most code shamelessly copied from Miles Burton's - Arduino Dallas library
** example 'Multiple'   
** 
*/
// To be included in OTGW-firmware.h 
// #include <OneWire.h>
// #include <DallasTemperature.h>
// GPIO Sensor Settings
// bool      settingGPIOSENSORSenabled = false;
// int8_t    settingGPIOSENSORSpin = 10;             
// int16_t   settingGPIOSENSORSinterval = 20;       // Interval time to read out temp and send to MQ
// byte      OTGWdallasdataid = 246;                // foney dataid to be used to do autoconfigure for temp sensors
// int       DallasrealDeviceCount = 0;             // Total temperature devices found on the bus
// #define   MAXDALLASDEVICES 16                    // maximum number of devices on the bus
//
// // Define structure to store temperature device addresses found on bus with their latest tempC value
// struct
// {
//   int id;
//   DeviceAddress addr;
//   float tempC;
//   time_t lasttime;
// } DallasrealDevice[MAXDALLASDEVICES];
//
// prototype to allow use in restAPI.ino
// char* getDallasAddress(DeviceAddress deviceAddress);

// Number of temperature devices found
int numberOfDevices;

// Setup a oneWire instance to communicate with any OneWire devices
// needs a PIN to init correctly, pin is changed when we initSensors()
// this still may cause problems though because we this configures the pin already
// BUT, we're an OTGW, on a NODO print, so we are pretty sure nothing
// else is connected on these pins unlease somebody tweaked something.
// still don't like this too much:
OneWire oneWire(settingGPIOSENSORSpin);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// Initialise the oneWire bus on the GPIO pin 
void initSensors() {
  if (!settingGPIOSENSORSenabled) return;

  if (bDebugSensors) DebugTf("Init GPIO temperature sensors on GPIO[%d]\r\n", settingGPIOSENSORSpin);

  oneWire.begin(settingGPIOSENSORSpin);

  // Start the DS18B20 sensor
  sensors.begin();

  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();
  
  DallasrealDeviceCount = 0;   // To determine the total found real temp sensors

  if (numberOfDevices > MAXDALLASDEVICES) 
  {
    DebugTf("***ERR More (%d) sensor devices found than allowed(%d) on the bus\r\n", numberOfDevices, MAXDALLASDEVICES);
    numberOfDevices = MAXDALLASDEVICES ;  // limit to max number of devices
  }
  if (bDebugSensors) DebugTf("Sensors: Found %d device(s)\r\n", numberOfDevices);
  // Loop through each device, check if it is real temp sensor
  for (int i = 0; i < numberOfDevices; i++)
  {
    // Search the wire for address
    if (sensors.getAddress(DallasrealDevice[i].addr, i))
    {
      if (bDebugSensors) DebugTf("Device address %u device(s)\r\n", (unsigned int) DallasrealDevice[i].addr);
      DallasrealDevice[i].id = DallasrealDeviceCount ;
      DallasrealDevice[i].tempC = 0 ;
      DallasrealDevice[i].lasttime = 0 ;
      DallasrealDeviceCount++;
    }
    else
    {
      DebugTf("***ERR Found ghost device %d but could not detect address. Check power and cabling\r\n", i);
    }
  }

  if (numberOfDevices < 1 or DallasrealDeviceCount  < 1)
  {
    DebugTln("***ERR No Sensors Found, disabled GPIO Sensors! Reboot node to search again.");
    settingGPIOSENSORSenabled = false;
    return;
  }
}

// Send the sensor device address to MQ for Autoconfigure
void configSensors() 
{
  if (settingMQTTenable) {
      if (bDebugSensors) DebugTf("Sensor Device MQ configuration started \r\n");

      for (int i = 0; i < DallasrealDeviceCount ; i++) 
      {
        // Now configure the MQ interface, it will return immediatly when already configured
        const char * strDeviceAddress = getDallasAddress(DallasrealDevice[i].addr);
        if (bDebugSensors) DebugTf("Sensor Device MQ configuration for device no[%d] addr[%s] \r\n", i, strDeviceAddress);
        sensorAutoConfigure(OTGWdallasdataid, false, strDeviceAddress) ;     // Configure sensor with the Dallas Deviceaddress
      }
    // after last sensor set the ConfigDone flag
    setMQTTConfigDone(OTGWdallasdataid);
  }
} // configSensors()

void pollSensors()
{
  if (!settingGPIOSENSORSenabled) return;
  sensors.requestTemperatures(); // Send the command to get temperatures

  // check if HA Autoconfigure must be performed (initial or as repeat for HA reboot)
  if (settingMQTTenable && getMQTTConfigDone(OTGWdallasdataid)==false) configSensors() ;

  // Loop through each real device, store temperature data and send to MQ 
  for (int i = 0; i < DallasrealDeviceCount; i++)
  {
    // Convert device address to string
    const char * strDeviceAddress = getDallasAddress(DallasrealDevice[i].addr);
    // Store the C temp in struc to allow it to be shown on Homepage through restAPI.ino
    DallasrealDevice[i].tempC = sensors.getTempC(DallasrealDevice[i].addr);
    DallasrealDevice[i].lasttime = now() ;
    
    if (bDebugSensors) DebugTf("Sensor device no[%d] addr[%s] TempC: %f\r\n", i, strDeviceAddress, DallasrealDevice[i].tempC);

    if (settingMQTTenable ) {

      //Build string for MQTT, rse sendMQTTData for this
      // ref MQTTPubNamespace = settingMQTTtopTopic + "/value/" + strDeviceAddress ;

      char _msg[15]{0};
      char _topic[50]{0};
      
      snprintf(_topic, sizeof _topic, "%s", strDeviceAddress);
      snprintf(_msg, sizeof _msg, "%4.1f", DallasrealDevice[i].tempC);

      // DebugTf("Topic: %s -- Payload: %s\r\n", _topic, _msg);
      if (bDebugSensors) DebugFlush();

      // sendMQTTData(_topic, _msg);
      sendMQTTData(_topic, _msg);
      // Serial.println(DallasTemperature::toFahrenheit(tempC)); // Converts tempC to Fahrenheit
      }  
    // }
  }
  // DebugTln("end polling sensors");
  DebugFlush();
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
