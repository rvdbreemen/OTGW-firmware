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
// TASK-508: roster size moved to SATtypes.h (SAT_BLE_MAX_ROSTER = 8) so
// settings.sat.sBleMac/sBleLabel can be sized symmetrically with the
// runtime slot table below. Old SAT_BLE_MAX_SENSORS=4 retired.
static const uint32_t BLE_STALE_MS          = 300000;   // 5 min stale timeout

// Compile-time guard: BLE_STALE_MS must comfortably exceed one scan window so
// the staleness check cannot evict a slot the radio just refreshed. Tripwire
// for any future re-tuning that mixes the _MS / _SEC constant suffixes.
static_assert(BLE_STALE_MS > 60000UL, "BLE_STALE_MS must be > 60 s; see SATble.ino BLE_STALE_MS / BLE_SCAN_INTERVAL_TICKS");

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

// NimBLE scan tuning. Argument unit is BLE-spec 0.625 ms ticks.
// 50 % duty (window/interval) keeps the radio active half the time without
// starving the WiFi/BT coexistence on a single-radio ESP32-S3.
static constexpr uint16_t BLE_SCAN_INTERVAL_TICKS = 160;  // 100 ms
static constexpr uint16_t BLE_SCAN_WINDOW_TICKS   = 80;   //  50 ms
// Magic-zero NimBLE idiom: callback-only mode, library never builds a result list.
static constexpr uint16_t BLE_SCAN_MAX_RESULTS    = 0;
// NimBLE rejects window > interval at runtime; catch it at compile time so
// any future re-tune flagging the wrong constant fails the build instead of
// silently disabling the scan.
static_assert(BLE_SCAN_WINDOW_TICKS <= BLE_SCAN_INTERVAL_TICKS,
              "BLE_SCAN_WINDOW_TICKS must be <= BLE_SCAN_INTERVAL_TICKS");

// TASK-508: transient runtime data, parallel to settings.sat.sBleMac[i] /
// sBleLabel[i]. Slot is "in use" iff settings.sat.sBleMac[i][0] != '\0';
// data is "fresh" iff iLastSeenMs > 0 AND (now - iLastSeenMs) <= BLE_STALE_MS.
// Hot-path BLE callback only touches this struct under _bleSensorsMux —
// labels live in settings (loop-task only) so the critical section stays
// short.
struct BLERuntime {
  float    fTemperature;        // 2 decimal precision
  float    fHumidity;
  uint8_t  iBattery;
  int8_t   iRssi;
  uint32_t iLastSeenMs;          // 0 = never seen since boot
  bool     bDiscoveryPublished;  // TASK-488: HA-discovery sent at least once for this MAC
  bool     bDiscoveryDirty;      // TASK-508: label changed → re-publish on next cycle
};

static BLERuntime         _bleRuntime[SAT_BLE_MAX_ROSTER] = {};
static NimBLEScan*        _pBLEScan          = nullptr;
static uint32_t           _bleLastPublishMs  = 0;
static bool               _bleInitialized    = false;
// TASK-508: BLE host task sets this when it allocates a new roster slot;
// satBLELoop() drains it (loop task) and triggers settingsDirty=true.
// Avoids cross-task flash writes from the BLE callback.
static volatile bool      _bleRosterDirty    = false;
// TASK-508: counts ads dropped because roster is at SAT_BLE_MAX_ROSTER.
// Surfaced in /api/v2/sat/ble/discovery so the UI can warn the user.
static volatile uint32_t  _bleRosterFullCount = 0;

