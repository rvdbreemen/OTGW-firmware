/*
***************************************************************************
**  Module   : SATble.ino
**  Description: BLE temperature sensor support for SAT (ESP32 only)
**
**  Part of SAT (Smart Autotune Thermostat) firmware port.
**  Original SAT component by Alex Wijnholds (https://github.com/Alexwijn/SAT)
**  SAT concept and algorithm design by George Dellas
**
**  Scans for BLE advertisements from common temperature/humidity sensors
**  (Xiaomi LYWSD03MMC with ATC/pvvx custom firmware, BTHome v2 protocol).
**  Provides room temperature input for SAT control loop.
**
**  TASK-487 / ADR-092: built on NimBLE-Arduino 2.x rather than the classic
**  Bluedroid stack. Async-by-default scanning means loop() is never blocked;
**  std::string service-data avoids the Arduino-String heap churn that the
**  hot-path scan callback used to cause (ADR-004).
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License. See bottom of OTGW-firmware.h
***************************************************************************
*/

#if defined(ESP32)

#include <NimBLEDevice.h>

// --- Switchable debug-trace macros (telnet key '7' toggles state.debug.bSATBLE) ---
// Init meldingen blijven ongewrapt zodat boot-time visibility behouden blijft.
#define SATBLEDebugTln(s)        do { if (state.debug.bSATBLE) DebugTln(s); } while(0)
#define SATBLEDebugTf(fmt, ...)  do { if (state.debug.bSATBLE) DebugTf(fmt, ##__VA_ARGS__); } while(0)
#define SATBLEDebugln(s)         do { if (state.debug.bSATBLE) Debugln(s); } while(0)
#define SATBLEDebugf(fmt, ...)   do { if (state.debug.bSATBLE) Debugf(fmt, ##__VA_ARGS__); } while(0)

// --- BLE Sensor Constants ---
#define SAT_BLE_MAX_SENSORS   4
static const uint32_t BLE_STALE_MS          = 300000;   // 5 min stale timeout

// ATC/pvvx custom firmware service data UUID: 0x181A (Environmental Sensing)
static const uint16_t ATC_SERVICE_UUID_16    = 0x181A;
// BTHome v2 service data UUID: 0xFCD2
static const uint16_t BTHOME_SERVICE_UUID_16 = 0xFCD2;

// BTHome v2 device-info / flag byte
//   bit6 = version 2 (must be 1)
//   bit0 = encryption (1 = encrypted, we skip those)
static const uint8_t BTHOME_V2_FLAG_VERSION_BIT = 0x40;
static const uint8_t BTHOME_V2_FLAG_ENCRYPTED   = 0x01;

// BTHome v2 object IDs
static const uint8_t BTHOME_OBJ_TEMPERATURE_S16 = 0x02;  // sint16, factor 0.01
static const uint8_t BTHOME_OBJ_HUMIDITY_U16    = 0x03;  // uint16, factor 0.01
static const uint8_t BTHOME_OBJ_BATTERY_U8      = 0x01;  // uint8, factor 1

struct BLESensorData {
  char     sMacAddress[18];     // "AA:BB:CC:DD:EE:FF"
  float    fTemperature;        // 2 decimal precision
  float    fHumidity;
  uint8_t  iBattery;
  int8_t   iRssi;
  bool     bValid;
  uint32_t iLastSeenMs;
  bool     bDiscoveryPublished; // TASK-488: HA-discovery sent at least once for this MAC
};

static BLESensorData _bleSensors[SAT_BLE_MAX_SENSORS];
static NimBLEScan*   _pBLEScan       = nullptr;
static uint32_t      _bleLastPublishMs = 0;
static bool          _bleInitialized = false;

