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
static const uint32_t BLE_SCAN_DURATION_SEC = 3;        // 3 second scan window per cycle

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
static uint32_t      _bleLastScanMs  = 0;
static bool          _bleInitialized = false;

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
class SATBLEScanCallbacks : public NimBLEScanCallbacks {
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

    // TASK-488 follow-up: when a slot is recycled (was occupied by a
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

  _bleInitialized = true;
  DebugTln(F("SAT BLE: initialized (NimBLE 2.x)"));

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

  // Rate-limit scans
  uint32_t interval = (uint32_t)settings.sat.iBleInterval * 1000UL;
  if ((millis() - _bleLastScanMs) < interval) return;
  _bleLastScanMs = millis();

  SATBLEDebugTf(PSTR("SAT BLE: scan starting async (interval=%us, duration=%us)\r\n"),
                (unsigned)(interval / 1000UL),
                (unsigned)BLE_SCAN_DURATION_SEC);

  // NimBLE 2.x async scan: 3-arg form returns immediately, callbacks fire on
  // the BLE host task. loop() is never blocked. Args: (duration_seconds,
  // is_continue=false, restart=true). 'restart=true' handles the case where a
  // previous scan window is still draining when our timer fires.
  _pBLEScan->start(BLE_SCAN_DURATION_SEC, false, true);

  // Update state from best sensor (uses whatever slots are valid right now;
  // fresh callbacks during the next 3 s will refresh slots in place).
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
    if (!_bleSensors[i].bValid) continue;

    // Mark stale sensors as invalid
    if ((now - _bleSensors[i].iLastSeenMs) > BLE_STALE_MS) {
      _bleSensors[i].bValid = false;
      continue;
    }

    sensorCount++;

    // Use the configured MAC sensor if set, otherwise use first valid
    if (!found) {
      if (settings.sat.sBleMAC[0] == '\0' ||
          strcasecmp(_bleSensors[i].sMacAddress, settings.sat.sBleMAC) == 0) {
        state.sat.fBleTemp      = _bleSensors[i].fTemperature;
        state.sat.fBleHumidity  = _bleSensors[i].fHumidity;
        state.sat.iBleBattery   = _bleSensors[i].iBattery;
        state.sat.iBleRssi      = _bleSensors[i].iRssi;
        state.sat.bBleTempValid = true;
        state.sat.iBleTempLastMs = _bleSensors[i].iLastSeenMs;
        found = true;
        SATBLEDebugTf(PSTR("SAT BLE: best sensor slot=%d mac=%s temp=%.1f age=%ums\r\n"),
                      i, _bleSensors[i].sMacAddress, _bleSensors[i].fTemperature,
                      (unsigned)(now - _bleSensors[i].iLastSeenMs));
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
// Compact a MAC string "AA:BB:CC:DD:EE:FF" -> "aabbccddeeff" (lowercase,
// no colons). Used to build per-sensor MQTT topic paths and HA object_ids.
// Bounded; on malformed input writes empty string.
//=====================================================================
static void bleMacCompactLocal(const char* macWithColons, char* out, size_t outSize)
{
  if (out == nullptr || outSize < 13) {
    if (out != nullptr && outSize > 0) out[0] = '\0';
    return;
  }
  out[0] = '\0';
  if (macWithColons == nullptr) return;
  size_t len = strnlen(macWithColons, 18);
  if (len != 17) return;
  size_t outPos = 0;
  for (size_t i = 0; i < 17; i++) {
    char c = macWithColons[i];
    if (i % 3 == 2) {
      if (c != ':') return;
      continue;
    }
    if (!isxdigit((unsigned char)c)) return;
    out[outPos++] = (char)tolower((unsigned char)c);
  }
  out[outPos] = '\0';
}

//=====================================================================
// Publish BLE sensor data to MQTT
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
  // For every valid slot: compact MAC, publish 4 state topics,
  // and (once per MAC) emit the HA discovery configs.
  for (int i = 0; i < SAT_BLE_MAX_SENSORS; i++) {
    if (!_bleSensors[i].bValid) continue;
    char macCompact[13];
    bleMacCompactLocal(_bleSensors[i].sMacAddress, macCompact, sizeof(macCompact));
    if (macCompact[0] == '\0') continue;  // skip malformed
    if (!_bleSensors[i].bDiscoveryPublished) {
      bleSensorPublishHaDiscovery(macCompact, _bleSensors[i].sMacAddress);
      _bleSensors[i].bDiscoveryPublished = true;
    }
    bleSensorPublishStateTopics(macCompact,
                                 _bleSensors[i].fTemperature,
                                 _bleSensors[i].fHumidity,
                                 _bleSensors[i].iBattery,
                                 _bleSensors[i].iRssi);
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
