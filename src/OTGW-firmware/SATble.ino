/*
***************************************************************************
**  Module   : SATble.ino
**  Description: BLE temperature sensor support for SAT (ESP32 only)
**
**  Part of SAT (Smart Autotune Thermostat) firmware port.
**  Original SAT component by Alexwijn (https://github.com/Alexwijn/SAT)
**  SAT concept and algorithm design by George Dellas
**
**  Scans for BLE advertisements from common temperature/humidity sensors
**  (Xiaomi LYWSD03MMC with ATC/pvvx custom firmware, BTHome protocol).
**  Provides room temperature input for SAT control loop.
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License. See bottom of OTGW-firmware.h
***************************************************************************
*/

#if defined(ESP32)

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

// --- BLE Sensor Constants ---
#define SAT_BLE_MAX_SENSORS   4
static const uint32_t BLE_SCAN_INTERVAL_MS  = 30000;   // 30s between scans (default)
static const uint32_t BLE_STALE_MS          = 300000;   // 5 min stale timeout
static const uint32_t BLE_SCAN_DURATION_SEC = 3;        // 3 second scan window

// ATC/pvvx custom firmware service data UUID: 0x181A (Environmental Sensing)
static const uint16_t ATC_SERVICE_UUID_16   = 0x181A;
// BTHome v2 service data UUID: 0xFCD2
static const uint16_t BTHOME_SERVICE_UUID_16 = 0xFCD2;

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
};

static BLESensorData _bleSensors[SAT_BLE_MAX_SENSORS];
static BLEScan* _pBLEScan  = nullptr;
static uint32_t _bleLastScanMs = 0;
static bool     _bleInitialized = false;

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

  // First byte is device info / flags; skip it
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
// BLE scan callback: parses advertisements for known sensor formats
//=====================================================================
class SATBLEScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override
  {
    float temp = 0.0f, hum = 0.0f;
    uint8_t batt = 0;
    bool parsed = false;

    // Try ATC/pvvx format: service data UUID 0x181A
    if (advertisedDevice.haveServiceData()) {
      BLEUUID svcUUID = advertisedDevice.getServiceDataUUID();
      std::string svcData = advertisedDevice.getServiceData();
      uint16_t uuid16 = 0;

      // Extract 16-bit UUID
      if (svcUUID.bitSize() == 16) {
        uuid16 = *(uint16_t*)svcUUID.getNative()->uuid.uuid16;
      } else {
        // Some BLE stacks return the full 128-bit form for 16-bit UUIDs
        // Try matching by string
        std::string uuidStr = svcUUID.toString();
        if (uuidStr.find("181a") != std::string::npos || uuidStr.find("181A") != std::string::npos) {
          uuid16 = ATC_SERVICE_UUID_16;
        } else if (uuidStr.find("fcd2") != std::string::npos || uuidStr.find("FCD2") != std::string::npos) {
          uuid16 = BTHOME_SERVICE_UUID_16;
        }
      }

      if (uuid16 == ATC_SERVICE_UUID_16 && svcData.length() >= 13) {
        parsed = parseBLEAtcFormat((const uint8_t*)svcData.data(), svcData.length(), &temp, &hum, &batt);
      } else if (uuid16 == BTHOME_SERVICE_UUID_16 && svcData.length() >= 3) {
        parsed = parseBLEBTHomeFormat((const uint8_t*)svcData.data(), svcData.length(), &temp, &hum, &batt);
      }
    }

    if (!parsed) return;

    // Get MAC address string
    std::string macStr = advertisedDevice.getAddress().toString();
    // Convert to uppercase AA:BB:CC:DD:EE:FF format
    char macBuf[18];
    strlcpy(macBuf, macStr.c_str(), sizeof(macBuf));
    for (int i = 0; macBuf[i]; i++) macBuf[i] = toupper(macBuf[i]);

    // Check MAC filter
    if (!bleMatchesConfiguredMAC(macBuf)) return;

    // Find or allocate slot
    int slot = bleFindOrAllocSlot(macBuf);
    if (slot < 0) return;  // All slots full

    // Update sensor data
    strlcpy(_bleSensors[slot].sMacAddress, macBuf, sizeof(_bleSensors[slot].sMacAddress));
    _bleSensors[slot].fTemperature = temp;
    _bleSensors[slot].fHumidity    = hum;
    _bleSensors[slot].iBattery     = batt;
    _bleSensors[slot].iRssi        = (int8_t)advertisedDevice.getRSSI();
    _bleSensors[slot].bValid       = true;
    _bleSensors[slot].iLastSeenMs  = millis();
  }
};

static SATBLEScanCallbacks _bleScanCallbacks;

//=====================================================================
void satBLEInit()
{
  if (!settings.sat.bBleEnable) return;

  BLEDevice::init("");
  _pBLEScan = BLEDevice::getScan();
  _pBLEScan->setAdvertisedDeviceCallbacks(&_bleScanCallbacks, true);  // true = allow duplicates
  _pBLEScan->setActiveScan(false);   // Passive scan to save power
  _pBLEScan->setInterval(100);
  _pBLEScan->setWindow(99);

  _bleInitialized = true;
  DebugTln(F("SAT BLE: initialized"));

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

  // Start non-blocking scan
  _pBLEScan->start(BLE_SCAN_DURATION_SEC, false);  // false = non-blocking
  _pBLEScan->clearResults();  // Free memory after scan

  // Update state from best sensor
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
      }
    }
  }

  state.sat.iBleSensorCount = sensorCount;

  // If no valid sensor found, mark state as invalid
  if (!found) {
    state.sat.bBleTempValid = false;
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
//=====================================================================
void satBLEPublishMQTT()
{
  if (!settings.mqtt.bEnable || !state.mqtt.bConnected) return;
  if (!settings.sat.bBleEnable) return;

  char valBuf[16];

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