// TASK-497 (cross-phase): NimBLE 2.x scan callback runs on a separate
// FreeRTOS task on ESP32-S3 (the BLE host task on core 0) while the
// Arduino loop task (core 1) reads _bleSensors[] in satBLEPublishMQTT()
// and satBLEUpdateState(). Without synchronisation, multi-byte fields
// (iLastSeenMs, fTemperature) can tear under concurrent access.
//
// Use portMUX to serialise: scan-callback writes hold the lock during the
// slot update; loop-task readers take a quick snapshot under the lock,
// then process the snapshot outside. Critical sections stay short (one
// struct copy or one slot-update block) to avoid blocking the BLE radio.
static portMUX_TYPE _bleSensorsMux = portMUX_INITIALIZER_UNLOCKED;

// --- Forward declarations ---
static bool parseBLEAtcFormat(const uint8_t* data, size_t len, float* temp, float* hum, uint8_t* batt);
static bool parseBLEBTHomeFormat(const uint8_t* data, size_t len, float* temp, float* hum, uint8_t* batt);
static int  bleFindOrAllocSlot(const char* mac);
static bool bleMatchesConfiguredMAC(const char* mac);

//=====================================================================
// Parse ATC/pvvx custom firmware format from service data UUID 0x181A
// Format: MAC(6) + temp_s16(2) + hum_u16(2) + batt_pct(1) + batt_mv(2) + counter(1)
// Total: 14 bytes (some variants 13)
//=====================================================================
static bool parseBLEAtcFormat(const uint8_t* data, size_t len, float* temp, float* hum, uint8_t* batt)
{
  if (len < 13) return false;

  // Temperature: int16_t at offset 6, little-endian, / 100.0
  int16_t rawTemp = (int16_t)(data[6] | (data[7] << 8));
  *temp = rawTemp / 100.0f;

  // Humidity: uint16_t at offset 8, little-endian, / 100.0
  uint16_t rawHum = (uint16_t)(data[8] | (data[9] << 8));
  *hum = rawHum / 100.0f;

  // Battery: uint8_t at offset 10
  *batt = data[10];

  // Sanity checks
  if (*temp < -40.0f || *temp > 60.0f) return false;
  if (*hum < 0.0f || *hum > 100.0f) return false;

  return true;
}

//=====================================================================
// Parse BTHome v2 format from service data UUID 0xFCD2
// Format: flags(1) + [object_id(1) + value(variable)]+
// We look for temperature (0x02) and humidity (0x03) objects.
//=====================================================================
static bool parseBLEBTHomeFormat(const uint8_t* data, size_t len, float* temp, float* hum, uint8_t* batt)
{
  if (len < 3) return false;

  // First byte is device info / flags. BTHome v2 requires bit6 = 1 (version 2),
  // and we skip encrypted advertisements (bit0 = 1) since we have no key.
  uint8_t flags = data[0];
  if ((flags & BTHOME_V2_FLAG_VERSION_BIT) == 0) return false;
  if ((flags & BTHOME_V2_FLAG_ENCRYPTED) != 0)   return false;

  size_t pos = 1;
  bool gotTemp = false;
  bool gotHum = false;

  while (pos < len) {
    if (pos >= len) break;
    uint8_t objId = data[pos++];

    switch (objId) {
      case BTHOME_OBJ_TEMPERATURE_S16:
        if (pos + 2 > len) return gotTemp;
        { int16_t rawT = (int16_t)(data[pos] | (data[pos + 1] << 8));
          *temp = rawT / 100.0f;
          if (*temp >= -40.0f && *temp <= 60.0f) gotTemp = true;
        }
        pos += 2;
        break;

      case BTHOME_OBJ_HUMIDITY_U16:
        if (pos + 2 > len) return gotTemp;
        { uint16_t rawH = (uint16_t)(data[pos] | (data[pos + 1] << 8));
          *hum = rawH / 100.0f;
          if (*hum >= 0.0f && *hum <= 100.0f) gotHum = true;
        }
        pos += 2;
        break;

      case BTHOME_OBJ_BATTERY_U8:
        if (pos + 1 > len) return gotTemp;
        *batt = data[pos];
        pos += 1;
        break;

      case 0x00:  // TASK-498 (2A-M3): BTHome v2 packet-id object — 1-byte payload, skip and continue
        if (pos + 1 > len) return gotTemp;
        pos += 1;
        break;

      default:
        // Unknown object: we cannot determine its length, so stop parsing
        return gotTemp;
    }
  }
  (void)gotHum;  // humidity is optional
  return gotTemp;
}