// TASK-506: aggregated scan-stats counters. Incremented on the BLE host
// task in onResult(); read+reset on the Arduino loop task in satBLELoop()
// once per iBleInterval. volatile (not atomic) is sufficient: a torn
// read or lost increment is invisible at logging granularity, and the
// codebase has no std::atomic precedent. uint32_t at ~200 ads/sec wraps
// in ~248d; window reset makes practical overflow impossible.
static volatile uint32_t _bleAdCount        = 0;
static volatile uint32_t _bleAcceptCount    = 0;
static volatile uint32_t _bleFilterRejCount = 0;
static volatile uint32_t _bleUnknownCount   = 0;
static volatile uint32_t _bleNoSlotCount    = 0;
static uint32_t          _bleStatsLastMs    = 0;

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
// Find existing slot for MAC or allocate a new one in the roster
// Returns slot index (0..SAT_BLE_MAX_ROSTER-1) or -1 if full
// TASK-508: the roster is settings-backed — `settings.sat.sBleMac[i][0]`
// is non-zero iff that slot is in use. strcasecmp tolerates legacy
// lowercase entries written by older firmware via the generic settings
// POST path; new writes from onResult() are uppercased in advance.
//=====================================================================
static int bleFindOrAllocSlot(const char* mac)
{
  int emptySlot = -1;
  for (int i = 0; i < SAT_BLE_MAX_ROSTER; i++) {
    if (settings.sat.sBleMac[i][0] != '\0' &&
        strcasecmp(settings.sat.sBleMac[i], mac) == 0) {
      return i;  // Found existing roster entry
    }
    if (settings.sat.sBleMac[i][0] == '\0' && emptySlot < 0) {
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

    // TASK-506: count every ad; per-ad logging removed to cut spam.
    // Aggregated stats emitted by satBLELoop() once per iBleInterval.
    _bleAdCount += 1;

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
      // TASK-506: counted only; per-ad reject log removed (was main spam source).
      _bleUnknownCount += 1;
      return;
    }

    // Get MAC address — copy to fixed char buffer for storage
    char macBuf[18];
    strlcpy(macBuf, dev->getAddress().toString().c_str(), sizeof(macBuf));
    // Convert to uppercase AA:BB:CC:DD:EE:FF format
    for (int i = 0; macBuf[i]; i++) macBuf[i] = toupper((unsigned char)macBuf[i]);

    // TASK-508: NO filter check here — every format-pass MAC enters the
    // roster so the UI can offer self-discovery. The MAC filter
    // (settings.sat.sBleMAC) is applied later in satBLEUpdateState() to
    // pick which roster slot feeds state.sat.fBleTemp / SAT control.
    int slot = bleFindOrAllocSlot(macBuf);
    if (slot < 0) {
      _bleNoSlotCount     += 1;   // legacy counter from TASK-506
      _bleRosterFullCount += 1;   // TASK-508: surfaced via /api/v2/sat/ble/discovery
      return;                     // no per-ad log: aggregated by stats counter
    }

    // TASK-508: was-this-slot-empty determines whether we touch settings
    // (and thus need to schedule a flush). Read BEFORE entering the
    // critical section: settings.sat.sBleMac[i] is loop-task territory
    // for everything except this single onResult write, and the BLE host
    // task is the only writer to it under the mutex below.
    bool isNewSlot = (settings.sat.sBleMac[slot][0] == '\0');

    // TASK-497/508: serialise slot updates against loop-task readers.
    // For a brand-new slot we also write `settings.sat.sBleMac[slot]`
    // here (loop task only reads outside the lock; writes are mutually
    // exclusive because there is exactly one BLE host task). The critical
    // section is bounded constant time (~30 stores) — microseconds.
    portENTER_CRITICAL(&_bleSensorsMux);
    if (isNewSlot) {
      strlcpy(settings.sat.sBleMac[slot], macBuf,
              sizeof(settings.sat.sBleMac[slot]));
      // Reset runtime defaults: a freshly-allocated slot must publish
      // HA discovery once before its first state update is meaningful.
      _bleRuntime[slot] = {};
    }
    _bleRuntime[slot].fTemperature = temp;
    _bleRuntime[slot].fHumidity    = hum;
    _bleRuntime[slot].iBattery     = batt;
    _bleRuntime[slot].iRssi        = static_cast<int8_t>(dev->getRSSI());
    _bleRuntime[slot].iLastSeenMs  = millis();
    portEXIT_CRITICAL(&_bleSensorsMux);

    if (isNewSlot) {
      // TASK-508: defer settings.flushSettings() to the loop task — that
      // is where settingsDirty=true is allowed to be set safely. The
      // flag is read+cleared once per iBleInterval in satBLELoop().
      _bleRosterDirty = true;
    }

    _bleAcceptCount += 1;
    SATBLEDebugTf(PSTR("SAT BLE: sensor %s slot=%d temp=%.2f hum=%.2f batt=%u rssi=%d\r\n"),
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
  _pBLEScan->setActiveScan(false);                    // Passive scan to save power
  _pBLEScan->setMaxResults(BLE_SCAN_MAX_RESULTS);     // see BLE_SCAN_MAX_RESULTS at top of file
  // NimBLE setInterval/setWindow take BLE-spec ticks of 0.625 ms each.
  _pBLEScan->setInterval(BLE_SCAN_INTERVAL_TICKS);    // 100 ms scan-interval
  _pBLEScan->setWindow(BLE_SCAN_WINDOW_TICKS);        //  50 ms scan-window (50 % radio duty)

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

  // TASK-506: emit aggregated scan-stats once per iBleInterval, then reset.
  // Replaces the per-ad SATBLEDebug spam in onResult(). Same gate
  // (state.debug.bSATBLE via SATBLEDebugTf macro) so toggling key '7'
  // still controls visibility, but at fixed volume regardless of RF traffic.
  uint32_t windowMs = millis() - _bleStatsLastMs;
  SATBLEDebugTf(PSTR("SAT BLE: %us window: %u ads, %u accepted, %u filter-rej, %u unknown, %u no-slot\r\n"),
                (unsigned)(windowMs / 1000),
                (unsigned)_bleAdCount, (unsigned)_bleAcceptCount,
                (unsigned)_bleFilterRejCount, (unsigned)_bleUnknownCount,
                (unsigned)_bleNoSlotCount);
  // C++20 deprecates chained assignment with a volatile-qualified left
  // operand (-Wvolatile). Reset each counter individually.
  _bleAdCount        = 0;
  _bleAcceptCount    = 0;
  _bleFilterRejCount = 0;
  _bleUnknownCount   = 0;
  _bleNoSlotCount    = 0;
  _bleStatsLastMs = millis();

  // TASK-508: drain roster-dirty flag set by onResult() on the BLE host
  // task when a NEW MAC was added. Recompute populated-slot count and
  // route through updateSetting() so the existing 2-s debounce flushes
  // /settings.ini exactly once per burst, regardless of how many new
  // MACs arrived. updateSetting() is the only public API that marks
  // the (file-static) settingsDirty flag.
  if (_bleRosterDirty) {
    _bleRosterDirty = false;
    uint8_t cnt = 0;
    for (int i = 0; i < SAT_BLE_MAX_ROSTER; i++) {
      if (settings.sat.sBleMac[i][0] != '\0') cnt++;
    }
    char cntBuf[8];
    snprintf_P(cntBuf, sizeof(cntBuf), PSTR("%u"), (unsigned)cnt);
    updateSetting("SATblerostercount", cntBuf);
    SATBLEDebugTf(PSTR("SAT BLE: roster updated, %u slot(s) in use\r\n"),
                  (unsigned)cnt);
  }

  // TASK-508: auto-select-if-only-one. Trigger when no MAC is selected
  // (sBleMAC empty) AND exactly one roster slot has produced a sample
  // within the last 2 × iBleInterval seconds. After promotion the filter
  // is non-empty, so this gate fires zero times forever after — single-shot
  // by construction. Runs on the loop task, so the updateSetting() flash
  // path is safe (BLE host task can't race against settings flush here).
  if (settings.sat.sBleMAC[0] == '\0') {
    const uint32_t freshMs = 2u * (uint32_t)settings.sat.iBleInterval * 1000u;
    const uint32_t now = millis();
    int seenIdx = -1;
    int seenCount = 0;
    for (int i = 0; i < SAT_BLE_MAX_ROSTER; i++) {
      if (settings.sat.sBleMac[i][0] == '\0') continue;
      BLERuntime snap;
      portENTER_CRITICAL(&_bleSensorsMux);
      snap = _bleRuntime[i];
      portEXIT_CRITICAL(&_bleSensorsMux);
      if (snap.iLastSeenMs && (now - snap.iLastSeenMs) <= freshMs) {
        seenIdx = i;
        seenCount++;
      }
    }
    if (seenCount == 1) {
      DebugTf(PSTR("SAT BLE: auto-select slot=%d mac=%s (only one fresh sensor)\r\n"),
              seenIdx, settings.sat.sBleMac[seenIdx]);
      updateSetting("SATblemac", settings.sat.sBleMac[seenIdx]);
    }
  }

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

  for (int i = 0; i < SAT_BLE_MAX_ROSTER; i++) {
    // TASK-508: roster-occupied test reads settings (loop-task only writer
    // outside this is via REST/Forget which also runs here). Skip empty
    // slots without taking the BLE mutex.
    if (settings.sat.sBleMac[i][0] == '\0') continue;

    // TASK-497: snapshot the runtime under the BLE mutex, then process
    // the local copy without the lock. Critical section is one struct
    // copy (~32 bytes).
    BLERuntime snap;
    portENTER_CRITICAL(&_bleSensorsMux);
    snap = _bleRuntime[i];
    portEXIT_CRITICAL(&_bleSensorsMux);

    // No sample yet (roster entry just allocated, ad never reseen),
    // or sample is stale beyond BLE_STALE_MS: the roster entry stays
    // (TASK-508: manual drop only via UI Forget) but data is not eligible
    // as a SAT input.
    if (snap.iLastSeenMs == 0) continue;
    if ((now - snap.iLastSeenMs) > BLE_STALE_MS) continue;

    sensorCount++;

    // Use the configured MAC sensor if set, otherwise use first fresh slot
    if (!found) {
      if (settings.sat.sBleMAC[0] == '\0' ||
          strcasecmp(settings.sat.sBleMac[i], settings.sat.sBleMAC) == 0) {
        state.sat.fBleTemp       = snap.fTemperature;
        state.sat.fBleHumidity   = snap.fHumidity;
        state.sat.iBleBattery    = snap.iBattery;
        state.sat.iBleRssi       = snap.iRssi;
        state.sat.bBleTempValid  = true;
        state.sat.iBleTempLastMs = snap.iLastSeenMs;
        found = true;
        SATBLEDebugTf(PSTR("SAT BLE: best sensor slot=%d mac=%s temp=%.2f age=%ums\r\n"),
                      i, settings.sat.sBleMac[i], snap.fTemperature,
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

  // --- TASK-488/508: per-MAC state topics + one-shot HA discovery ---
  // TASK-508 changes: publish ONLY for the user-selected MAC (one in the
  // roster). Other roster slots are visible in /api/v2/sat/ble/discovery
  // for the UI, but do not flood HA with passive entries the user never
  // configured. Forget-with-HA-cleanup (Block C) wipes the selected
  // sensor's retained discovery configs when removed.
  if (settings.sat.sBleMAC[0] == '\0') return;  // no selection → no publish

  int selectedSlot = -1;
  for (int i = 0; i < SAT_BLE_MAX_ROSTER; i++) {
    if (settings.sat.sBleMac[i][0] == '\0') continue;
    if (strcasecmp(settings.sat.sBleMac[i], settings.sat.sBleMAC) == 0) {
      selectedSlot = i;
      break;
    }
  }
  if (selectedSlot < 0) return;  // selected MAC not in roster (yet)

  // Snapshot runtime under the mutex; publish from the local copy so
  // network I/O never holds the BLE lock.
  BLERuntime snap;
  portENTER_CRITICAL(&_bleSensorsMux);
  snap = _bleRuntime[selectedSlot];
  portEXIT_CRITICAL(&_bleSensorsMux);

  if (snap.iLastSeenMs == 0) return;
  if ((millis() - snap.iLastSeenMs) > BLE_STALE_MS) return;

  char macCompact[BLE_MAC_COMPACT_SIZE];
  satBLEMacToCompact(settings.sat.sBleMac[selectedSlot], macCompact,
                     sizeof(macCompact));
  if (macCompact[0] == '\0') return;  // malformed MAC

  if (!snap.bDiscoveryPublished) {
    // TASK-493 (1A-H1): only mark discovery as published when the helper
    // actually succeeded. A transient first-scan failure (MQTT not yet
    // connected, low heap, broker hiccup) will retry on the next
    // iBleInterval cycle instead of permanently silencing HA discovery.
    // TASK-508: pass the user-set label (may be empty — falls back to
    // legacy "BLE <mac>" form inside satBLEPublishOneDiscovery).
    const char* lbl = settings.sat.sBleLabel[selectedSlot];
    if (satBLEPublishHaDiscovery(macCompact,
                                  settings.sat.sBleMac[selectedSlot],
                                  lbl)) {
      portENTER_CRITICAL(&_bleSensorsMux);
      _bleRuntime[selectedSlot].bDiscoveryPublished = true;
      _bleRuntime[selectedSlot].bDiscoveryDirty     = false;
      portEXIT_CRITICAL(&_bleSensorsMux);
    }
  }

  satBLEPublishStateTopics(macCompact,
                           snap.fTemperature,
                           snap.fHumidity,
                           snap.iBattery,
                           snap.iRssi);
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

//=====================================================================
// TASK-508: BLE roster REST helpers
// All run on the Arduino loop task (called from handleSAT() in
// restAPI.ino). Settings mutations go through updateSetting() so the
// existing 2-s debounce / flushSettings() pipeline persists them.
//=====================================================================

// Find the slot index that contains `mac`, or -1 if not in roster.
static int satBLERosterFindSlot(const char* mac)
{
  if (!mac || !mac[0]) return -1;
  for (int i = 0; i < SAT_BLE_MAX_ROSTER; i++) {
    if (settings.sat.sBleMac[i][0] != '\0' &&
        strcasecmp(settings.sat.sBleMac[i], mac) == 0) {
      return i;
    }
  }
  return -1;
}

// Stream the discovery roster as JSON (one map with top-level meta plus
// a "sensors" array of per-slot objects). Uses sendStartJsonMap +
// sendJsonMapEntry for the meta fields, then drops to raw sendContent
// for the array (the helpers do not support nested arrays).
void satBLERosterSendJSON()
{
  const uint32_t now = millis();

  uint8_t cnt = 0;
  for (int i = 0; i < SAT_BLE_MAX_ROSTER; i++) {
    if (settings.sat.sBleMac[i][0] != '\0') cnt++;
  }

  sendStartJsonMap("");
  sendJsonMapEntry(F("max_slots"),          (int32_t)SAT_BLE_MAX_ROSTER);
  sendJsonMapEntry(F("populated_slots"),    (int32_t)cnt);
  sendJsonMapEntry(F("roster_full"),        (cnt >= SAT_BLE_MAX_ROSTER));
  sendJsonMapEntry(F("dropped_since_full"), (int32_t)_bleRosterFullCount);
  sendJsonMapEntry(F("selected_mac"),       settings.sat.sBleMAC);

  // Sensors array — manual JSON since the helpers do not nest arrays.
  sendBeforenext(); sendIdent();
  httpServer.sendContent_P(PSTR("\"sensors\": ["));

  bool firstSensor = true;
  for (int i = 0; i < SAT_BLE_MAX_ROSTER; i++) {
    if (settings.sat.sBleMac[i][0] == '\0') continue;

    BLERuntime snap;
    portENTER_CRITICAL(&_bleSensorsMux);
    snap = _bleRuntime[i];
    portEXIT_CRITICAL(&_bleSensorsMux);

    bool fresh      = (snap.iLastSeenMs > 0 &&
                       (now - snap.iLastSeenMs) <= BLE_STALE_MS);
    bool isSelected = (settings.sat.sBleMAC[0] != '\0' &&
                       strcasecmp(settings.sat.sBleMac[i],
                                  settings.sat.sBleMAC) == 0);

    char escapedLabel[64];
    escapeJsonStringTo(settings.sat.sBleLabel[i], escapedLabel,
                       sizeof(escapedLabel));

    char tempBuf[12], humBuf[12];
    dtostrf(snap.fTemperature, 1, 2, tempBuf);
    dtostrf(snap.fHumidity,    1, 2, humBuf);

    char buf[256];
    snprintf_P(buf, sizeof(buf),
               PSTR("%s{\"slot\":%d,\"mac\":\"%s\",\"label\":\"%s\","
                    "\"temp\":%s,\"hum\":%s,\"battery\":%u,\"rssi\":%d,"
                    "\"age_ms\":%u,\"valid\":%s,\"selected\":%s}"),
               firstSensor ? "" : ",",
               i,
               settings.sat.sBleMac[i],
               escapedLabel,
               tempBuf, humBuf,
               (unsigned)snap.iBattery,
               (int)snap.iRssi,
               (unsigned)(fresh ? (now - snap.iLastSeenMs) : 0),
               fresh      ? "true" : "false",
               isSelected ? "true" : "false");
    httpServer.sendContent(buf);
    firstSensor = false;
  }
  httpServer.sendContent_P(PSTR("]"));

  sendEndJsonMap("");
}

// Promote a roster MAC to the active SAT input. Returns false if mac
// is not in the roster.
bool satBLERosterSelect(const char* mac)
{
  if (!mac || !mac[0]) return false;
  int slot = satBLERosterFindSlot(mac);
  if (slot < 0) return false;

  // Canonicalise to uppercase before persisting (matches onResult form).
  char macUpper[18];
  strlcpy(macUpper, mac, sizeof(macUpper));
  for (int p = 0; macUpper[p]; p++) {
    macUpper[p] = toupper((unsigned char)macUpper[p]);
  }
  updateSetting("SATblemac", macUpper);

  // Force re-publish so HA discovery reflects the new selection
  // (label may have changed since this slot was last published).
  portENTER_CRITICAL(&_bleSensorsMux);
  _bleRuntime[slot].bDiscoveryPublished = false;
  _bleRuntime[slot].bDiscoveryDirty     = true;
  portEXIT_CRITICAL(&_bleSensorsMux);
  return true;
}

// Set or update the user-friendly label for a roster slot.
// Empty `label` clears the label. Returns false if mac is not in roster.
bool satBLERosterSetLabel(const char* mac, const char* label)
{
  if (!mac || !mac[0] || !label) return false;
  int slot = satBLERosterFindSlot(mac);
  if (slot < 0) return false;

  char keyBuf[20];
  snprintf_P(keyBuf, sizeof(keyBuf), PSTR("SATblelabel%d"), slot);
  updateSetting(keyBuf, label);

  // If this is the selected MAC, queue a HA-discovery refresh so the
  // friendly_name updates on the next publish cycle.
  bool isSelected = (settings.sat.sBleMAC[0] != '\0' &&
                     strcasecmp(settings.sat.sBleMac[slot],
                                settings.sat.sBleMAC) == 0);
  if (isSelected) {
    portENTER_CRITICAL(&_bleSensorsMux);
    _bleRuntime[slot].bDiscoveryPublished = false;
    _bleRuntime[slot].bDiscoveryDirty     = true;
    portEXIT_CRITICAL(&_bleSensorsMux);
  }
  return true;
}

// Drop a roster slot — clears persistent fields and runtime data.
// If the forgotten MAC was the active selection, also clears sBleMAC
// AND wipes its retained HA-discovery configs (Block C extension).
// Returns false if mac is not in roster.
bool satBLERosterForget(const char* mac)
{
  if (!mac || !mac[0]) return false;
  int slot = satBLERosterFindSlot(mac);
  if (slot < 0) return false;

  bool isSelected = (settings.sat.sBleMAC[0] != '\0' &&
                     strcasecmp(settings.sat.sBleMac[slot],
                                settings.sat.sBleMAC) == 0);

  // Snapshot publish state and reset runtime under the mutex.
  bool wasPublished;
  portENTER_CRITICAL(&_bleSensorsMux);
  wasPublished = _bleRuntime[slot].bDiscoveryPublished;
  _bleRuntime[slot] = {};
  portEXIT_CRITICAL(&_bleSensorsMux);

  // HA cleanup BEFORE clearing the MAC string — we still need it for
  // the topic path. Publishes 4 zero-byte retained payloads so HA
  // removes the device. Only relevant when the selected slot's
  // discovery configs were ever published; an unpublished slot has
  // nothing to wipe.
  if (isSelected && wasPublished) {
    char macCompact[BLE_MAC_COMPACT_SIZE];
    satBLEMacToCompact(settings.sat.sBleMac[slot], macCompact,
                       sizeof(macCompact));
    if (macCompact[0] != '\0') {
      satBLEUnpublishDiscovery(macCompact);
    }
  }

  // Clear persistent slot fields via updateSetting (settings.ini flush).
  char keyBuf[20];
  snprintf_P(keyBuf, sizeof(keyBuf), PSTR("SATblemac%d"), slot);
  updateSetting(keyBuf, "");
  snprintf_P(keyBuf, sizeof(keyBuf), PSTR("SATblelabel%d"), slot);
  updateSetting(keyBuf, "");

  if (isSelected) {
    updateSetting("SATblemac", "");
  }

  // Recompute populated count.
  uint8_t cnt = 0;
  for (int i = 0; i < SAT_BLE_MAX_ROSTER; i++) {
    if (settings.sat.sBleMac[i][0] != '\0') cnt++;
  }
  char cntBuf[8];
  snprintf_P(cntBuf, sizeof(cntBuf), PSTR("%u"), (unsigned)cnt);
  updateSetting("SATblerostercount", cntBuf);
  return true;
}

#endif // defined(ESP32)
