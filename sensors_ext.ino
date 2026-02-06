/*
**  Program  : output_ext.ino
**  Version  : v1.0.0-rc6
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  Contributed by Sjorsjuhmaniac
**  Modified by Rob Roos to enable MQ autoconfigure and cleanup
**
**  TERMS OF USE: MIT License. See bottom of file.   
** 
** most code shamelessly copied from Miles Burton's - Arduino Dallas library
** example 'Multiple'   
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

const float SIM_SENSOR_MIN = 20.0f;
const float SIM_SENSOR_MAX = 60.0f;
const int SIM_SENSOR_COUNT = 3;
const uint32_t SIM_SENSOR_UPDATE_INTERVAL_SECONDS = 10;

static time_t simLastUpdateTime = 0;

const uint8_t DallasSimDeviceAddresses[SIM_SENSOR_COUNT][8] = {
  {0x28, 0xD0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
  {0x28, 0xD0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02},
  {0x28, 0xD0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03}
};

void initSimulatedDallasSensors()
{
  DallasrealDeviceCount = SIM_SENSOR_COUNT;
  numberOfDevices = SIM_SENSOR_COUNT;
  simLastUpdateTime = 0;

  for (int i = 0; i < SIM_SENSOR_COUNT; i++)
  {
    DallasrealDevice[i].id = i;
    memcpy(DallasrealDevice[i].addr, DallasSimDeviceAddresses[i], sizeof(DeviceAddress));
    DallasrealDevice[i].tempC = 30.0f + (i * 5.0f);
    DallasrealDevice[i].lasttime = 0;

    const char* hexAddr = getDallasAddress(DallasrealDevice[i].addr);
    strlcpy(DallasrealDevice[i].label, hexAddr, sizeof(DallasrealDevice[i].label));
    loadSensorLabel(hexAddr, DallasrealDevice[i].label, sizeof(DallasrealDevice[i].label));

  }

  if (bDebugSensors)
  {
    DebugTf(PSTR("Sensor simulation enabled: %d virtual sensors initialized\r\n"), SIM_SENSOR_COUNT);
  }
}

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
  if (!settingGPIOSENSORSenabled && !bDebugSensorSimulation)
  {
    DallasrealDeviceCount = 0;
    numberOfDevices = 0;
    return;
  }

  if (bDebugSensorSimulation)
  {
    initSimulatedDallasSensors();
    return;
  }

  if (bDebugSensors)DebugTf(PSTR("init GPIO Temperature sensors on GPIO%d...\r\n"), settingGPIOSENSORSpin);

  oneWire.begin(settingGPIOSENSORSpin);

  // Start the DS18B20 sensor
  sensors.begin();

  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();

  DallasrealDeviceCount = 0;   // To determine the total found real temp sensors

  if (numberOfDevices > MAXDALLASDEVICES) 
  {
    DebugTf(PSTR("***ERR More (%d) sensor devices found than allowed(%d) on the bus\r\n"), numberOfDevices, MAXDALLASDEVICES);
    numberOfDevices = MAXDALLASDEVICES ;  // limit to max number of devices
  }
  if (bDebugSensors) DebugTf(PSTR("Sensors: Found %d device(s)\r\n"), numberOfDevices);
   // Loop through each device, check if it is real temp sensor

  for (int i = 0; i < numberOfDevices; i++)
  {
    // Search the wire for address
    if (sensors.getAddress(DallasrealDevice[i].addr, i))
    {
    if (bDebugSensors) DebugTf(PSTR("Device address %u device(s)\r\n"), (unsigned int) DallasrealDevice[i].addr);
    DallasrealDevice[i].id = DallasrealDeviceCount ;
    DallasrealDevice[i].tempC = 0 ;
    DallasrealDevice[i].lasttime = 0 ;
    
    // Initialize label with hex address (can be changed by user)
    const char* hexAddr = getDallasAddress(DallasrealDevice[i].addr);
    strlcpy(DallasrealDevice[i].label, hexAddr, sizeof(DallasrealDevice[i].label));
    
    // Try to load custom label from settings
    loadSensorLabel(hexAddr, DallasrealDevice[i].label, sizeof(DallasrealDevice[i].label));
    
    DallasrealDeviceCount++;
    }
    else
    {
      DebugTf(PSTR("***ERR Found ghost device %d but could not detect address. Check power and cabling\r\n"), i);
    }
  }

  if (numberOfDevices < 1 or DallasrealDeviceCount < 1)
  {
    DebugTln(F("***ERR No Sensors Found, disabled GPIO Sensors! Reboot node to search again."));
    settingGPIOSENSORSenabled = false;
    return;
  }
}

// Send the sensor device address to MQ for Autoconfigure
void configSensors() 
{
if (settingMQTTenable) {
    if (bDebugSensors) DebugTf(PSTR("Sensor Device MQ configuration started \r\n"));

    for (int i = 0; i < DallasrealDeviceCount ; i++) 
    {
      // Now configure the MQ interface, it will return immediatly when already configured
      const char * strDeviceAddress = getDallasAddress(DallasrealDevice[i].addr);
      if (bDebugSensors) DebugTf(PSTR("Sensor Device MQ configuration for device no[%d] addr[%s] \r\n"), i, strDeviceAddress);
      sensorAutoConfigure(OTGWdallasdataid, false, strDeviceAddress) ;     // Configure sensor with the Dallas Deviceaddress
    }
    // after last sensor set the ConfigDone flag
    setMQTTConfigDone(OTGWdallasdataid);
  }
} // configSensors()

 void pollSensors()
 {
  time_t now = time(nullptr);
  if (!settingGPIOSENSORSenabled && !bDebugSensorSimulation) return;

  if (!bDebugSensorSimulation)
  {
    sensors.requestTemperatures(); // Send the command to get temperatures
  }
  else if (simLastUpdateTime != 0 && (now - simLastUpdateTime) < SIM_SENSOR_UPDATE_INTERVAL_SECONDS)
  {
    return;
  }

  // check if HA Autoconfigure must be performed (initial or as repeat for HA reboot)
  if (settingMQTTenable && getMQTTConfigDone(OTGWdallasdataid)==false) configSensors() ;
  // Loop through each real device, store temperature data and send to MQ 
  for (int i = 0; i < DallasrealDeviceCount; i++)
  {
    // Convert device address to string
    const char * strDeviceAddress = getDallasAddress(DallasrealDevice[i].addr);
    // Store the C temp in struc to allow it to be shown on Homepage through restAPI.ino
    if (bDebugSensorSimulation)
    {
      float delta = (random(0, 21) - 10) / 20.0f; // -0.5 to +0.5
      float nextTemp = DallasrealDevice[i].tempC + delta;
      if (nextTemp < SIM_SENSOR_MIN) nextTemp = SIM_SENSOR_MIN;
      if (nextTemp > SIM_SENSOR_MAX) nextTemp = SIM_SENSOR_MAX;
      DallasrealDevice[i].tempC = nextTemp;
    }
    else
    {
      DallasrealDevice[i].tempC = sensors.getTempC(DallasrealDevice[i].addr);
    }
    DallasrealDevice[i].lasttime = now ;
    
    if (bDebugSensorSimulation)
    {
      DebugTf(PSTR("[SIM] Sensor device no[%d] addr[%s] TempC: %4.1f\r\n"), i, strDeviceAddress, DallasrealDevice[i].tempC);
    }
    else if (bDebugSensors)
    {
      DebugTf(PSTR("Sensor device no[%d] addr[%s] TempC: %f\r\n"), i, strDeviceAddress, DallasrealDevice[i].tempC);
    }

    if (settingMQTTenable ) {
      //Build string for MQTT, use sendMQTTData for this
      // ref MQTTPubNamespace = settingMQTTtopTopic + "/value/" + strDeviceAddress ;
      char _msg[15]{0};
      // strDeviceAddress is already a const char* from getDallasAddress()
      // Just format the temperature value
      snprintf_P(_msg, sizeof _msg, PSTR("%4.1f"), DallasrealDevice[i].tempC);

      // DebugTf(PSTR("Topic: %s -- Payload: %s\r\n"), strDeviceAddress, _msg);
      if (bDebugSensors) DebugFlush();
      sendMQTTData(strDeviceAddress, _msg);

      // Serial.println(DallasTemperature::toFahrenheit(tempC)); // Converts tempC to Fahrenheit
    }
  }
  if (bDebugSensorSimulation)
  {
    simLastUpdateTime = now;
  }

  // DebugTln(F("end polling sensors"));
  DebugFlush();
}

// function to print a device address
char* getDallasAddress(DeviceAddress deviceAddress)
{
  static char dest[17]; // 8 bytes * 2 chars + 1 null
  static const char hexchars[] PROGMEM = "0123456789ABCDEF";
  
  if (settingGPIOSENSORSlegacyformat) {
    // Replicate the "buggy" behavior of previous versions (~v0.10.x)
    // which produced a compressed/corrupted ID (approx 9-10 chars)
    // This provides backward compatibility for Home Assistant automations.
    memset(dest, 0, sizeof(dest));
    
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t val = deviceAddress[i];
        if (val < 16) {
           strlcat(dest, "0", sizeof(dest));
        }
        // Emulate: s-printf(dest+i, "%X", val);
        char hexBuffer[4];
        snprintf_P(hexBuffer, sizeof(hexBuffer), PSTR("%X"), val);
        size_t len = strlen(hexBuffer);
        
        // Write at offset i, overwriting existing chars
        for(size_t k=0; k<len; k++) {
            if (i+k < sizeof(dest)-1) {
                dest[i+k] = hexBuffer[k];
            }
        }
        // Ensure null termination at the end of what we just wrote
        if (i+len < sizeof(dest)) {
            dest[i+len] = '\0';
        }
    }
    return dest;
  }

  // Standard Correct Format (16 hex chars)
  for (uint8_t i = 0; i < 8; i++)
  {
    uint8_t b = deviceAddress[i];
    dest[i*2]   = pgm_read_byte(&hexchars[b >> 4]);
    dest[i*2+1] = pgm_read_byte(&hexchars[b & 0x0F]);
  }
  dest[16] = '\0';
  return dest;
}

// Load custom label for a sensor from settings
void loadSensorLabel(const char* hexAddress, char* label, size_t labelSize) {
  if (!hexAddress || !label || labelSize == 0) return;
  
  // Parse settingDallasLabels JSON to find this address
  if (strlen(settingDallasLabels) > 0) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, settingDallasLabels);
    
    if (!error && doc.containsKey(hexAddress)) {
      const char* customLabel = doc[hexAddress];
      if (customLabel && strlen(customLabel) > 0) {
        strlcpy(label, customLabel, labelSize);
        return;
      }
    }
  }
  
  // If no custom label found, label already contains hex address (default)
}

// Save custom label for a sensor to settings
void saveSensorLabel(const char* hexAddress, const char* newLabel) {
  if (!hexAddress || !newLabel) return;
  
  // Parse existing labels
  StaticJsonDocument<512> doc;
  if (strlen(settingDallasLabels) > 0) {
    deserializeJson(doc, settingDallasLabels);
  }
  
  // Update or add the label
  doc[hexAddress] = newLabel;
  
  // Serialize back to string
  serializeJson(doc, settingDallasLabels, sizeof(settingDallasLabels));
  
  // Update the sensor structure if it's currently loaded
  for (int i = 0; i < DallasrealDeviceCount; i++) {
    const char* addr = getDallasAddress(DallasrealDevice[i].addr);
    if (strcmp(addr, hexAddress) == 0) {
      strlcpy(DallasrealDevice[i].label, newLabel, sizeof(DallasrealDevice[i].label));
      break;
    }
  }
  
  // Save settings to file
  writeSettings(false);
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