//=====================================================================
// Check if a MAC address matches the configured BLE MAC filter
// Empty/unset MAC means accept all sensors
//=====================================================================
static bool bleMatchesConfiguredMAC(const char* mac)
{
  if (settings.sat.sBleMAC[0] == '\0') return true;  // No filter: accept all
  return (strcasecmp(mac, settings.sat.sBleMAC) == 0);
}

//=====================================================================
// Find existing slot for MAC or allocate a new one
// Returns slot index (0..SAT_BLE_MAX_SENSORS-1) or -1 if full
//=====================================================================
static int bleFindOrAllocSlot(const char* mac)
{
  int emptySlot = -1;
  for (int i = 0; i < SAT_BLE_MAX_SENSORS; i++) {
    if (_bleSensors[i].bValid && strcmp(_bleSensors[i].sMacAddress, mac) == 0) {
      return i;  // Found existing
    }
    if (!_bleSensors[i].bValid && emptySlot < 0) {
      emptySlot = i;
    }
  }
  return emptySlot;
}

//=====================================================================
// NimBLE 2.x scan callback: parses advertisements for known sensor formats
//=====================================================================
class SATBLEScanCallbacks final : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dev) override
  {
    float temp = 0.0f, hum = 0.0f;
    uint8_t batt = 0;
    bool parsed = false;

    SATBLEDebugTf(PSTR("SAT BLE: ad from %s rssi=%d\r\n"),
                  dev->getAddress().toString().c_str(),
                  dev->getRSSI());

    // Try ATC/pvvx (UUID 0x181A) first.
    // NimBLE returns std::string by value; lifetime ends at end of this scope.
    {
      std::string atcData = dev->getServiceData(NimBLEUUID((uint16_t)ATC_SERVICE_UUID_16));
      if (atcData.length() >= 13) {
        parsed = parseBLEAtcFormat(reinterpret_cast<const uint8_t*>(atcData.data()),
                                    atcData.length(), &temp, &hum, &batt);
      }
    }

    // Fall through to BTHome v2 (UUID 0xFCD2) if ATC didn't match.
    if (!parsed) {
      std::string bthomeData = dev->getServiceData(NimBLEUUID((uint16_t)BTHOME_SERVICE_UUID_16));
      if (bthomeData.length() >= 3) {
        parsed = parseBLEBTHomeFormat(reinterpret_cast<const uint8_t*>(bthomeData.data()),
                                       bthomeData.length(), &temp, &hum, &batt);
      }
    }

    if (!parsed) {
      SATBLEDebugTf(PSTR("SAT BLE: ad rejected (unknown format)\r\n"));
      return;
    }

    // Get MAC address — copy to fixed char buffer for storage
    char macBuf[18];
    strlcpy(macBuf, dev->getAddress().toString().c_str(), sizeof(macBuf));
    // Convert to uppercase AA:BB:CC:DD:EE:FF format
    for (int i = 0; macBuf[i]; i++) macBuf[i] = toupper((unsigned char)macBuf[i]);

    // Check MAC filter
    if (!bleMatchesConfiguredMAC(macBuf)) {
      SATBLEDebugTf(PSTR("SAT BLE: ad from %s rejected (filter='%s')\r\n"),
                    macBuf, settings.sat.sBleMAC);
      return;
    }

    // Find or allocate slot
    int slot = bleFindOrAllocSlot(macBuf);
    if (slot < 0) {
      SATBLEDebugTf(PSTR("SAT BLE: ad from %s rejected (no free slot)\r\n"), macBuf);
      return;
    }

    // TASK-497: serialise the slot update against loop-task reads.
    // The block below is bounded constant time (~30 stores), so the
    // critical section is microseconds, never blocking the BLE radio.
    portENTER_CRITICAL(&_bleSensorsMux);
    // TASK-489 follow-up: when a slot is recycled (was occupied by a
    // different MAC that went stale), the previous tenant's
    // bDiscoveryPublished flag is still set. Reset it so the new MAC
    // gets a fresh HA-discovery config published. For an unchanged
    // MAC the strcmp matches and the flag is preserved (no spam).
    if (strcmp(_bleSensors[slot].sMacAddress, macBuf) != 0) {
      _bleSensors[slot].bDiscoveryPublished = false;
    }

    // Update sensor data
    strlcpy(_bleSensors[slot].sMacAddress, macBuf, sizeof(_bleSensors[slot].sMacAddress));
    _bleSensors[slot].fTemperature = temp;
    _bleSensors[slot].fHumidity    = hum;
    _bleSensors[slot].iBattery     = batt;
    _bleSensors[slot].iRssi        = (int8_t)dev->getRSSI();
    _bleSensors[slot].bValid       = true;
    _bleSensors[slot].iLastSeenMs  = millis();
    portEXIT_CRITICAL(&_bleSensorsMux);

    SATBLEDebugTf(PSTR("SAT BLE: sensor %s slot=%d temp=%.1f hum=%.1f batt=%u rssi=%d\r\n"),
                  macBuf, slot, temp, hum, (unsigned)batt,
                  (int)dev->getRSSI());
  }
};

