/*
**  Program  : output_ext.ino
**  Version  : v2.0.0-beta.247
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  Contributed by Sjorsjuhmaniac
**  Modified by Rob Roos to enable MQ autoconfigure and cleanup
**
**  TERMS OF USE: GNU GPLv3. See bottom of file.   
** 
** most code shamelessly copied from Miles Burton's - Arduino Dallas library
** example 'Multiple'   
*/
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

// --- dallasLabels begin
// TASK-1912: the RAM copy of /dallas_labels.ini. GET /api/v2/sensors/labels answers from it and POST/PUT replaces it, so
// the web UI's read-merge-write (v2.js saveDallasLabel) always reads the last write: before, a POST wrote the file on
// async_tcp and a second edit could merge onto a stale read. The loop task writes the file when the copy changed (temp
// file + rename, so a power cut keeps the old file), never while flashing, and doRestart() flushes it like the
// settings. ensureSensorDefaultLabels() (loop task) works on the file under the same lock and reloads the copy.
// The mutex guards the copy; REST takes it with a short wait and answers 503 when busy (fetchWithRetry retries).
// The flush holds it only to copy the labels into s_dlFlushBuf: the flash write (up to ~1 s) runs without it, so REST
// never waits on flash. A generation counter tells the flush whether a POST arrived during the write; then the copy
// stays dirty and the next pass writes it. Every flush runs on the loop task (loopDallasLabels, doRestart), so
// s_dlFlushBuf has one writer.
// FSexplorer can upload or delete the file directly (async_tcp). It calls dallasLabelsFileChanged(), which only
// records a reload request and the generation at that moment; the loop task reads the file and takes it over unless
// a POST came after the request (the last action wins). The reload runs before the flush in the same pass, so an
// older pending edit never writes over the user's file.
static const size_t   DALLAS_LABELS_MAX = 2048;              // = the POST body limit in updateAllDallasLabels()
static const char     kDallasLabelsFile[] = "/dallas_labels.ini";
static const char     kDallasLabelsTmp[]  = "/dallas_labels.tmp";
static char           s_dlBuf[DALLAS_LABELS_MAX + 1] = "{}";
static char           s_dlFlushBuf[DALLAS_LABELS_MAX + 1];   // snapshot the flush writes outside the lock
static uint32_t       s_dlGen = 0;                           // counts replacements of the copy
static bool           s_dlReloadReq = false;                 // FSexplorer changed the file: reload on the loop task
static uint32_t       s_dlReloadGen = 0;                     // s_dlGen when that request was made
static bool           s_dlDirty = false;                     // the copy differs from the file
static uint32_t       s_dlRetryMs = 0;                       // a failed write waits before the next try
static PlatformMutex  s_dlMutex = nullptr;

bool dallasLabelsLock(uint32_t waitMs) { return platformMutexLock(s_dlMutex, waitMs); }
void dallasLabelsUnlock() { platformMutexUnlock(s_dlMutex); }
const char* dallasLabelsJson() { return s_dlBuf; }           // caller holds the lock

// Caller holds the lock and has validated json as a flat object of at most DALLAS_LABELS_MAX bytes.
void dallasLabelsReplaceLocked(const char* json, size_t len) {
  if (len > DALLAS_LABELS_MAX) len = DALLAS_LABELS_MAX;
  memcpy(s_dlBuf, json, len);
  s_dlBuf[len] = '\0';
  s_dlGen++;
  s_dlDirty = true;
  s_dlRetryMs = 0;
}

// Read /dallas_labels.ini into buf (DALLAS_LABELS_MAX + 1 bytes); "{}" when it is missing, empty, too large or not
// an object.
static void readDallasLabelsFile(char* buf) {
  strlcpy(buf, "{}", DALLAS_LABELS_MAX + 1);
  if (!LittleFSmounted) return;
  File f = LittleFS.open(kDallasLabelsFile, "r");
  if (!f) return;
  const size_t size = f.size();
  if (size == 0 || size > DALLAS_LABELS_MAX) {
    DebugTf(PSTR("Dallas labels: %s has %u bytes, not loaded (max %u)\r\n"), kDallasLabelsFile, (unsigned)size,
            (unsigned)DALLAS_LABELS_MAX);
    f.close();
    return;
  }
  const size_t n = f.read(reinterpret_cast<uint8_t*>(buf), DALLAS_LABELS_MAX);
  f.close();
  buf[n] = '\0';
  if (n != size || buf[0] != '{' || buf[n - 1] != '}') strlcpy(buf, "{}", DALLAS_LABELS_MAX + 1);
}

