/*
**  Program  : output_ext.ino
**  Version  : v1.3.0-beta
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
// bool      settings.sensors.bEnabled = false;
// int8_t    settings.sensors.iPin = 10;             
// int16_t   settings.sensors.iInterval = 20;       // Interval time to read out temp and send to MQ
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

// Ensure all discovered sensors have default labels in /dallas_labels.ini
// Works for both real and simulated sensors
void ensureSensorDefaultLabels()
{
  if (DallasrealDeviceCount < 1) return;

  // ── Pass 1: read existing key→label pairs from file via readJsonStringPair() ──
  struct { char addr[17]; char label[24]; } existing[MAXDALLASDEVICES];
  int existingCount = 0;

  File labelsFile = LittleFS.open(F("/dallas_labels.ini"), "r");
  if (labelsFile)
  {
    char key[17], val[24];
    while (existingCount < MAXDALLASDEVICES &&
           readJsonStringPair(labelsFile, key, sizeof(key), val, sizeof(val)))
    {
      strlcpy(existing[existingCount].addr,  key, sizeof(existing[0].addr));
      strlcpy(existing[existingCount].label, val, sizeof(existing[0].label));
      existingCount++;
    }
    labelsFile.close();
  }

  // ── Check whether any currently-found sensors are missing a label ──
  bool changed = false;
  for (int i = 0; i < DallasrealDeviceCount; i++)
  {
    const char* addr = getDallasAddress(DallasrealDevice[i].addr);
    bool found = false;
    for (int j = 0; j < existingCount; j++) {
      if (strcmp(existing[j].addr, addr) == 0) { found = true; break; }
    }
    if (!found) { changed = true; break; }
  }
  if (!changed) return;

  // ── Pass 2: rebuild the file ──
  // Write all existing labels back first, then append defaults for new sensors.
  File outFile = LittleFS.open(F("/dallas_labels.ini"), "w");
  if (!outFile) return;

  const char* prefix = state.debug.bSensorSim ? "Sim Sensor" : "Sensor";
  outFile.print('{');
  bool first = true;

  // Preserve existing labels
  for (int j = 0; j < existingCount; j++)
  {
    writeJsonStringPair(outFile, existing[j].addr, existing[j].label, !first);
    first = false;
  }

  // Append defaults for sensors not yet in file
  for (int i = 0; i < DallasrealDeviceCount; i++)
  {
    const char* addr = getDallasAddress(DallasrealDevice[i].addr);
    bool found = false;
    for (int j = 0; j < existingCount; j++) {
      if (strcmp(existing[j].addr, addr) == 0) { found = true; break; }
    }
    if (found) continue;

    char label[24];
    snprintf_P(label, sizeof(label), PSTR("%s %d"), prefix, i + 1);
    DebugTf(PSTR("Created default label '%s' for sensor %s\r\n"), label, addr);
    writeJsonStringPair(outFile, addr, label, !first);
    first = false;
  }

  outFile.print('}');
  outFile.close();
  DebugTf(PSTR("Saved %d sensor label(s) to dallas_labels.ini\r\n"), DallasrealDeviceCount);
}

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
  }

  if (state.debug.bSensors)
  {
    DebugTf(PSTR("Sensor simulation enabled: %d virtual sensors initialized\r\n"), SIM_SENSOR_COUNT);
  }
}

// Use default constructor (no pin) so no GPIO is touched before settings load.
// oneWire.begin(settingGPIOSENSORSpin) is called inside initSensors() after
// readSettings() has loaded the correct pin value. (ADR-020)
OneWire oneWire;

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// Initialise the oneWire bus on the GPIO pin 
void initSensors() {
  bSensorsDetected = false;  // Reset runtime detection state on each init call
  if (!settings.sensors.bEnabled && !state.debug.bSensorSim)
  {
    DallasrealDeviceCount = 0;
    numberOfDevices = 0;
    return;
  }

  if (state.debug.bSensorSim)
  {
    initSimulatedDallasSensors();
    ensureSensorDefaultLabels();
    bSensorsDetected = true;  // Simulation counts as successfully initialized
    return;
  }

  if (state.debug.bSensors)DebugTf(PSTR("init GPIO Temperature sensors on GPIO%d...\r\n"), settings.sensors.iPin);

  oneWire.begin(settings.sensors.iPin);

  // Start the DS18B20 sensor
  sensors.begin();
  sensors.setWaitForConversion(false);  // Use async mode to avoid blocking main loop for ~750ms

  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();

  DallasrealDeviceCount = 0;   // To determine the total found real temp sensors

  if (numberOfDevices > MAXDALLASDEVICES) 
  {
    DebugTf(PSTR("***ERR More (%d) sensor devices found than allowed(%d) on the bus\r\n"), numberOfDevices, MAXDALLASDEVICES);
    numberOfDevices = MAXDALLASDEVICES ;  // limit to max number of devices
  }
  if (state.debug.bSensors) DebugTf(PSTR("Sensors: Found %d device(s)\r\n"), numberOfDevices);
   // Loop through each device, check if it is real temp sensor

  for (int i = 0; i < numberOfDevices; i++)
  {
    // Search the wire for address
    if (sensors.getAddress(DallasrealDevice[i].addr, i))
    {
    if (state.debug.bSensors) DebugTf(PSTR("Device address %u device(s)\r\n"), (unsigned int) DallasrealDevice[i].addr);
    DallasrealDevice[i].id = DallasrealDeviceCount ;
    DallasrealDevice[i].tempC = 0 ;
    DallasrealDevice[i].lasttime = 0 ;
    
    // Labels are now managed by Web UI via /dallas_labels.json file
    // No label storage in backend memory
    
    DallasrealDeviceCount++;
    }
    else
    {
      DebugTf(PSTR("***ERR Found ghost device %d but could not detect address. Check power and cabling\r\n"), i);
    }
  }

  if (numberOfDevices < 1 or DallasrealDeviceCount < 1)
  {
    DebugTf(PSTR("***ERR No Sensors Found on GPIO%d. Check wiring and reboot to search again.\r\n"), settings.sensors.iPin);
    return;  // bSensorsDetected stays false
  }

  // Create default labels for discovered sensors if they don't exist yet
  ensureSensorDefaultLabels();
  bSensorsDetected = true;  // Sensors successfully detected and initialized
}

// Send the sensor device address to MQ for Autoconfigure
void configSensors() 
{
if (settings.mqtt.bEnable) {
    if (state.debug.bSensors) DebugTf(PSTR("Sensor Device MQ configuration started \r\n"));

    for (int i = 0; i < DallasrealDeviceCount ; i++) 
    {
      // Now configure the MQ interface, it will return immediatly when already configured
      const char * strDeviceAddress = getDallasAddress(DallasrealDevice[i].addr);
      if (state.debug.bSensors) DebugTf(PSTR("Sensor Device MQ configuration for device no[%d] addr[%s] \r\n"), i, strDeviceAddress);
      sensorAutoConfigure(OTGWdallasdataid, false, strDeviceAddress) ;     // Configure sensor with the Dallas Deviceaddress
    }
    // after last sensor set the ConfigDone flag
    setMQTTConfigDone(OTGWdallasdataid);
  }
} // configSensors()

 void pollSensors()
 {
  time_t now = time(nullptr);
  if (!bSensorsDetected) return;  // Guard on runtime detection state, not persisted setting

  if (!state.debug.bSensorSim)
  {
    sensors.requestTemperatures(); // Non-blocking: setWaitForConversion(false) in initSensors()
  }
  bool simUpdateDue = true;
  if (state.debug.bSensorSim && simLastUpdateTime != 0 && (now - simLastUpdateTime) < SIM_SENSOR_UPDATE_INTERVAL_SECONDS)
  {
    simUpdateDue = false;
  }

  // check if HA Autoconfigure must be performed (initial or as repeat for HA reboot)
  if (settings.mqtt.bEnable && getMQTTConfigDone(OTGWdallasdataid)==false) configSensors() ;
  // Loop through each real device, store temperature data and send to MQ 
  for (int i = 0; i < DallasrealDeviceCount; i++)
  {
    // Convert device address to string
    const char * strDeviceAddress = getDallasAddress(DallasrealDevice[i].addr);
    // Store the C temp in struc to allow it to be shown on Homepage through restAPI.ino
    if (state.debug.bSensorSim)
    {
      if (simUpdateDue)
      {
        float delta = (random(0, 21) - 10) / 20.0f; // -0.5 to +0.5
        float nextTemp = DallasrealDevice[i].tempC + delta;
        if (nextTemp < SIM_SENSOR_MIN) nextTemp = SIM_SENSOR_MIN;
        if (nextTemp > SIM_SENSOR_MAX) nextTemp = SIM_SENSOR_MAX;
        DallasrealDevice[i].tempC = nextTemp;
      }
    }
    else
    {
      float tempC = sensors.getTempC(DallasrealDevice[i].addr);
      if (tempC == DEVICE_DISCONNECTED_C) {
        // Sensor disconnected or read error — skip, keep previous value (Finding #29)
        if (state.debug.bSensors) DebugTf(PSTR("Sensor [%s] disconnected or read error, skipping\r\n"), strDeviceAddress);
        continue;
      }
      DallasrealDevice[i].tempC = tempC;
    }
    DallasrealDevice[i].lasttime = now ;
    
    // Debug logging: always show simulated values in telnet for visibility
    if (state.debug.bSensorSim)
    {
      if (simUpdateDue)
      {
        DebugTf(PSTR("[SIM] Sensor device no[%d] addr[%s] TempC: %4.1f\r\n"), i, strDeviceAddress, DallasrealDevice[i].tempC);
      }
    }
    else if (state.debug.bSensors)
    {
      DebugTf(PSTR("Sensor device no[%d] addr[%s] TempC: %f\r\n"), i, strDeviceAddress, DallasrealDevice[i].tempC);
    }

    if (settings.mqtt.bEnable ) {
      //Build string for MQTT, use sendMQTTData for this
      // ref MQTTPubNamespace = settings.mqtt.sTopTopic + "/value/" + strDeviceAddress ;
      char _msg[15]{0};
      // strDeviceAddress is already a const char* from getDallasAddress()
      // Just format the temperature value
      snprintf_P(_msg, sizeof _msg, PSTR("%4.1f"), DallasrealDevice[i].tempC);

      // DebugTf(PSTR("Topic: %s -- Payload: %s\r\n"), strDeviceAddress, _msg);
      if (state.debug.bSensors) DebugFlush();
      sendMQTTData(strDeviceAddress, _msg);
    }
  }
  if (state.debug.bSensorSim)
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
  
  if (settings.sensors.bLegacyFormat) {
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