static SATBLEScanCallbacks _bleScanCallbacks;

//=====================================================================
void satBLEInit()
{
  if (!settings.sat.bBleEnable) return;

  NimBLEDevice::init("");
  _pBLEScan = NimBLEDevice::getScan();
  // wantDuplicates=true: callback fires for every advertisement, not just first
  _pBLEScan->setScanCallbacks(&_bleScanCallbacks, true);
  _pBLEScan->setActiveScan(false);  // Passive scan to save power
  _pBLEScan->setMaxResults(0);      // Callback-only mode; library never builds a result list
  _pBLEScan->setInterval(160);       // 100 ms BLE scan-interval
  _pBLEScan->setWindow(80);          // 50 ms BLE scan-window (50% radio duty)

  // TASK-494: Continuous-scan model — start the scan ONCE, run forever.
  // Args: (duration_seconds=0 means forever, is_continue=false, restart=true).
  // Callbacks fire on the BLE host task as advertisements arrive; loop() is
  // never blocked. Matches the OT-Thing reference implementation, which
  // discovers BLE sensors within seconds of boot. The earlier periodic
  // 3-s/30-s model created a 30-s startup blackout plus 90 % off-time —
  // sensors were rarely seen even when in range.
  _pBLEScan->start(0, false, true);

  _bleInitialized = true;
  DebugTln(F("SAT BLE: initialized (NimBLE 2.x, continuous scan)"));

  if (settings.sat.sBleMAC[0] != '\0') {
    DebugTf(PSTR("SAT BLE: bound to MAC %s\r\n"), settings.sat.sBleMAC);
  } else {
    DebugTln(F("SAT BLE: accepting all compatible sensors"));
  }
}

//=====================================================================
void satBLELoop()
{
  if (!settings.sat.bBleEnable) return;

  // Lazy init: if BLE was enabled at runtime via settings change
  if (!_bleInitialized) {
    satBLEInit();
    if (!_bleInitialized) return;
  }

  // TASK-494: scan runs continuously since satBLEInit(). Here we only
  // throttle the publish/state-update cadence so MQTT and state.sat.*
  // are refreshed every iBleInterval seconds rather than on every loop
  // iteration. Scan callbacks update _bleSensors[] in real time on the
  // BLE host task; satBLEUpdateState() picks the best slot and copies
  // it into state.sat.* for SAT control input + MQTT publishing.
  uint32_t interval = (uint32_t)settings.sat.iBleInterval * 1000UL;
  if ((millis() - _bleLastPublishMs) < interval) return;
  _bleLastPublishMs = millis();

  satBLEUpdateState();
}