static void loadDallasLabelsLocked() {
  readDallasLabelsFile(s_dlBuf);
  s_dlDirty = false;
}

// Setup, loop task: read the file once, before the web server starts.
void loadDallasLabels() {
  if (s_dlMutex == nullptr) s_dlMutex = platformMutexCreate();
  platformMutexLock(s_dlMutex);
  loadDallasLabelsLocked();
  platformMutexUnlock(s_dlMutex);
}

// Write json to /dallas_labels.ini through the temp file; fsCommitTempFile() (TASK-1913) checks the size and renames,
// and on a failure removes the temp file, so the old file stays.
static bool writeDallasLabelsFile(const char* json, size_t len) {
  if (!LittleFSmounted) return false;
  File f = LittleFS.open(kDallasLabelsTmp, "w");
  if (f) {
    f.write(reinterpret_cast<const uint8_t*>(json), len);
    f.close();
  }
  const bool ok = fsCommitTempFile(kDallasLabelsTmp, kDallasLabelsFile, len);
  if (!ok) DebugTln(F("Dallas labels: write failed, the old file stays; retry in 30 s"));
  return ok;
}

// Caller holds the lock (ensureSensorDefaultLabels only).
static bool flushDallasLabelsLocked() {
  if (!s_dlDirty) return true;
  if (!writeDallasLabelsFile(s_dlBuf, strlen(s_dlBuf))) return false;
  s_dlDirty = false;
  return true;
}

// Loop task (and doRestart()): write the copy when it changed. The lock is held for the snapshot and the bookkeeping,
// not across the flash write.
void flushDallasLabels() {
  platformMutexLock(s_dlMutex);
  if (!s_dlDirty) { platformMutexUnlock(s_dlMutex); return; }
  const size_t len = strlen(s_dlBuf);
  memcpy(s_dlFlushBuf, s_dlBuf, len + 1);
  const uint32_t gen = s_dlGen;
  platformMutexUnlock(s_dlMutex);

  const bool ok = writeDallasLabelsFile(s_dlFlushBuf, len);

  platformMutexLock(s_dlMutex);
  if (!ok) s_dlRetryMs = millis();
  else if (s_dlGen == gen) s_dlDirty = false;   // a POST during the write keeps the copy dirty for the next pass
  platformMutexUnlock(s_dlMutex);
}

// Any task (FSexplorer runs on async_tcp): path was uploaded or deleted. Only the labels file asks for a reload.
void dallasLabelsFileChanged(const char* path) {
  if (path == nullptr) return;
  while (*path == '/') path++;
  if (strcmp(path, kDallasLabelsFile + 1) != 0) return;
  __atomic_store_n(&s_dlReloadGen, __atomic_load_n(&s_dlGen, __ATOMIC_ACQUIRE), __ATOMIC_RELAXED);
  __atomic_store_n(&s_dlReloadReq, true, __ATOMIC_RELEASE);
}

// Loop task: take the file over, read outside the lock, unless a POST came after the request.
static void reloadDallasLabels() {
  const uint32_t gen = __atomic_load_n(&s_dlReloadGen, __ATOMIC_RELAXED);
  readDallasLabelsFile(s_dlFlushBuf);
  platformMutexLock(s_dlMutex);
  if (s_dlGen == gen) {
    memcpy(s_dlBuf, s_dlFlushBuf, strlen(s_dlFlushBuf) + 1);
    s_dlDirty = false;
    s_dlRetryMs = 0;
  }
  platformMutexUnlock(s_dlMutex);
}

// Loop task, from loopLittleFSStatus() (so never while flashing).
void loopDallasLabels() {
  if (__atomic_exchange_n(&s_dlReloadReq, false, __ATOMIC_ACQ_REL)) reloadDallasLabels();
  if (!__atomic_load_n(&s_dlDirty, __ATOMIC_ACQUIRE)) return;   // a hint only; the flush checks again under the lock
  if (s_dlRetryMs != 0 && (uint32_t)(millis() - s_dlRetryMs) < 30000UL) return;
  flushDallasLabels();
}
// --- dallasLabels end