//=====================================================================
// Update global state from best available BLE sensor
//=====================================================================
void satBLEUpdateState()
{
  uint32_t now = millis();
  bool found = false;
  uint8_t sensorCount = 0;

  for (int i = 0; i < SAT_BLE_MAX_SENSORS; i++) {
    // TASK-497: snapshot the slot under the BLE mutex, then process the local
    // copy without the lock. Critical section is one struct copy (~40 bytes).
    BLESensorData snap;
    portENTER_CRITICAL(&_bleSensorsMux);
    snap = _bleSensors[i];
    portEXIT_CRITICAL(&_bleSensorsMux);

    if (!snap.bValid) continue;

    // Mark stale sensors as invalid (write back through the lock)
    if ((now - snap.iLastSeenMs) > BLE_STALE_MS) {
      portENTER_CRITICAL(&_bleSensorsMux);
      _bleSensors[i].bValid = false;
      portEXIT_CRITICAL(&_bleSensorsMux);
      continue;
    }

    sensorCount++;

    // Use the configured MAC sensor if set, otherwise use first valid
    if (!found) {
      if (settings.sat.sBleMAC[0] == '\0' ||
          strcasecmp(snap.sMacAddress, settings.sat.sBleMAC) == 0) {
        state.sat.fBleTemp      = snap.fTemperature;
        state.sat.fBleHumidity  = snap.fHumidity;
        state.sat.iBleBattery   = snap.iBattery;
        state.sat.iBleRssi      = snap.iRssi;
        state.sat.bBleTempValid = true;
        state.sat.iBleTempLastMs = snap.iLastSeenMs;
        found = true;
        SATBLEDebugTf(PSTR("SAT BLE: best sensor slot=%d mac=%s temp=%.1f age=%ums\r\n"),
                      i, snap.sMacAddress, snap.fTemperature,
                      (unsigned)(now - snap.iLastSeenMs));
      }
    }
  }

  state.sat.iBleSensorCount = sensorCount;

  // If no valid sensor found, mark state as invalid
  if (!found) {
    state.sat.bBleTempValid = false;
    SATBLEDebugTf(PSTR("SAT BLE: no valid sensor (count=%u, all stale or filter mismatch)\r\n"),
                  (unsigned)sensorCount);
  }
}

//=====================================================================
// Get best BLE temperature for SAT (returns 0.0 if no valid reading)
//=====================================================================
float satBLEGetTemperature()
{
  if (state.sat.bBleTempValid) {
    return state.sat.fBleTemp;
  }
  return 0.0f;
}

//=====================================================================
// Get BLE humidity
//=====================================================================
float satBLEGetHumidity()
{
  if (state.sat.bBleTempValid) {
    return state.sat.fBleHumidity;
  }
  return 0.0f;
}

//=====================================================================
// Publish BLE sensor data to MQTT
// (MAC compact-form helper satBLEMacToCompact() lives in MQTTstuff.ino;
//  declared in OTGW-firmware.h. TASK-492 consolidated the duplicate
//  bleMacCompactLocal helper that previously lived here.)
//=====================================================================
void satBLEPublishMQTT()
{
  if (!settings.mqtt.bEnable || !state.mqtt.bConnected) return;
  if (!settings.sat.bBleEnable) return;

  char valBuf[16];

  // --- Legacy flat topics (best/configured slot) — backwards compat ---
  if (state.sat.bBleTempValid) {
    dtostrf(state.sat.fBleTemp, 1, 2, valBuf);
    sendMQTTData(F("sat/ble_temp"), valBuf, false);

    dtostrf(state.sat.fBleHumidity, 1, 2, valBuf);
    sendMQTTData(F("sat/ble_humidity"), valBuf, false);

    snprintf_P(valBuf, sizeof(valBuf), PSTR("%d"), (int)state.sat.iBleRssi);
    sendMQTTData(F("sat/ble_sensor_rssi"), valBuf, false);

    snprintf_P(valBuf, sizeof(valBuf), PSTR("%u"), (unsigned)state.sat.iBleBattery);
    sendMQTTData(F("sat/ble_battery"), valBuf, false);
  }

  snprintf_P(valBuf, sizeof(valBuf), PSTR("%u"), (unsigned)state.sat.iBleSensorCount);
  sendMQTTData(F("sat/ble_sensor_count"), valBuf, false);
  sendMQTTData(F("sat/ble_temp_valid"), state.sat.bBleTempValid ? "true" : "false", false);

  // --- TASK-488: per-MAC state topics + one-shot HA discovery ---
  // For every valid slot: snapshot under the BLE mutex (TASK-497), then
  // publish from the local copy so the network I/O does not hold the lock.
  for (int i = 0; i < SAT_BLE_MAX_SENSORS; i++) {
    BLESensorData snap;
    portENTER_CRITICAL(&_bleSensorsMux);
    snap = _bleSensors[i];
    portEXIT_CRITICAL(&_bleSensorsMux);

    if (!snap.bValid) continue;
    char macCompact[13];
    satBLEMacToCompact(snap.sMacAddress, macCompact, sizeof(macCompact));
    if (macCompact[0] == '\0') continue;  // skip malformed

    if (!snap.bDiscoveryPublished) {
      // TASK-493 (1A-H1): only mark discovery as published when the helper
      // actually succeeded. A transient first-scan failure (MQTT not yet
      // connected, low heap, broker hiccup) will retry on the next
      // iBleInterval cycle instead of permanently silencing HA discovery
      // for this MAC. Write the flag back to the real slot under the lock.
      if (satBLEPublishHaDiscovery(macCompact, snap.sMacAddress)) {
        portENTER_CRITICAL(&_bleSensorsMux);
        _bleSensors[i].bDiscoveryPublished = true;
        portEXIT_CRITICAL(&_bleSensorsMux);
      }
    }
    satBLEPublishStateTopics(macCompact,
                                 snap.fTemperature,
                                 snap.fHumidity,
                                 snap.iBattery,
                                 snap.iRssi);

    // TASK-500 (2B-M1): worst-case first-scan burst is 4 sensors × (4 discovery
    // configs + 4 state topics) = 32 publishes back-to-back. canPublishMQTT()
    // gates start-of-burst, but does not throttle within the burst. A short
    // yield between sensors releases the FreeRTOS loop task on ESP32 and ticks
    // the cooperative scheduler on ESP8266 so other doBackgroundTasks consumers
    // (PIC serial, OT scheduler, NTP) do not starve. delay(0) is the
    // canonical Arduino "yield to other tasks without sleeping".
    delay(0);
  }
}

//=====================================================================
// Send BLE status as part of SAT JSON status
//=====================================================================
void satBLESendStatusJSON()
{
  sendJsonMapEntry(F("ble_enable"),       settings.sat.bBleEnable);
  sendJsonMapEntry(F("ble_temp_valid"),   state.sat.bBleTempValid);
  if (state.sat.bBleTempValid) {
    { char buf[12]; dtostrf(state.sat.fBleTemp, 1, 2, buf);
      sendBeforenext(); sendIdent();
      char json[40]; snprintf_P(json, sizeof(json), PSTR("\"ble_temp\": %s"), buf);
      httpServer.sendContent(json); }
    { char buf[12]; dtostrf(state.sat.fBleHumidity, 1, 2, buf);
      sendBeforenext(); sendIdent();
      char json[40]; snprintf_P(json, sizeof(json), PSTR("\"ble_humidity\": %s"), buf);
      httpServer.sendContent(json); }
    sendJsonMapEntry(F("ble_rssi"),        (int32_t)state.sat.iBleRssi);
    sendJsonMapEntry(F("ble_battery"),     (int32_t)state.sat.iBleBattery);
  }
  sendJsonMapEntry(F("ble_sensor_count"), (int32_t)state.sat.iBleSensorCount);
  if (settings.sat.sBleMAC[0] != '\0') {
    sendJsonMapEntry(F("ble_mac"),        settings.sat.sBleMAC);
  }
}

#endif // defined(ESP32)