// Ensure all discovered sensors have default labels in /dallas_labels.ini
// Works for both real and simulated sensors. TASK-1912: runs on the file under the labels lock (below).
static void ensureSensorDefaultLabelsFile()
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

// TASK-1912: the default labels go into the file and the RAM copy together. A pending REST edit is written first, so
// the file is current; when that write fails the defaults wait for the next call, and the edit is kept.
void ensureSensorDefaultLabels()
{
  platformMutexLock(s_dlMutex);
  if (flushDallasLabelsLocked()) {
    ensureSensorDefaultLabelsFile();
    loadDallasLabelsLocked();
  }
  platformMutexUnlock(s_dlMutex);
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

  if (isBoardReservedPin(settings.sensors.iPin)) {   // TASK-1552: never put the 1-Wire bus on a board pin
    DebugTf(PSTR("Dallas sensors: GPIO%d is used by the board, choose another pin; sensors not started\r\n"), settings.sensors.iPin);
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

// begin dallas-read record (sliced by tests/test_dallas_first_reading.py)
// TASK-1824: with setWaitForConversion(false), requestTemperatures() only starts a conversion and getTempC()
// reads the scratchpad, which holds the result of the PREVIOUS conversion. Until a first conversion has
// completed it holds the DS18B20 power-on value 85.0, and a sensor that browns out returns to it.
static bool dallasConversionRequested = false;   // a conversion was started on an earlier poll

#define DALLAS_POWER_ON_C       85.0f   // DS18B20 scratchpad value after power-on (0x0550)
#define DALLAS_POWER_ON_JUMP_C  10.0f   // an exact 85.0 this far from the previous value is a reset, not a reading

// true when tempC is a measurement to store and publish. havePrev: this sensor already has a valid value.
static bool dallasReadingValid(float tempC, bool havePrev, float prevC)
{
  if (tempC == DEVICE_DISCONNECTED_C) return false;
  if (tempC == DALLAS_POWER_ON_C && (!havePrev || fabsf(tempC - prevC) > DALLAS_POWER_ON_JUMP_C)) return false;
  return true;
}
// end dallas-read record

 void pollSensors()
 {
  time_t now = time(nullptr);
  if (!bSensorsDetected) return;  // Guard on runtime detection state, not persisted setting

  // TASK-1824: read only after a conversion was started on an earlier poll; the first poll after boot only starts one.
  const bool conversionDone = dallasConversionRequested;
  if (!state.debug.bSensorSim)
  {
    sensors.requestTemperatures(); // Non-blocking: setWaitForConversion(false) in initSensors()
    dallasConversionRequested = true;
  }
  bool simUpdateDue = true;
  if (state.debug.bSensorSim && simLastUpdateTime != 0 && (now - simLastUpdateTime) < (time_t)SIM_SENSOR_UPDATE_INTERVAL_SECONDS)
  {
    simUpdateDue = false;
  }

  // Queue sensor discovery if not yet published; drainOnePendingDiscovery() handles it.
  if (settings.mqtt.bEnable && !getMQTTConfigDone(OTGWdallasdataid)) {
    setMQTTConfigPending(OTGWdallasdataid);
  }
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
      if (!conversionDone) continue;   // TASK-1824: no completed conversion yet, the scratchpad still says 85.0
      float tempC = sensors.getTempC(DallasrealDevice[i].addr);
      if (!dallasReadingValid(tempC, DallasrealDevice[i].lasttime != 0, DallasrealDevice[i].tempC)) {
        // Disconnected or read error (Finding #29), or the 85.0 power-on value (TASK-1824): keep the previous value
        if (state.debug.bSensors) DebugTf(PSTR("Sensor [%s] no valid reading (%.1f), skipping\r\n"), strDeviceAddress, tempC);
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

    // TASK-587: Auto-route sensor temperature to SAT area if address is mapped
    for (int areaIdx = 0; areaIdx < 4; areaIdx++) {
      if (settings.sat.sSensorArea[areaIdx][0] != '\0' &&
          strcasecmp(strDeviceAddress, settings.sat.sSensorArea[areaIdx]) == 0) {
        satSetAreaTemp(areaIdx, DallasrealDevice[i].tempC);
        if (state.debug.bSensors) {
          DebugTf(PSTR("Sensor [%s] auto-routed to SAT area %d: %4.1f\r\n"),
                  strDeviceAddress, areaIdx, DallasrealDevice[i].tempC);
        }
      }
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
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
* 
****************************************************************************
*/
