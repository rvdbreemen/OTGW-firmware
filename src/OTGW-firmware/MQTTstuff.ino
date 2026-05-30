/* 
***************************************************************************  
**  Program  : MQTTstuff
**  Version  : v2.0.0-alpha.107
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**      Modified version from (c) 2020 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#include <PubSubClient.h>           // MQTT client publish and subscribe functionality
#include <ctype.h>
#include <pgmspace.h>
#include "OTGW-Core.h"              // Core OpenTherm data structures and functions
#include "MQTTstuff.h"              // Structured discovery data layer (enums, structs, streaming API)
#include "mqtt_discovery_verify.h"  // Discovery-verify state machine (TASK-363: separate TU)

// MQTT Streaming Mode - ALWAYS ENABLED
// Large auto-discovery messages are sent in 128-byte chunks instead of
// requiring a large buffer resize. This prevents heap fragmentation on ESP8266.
// Similar to ESPHome's chunked MQTT publishing strategy.

#define MQTTDebugTln(...) ({ if (state.debug.bMQTT) DebugTln(__VA_ARGS__);    })
#define MQTTDebugln(...)  ({ if (state.debug.bMQTT) Debugln(__VA_ARGS__);    })
#define MQTTDebugTf(...)  ({ if (state.debug.bMQTT) DebugTf(__VA_ARGS__);    })
#define MQTTDebugf(...)   ({ if (state.debug.bMQTT) Debugf(__VA_ARGS__);    })
#define MQTTDebugT(...)   ({ if (state.debug.bMQTT) DebugT(__VA_ARGS__);    })
#define MQTTDebug(...)    ({ if (state.debug.bMQTT) Debug(__VA_ARGS__);    })

void doAutoConfigure();
void setMQTTConfigPending(const uint8_t MSGid);
void markAllMQTTConfigPending();
void clearMQTTConfigPending();
void publishNonOTDiscoveryConfigs();
void loopMQTTDiscovery();
// ADR-106: topic-naming-mode cleanup helpers
void armTopicCleanupOnLegacyToggle(bool newUseLegacy);
void runTopicCleanupStep();

// Declare some variables within global scope

static IPAddress  MQTTbrokerIP;
static char       MQTTbrokerIPchar[20];
constexpr size_t  MQTT_ID_MAX_LEN = 96;
constexpr size_t  MQTT_NAMESPACE_MAX_LEN = 192;
constexpr size_t  MQTT_TOPIC_MAX_LEN = 200;
constexpr size_t  MQTT_CLIENT_BUFFER_SIZE = 384;
constexpr size_t  MQTT_PROGMEM_STAGE_LEN = 63;
// Minimum free heap required before attempting a discovery publish.
// Streaming HA discovery (ADR-042: streaming JSON, no ArduinoJson) only needs
// ~200 bytes per chunk, so the guard is an absolute "last safety rail", not a
// performance throttle. MQTT_DISCOVERY_HEAP_MIN is board-defined in boards.h
// (ESP-abstraction Tier 3): 3000 on ESP8266 (WARNING-tier floor, ~80KB RAM
// total), 2048 on ESP32 (larger DRAM budget should not block discovery while
// tens of KB are still free).
constexpr uint32_t MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS = 300000UL;  // 5 minutes

// PIC subtree prefix -- single source of truth for the otgw-pic/ MQTT subtree.
// Declared extern in MQTTstuff.h. See ADR-065 for the public-API contract.
// Consumed on the discovery side by composeBinSensorPayload/composeSensorPayload/
// climate payload in mqtt_configuratie.cpp, and on the publish side by the
// sendMQTTDataPic() helper below. TASK-390 migrated 9 direct publish call-sites
// in this file and 4 in OTGW-Core.ino to the helper. The indirect dispatcher
// path at OTGW-Core.ino:707-794 (picSettings switch-case + picSettings publish
// block, 30 literals total) still uses F("otgw-pic/settings/...") directly;
// migrating that set requires a dispatcher refactor and is tracked separately.
const char kPicSubtreePrefix[] PROGMEM = "otgw-pic/";

// MQTT autoconfig buffer design:
// feedWatchDog() is used (not doBackgroundTasks()) during autoconfig iterations
// to prevent cMsg from being overwritten by HTTP/MQTT callbacks.

// Guard shared MQTT autoconfig buffer (cMsg for sTopic) against accidental re-entry.
// feedWatchDog() (not doBackgroundTasks()) is the only yield used during autoconfig,
// so no HTTP/MQTT callback can overwrite cMsg mid-use.
// Not volatile: ESP8266 is cooperative single-threaded; no ISR enters this path.
static bool mqttAutoConfigInProgress = false;

struct MQTTAutoConfigSessionLock {
  bool locked = false;
  MQTTAutoConfigSessionLock() {
    if (!mqttAutoConfigInProgress) {
      mqttAutoConfigInProgress = true;
      locked = true;
    }
  }
  ~MQTTAutoConfigSessionLock() {
    if (locked) mqttAutoConfigInProgress = false;
  }
  // Prevent accidental copy/move which would cause double-release or leaked lock.
  MQTTAutoConfigSessionLock(const MQTTAutoConfigSessionLock&) = delete;
  MQTTAutoConfigSessionLock& operator=(const MQTTAutoConfigSessionLock&) = delete;
};

// pgm_strncmp_PP() and pgm_read_char() are in MQTTstuff.h (inline, shared with MQTTHaDiscovery.cpp)

// Status-burst quiesce (TASK-342) + post-burst cooldown (TASK-347).
// A Status-frame fanout publishes status_master plus 7 bits, and/or status_slave
// plus 8 bits in rapid succession (~20ms). If loopMQTTDiscovery fires a drip in
// the middle of that burst, the two allocations overlap and push the heap into
// the throttle band unnecessarily. beginStatusBurst() opens the window,
// endStatusBurst() closes it; during the window the drip skips.
//
// After endStatusBurst(), a cooldown of STATUS_BURST_COOLDOWN_MS is armed so
// lwIP pbufs from the just-finished publishes can drain before the next heap
// allocation. The cooldown is armed ONLY if the burst had at least one real
// MQTT publish (statusBurstPublishCount > 0). Empty bursts (every single bit
// was gated out by shouldPublishStatusBit) cost no TCP bandwidth, so no
// cooldown is needed — this prevents pointless drip pauses in idle state.
//
// TUNING (TASK-353): the Crashevans log shows Status-frames arriving at ~3s
// cadence. A 10s cooldown overlapped consecutive bursts and stalled the drip
// under heavy Status traffic (iDripCooldownSkipCount grew without discovery
// progressing). ESP8266 keeps a 2000ms cooldown: it fits comfortably between
// Status-frames (~3s cadence leaves ~1s of drip window per cycle) while still
// giving lwIP pbufs from the just-finished burst time to drain before the next
// heap allocation. ESP32 can drain the same burst with much more headroom, so
// it uses a shorter cooldown to avoid counting ordinary discovery progress as
// "pressure" when free heap is still tens of KB.
//
// Timeout safety: if endStatusBurst() is never reached (exception path),
// isStatusBurstActive() auto-clears after STATUS_BURST_TIMEOUT_MS.
static bool            statusBurstActive     = false;
static unsigned long   statusBurstStartMs    = 0;
static uint16_t        statusBurstPublishCount = 0;
static unsigned long   burstCooldownUntilMs  = 0;
static uint32_t        sDripDueAtMs          = 0;   // updated by loopMQTTDiscovery(); read by dripDueWithinMs()
static bool            dripDeviceInfoPending = false; // true after markAllMQTTConfigPending(); first drip entity carries full device block
constexpr unsigned long STATUS_BURST_TIMEOUT_MS  = 500;
// STATUS_BURST_COOLDOWN_MS is board-defined in boards.h (ESP-abstraction Tier 3,
// ADR-088): 2000 on ESP8266 (TASK-353: stays under the ~3s Status cadence so the
// drip gets a window per cycle), 250 on ESP32 (more heap headroom to drain a
// burst). Bound enforced by check_status_burst_cooldown_bound in evaluate.py.

void beginStatusBurst() {
  statusBurstActive = true;
  statusBurstStartMs = millis();
  statusBurstPublishCount = 0;
}

void endStatusBurst() {
  statusBurstActive = false;
  if (statusBurstPublishCount > 0) {
    burstCooldownUntilMs = millis() + STATUS_BURST_COOLDOWN_MS;
  }
  // else: empty burst, no cooldown armed
}

void incrementStatusBurstPublishCount() {
  if (statusBurstActive) statusBurstPublishCount++;
}

bool isStatusBurstActive() {
  if (!statusBurstActive) return false;
  // Self-heal against a missed endStatusBurst() call.
  if ((unsigned long)(millis() - statusBurstStartMs) > STATUS_BURST_TIMEOUT_MS) {
    statusBurstActive = false;
    return false;
  }
  return true;
}

// Returns true when the next drip tick will fire within windowMs milliseconds,
// or is already overdue. Callers (e.g. queryNextPICsetting) use this to avoid
// issuing MQTT publishes that would land on top of a drip allocation.
bool dripDueWithinMs(uint32_t windowMs) {
  long timeLeft = (long)(sDripDueAtMs - millis());
  return timeLeft <= (long)windowMs;
}

static bool isDripDeferred() {
  if (isStatusBurstActive()) return true;
  // Post-burst cooldown window still open?
  // Use signed diff for millis() rollover safety.
  if (burstCooldownUntilMs != 0 && (long)(millis() - burstCooldownUntilMs) < 0) {
    return true;
  }
  return false;
}

// MQTT auto-discovery verification implementation lives below the MQTTclient +
// NodeId static declarations (search for "MQTT auto-discovery verification (ADR-062").

static            PubSubClient MQTTclient(wifiClient);

int8_t            reconnectAttempts = 0;
char              lastMQTTtimestamp[15] = "";

enum states_of_MQTT { MQTT_STATE_INIT, MQTT_STATE_TRY_TO_CONNECT, MQTT_STATE_IS_CONNECTED, MQTT_STATE_WAIT_CONNECTION_ATTEMPT, MQTT_STATE_WAIT_FOR_RECONNECT, MQTT_STATE_ERROR };
enum states_of_MQTT stateMQTT = MQTT_STATE_INIT;

static char       MQTTclientId[MQTT_ID_MAX_LEN];
static char       MQTTPubNamespace[MQTT_NAMESPACE_MAX_LEN];
static char       MQTTSubNamespace[MQTT_NAMESPACE_MAX_LEN];
static char       NodeId[MQTT_ID_MAX_LEN];

// =====================================================================
// MQTT auto-discovery verification (ADR-062, TASK-349)
// State machine extracted to mqtt_discovery_verify.cpp under TASK-363.
// MQTTstuff.ino keeps only the small accessor surface that the extracted
// TU calls back into (state/settings access, MQTT client ops, logging).
// Public API: startDiscoveryVerification(), isDiscoveryVerificationActive(),
// tickDiscoveryVerification(), handleDiscoveryVerifyMessage()
//   - all declared in mqtt_discovery_verify.h.
// =====================================================================

// Pin the numeric mapping between VerifyOutcome (OTGW-firmware.h) and the
// uint8_t codes used across the extracted TU boundary. If the enum ever
// gets reordered this will fail to compile.
static_assert((uint8_t)VerifyOutcome::UNKNOWN            == 0, "VerifyOutcome UNKNOWN must be 0");
static_assert((uint8_t)VerifyOutcome::CLEAN              == 1, "VerifyOutcome CLEAN must be 1");
static_assert((uint8_t)VerifyOutcome::MISSING            == 2, "VerifyOutcome MISSING must be 2");
static_assert((uint8_t)VerifyOutcome::ABORTED_HEAP       == 3, "VerifyOutcome ABORTED_HEAP must be 3");
static_assert((uint8_t)VerifyOutcome::ABORTED_DISCONNECT == 4, "VerifyOutcome ABORTED_DISCONNECT must be 4");

// Counter increment shim for mqtt_configuratie.cpp (ADR-044: that TU does not
// include OTGW-firmware.h / the state global). Must be called from each
// stream*Discovery() helper after a successful endPublish().
void incPublishedTopicCount() {
  state.discovery.iPublishedTopicCount++;
}

// Returns number of msgids currently pending discovery publication.
uint16_t countPendingDiscoveryIds() {
  uint16_t n = 0;
  for (uint8_t g = 0; g < 8; g++) {
    uint32_t m = MQTTautoCfgPendingMap[g];
    while (m) { n += (m & 1u); m >>= 1; }
  }
  return n;
}

// ---------------------------------------------------------------------
// Discovery-verify TU accessors (TASK-363). Implemented here because
// mqtt_discovery_verify.cpp is a separate translation unit and cannot
// include OTGW-firmware.h (the header defines sketch-level globals that
// would cause ODR violations when pulled into a second TU). All access
// to state/settings/MQTTclient/NodeId/etc. from the extracted file goes
// through these narrow bridges.
// ---------------------------------------------------------------------
bool        verifyAccessorMqttConnected()            { return state.mqtt.bConnected; }
bool        verifyAccessorPicFlashing()              { return isFlashing(); }
bool        verifyAccessorNtpTimeSet()               { return isNTPtimeSet(); }
uint32_t    verifyAccessorUptimeSeconds()            { return state.uptime.iSeconds; }
uint32_t    verifyAccessorPublishedTopicCount()      { return state.discovery.iPublishedTopicCount; }
uint16_t    verifyAccessorCountPendingDiscoveryIds() { return countPendingDiscoveryIds(); }
const char* verifyAccessorHaPrefix()                 { return CSTR(settings.mqtt.sHaprefix); }
const char* verifyAccessorNodeId()                   { return NodeId; }

void        verifyAccessorSetOutcome(uint8_t outcome) {
  state.discovery.eLastOutcome = (VerifyOutcome)outcome;
}
uint8_t     verifyAccessorGetOutcome() {
  return (uint8_t)state.discovery.eLastOutcome;
}
void        verifyAccessorIncVerifyRunCount()         { state.discovery.iVerifyRunCount++; }
void        verifyAccessorIncRepublishTriggeredCount(){ state.discovery.iRepublishTriggeredCount++; }
void        verifyAccessorSetLastVerifyEpoch(uint32_t epoch)   { state.discovery.iLastVerifyEpoch = epoch; }
void        verifyAccessorSetLastMissingCount(uint16_t missing){ state.discovery.iLastMissingCount = missing; }
void        verifyAccessorSetLastOrphanCount(uint16_t orphan)  { state.discovery.iLastOrphanCount = orphan; }
void        verifyAccessorMarkAllMQTTConfigPending()           { markAllMQTTConfigPending(); }

bool        verifyAccessorSetMqttBufferSize(uint16_t sizeBytes) {
  return MQTTclient.setBufferSize(sizeBytes);
}
bool        verifyAccessorRestoreMqttBufferSize() {
  return MQTTclient.setBufferSize(MQTT_CLIENT_BUFFER_SIZE);
}
bool        verifyAccessorMqttSubscribe(const char *topic) {
  return MQTTclient.subscribe(topic, 0);
}
bool        verifyAccessorMqttUnsubscribe(const char *topic) {
  return MQTTclient.unsubscribe(topic);
}

// Logging bridge: accept a pre-formatted RAM string and dispatch through
// the usual DebugTln helper so the telnet BOL prefix is preserved.
// The verify TU pre-formats with snprintf_P because it cannot include
// Debug.h (ODR violation on function bodies defined there).
void verifyAccessorLogLine(const char* ramMessage) {
  if (ramMessage != nullptr) DebugTln(ramMessage);
}

static bool writeMqttChunk(const char *data, size_t len)
{
  const size_t CHUNK_SIZE = 128;
  size_t pos = 0;
  while (pos < len) {
    size_t chunkLen = (len - pos) > CHUNK_SIZE ? CHUNK_SIZE : (len - pos);
    size_t written = MQTTclient.write(reinterpret_cast<const uint8_t*>(data + pos), chunkLen);
    if (written != chunkLen) {
      PrintMQTTError();
      return false;
    }
    pos += chunkLen;
    feedWatchDog();
  }
  return true;
}

static bool writeMqttProgmemChunk(PGM_P data, size_t len)
{
  char stage[MQTT_PROGMEM_STAGE_LEN + 1];
  size_t pos = 0;
  while (pos < len) {
    size_t chunkLen = (len - pos) > MQTT_PROGMEM_STAGE_LEN ? MQTT_PROGMEM_STAGE_LEN : (len - pos);
    for (size_t i = 0; i < chunkLen; i++) {
      stage[i] = static_cast<char>(pgm_read_byte(data + pos + i));
    }
    size_t written = MQTTclient.write(reinterpret_cast<const uint8_t*>(stage), chunkLen);
    if (written != chunkLen) {
      PrintMQTTError();
      return false;
    }
    pos += chunkLen;
    feedWatchDog();
  }
  return true;
}

static bool beginMqttPublish(const char *topic, size_t len, bool retain)
{
  if (!MQTTclient.beginPublish(topic, len, retain)) {
    PrintMQTTError();
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Forwarding functions for MqttJsonWriter (MQTTstuff.h).
// Bridge to the file-static writeMqttChunk helpers and MQTTclient.
// ---------------------------------------------------------------------------
bool writeMqttChunkExt(const char *data, size_t len) {
  return writeMqttChunk(data, len);
}

bool writeMqttProgmemChunkExt(PGM_P data, size_t len) {
  return writeMqttProgmemChunk(data, len);
}

bool writeMqttByteExt(uint8_t b) {
  return MQTTclient.write(b) == 1;
}

// ---------------------------------------------------------------------------
// The streaming JSON helpers, compose functions, topic builders, and the
// public streamXxxDiscovery() API live in MQTTHaDiscovery.cpp to avoid
// Arduino's auto-prototype generator mangling custom-type parameters.
// ---------------------------------------------------------------------------

static void buildNamespace(char *dest, size_t destSize, const char *base, const char *segment, const char *node) {
  if (!dest) return;
  dest[0] = '\0';
  if (!base || !segment || !node) return;
  strlcpy(dest, base, destSize);
  size_t len = strlen(dest);
  if (len > 0 && dest[len - 1] == '/') dest[len - 1] = '\0';
  strlcat(dest, "/", destSize);
  strlcat(dest, segment, destSize);
  strlcat(dest, "/", destSize);
  strlcat(dest, node, destSize);
}

static size_t copyMQTTPayloadToBuffer(const byte *payload, unsigned int length, char *dest, size_t destSize) {
  if (!dest || destSize == 0) return 0;
  dest[0] = '\0';
  if (!payload || length == 0) return 0;

  const size_t copyLen = (length < (destSize - 1)) ? length : (destSize - 1);
  memcpy(dest, payload, copyLen);
  dest[copyLen] = '\0';
  return copyLen;
}

static bool readMQTTTopicToken(const char *&cursor, char *token, size_t tokenSize) {
  if (!cursor || !token || tokenSize == 0) return false;

  while (*cursor == '/') {
    cursor++;
  }
  if (*cursor == '\0') {
    token[0] = '\0';
    return false;
  }

  size_t len = 0;
  while (*cursor != '\0' && *cursor != '/') {
    if (len < (tokenSize - 1)) {
      token[len++] = *cursor;
    }
    cursor++;
  }
  token[len] = '\0';
  return (len > 0);
}

//set command list
// Move strings to PROGMEM to save RAM
const char s_raw[] PROGMEM = "raw";
const char s_temp[] PROGMEM = "temp";
const char s_on[] PROGMEM = "on";
const char s_level[] PROGMEM = "level";
const char s_function[] PROGMEM = "function";
const char s_reset[] PROGMEM = "reset";
const char s_empty[] PROGMEM = "";

const char s_cmd_command[] PROGMEM = "command";
const char s_cmd_setpoint[] PROGMEM = "setpoint";
const char s_cmd_constant[] PROGMEM = "constant";
const char s_cmd_outside[] PROGMEM = "outside";
const char s_cmd_outside_temp[] PROGMEM = "outside_temp";
const char s_cmd_hotwater[] PROGMEM = "hotwater";
const char s_cmd_gatewaymode[] PROGMEM = "gatewaymode";
const char s_cmd_setback[] PROGMEM = "setback";
const char s_cmd_maxchsetpt[] PROGMEM = "maxchsetpt";
const char s_cmd_maxdhwsetpt[] PROGMEM = "maxdhwsetpt";
const char s_cmd_maxmodulation[] PROGMEM = "maxmodulation";
const char s_cmd_ctrlsetpt[] PROGMEM = "ctrlsetpt";
const char s_cmd_ctrlsetpt2[] PROGMEM = "ctrlsetpt2";
const char s_cmd_chenable[] PROGMEM = "chenable";
const char s_cmd_chenable2[] PROGMEM = "chenable2";
const char s_cmd_ventsetpt[] PROGMEM = "ventsetpt";
const char s_cmd_temperaturesensor[] PROGMEM = "temperaturesensor";
const char s_cmd_addalternative[] PROGMEM = "addalternative";
const char s_cmd_delalternative[] PROGMEM = "delalternative";
const char s_cmd_unknownid[] PROGMEM = "unknownid";
const char s_cmd_knownid[] PROGMEM = "knownid";
const char s_cmd_priomsg[] PROGMEM = "priomsg";
const char s_cmd_setresponse[] PROGMEM = "setresponse";
const char s_cmd_clearrespons[] PROGMEM = "clearrespons";
const char s_cmd_resetcounter[] PROGMEM = "resetcounter";
const char s_cmd_ignoretransitations[] PROGMEM = "ignoretransitations";
const char s_cmd_overridehb[] PROGMEM = "overridehb";
const char s_cmd_forcethermostat[] PROGMEM = "forcethermostat";
const char s_cmd_voltageref[] PROGMEM = "voltageref";
const char s_cmd_debugptr[] PROGMEM = "debugptr";
const char s_cmd_gpioa[] PROGMEM = "gpioa";
const char s_cmd_gpiob[] PROGMEM = "gpiob";
const char s_cmd_leda[] PROGMEM = "leda";
const char s_cmd_ledb[] PROGMEM = "ledb";
const char s_cmd_ledc[] PROGMEM = "ledc";
const char s_cmd_ledd[] PROGMEM = "ledd";
const char s_cmd_lede[] PROGMEM = "lede";
const char s_cmd_ledf[] PROGMEM = "ledf";
const char s_cmd_resetgateway[] PROGMEM = "resetgateway";

// ADR-096: subtopic names "thermostat" and "boiler" are inlined into
// snprintf_P calls in publishToSourceTopic(). The earlier table-based
// dispatch (mqttSourceKeys[] / resolveSourceIndex / copySourceTableEntry)
// was removed when worldview routing replaced the 1-of-N source mapping.

const char s_otgw_TT[] PROGMEM = "TT";
const char s_otgw_TC[] PROGMEM = "TC";
const char s_otgw_OT[] PROGMEM = "OT";
const char s_otgw_HW[] PROGMEM = "HW";
const char s_otgw_GW[] PROGMEM = "GW";
const char s_otgw_SB[] PROGMEM = "SB";
const char s_otgw_SH[] PROGMEM = "SH";
const char s_otgw_SW[] PROGMEM = "SW";
const char s_otgw_MM[] PROGMEM = "MM";
const char s_otgw_CS[] PROGMEM = "CS";
const char s_otgw_C2[] PROGMEM = "C2";
const char s_otgw_CH[] PROGMEM = "CH";
const char s_otgw_H2[] PROGMEM = "H2";
const char s_otgw_VS[] PROGMEM = "VS";
const char s_otgw_TS[] PROGMEM = "TS";
const char s_otgw_AA[] PROGMEM = "AA";
const char s_otgw_DA[] PROGMEM = "DA";
const char s_otgw_UI[] PROGMEM = "UI";
const char s_otgw_KI[] PROGMEM = "KI";
const char s_otgw_PM[] PROGMEM = "PM";
const char s_otgw_SR[] PROGMEM = "SR";
const char s_otgw_CR[] PROGMEM = "CR";
const char s_otgw_RS[] PROGMEM = "RS";
const char s_otgw_IT[] PROGMEM = "IT";
const char s_otgw_OH[] PROGMEM = "OH";
const char s_otgw_FT[] PROGMEM = "FT";
const char s_otgw_VR[] PROGMEM = "VR";
const char s_otgw_DP[] PROGMEM = "DP";
const char s_otgw_GA[] PROGMEM = "GA";
const char s_otgw_GB[] PROGMEM = "GB";
const char s_otgw_LA[] PROGMEM = "LA";
const char s_otgw_LB[] PROGMEM = "LB";
const char s_otgw_LC[] PROGMEM = "LC";
const char s_otgw_LD[] PROGMEM = "LD";
const char s_otgw_LE[] PROGMEM = "LE";
const char s_otgw_LF[] PROGMEM = "LF";

struct MQTT_set_cmd_t
{
    PGM_P setcmd;
    PGM_P otgwcmd;
    PGM_P ottype;
};


const MQTT_set_cmd_t setcmds[] PROGMEM = {
  {   s_cmd_command, s_empty, s_raw },
  {   s_cmd_setpoint, s_otgw_TT, s_temp },
  {   s_cmd_constant, s_otgw_TC, s_temp },
  {   s_cmd_outside, s_otgw_OT, s_temp },
  {   s_cmd_outside_temp, s_otgw_OT, s_temp },
  {   s_cmd_hotwater, s_otgw_HW, s_on },  // HW=0 (off), HW=1 (on), HW=P (DHW push), HW=<other> (auto)
  {   s_cmd_gatewaymode, s_otgw_GW, s_on },
  {   s_cmd_setback, s_otgw_SB, s_temp },
  {   s_cmd_maxchsetpt, s_otgw_SH, s_temp },
  {   s_cmd_maxdhwsetpt, s_otgw_SW, s_temp },
  {   s_cmd_maxmodulation, s_otgw_MM, s_level },        
  {   s_cmd_ctrlsetpt, s_otgw_CS, s_temp },        
  {   s_cmd_ctrlsetpt2, s_otgw_C2, s_temp },        
  {   s_cmd_chenable, s_otgw_CH, s_on },        
  {   s_cmd_chenable2, s_otgw_H2, s_on },        
  {   s_cmd_ventsetpt, s_otgw_VS, s_level },
  {   s_cmd_temperaturesensor, s_otgw_TS, s_function },
  {   s_cmd_addalternative, s_otgw_AA, s_function },
  {   s_cmd_delalternative, s_otgw_DA, s_function },
  {   s_cmd_unknownid, s_otgw_UI, s_function },
  {   s_cmd_knownid, s_otgw_KI, s_function },
  {   s_cmd_priomsg, s_otgw_PM, s_function },
  {   s_cmd_setresponse, s_otgw_SR, s_function },
  {   s_cmd_clearrespons, s_otgw_CR, s_function },
  {   s_cmd_resetcounter, s_otgw_RS, s_function },
  {   s_cmd_ignoretransitations, s_otgw_IT, s_function },
  {   s_cmd_overridehb, s_otgw_OH, s_function },
  {   s_cmd_forcethermostat, s_otgw_FT, s_function },
  {   s_cmd_voltageref, s_otgw_VR, s_function },
  {   s_cmd_debugptr, s_otgw_DP, s_function },
  // GPIO / LED / clock / reset — parity with HA Core opentherm_gw named services
  {   s_cmd_gpioa, s_otgw_GA, s_function },        // GA=0..7 (GPIO A function)
  {   s_cmd_gpiob, s_otgw_GB, s_function },        // GB=0..7 (GPIO B function)
  {   s_cmd_leda, s_otgw_LA, s_function },         // LA=B/C/E/F/H/M/O/P/R/T/W/X
  {   s_cmd_ledb, s_otgw_LB, s_function },
  {   s_cmd_ledc, s_otgw_LC, s_function },
  {   s_cmd_ledd, s_otgw_LD, s_function },
  {   s_cmd_lede, s_otgw_LE, s_function },
  {   s_cmd_ledf, s_otgw_LF, s_function },
  {   s_cmd_resetgateway, s_empty, s_reset },      // hardware PIC reset, payload ignored
} ;

const int nrcmds = sizeof(setcmds) / sizeof(setcmds[0]);

static int findMQTTSetCommandIndex(const char *topicToken)
{
  if (!topicToken) return -1;

  for (int i = 0; i < nrcmds; i++) {
    PGM_P pSetCmd = (PGM_P)pgm_read_ptr(&setcmds[i].setcmd);
    if (strcasecmp_P(topicToken, pSetCmd) == 0) {
      return i;
    }

    PGM_P pOtgwCmd = (PGM_P)pgm_read_ptr(&setcmds[i].otgwcmd);
    if (strlen_P(pOtgwCmd) > 0 && strcasecmp_P(topicToken, pOtgwCmd) == 0) {
      return i;
    }
  }

  return -1;
}

//===========================================================================================
// Clean MQTT disconnect for reboot path. MQTTclient is file-static so we expose
// a wrapper; called from prepareForReboot() in helperStuff.ino before ESP.restart().
// Arduino Core 3.1.0 removed implicit WiFiClient::stopAll() from the Update path,
// so without this the TCP socket lingers in lwIP and the next boot comes up in a
// weird half-state (WiFi associated but services non-responsive).
void doMqttDisconnect() {
  if (MQTTclient.connected()) MQTTclient.disconnect();
}

//===========================================================================================
void startMQTT()
{
  if (!settings.mqtt.bEnable) return;

  // Eliminate the TCP_SND_BUF temporary copy in WiFiClient (~1072 bytes saved).
  // With sync mode, writes flush directly to lwIP without intermediate buffering.
  // setSync is ESP8266-only: the ESP32 NetworkClient has no equivalent API, and
  // ESP32 already streams writes without the same intermediate copy.
#ifdef ARDUINO_ARCH_ESP8266
  wifiClient.setSync(true);
#endif
  wifiClient.setNoDelay(true);

  // Outbound publishes stream via beginPublish/write/endPublish.
  // Keep only enough client buffer for inbound subscribed topics and payloads.
  MQTTclient.setBufferSize(MQTT_CLIENT_BUFFER_SIZE);
  
  stateMQTT = MQTT_STATE_INIT;
  // Rebuild namespaces (also needed when top-topic changes).
  strlcpy(NodeId, CSTR(settings.mqtt.sUniqueid), sizeof(NodeId));
  buildNamespace(MQTTPubNamespace, sizeof(MQTTPubNamespace), CSTR(settings.mqtt.sTopTopic), "value", NodeId);
  buildNamespace(MQTTSubNamespace, sizeof(MQTTSubNamespace), CSTR(settings.mqtt.sTopTopic), "set", NodeId);
  // Fresh start: clear done/pending bitmaps, then queue only non-OT configs.
  // OT ID configs publish JIT as each MsgID is received on the bus (ADR-100).
  clearMQTTConfigDone();
  clearMQTTConfigPending();
  publishNonOTDiscoveryConfigs();
  handleMQTT(); //initialize the MQTT statemachine
}

bool bHAcycle = false;

// ===========================================================================
// SAT MQTT sub-command dispatch table (ADR-078)
// ===========================================================================
// Replaces the 76-branch chained strcasecmp_P that used to live inline inside
// handleMQTTcallback. Each entry is either a handler delegate (for commands
// whose payload needs typed parsing) or a setting-key passthrough (for
// commands that just forward the payload to updateSetting). Sub-token commands
// (sat/area/<idx>, sat/zone/<idx>/<cmd>) stay outside the table as special
// cases because they consume additional topic segments. See ADR-078 for the
// rationale + the kV2Routes[] precedent this mirrors.

// --- Adapters: wrap bool-returning and no-arg handlers into the uniform
// void(const char*) signature expected by the dispatch table. ---
static void _satTargetTempCmd(const char* v)     { satHandleTargetTemp(v); }
static void _satExtTempCmd(const char* v)        { satHandleExternalTemp(v); }
static void _satExtOutdoorCmd(const char* v)     { satHandleExternalOutdoor(v); }
static void _satPvSurplusCmd(const char* v)      { satHandlePvSurplus(v); }                  // TASK-640
static void _satPvBoostEnabledCmd(const char* v) { satHandlePvBoostEnabled(v); }             // TASK-640
static void _satHumidityCmd(const char* v)       { satHandleHumidity(v); }
static void _satSunElevationCmd(const char* v)   { satHandleSunElevation(v); }

static void _satOvpStartCmd(const char* v)       { (void)v; satOvpStartCalibration(); }
static void _satOvpStopCmd(const char* v)        { (void)v; satOvpStopCalibration(); }
static void _satResetIntegralCmd(const char* v)  { (void)v; satResetIntegral(); }
static void _satFlushCmd(const char* v) {
  (void)v;
  satFlushShortLivedData();
  MQTTDebugln(F("SAT: flushed short-lived data via MQTT"));
}

static void _satWindowCmd(const char* v) {
  bool isOpen = (strcasecmp_P(v, PSTR("open")) == 0 ||
                 strcasecmp_P(v, PSTR("1"))    == 0 ||
                 strcasecmp_P(v, PSTR("ON"))   == 0);
  satHandleWindow(isOpen);
}

static void _satValvesOpenCmd(const char* v) {
  state.sat.bValvesOpen = (strcasecmp_P(v, PSTR("true")) == 0 ||
                           strcmp_P   (v, PSTR("1"))    == 0 ||
                           strcasecmp_P(v, PSTR("open")) == 0);
  DebugTf(PSTR("SAT: valves_open = %s\r\n"), state.sat.bValvesOpen ? "true" : "false");
}

typedef void (*SatMqttHandler)(const char*);

struct SatMqttCmdEntry {
  const char*    cmd;         // command token (lower-case). Compared with strcasecmp.
  const char*    settingKey;  // NULL when handler is used
  SatMqttHandler handler;     // NULL when settingKey is used
};

// Dispatch table. Terminated by a { nullptr, nullptr, nullptr } sentinel,
// same convention as kV2Routes[] in restAPI.ino. Placed in PROGMEM so the
// ~960 B of struct entries lives in flash, not DRAM. The string literals
// reachable via cmd/settingKey remain in their default placement (rodata
// .str pools, which the ESP8266 linker script maps to .irom0.text). Read
// each row with memcpy_P in the dispatch loop below.
static const SatMqttCmdEntry kSatMqttCmds[] PROGMEM = {
  // --- Typed handlers (payload parsing lives in the handler) ---
  { "target",                 nullptr,                _satTargetTempCmd },
  { "indoor_temp",            nullptr,                _satExtTempCmd },
  { "outdoor_temp",           nullptr,                _satExtOutdoorCmd },
  // TASK-640: PV-surplus boost. set/<nodeId>/sat/pv_surplus_w (watts),
  //                              set/<nodeId>/sat/pv_boost_enabled (0/1).
  { "pv_surplus_w",           nullptr,                _satPvSurplusCmd },
  { "pv_boost_enabled",       nullptr,                _satPvBoostEnabledCmd },
  { "enabled",                nullptr,                satHandleEnabled },
  { "control_mode",           nullptr,                satHandleControlMode },
  { "preset",                 nullptr,                satHandlePreset },
  { "humidity",               nullptr,                _satHumidityCmd },
  { "heating_mode",           nullptr,                satHandleHeatingMode },
  { "sun_elevation",          nullptr,                _satSunElevationCmd },
  { "window",                 nullptr,                _satWindowCmd },
  { "valves_open",            nullptr,                _satValvesOpenCmd },
  { "ovp_start",              nullptr,                _satOvpStartCmd },
  { "ovp_stop",               nullptr,                _satOvpStopCmd },
  { "reset_integral",         nullptr,                _satResetIntegralCmd },
  { "flush",                  nullptr,                _satFlushCmd },

  // --- Direct setting-key passthrough ---
  { "overshoot_margin",       "SATovershootmargin",   nullptr },
  { "heating_system",         "SATsystem",            nullptr },
  { "manufacturer",           "SATmanufacturer",      nullptr },
  { "max_modulation",         "SATmaxmodulation",     nullptr },
  { "dhw_setpoint",           "SATdhwsetpoint",       nullptr },
  { "dhw_enabled",            "SATdhwenabled",        nullptr },
  { "dhw_enable",             "SATdhwenable",         nullptr }, // TASK-516: master DHW enable (HW= command, gated on MsgID 3 HB3)
  { "interval",               "SATinterval",          nullptr },
  { "ovp_value",              "SATovpvalue",          nullptr },
  { "ovp_enabled",            "SATovpenabled",        nullptr },
  { "push_setpoint",          "SATpushsetpoint",      nullptr },
  { "flame_off_offset",       "SATflameoffset",       nullptr },
  { "force_pwm",              "SATforcepwm",          nullptr },
  { "flow_offset",            "SATflowoffset",        nullptr },
  { "summer_simmer",          "SATsummersimmer",      nullptr },
  { "summer_threshold",       "SATsummerthreshold",   nullptr },
  { "summer_min_hours",       "SATsummerminhours",    nullptr },
  { "thermal_comfort",        "SATthermalcomfort",    nullptr },
  { "humidity_timeout_s",     "SAThumiditytimeout",   nullptr },
  { "comfort_adjust",         "SATcomfortadjust",     nullptr },
  { "comfort_humidity",       "SATcomforthumidity",   nullptr },
  { "comfort_max_offset",     "SATcomfortmaxoffset",  nullptr },
  { "simulation",             "SATsimulation",        nullptr },
  { "ble_enable",             "SATbleenable",         nullptr },
  { "ble_failover",           "SATblefailover",       nullptr },
  { "ble_mac",                "SATblemac",            nullptr },
  { "ble_interval",           "SATbleinterval",       nullptr },
  { "preset_sync",            "SATpresetsync",        nullptr },
  { "preset_sync_topic",      "SATpresetsynctopic",   nullptr },
  { "multi_area",             "SATmultiarea",         nullptr },
  { "multi_area_count",       "SATmultiareacount",    nullptr },
  { "auto_tune",              "SATautotune",          nullptr },
  { "auto_tune_rate",         "SATautotunerate",      nullptr },
  { "heating_curve",          "SATcoefficient",       nullptr },
  { "deadband",               "SATdeadband",          nullptr },
  { "mod_sup_delay",          "SATmodsupdelay",       nullptr },
  { "mod_sup_offset",         "SATmodsupoffset",      nullptr },
  { "boiler_capacity",        "SATboilercapacity",    nullptr },
  { "target_temp_step",       "SATtempstep",          nullptr },
  { "min_pressure",           "SATminpressure",       nullptr },
  { "max_pressure",           "SATmaxpressure",       nullptr },
  { "max_pressure_drop",      "SATmaxpressdrop",      nullptr },
  { "preset_comfort",         "SATpresetcomfort",     nullptr },
  { "preset_eco",             "SATpreseteco",         nullptr },
  { "preset_away",            "SATpresetaway",        nullptr },
  { "preset_sleep",           "SATpresetsleep",       nullptr },
  { "preset_activity",        "SATpresetactivity",    nullptr },
  { "preset_home",            "SATpresethome",        nullptr },
  { "solar_gain",             "SATsolargain",         nullptr },
  { "window_detection",       "SATwindowdetect",      nullptr },
  { "pwm_auto_switch",        "SATpwmautoswitch",     nullptr },
  { "sensor_max_age",         "SATsensormaxage",      nullptr },
  { "error_monitoring",       "SATerrormon",          nullptr },
  { "auto_gains_value",       "SATautogains",         nullptr },
  { "cycles_per_hour",        "SATcyclesperhour",     nullptr },
  { "valve_offset",           "SATvalveoffset",       nullptr },
  { "solar_freeze_integral",  "SATsolarfreezeint",    nullptr },
  { "zone_count",             "SATzonecount",         nullptr },
  { "zone_timeout_s",         "SATzonetimeout",       nullptr },
  { "solar_min_elevation",    "SATsolarminelev",      nullptr },
  { "flush_threshold_h",      "SATflushtreshold",     nullptr },
  // TASK-640: PV-surplus boost settings (numeric set-commands).
  { "pv_boost_threshold_w",      "SATpvboostthresholdw",   nullptr },
  { "pv_boost_hold_s",           "SATpvboostholds",        nullptr },
  { "pv_boost_delta_c",          "SATpvboostdeltac",       nullptr },
  { "pv_boost_max_indoor_c",     "SATpvboostmaxindoorc",   nullptr },
  { "pv_boost_max_duration_min", "SATpvboostmaxdurationmin", nullptr },

  { nullptr, nullptr, nullptr } // sentinel
};

// Returns true when the sub-command was found in the table and dispatched.
// kSatMqttCmds is PROGMEM; copy each row into a stack-local before reading
// its fields (the pointer values inside the row point at .rodata-string-pool
// content which is itself in flash; once memcpy'd to RAM they can be passed
// to strcasecmp / updateSetting like any normal const char*).
static bool dispatchSatMqttCmd(const char* cmd, const char* payload) {
  for (size_t i = 0; ; i++) {
    SatMqttCmdEntry e;
    memcpy_P(&e, &kSatMqttCmds[i], sizeof(SatMqttCmdEntry));
    if (e.cmd == nullptr) break;  // sentinel
    if (strcasecmp(cmd, e.cmd) == 0) {
      if (e.handler) {
        e.handler(payload);
      } else if (e.settingKey) {
        updateSetting(e.settingKey, payload);
      }
      return true;
    }
  }
  return false;
}

// handles MQTT subscribe incoming stuff
void handleMQTTcallback(char* topic, byte* payload, unsigned int length) {

  if (state.debug.bMQTT) {
    DebugT(F("Message arrived on topic [")); Debug(topic); Debug(F("] = ["));
    for (unsigned int i = 0; i < length; i++) {
      Debug((char)payload[i]);
    }
    Debug(F("] (")); Debug(length); Debug(F(")")); Debugln(); DebugFlush();
  }

  // TASK-410 / ADR-084: catch retained payloads on the six deprecated OT-bus
  // topics before they reach the normal set/* dispatcher. The helper clears
  // the retained entry on the broker and unsubscribes for that specific topic.
  // Payload content is ignored: the mere arrival of the message is the trigger.
  if (mqttV2MigrationHandleIfDeprecated(topic)) return;

  // Verify-window retained-config filter (ADR-062, TASK-349, TASK-357).
  // TASK-363: extracted to mqtt_discovery_verify.cpp. If the verify window is
  // active and the topic matches our <haprefix>/ prefix, the extracted filter
  // consumes the message and returns true; we must not fall through to the
  // OT command dispatcher in that case.
  if (handleDiscoveryVerifyMessage(topic, length)) return;

  //detect home assistant going down...
  char msgPayload[128];
  copyMQTTPayloadToBuffer(payload, length, msgPayload, sizeof(msgPayload));

  // Check if payload was truncated — refuse to forward truncated commands to PIC
  if (length >= sizeof(msgPayload)) {
    DebugTf(PSTR("WARNING: MQTT payload truncated (%u > %u), skipping command\r\n"),
            length, (unsigned int)(sizeof(msgPayload) - 1));
    return;
  }

  if (strcasecmp_P(topic, PSTR("homeassistant/status")) == 0) {
    //incoming message on status, detect going down
    if (!settings.mqtt.bHaRebootDetect) {
      //So if the HA reboot detection is turned of, we will just look for HA going online.
      //This means everytime there is "online" message, we will restart MQTT configuration, including the HA Auto Discovery. 
      bHAcycle = true; 
    }
    if (strcasecmp_P(msgPayload, PSTR("offline")) == 0){
      //home assistant went down
      DebugTln(F("Home Assistant went offline!"));
      bHAcycle = true; //set flag, so it triggers when it goes back online
    } else if ((strcasecmp_P(msgPayload, PSTR("online")) == 0) && bHAcycle){
      DebugTln(F("Home Assistant went online!"));
      bHAcycle = false; //clear flag, so it does not trigger again
      // HA restart does not affect broker retained messages; no discovery republish needed.
      // Retained configs are already on the broker; HA reads them via homeassistant/# subscription.
    } else {
      DebugTf(PSTR("Home Assistant Status=[%s] and HA cycle status [%s]\r\n"), msgPayload, CBOOLEAN(bHAcycle)); 
    }
  }


  // parse the incoming topic and execute commands
  char otgwcmd[51]={0};
  static char topicToken[MQTT_ID_MAX_LEN];
  topicToken[0] = '\0';
  const char *topicCursor = topic;

  //first check toptopic part, it can include the seperator, e.g. "myHome/OTGW" or "OTGW""
  const size_t topTopicLen = strlen(CSTR(settings.mqtt.sTopTopic));
  if (strncmp(topicCursor, CSTR(settings.mqtt.sTopTopic), topTopicLen) != 0) {
    MQTTDebugln(F("MQTT: wrong top topic"));
    return;
  } else {
    //remove the top topic part
    MQTTDebugTf(PSTR("Parsing topic: %s/"), CSTR(settings.mqtt.sTopTopic));
    topicCursor += topTopicLen;
    while (*topicCursor == '/') {
      topicCursor++;
    }
  }
  // naming convention /set/<node id>/<command>
  if (!readMQTTTopicToken(topicCursor, topicToken, sizeof(topicToken))) {
    MQTTDebugln(F("MQTT: missing 'set' token"));
    return;
  }
  MQTTDebugf(PSTR("%s/"), topicToken);
  if (strcasecmp_P(topicToken, PSTR("set")) == 0) {
    if (!readMQTTTopicToken(topicCursor, topicToken, sizeof(topicToken))) {
      MQTTDebugln(F("MQTT: missing node-id token"));
      return;
    }
    MQTTDebugf(PSTR("%s/"), topicToken);
    if (strcasecmp(topicToken, NodeId) == 0) {
      if (!readMQTTTopicToken(topicCursor, topicToken, sizeof(topicToken))) {
        MQTTDebugln(F("MQTT: missing command token"));
        return;
      }
      MQTTDebugf(PSTR("%s"), topicToken);
      if (topicToken[0] != '\0') {
        // --- SAT MQTT commands: set/<nodeId>/sat/<sub-command> ---
        if (strcasecmp_P(topicToken, PSTR("sat")) == 0) {
          // Must hold the longest sub-command name + NUL. Current longest is
          // "solar_freeze_integral" at 21 chars (TASK-292). 24 gives headroom
          // for future SAT MQTT command names without another resize.
          char satSubCmd[24];
          if (readMQTTTopicToken(topicCursor, satSubCmd, sizeof(satSubCmd))) {
            MQTTDebugTf(PSTR("MQTT SAT cmd: %s [%s]\r\n"), satSubCmd, msgPayload);
            // ADR-078 dispatch. Sub-token commands (area, zone) cannot fit the
            // uniform (cmd, payload) table because they consume additional
            // topic segments, so they stay as special cases before the table scan.
            if (strcasecmp_P(satSubCmd, PSTR("area")) == 0) {
              char areaIdx[4];
              if (readMQTTTopicToken(topicCursor, areaIdx, sizeof(areaIdx))) {
                int idx = atoi(areaIdx);
                if (idx >= 0 && idx < 4) {
                  satHandleAreaTemp((uint8_t)idx, msgPayload);
                } else {
                  MQTTDebugTf(PSTR("SAT: area index out of range [%s]\r\n"), areaIdx);
                }
              }
            } else if (strcasecmp_P(satSubCmd, PSTR("zone")) == 0) {
              char zoneIdx[4];
              if (readMQTTTopicToken(topicCursor, zoneIdx, sizeof(zoneIdx))) {
                int zn = atoi(zoneIdx);
                char zoneCmd[16];
                if (readMQTTTopicToken(topicCursor, zoneCmd, sizeof(zoneCmd))) {
                  if (strcasecmp_P(zoneCmd, PSTR("room_temp")) == 0) {
                    satHandleZoneRoomTemp((uint8_t)zn, msgPayload);
                  } else if (strcasecmp_P(zoneCmd, PSTR("setpoint")) == 0) {
                    satHandleZoneSetpoint((uint8_t)zn, msgPayload);
                  } else {
                    MQTTDebugTf(PSTR("SAT zone: unknown cmd [%s]\r\n"), zoneCmd);
                  }
                }
              }
            } else if (!dispatchSatMqttCmd(satSubCmd, msgPayload)) {
              MQTTDebugTf(PSTR("SAT: unknown sub-command [%s]\r\n"), satSubCmd);
            }
          } else {
            MQTTDebugTln(F("MQTT SAT: missing sub-command"));
          }
          return;
        }
        // --- OTGW32 OT-direct MQTT commands: set/<nodeId>/otgw32/<sub-command> ---
        if (strcasecmp_P(topicToken, PSTR("otgw32")) == 0) {
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
          char otgw32Cmd[20];
          if (readMQTTTopicToken(topicCursor, otgw32Cmd, sizeof(otgw32Cmd))) {
            MQTTDebugTf(PSTR("MQTT OTGW32 cmd: %s [%s]\r\n"), otgw32Cmd, msgPayload);
            float val = atof(msgPayload);
            if (strcasecmp_P(otgw32Cmd, PSTR("room_temp")) == 0) {
              otdMqttSetRoomTemp(val);
            } else if (strcasecmp_P(otgw32Cmd, PSTR("room_setpoint")) == 0) {
              otdMqttSetRoomSetpoint(val);
            } else {
              MQTTDebugTf(PSTR("OTGW32: unknown sub-command [%s]\r\n"), otgw32Cmd);
            }
          } else {
            MQTTDebugTln(F("MQTT OTGW32: missing sub-command"));
          }
#else
          MQTTDebugTln(F("MQTT OTGW32: OT-direct not available on this build"));
#endif
          return;
        }
        // TASK-439: gate generic OTGW command topics on hasOTCommandInterface()
        // (true for PIC OR OTDirect) instead of isPICEnabled(). addCommandToQueue()
        // already fans out to handleOTDirectCommand() on PIC-less builds, so OTGW32/
        // OTDirect targets must accept setpoint/constant/hotwater/outside/ctrlsetpt/
        // gatewaymode/raw command topics. PIC-only behaviour (firmware flashing,
        // PIC availability, PIC settings) stays gated on isPICEnabled() elsewhere.
        if (!hasOTCommandInterface()) {
          DebugTf(PSTR("MQTT command [%s] dropped: no OT command interface available\r\n"), topicToken);
          return;
        }
        const int cmdIndex = findMQTTSetCommandIndex(topicToken);
        if (cmdIndex >= 0) {
          PGM_P pOtgwCmd = (PGM_P)pgm_read_ptr(&setcmds[cmdIndex].otgwcmd);
          PGM_P pOtType = (PGM_P)pgm_read_ptr(&setcmds[cmdIndex].ottype);

          if (pOtType == s_raw){
            //raw command
            snprintf_P(otgwcmd, sizeof(otgwcmd), PSTR("%s"), msgPayload);
            MQTTDebugf(PSTR(" found command, sending payload [%s]\r\n"), otgwcmd);
            addCommandToQueue(otgwcmd, strlen(otgwcmd), true);
          } else if (pOtType == s_reset) {
            // TASK-669 (port of dev TASK-661): payload validation + rate-limit.
            // Hardware PIC reset is disruptive (interrupts any in-flight OT
            // command); the older code ignored payload entirely. Match the
            // HA-discovery payload_press="1" already published by the button
            // entity, and rate-limit to one reset per RESETGATEWAY_COOLDOWN_MS
            // to absorb storms from misconfigured automations.
            if (strcmp_P(msgPayload, PSTR("1")) != 0) {
              MQTTDebugf(PSTR(" command: resetgateway - ignored, payload [%s] != \"1\"\r\n"), msgPayload);
            } else {
              static uint32_t lastResetMs = 0;
              const uint32_t RESETGATEWAY_COOLDOWN_MS = 5000;
              uint32_t now = millis();
              if (lastResetMs != 0 && (uint32_t)(now - lastResetMs) < RESETGATEWAY_COOLDOWN_MS) {
                MQTTDebugf(PSTR(" command: resetgateway - rate-limited (%lu ms cooldown remaining)\r\n"),
                           (unsigned long)(RESETGATEWAY_COOLDOWN_MS - (now - lastResetMs)));
              } else {
                lastResetMs = now;
                MQTTDebugf(PSTR(" found command: resetgateway - resetting PIC\r\n"));
                resetOTGW();
              }
            }
          } else {
            //all other commands are <otgwcmd>=<payload message>
            // Copy command string from Flash to temp buffer for snprintf
            char cmdBuf[10];
            strncpy_P(cmdBuf, pOtgwCmd, sizeof(cmdBuf));
            cmdBuf[sizeof(cmdBuf)-1] = 0; // Ensure null termination

            snprintf_P(otgwcmd, sizeof(otgwcmd), PSTR("%s=%s"), cmdBuf, msgPayload);
            MQTTDebugf(PSTR(" found command, sending payload [%s]\r\n"), otgwcmd);
            addCommandToQueue(otgwcmd, strlen(otgwcmd), true);
          }
        } else {
          //no match found
          MQTTDebugln();
          DebugTf(PSTR("MQTT command [%s] dropped: no matching OTGW command (check topic spelling)\r\n"), topicToken);
        }
      }
    }
  }
}

void sendMQTT(const char* topic, const char *json);

void handleMQTT() 
{  
  if (!settings.mqtt.bEnable) return;
  DECLARE_TIMER_SEC(timerMQTTwaitforconnect, 42, CATCH_UP_MISSED_TICKS);   // wait before trying to connect again
  DECLARE_TIMER_SEC(timerMQTTwaitforretry, 3, CATCH_UP_MISSED_TICKS);     // wait for retry

  //State debug timers
  DECLARE_TIMER_SEC(timerMQTTdebugwaitforreconnect, 13);
  DECLARE_TIMER_SEC(timerMQTTdebugerrorstate, 13);
  DECLARE_TIMER_SEC(timerMQTTdebugwaitconnectionattempt, 1);
  DECLARE_TIMER_SEC(timerMQTTdebugisconnected, 60);
  
  if (MQTTclient.connected()) MQTTclient.loop();  //always do a MQTTclient.loop() first

  // Poll the discovery-verify window closer (ADR-062). Handles timeout,
  // MQTT-disconnect fast-close, and heap-abort.
  tickDiscoveryVerification();

  // TASK-410 / ADR-084: close the V2 retained-cleanup subscribe window a few
  // seconds after MQTT (re)connect, so the six deprecated topics no longer
  // deliver traffic once the initial retained sweep is done.
  mqttV2MigrationTick();

  switch(stateMQTT) 
  {
    case MQTT_STATE_INIT:  
      MQTTDebugTln(F("MQTT State: MQTT Initializing")); 
      WiFi.hostByName(CSTR(settings.mqtt.sBroker), MQTTbrokerIP);  // lookup the MQTTbroker convert to IP
      snprintf_P(MQTTbrokerIPchar, sizeof(MQTTbrokerIPchar), PSTR("%d.%d.%d.%d"), MQTTbrokerIP[0], MQTTbrokerIP[1], MQTTbrokerIP[2], MQTTbrokerIP[3]);
      if (isValidIP(MQTTbrokerIP))  
      {
        MQTTDebugTf(PSTR("[%s] => setServer(%s, %d)\r\n"), CSTR(settings.mqtt.sBroker), MQTTbrokerIPchar, settings.mqtt.iBrokerPort);
        MQTTclient.disconnect();
        MQTTclient.setServer(MQTTbrokerIPchar, settings.mqtt.iBrokerPort);
        MQTTclient.setCallback(handleMQTTcallback);
        // Sync webserver: socket timeout caps the worst-case freeze when the
        // broker is unreachable (PubSubClient.connect() blocks for this long).
        // 5s keeps HTTP/WS responsive during outages; the state machine still
        // backs off cleanly via timerMQTTwaitforretry between attempts and
        // falls back to a 10-minute wait after 5 failures.
        // Accepted sync-blocker — see ADR-108 (sibling of dev ADR-080).
        MQTTclient.setSocketTimeout(5);
        MQTTclient.setKeepAlive(60);      // Set to 60 seconds (default was 15) to reduce reconnections
        uint8_t mac[6]{0};
        WiFi.macAddress(mac);
        snprintf_P(MQTTclientId, sizeof(MQTTclientId), PSTR("%s%02X%02X%02X%02X%02X%02X"), _HOSTNAME, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        //skip try to connect
        reconnectAttempts =0;
        stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
      }
      else
      { // invalid IP, then goto error state
        MQTTDebugTf(PSTR("ERROR: [%s] => is not a valid URL\r\n"), CSTR(settings.mqtt.sBroker));
        stateMQTT = MQTT_STATE_ERROR;
        //DebugTln(F("Next State: MQTT_STATE_ERROR"));
      }    
      RESTART_TIMER(timerMQTTwaitforconnect); 
    break;

    case MQTT_STATE_TRY_TO_CONNECT:
      MQTTDebugTln(F("MQTT State: MQTT try to connect"));
      MQTTDebugTf(PSTR("MQTT server is [%s], IP[%s]\r\n"), settings.mqtt.sBroker, MQTTbrokerIPchar);
      DebugTf(PSTR("[HEAP] pre-connect: free=%u max_block=%u\r\n"), platformFreeHeap(), platformMaxFreeBlock());

      MQTTDebugT(F("Attempting MQTT connection .. "));
      reconnectAttempts++;

      //If no username, then anonymous connection to broker, otherwise assume username/password.
      if (strlen(settings.mqtt.sUser) == 0)
      {
        MQTTDebug(F("without a Username/Password "));
        if(!MQTTclient.connect(MQTTclientId, MQTTPubNamespace, 0, true, "offline")) PrintMQTTError();
      }
      else
      {
        MQTTDebugf(PSTR("Username [%s] "), CSTR(settings.mqtt.sUser));
        if(!MQTTclient.connect(MQTTclientId, CSTR(settings.mqtt.sUser), CSTR(settings.mqtt.sPasswd), MQTTPubNamespace, 0, true, "offline")) PrintMQTTError();
      }
      DebugTf(PSTR("[HEAP] post-connect: free=%u max_block=%u\r\n"), platformFreeHeap(), platformMaxFreeBlock());

      //If connection was made succesful, move on to next state...
      if (MQTTclient.connected())
      {
        reconnectAttempts = 0;
        MQTTDebugln(F(" .. connected\r"));
        Debugln(F("MQTT connected"));
        stateMQTT = MQTT_STATE_IS_CONNECTED;
        MQTTDebugTln(F("Next State: MQTT_STATE_IS_CONNECTED"));
        // birth message, sendMQTT retains  by default
        sendMQTT(MQTTPubNamespace, "online");
        DebugTf(PSTR("[HEAP] post-birth: free=%u max_block=%u\r\n"), platformFreeHeap(), platformMaxFreeBlock());

        // Republish OT retained topics and reset discovery only if offline long enough
        // that the broker may have lost its retained state (e.g. broker restart without
        // persistence). Short outages are network blips — broker still holds everything.
        // iLastConnectedMs == 0 means never connected (first boot): skip republish.
        {
          uint32_t offlineMs = (state.mqtt.iLastConnectedMs > 0)
                               ? (millis() - state.mqtt.iLastConnectedMs)
                               : 0;
          if (offlineMs > MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS) {
            DebugTf(PSTR("[MQTT] offline %lums > threshold — broker may have restarted; republishing values + resetting discovery\r\n"), (unsigned long)offlineMs);
            requestMQTTRepublishAll();
            // Broker restart assumed: retained discovery configs may be gone (ADR-100).
            clearMQTTConfigDone();
            clearMQTTConfigPending();
            publishNonOTDiscoveryConfigs();
          } else {
            DebugTf(PSTR("[MQTT] offline %lums <= threshold, broker retains topics — skipping republish\r\n"), (unsigned long)offlineMs);
          }
        }
        DebugTf(PSTR("[HEAP] post-republish: free=%u max_block=%u\r\n"), platformFreeHeap(), platformMaxFreeBlock());

        //Subscribe to topics
        char topic[MQTT_TOPIC_MAX_LEN];
        strlcpy(topic, MQTTSubNamespace, sizeof(topic));
        strlcat(topic, "/#", sizeof(topic));
        MQTTDebugTf(PSTR("Subscribe to MQTT: TopicId [%s]\r\n"), topic);
        if (MQTTclient.subscribe(topic)){
          MQTTDebugTf(PSTR("MQTT: Subscribed successfully to TopicId [%s]\r\n"), topic);
        }
        else
        {
          MQTTDebugTf(PSTR("MQTT: Subscribe TopicId [%s] FAILED! \r\n"), topic);
          PrintMQTTError();
        }
        MQTTclient.subscribe("homeassistant/status");
        // TASK-410 / ADR-084: briefly subscribe to the six deprecated OT-bus
        // topics so the broker delivers any lingering retained payloads from
        // pre-2.0.0 firmware. The callback clears them; mqttV2MigrationTick()
        // closes the window a few seconds later. No-op on clean brokers.
        mqttV2MigrationOnConnect();
        DebugTf(PSTR("[HEAP] post-subscribe: free=%u max_block=%u\r\n"), platformFreeHeap(), platformMaxFreeBlock());
        sendMQTTversioninfo();
        publishAllPICsettings();
        publishBoilerUnsupportedMsgids();  // TASK-692 port: republish the retained CSV so HA/dashboards see it after every reconnect.
        clearBoilerUnsupportedDirty();
        DebugTf(PSTR("[HEAP] post-versioninfo: free=%u max_block=%u\r\n"), platformFreeHeap(), platformMaxFreeBlock());
      }
      else
      { // no connection, back off non-blockingly (3s, 6s, 9s, 12s between attempts)
        // so HTTP/WebSocket keep getting served between connect tries.
        // Inline timer-bump (vs CHANGE_INTERVAL_SEC) to save ~50 bytes of flash
        // on ESP32; safeTimers.h DUE() only reads _due, so this is sufficient.
        timerMQTTwaitforretry_due = millis() + (3000UL * reconnectAttempts);
        MQTTDebugln(F(" .. \r"));
        MQTTDebugTf(PSTR("failed, retrycount=[%d], rc=[%d] ..  try again\r\n"), reconnectAttempts, MQTTclient.state());
        stateMQTT = MQTT_STATE_WAIT_CONNECTION_ATTEMPT;  // if the re-connect did not work, then return to wait for reconnect
        MQTTDebugTln(F("Next State: MQTT_STATE_WAIT_CONNECTION_ATTEMPT"));
      }
      
      //After 5 attempts... go wait for a while.
      if (reconnectAttempts >= 5)
      {
        MQTTDebugTln(F("5 attempts have failed. Retry wait for next reconnect in 10 minutes\r"));
        RESTART_TIMER(timerMQTTwaitforconnect);
        stateMQTT = MQTT_STATE_WAIT_FOR_RECONNECT;  // if the re-connect did not work, then return to wait for reconnect
        MQTTDebugTln(F("Next State: MQTT_STATE_WAIT_FOR_RECONNECT"));
      }   
    break;
    
    case MQTT_STATE_IS_CONNECTED:
      if DUE(timerMQTTdebugisconnected) MQTTDebugTln(F("MQTT State: MQTT is Connected"));
      if (MQTTclient.connected())
      { //if the MQTT client is connected, then please do a .loop call...
        MQTTclient.loop();
        state.mqtt.iLastConnectedMs = millis();  // stamp each confirmed-live tick for offline-duration tracking
      }
      else
      { //else go and wait 10 minutes, before trying again.
        RESTART_TIMER(timerMQTTwaitforconnect);
        stateMQTT = MQTT_STATE_WAIT_FOR_RECONNECT;
        MQTTDebugTln(F("Next State: MQTT_STATE_WAIT_FOR_RECONNECT"));
      }  
    break;

    case MQTT_STATE_WAIT_CONNECTION_ATTEMPT:
      //do non-blocking wait for 3 seconds
      if  DUE(timerMQTTdebugwaitconnectionattempt) MQTTDebugTln(F("MQTT State: MQTT_WAIT_CONNECTION_ATTEMPT"));
      if (DUE(timerMQTTwaitforretry))
      {
        //Try again... after waitforretry non-blocking delay
        stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
        MQTTDebugTln(F("Next State: MQTT_STATE_TRY_TO_CONNECT"));
      }
    break;
    
    case MQTT_STATE_WAIT_FOR_RECONNECT:
      //do non-blocking wait for 10 minutes, then try to connect again. 
      if DUE(timerMQTTdebugwaitforreconnect) MQTTDebugTln(F("MQTT State: MQTT wait for reconnect"));
      if (DUE(timerMQTTwaitforconnect))
      {
        //remember when you tried last time to reconnect
        RESTART_TIMER(timerMQTTwaitforretry);
        reconnectAttempts = 0; 
        stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
        MQTTDebugTln(F("Next State: MQTT_STATE_TRY_TO_CONNECT"));
      }
    break;

    case MQTT_STATE_ERROR:
      if DUE(timerMQTTdebugerrorstate) MQTTDebugTln(F("MQTT State: MQTT ERROR, wait for 10 minutes, before trying again"));
      //wait for next retry
      RESTART_TIMER(timerMQTTwaitforconnect);
      stateMQTT = MQTT_STATE_WAIT_FOR_RECONNECT;
      MQTTDebugTln(F("Next State: MQTT_STATE_WAIT_FOR_RECONNECT"));
    break;

    default:
      MQTTDebugTln(F("MQTT State: default, this should NEVER happen!"));
      //do nothing, this state should not happen
      stateMQTT = MQTT_STATE_INIT;
      DebugTln(F("Next State: MQTT_STATE_INIT"));
    break;
  }
  state.mqtt.bConnected = MQTTclient.connected();
} // handleMQTT()

void PrintMQTTError(){
  MQTTDebugln();
  switch (MQTTclient.state())
  {
    case MQTT_CONNECTION_TIMEOUT     : MQTTDebugTln(F("Error: MQTT connection timeout"));break;
    case MQTT_CONNECTION_LOST        : MQTTDebugTln(F("Error: MQTT connections lost"));break;
    case MQTT_CONNECT_FAILED         : MQTTDebugTln(F("Error: MQTT connection failed"));break;
    case MQTT_DISCONNECTED           : MQTTDebugTln(F("Error: MQTT disconnected"));break;
    case MQTT_CONNECTED              : MQTTDebugTln(F("Error: MQTT connected"));break;
    case MQTT_CONNECT_BAD_PROTOCOL   : MQTTDebugTln(F("Error: MQTT connect bad protocol"));break;
    case MQTT_CONNECT_BAD_CLIENT_ID  : MQTTDebugTln(F("Error: MQTT connect bad client id"));break;
    case MQTT_CONNECT_UNAVAILABLE    : MQTTDebugTln(F("Error: MQTT connect unavailable"));break;
    case MQTT_CONNECT_BAD_CREDENTIALS: MQTTDebugTln(F("Error: MQTT connect bad credentials"));break;
    case MQTT_CONNECT_UNAUTHORIZED   : MQTTDebugTln(F("Error: MQTT connect unauthorized"));break;
    default: MQTTDebugTln(F("Error: MQTT unknown error"));
  }
}

// ADR-104 Decision item 7: monotonic publish-success counter. Each
// sendMQTTData() success path increments this exactly once. OTPublishGate
// callers capture pre/post values to determine whether any send landed during
// their frame and commit the matching mqttPendingSlot accordingly.
uint32_t mqttSendSuccessCount = 0;

/*
  topic:  <string> , sensor topic, will be automatically prefixed with <mqtt topic>/value/<node_id>
  json:   <string> , payload to send
  retain: <bool> , retain mqtt message
*/
bool sendMQTTData(const char* topic, const char *json, const bool retain)
{
  if (!settings.mqtt.bEnable) return false;
  if (!mqttPublishAllowed) return false;
  if (!MQTTclient.connected()) { return false; }  // handleMQTT() logs disconnect and manages reconnect
  if (!isValidIP(MQTTbrokerIP)) {DebugTln(F("Error: MQTT broker IP not valid.")); return false;}

  // Check heap health before publishing
  if (!canPublishMQTT()) {
    // Message dropped due to low heap - canPublishMQTT() handles logging
    return false;
  }

  char full_topic[MQTT_TOPIC_MAX_LEN];
  snprintf_P(full_topic, sizeof(full_topic), PSTR("%s/"), MQTTPubNamespace);
  strlcat(full_topic, topic, sizeof(full_topic));
  MQTTDebugTf(PSTR("Sending MQTT: server %s:%d => TopicId [%s] --> Message [%s]\r\n"), settings.mqtt.sBroker, settings.mqtt.iBrokerPort, full_topic, json);
  const size_t payloadLen = strlen(json);
  if (!beginMqttPublish(full_topic, payloadLen, retain)) return false;
  if (!writeMqttChunk(json, payloadLen)) {
    MQTTclient.endPublish();
    return false;
  }
  if (!MQTTclient.endPublish()) { PrintMQTTError(); return false; }
  // ADR-104 Decision item 7: no auto-commit of pending slot updates inside
  // sendMQTTData. Bit/byte slots commit-or-discard in their per-helper publish
  // path; the normal-msgId mqttPendingSlot is committed-or-discarded by the
  // OTPublishGate caller using the mqttSendSuccessCount delta to detect
  // whether any send landed during the frame.
  ++mqttSendSuccessCount;
  feedWatchDog();//feed the dog
  return true;
} // sendMQTTData()

bool sendMQTTData(const __FlashStringHelper *topic, const char *json, const bool retain)
{
  char topicBuf[MQTT_TOPIC_MAX_LEN];
  strncpy_P(topicBuf, reinterpret_cast<PGM_P>(topic), sizeof(topicBuf) - 1);
  topicBuf[sizeof(topicBuf) - 1] = '\0';
  return sendMQTTData(topicBuf, json, retain);
}

bool sendMQTTData(const __FlashStringHelper *topic, const __FlashStringHelper *json, const bool retain)
{
  if (!settings.mqtt.bEnable) return false;
  if (!mqttPublishAllowed) return false;
  if (!MQTTclient.connected()) { return false; }  // handleMQTT() logs disconnect and manages reconnect
  if (!isValidIP(MQTTbrokerIP)) {DebugTln(F("Error: MQTT broker IP not valid.")); return false;}
  if (!canPublishMQTT()) return false;

  char topicBuf[MQTT_TOPIC_MAX_LEN];
  char full_topic[MQTT_TOPIC_MAX_LEN];
  strncpy_P(topicBuf, reinterpret_cast<PGM_P>(topic), sizeof(topicBuf) - 1);
  topicBuf[sizeof(topicBuf) - 1] = '\0';
  snprintf_P(full_topic, sizeof(full_topic), PSTR("%s/"), MQTTPubNamespace);
  strlcat(full_topic, topicBuf, sizeof(full_topic));

  MQTTDebugTf(PSTR("Sending MQTT: server %s:%d => TopicId [%s] --> Message ["),
              settings.mqtt.sBroker,
              settings.mqtt.iBrokerPort,
              full_topic);
  MQTTDebug(json);
  MQTTDebugln(F("]"));

  PGM_P payload = reinterpret_cast<PGM_P>(json);
  const size_t payloadLen = strlen_P(payload);
  if (!beginMqttPublish(full_topic, payloadLen, retain)) return false;
  if (!writeMqttProgmemChunk(payload, payloadLen)) {
    MQTTclient.endPublish();
    return false;
  }
  if (!MQTTclient.endPublish()) { PrintMQTTError(); return false; }
  // ADR-104 Decision item 7: no auto-commit. See char* overload comment.
  ++mqttSendSuccessCount;
  feedWatchDog();
  return true;
}

//===========================================================================================
// sendMQTTDataPic -- publish to the otgw-pic/ subtree using kPicSubtreePrefix.
// Single source of truth for the subtree name (ADR-065). Callers pass the
// label without the otgw-pic/ prefix; the helper composes the full topic on
// a local 128-byte stack buffer (longest in-scope topic is
// "otgw-pic/thermostat_connected" = 29 chars + NUL, 64 bytes margin).
// PROGMEM-correct per ADR-004: no String class. Uses strlcpy_P + strncat_P;
// strlcat_P is not declared in ESP8266 Arduino Core 3.1.2, so the canonical
// project pattern is strncat_P with (dstSize - strlen(dst) - 1) bound
// (mirrors OTGW-Core.ino:475).
//===========================================================================================
void sendMQTTDataPic(const __FlashStringHelper* label, const char* value) {
  char topic[128];
  strlcpy_P(topic, reinterpret_cast<PGM_P>(kPicSubtreePrefix), sizeof(topic));
  strncat_P(topic, reinterpret_cast<PGM_P>(label), sizeof(topic) - strlen(topic) - 1);
  sendMQTTData(topic, value);
}

// F-value overload: preserves PROGMEM semantics for flash-literal values
// (e.g. sendMQTTDataPic(F("designer"), F("Schelte Bron"))). Copies value to
// a stack buffer then delegates to the char*-value overload.
void sendMQTTDataPic(const __FlashStringHelper* label, const __FlashStringHelper* value) {
  char valueBuf[64];
  strlcpy_P(valueBuf, reinterpret_cast<PGM_P>(value), sizeof(valueBuf));
  sendMQTTDataPic(label, static_cast<const char*>(valueBuf));
}

// ---------------------------------------------------------------------------
// TEMPORARY MIGRATION CODE -- added in firmware 2.0.0 (2026-04-24) for TASK-410.
//
// Why this exists:
//   Up to and including 1.4.x the OT-bus presence values were published under
//   hardware-specific subtrees (`otgw-pic/*` and `otgw-otdirect/*`). In 2.0.0
//   they moved to the generic `OTGW/value/<uniqueId>/<label>` namespace
//   (see ADR-084, amends ADR-065). Brokers that previously served those
//   topics still hold retained payloads on the old topics, which would
//   otherwise confuse custom consumers and leave ghost state visible forever.
//
//   This block subscribes briefly after each MQTT reconnect, wipes any
//   retained payload it encounters on the deprecated topics, and unsubscribes
//   after a short timeout. Idempotent: no-op on brokers that never saw the
//   pre-2.0.0 firmware.
//
// When can this be removed:
//   Safe to delete once 2.0.0 (or a newer version containing this block) has
//   been in the field long enough that all brokers serving OTGW data have
//   seen at least one reconnect of the 2.0.0+ firmware. Practical target:
//   remove in 2.3.0 at the earliest, or as part of the 3.0.0 cleanup --
//   whichever comes first. Backlog follow-up: TASK-409
//   (remove 2.0.0 retained-cleanup migration helper).
//
//   Do not remove this block unless ADR-084 is superseded by a new ADR that
//   explicitly declares the migration complete.
// ---------------------------------------------------------------------------
static const char kV2Dep0[] PROGMEM = "otgw-pic/boiler_connected";
static const char kV2Dep1[] PROGMEM = "otgw-pic/thermostat_connected";
static const char kV2Dep2[] PROGMEM = "otgw-pic/otgw_connected";
static const char kV2Dep3[] PROGMEM = "otgw-otdirect/boiler_connected";
static const char kV2Dep4[] PROGMEM = "otgw-otdirect/thermostat_connected";
static const char kV2Dep5[] PROGMEM = "otgw-otdirect/ot_online";

static const char * const kV2DeprecatedTopics[] PROGMEM = {
  kV2Dep0, kV2Dep1, kV2Dep2, kV2Dep3, kV2Dep4, kV2Dep5
};
static constexpr size_t kV2DeprecatedCount =
  sizeof(kV2DeprecatedTopics) / sizeof(kV2DeprecatedTopics[0]);

static bool     v2MigrationSubscribed = false;
static uint32_t v2MigrationSubscribeMs = 0;   // millis() at subscribe time
static constexpr uint32_t V2_MIGRATION_WINDOW_MS = 5000;

// Compose "<MQTTPubNamespace>/<leaf>" into outBuf, mirroring what sendMQTTData
// does internally. Leaf is a PROGMEM pointer (read via strlcpy_P / strlcat).
static void v2MigrationBuildFullTopic(char *outBuf, size_t outSize, PGM_P leafProgmem) {
  snprintf_P(outBuf, outSize, PSTR("%s/"), MQTTPubNamespace);
  size_t used = strlen(outBuf);
  if (used < outSize) {
    strlcpy_P(outBuf + used, leafProgmem, outSize - used);
  }
}

static void mqttV2MigrationOnConnect() {
  char fullTopic[MQTT_TOPIC_MAX_LEN];
  for (size_t i = 0; i < kV2DeprecatedCount; ++i) {
    PGM_P leaf = reinterpret_cast<PGM_P>(pgm_read_ptr(&kV2DeprecatedTopics[i]));
    v2MigrationBuildFullTopic(fullTopic, sizeof(fullTopic), leaf);
    MQTTclient.subscribe(fullTopic);
  }
  v2MigrationSubscribed = true;
  v2MigrationSubscribeMs = millis();
  DebugTln(F("V2 migration: subscribed to deprecated topics for retained cleanup"));
}

static bool mqttV2MigrationHandleIfDeprecated(const char* topic) {
  if (!v2MigrationSubscribed) return false;
  char fullTopic[MQTT_TOPIC_MAX_LEN];
  for (size_t i = 0; i < kV2DeprecatedCount; ++i) {
    PGM_P leaf = reinterpret_cast<PGM_P>(pgm_read_ptr(&kV2DeprecatedTopics[i]));
    v2MigrationBuildFullTopic(fullTopic, sizeof(fullTopic), leaf);
    if (strcmp(topic, fullTopic) == 0) {
      // Publish zero-length retained payload => broker deletes the retained
      // entry for this topic (MQTT 3.1.1 section 3.3.1.3).
      MQTTclient.publish(fullTopic, (const uint8_t*)nullptr, 0, /*retain=*/true);
      MQTTclient.unsubscribe(fullTopic);
      DebugTf(PSTR("V2 migration: cleared retained on %s\r\n"), fullTopic);
      return true;
    }
  }
  return false;
}

static void mqttV2MigrationTick() {
  if (!v2MigrationSubscribed) return;
  // Rollover-safe elapsed check: unsigned subtraction wraps correctly so the
  // window closes ~5s after subscribe even across the 49.7-day millis() wrap.
  if ((uint32_t)(millis() - v2MigrationSubscribeMs) < V2_MIGRATION_WINDOW_MS) return;
  char fullTopic[MQTT_TOPIC_MAX_LEN];
  for (size_t i = 0; i < kV2DeprecatedCount; ++i) {
    PGM_P leaf = reinterpret_cast<PGM_P>(pgm_read_ptr(&kV2DeprecatedTopics[i]));
    v2MigrationBuildFullTopic(fullTopic, sizeof(fullTopic), leaf);
    MQTTclient.unsubscribe(fullTopic);
  }
  v2MigrationSubscribed = false;
  DebugTln(F("V2 migration: cleanup window closed, unsubscribed from deprecated topics"));
}
// ---------------------------------------------------------------------------
// END TEMPORARY MIGRATION CODE (TASK-410 / ADR-084)
// ---------------------------------------------------------------------------

//===================[ Send useful information to MQTT ]======================

void sendMQTTuptime(){
  DebugTf(PSTR("Uptime seconds: %lu\r\n"), (unsigned long)state.uptime.iSeconds);
  char uptimeBuf[11] = {0};
  snprintf_P(uptimeBuf, sizeof(uptimeBuf), PSTR("%lu"), (unsigned long)state.uptime.iSeconds);
  sendMQTTData(F("otgw-firmware/uptime"), uptimeBuf, false);
}

/*
Publish usefull firmware version information to MQTT broker.
*/
void sendMQTTversioninfo(){
  char rebootCountBuf[12];
  snprintf_P(rebootCountBuf, sizeof(rebootCountBuf), PSTR("%lu"), static_cast<unsigned long>(state.uptime.iRebootCount));
  sendMQTTData(F("otgw-firmware/hostname"), CSTR(settings.sHostname), true);  // retained: human-readable mapping of <uniqueid> to device name
  sendMQTTData("otgw-firmware/version", _SEMVER_FULL);
  sendMQTTData("otgw-firmware/reboot_count", rebootCountBuf);
  sendMQTTData("otgw-firmware/reboot_reason", lastReset);
  // ADR-113: static, machine-readable hardware-type slug (board class). Retained so
  // HA/UI can select codepath on hardware identity instead of runtime PIC liveness.
  sendMQTTData(F("otgw-firmware/hardware_type"), hardwareTypeName(), true);
  if (isPICEnabled()) {
    sendMQTTDataPic(F("version"), state.pic.sFwversion);
    sendMQTTDataPic(F("deviceid"), state.pic.sDeviceid);
    sendMQTTDataPic(F("firmwaretype"), state.pic.sType);
    sendMQTTDataPic(F("designer"), F("Schelte Bron"));
  }
  // ADR-113 stage 2 (TASK-754): picavailable removed. Selection uses
  // otgw-firmware/hardware_type; liveness uses isPICEnabled(). Deprecation
  // window (one release after alpha.89) has passed.
}

/*
Publish cumulative heap-pressure and drop diagnostics as individual retained
topics under <topTopic>/value/<uniqueid>/otgw-firmware/stats/*. Each metric
lives on its own topic (not bundled in a JSON blob) so consumers can subscribe
to a single counter, expose it as a Home Assistant entity without JSON path
templating, or graph it in Grafana without parsing. sendMQTTData() prepends
MQTTPubNamespace, so uniqueid already scopes the topics per device - multiple
OTGWs on one broker never collide.

Called from doTaskMinuteChanged() under if (hourFlag) per ADR-086 (wall-clock
aligned hourly dispatch). NOT piggybacked on the 5-minute loop to keep traffic
low. Counters reset on reboot; correlate with otgw-firmware/reboot_count and
/uptime to reason about lifetime vs. session rates. To map <uniqueid> back to
a human-readable device name, subscribe to
<topTopic>/value/<uniqueid>/otgw-firmware/hostname (retained).
*/
static void publishStatU32(const __FlashStringHelper *topic, unsigned long value) {
  char buf[12];  // max uint32 decimal = 10 digits + sign + NUL
  snprintf_P(buf, sizeof(buf), PSTR("%lu"), value);
  sendMQTTData(topic, buf, true);  // retained
}

// TASK-692 port (dev TASK-686): publish the boiler unsupported-msgID map as one
// MQTT retained CSV. Payload shape: "<id>R" for read-direction Unknown-Data-Id,
// "<id>W" for write. Examples (typical boiler): "14W,16W,24R,26R,27R,33R,36R".
// Empty bitmap -> "". Single 256-byte stack buffer: realistic boilers flag <20
// msgIDs so the buffer is comfortably large. The (pos + 6 > sizeof) guards
// truncate silently rather than overflow on pathological inputs.
void publishBoilerUnsupportedMsgids() {
  if (!settings.mqtt.bEnable || !state.mqtt.bConnected) return;
  // TASK-696: single PSTR format string with a sep prefix ("" or ",") replaces
  // the previous pos==0 dual-format pattern — one fewer PSTR literal per loop.
  char csv[256] = {0};
  size_t pos = 0;
  for (int i = 0; i <= 255; i++) {
    if (!isBoilerMsgIdUnsupportedRead((uint8_t)i)) continue;
    if (pos + 6 > sizeof(csv)) break;  // 6 = ",NNNR" + NUL
    int n = snprintf_P(csv + pos, sizeof(csv) - pos,
                       PSTR("%s%dR"), pos == 0 ? "" : ",", i);
    if (n < 0) break;
    pos += (size_t)n;
  }
  for (int i = 0; i <= 255; i++) {
    if (!isBoilerMsgIdUnsupportedWrite((uint8_t)i)) continue;
    if (pos + 6 > sizeof(csv)) break;
    int n = snprintf_P(csv + pos, sizeof(csv) - pos,
                       PSTR("%s%dW"), pos == 0 ? "" : ",", i);
    if (n < 0) break;
    pos += (size_t)n;
  }
  sendMQTTData(F("otgw-firmware/boiler/unsupported_msgids"), csv, true);  // retained
}

void sendMQTTheapdiag(){
  if (!settings.mqtt.bEnable) return;
  if (!state.mqtt.bConnected) return;

  // Heap pressure tier transitions and drop counters
  publishStatU32(F("otgw-firmware/stats/ws_drops"),              (unsigned long)state.heapdiag.iWsDropsTotal);
  publishStatU32(F("otgw-firmware/stats/mqtt_drops"),            (unsigned long)state.heapdiag.iMqttDropsTotal);
  publishStatU32(F("otgw-firmware/stats/enter_low"),             (unsigned long)state.heapdiag.iEnteredLowCount);
  publishStatU32(F("otgw-firmware/stats/enter_warning"),         (unsigned long)state.heapdiag.iEnteredWarningCount);
  publishStatU32(F("otgw-firmware/stats/enter_critical"),        (unsigned long)state.heapdiag.iEnteredCriticalCount);

  // Discovery drip throttle counters
  publishStatU32(F("otgw-firmware/stats/drip_burst_skip"),       (unsigned long)state.heapdiag.iDripActiveBurstSkipCount);
  publishStatU32(F("otgw-firmware/stats/drip_cooldown_skip"),    (unsigned long)state.heapdiag.iDripCooldownSkipCount);
  publishStatU32(F("otgw-firmware/stats/drip_slowmode"),         (unsigned long)state.heapdiag.iDripSlowModeCount);

  // Live heap snapshot at publish time
  publishStatU32(F("otgw-firmware/stats/free_heap"),             (unsigned long)platformFreeHeap());
  publishStatU32(F("otgw-firmware/stats/max_block"),             (unsigned long)platformMaxFreeBlock());
  publishStatU32(F("otgw-firmware/stats/frag_pct"),              (unsigned long)getHeapFragmentation());

  // Discovery verification telemetry (ADR-062)
  publishStatU32(F("otgw-firmware/stats/disc_verify_runs"),         (unsigned long)state.discovery.iVerifyRunCount);
  publishStatU32(F("otgw-firmware/stats/disc_republish_triggered"), (unsigned long)state.discovery.iRepublishTriggeredCount);
  publishStatU32(F("otgw-firmware/stats/disc_last_missing"),        (unsigned long)state.discovery.iLastMissingCount);
  publishStatU32(F("otgw-firmware/stats/disc_last_orphan"),         (unsigned long)state.discovery.iLastOrphanCount);
  publishStatU32(F("otgw-firmware/stats/disc_published_topics"),    (unsigned long)state.discovery.iPublishedTopicCount);
  publishStatU32(F("otgw-firmware/stats/disc_last_verify_epoch"),   (unsigned long)state.discovery.iLastVerifyEpoch);
}

// ADR-084: OT-bus presence values (boiler_connected, thermostat_connected,
// otgw_connected) are published only under the generic
// OTGW/value/<uniqueId>/<label> namespace. Hardware-specific duplicates that
// used to live under otgw-pic/* and otgw-otdirect/* have been removed in
// 2.0.0. See also the V2 migration helpers below for retained cleanup of
// pre-2.0.0 payloads on the deprecated topics.
static void publishBoilerConnectedState()
{
  sendMQTTData(F("boiler_connected"), CCONOFF(state.otBus.bBoilerState));
}

static void publishThermostatConnectedState()
{
  sendMQTTData(F("thermostat_connected"), CCONOFF(state.otBus.bThermostatState));
}

static void publishOTGWConnectedState()
{
  // ADR-102: OT-bus liveness drives only the otgw_connected sensor. The base
  // namespace topic is the HA avty_t and is owned solely by the MQTT
  // birth/LWT mechanism (birth "online" on connect, broker LWT "offline").
  sendMQTTData(F("otgw_connected"), CCONOFF(state.otBus.bOnline));
}

/*
Publish state information of PIC firmware version information to MQTT broker.
*/
void sendMQTTstateinformation(){
  publishBoilerConnectedState();
  publishThermostatConnectedState();
  if (state.otBus.bGatewayModeKnown) {
    sendMQTTData(F("otgw-pic/gateway_mode"), CCONOFF(state.otBus.bGatewayMode));
  }
  publishOTGWConnectedState();
}

/*
* topic:  <string> , topic will be used as is (no prefixing), retained = true
* json:   <string> , payload to send
*/
//===========================================================================================
// Streaming version - sends message in 128-byte chunks to avoid buffer reallocation
// This prevents heap fragmentation on ESP8266 (similar to ESPHome's approach)
void sendMQTT(const char* topic, const char *json) {
  if (!settings.mqtt.bEnable) return;
  if (!MQTTclient.connected()) return;
  if (!isValidIP(MQTTbrokerIP)) return;
  if (!canPublishMQTT()) return;

  const size_t len = strlen(json);
  if (!beginMqttPublish(topic, len, true)) return;
  if (!writeMqttChunk(json, len)) { MQTTclient.endPublish(); return; }
  if (!MQTTclient.endPublish()) PrintMQTTError();
  feedWatchDog();
}

//===========================================================================================
// Helper functions to reduce duplicated MQTT topic building patterns
//===========================================================================================

/**
 * Publish ON/OFF value to MQTT topic.
 * ADR-104: returns sendMQTTData()'s success so bit/byte publish helpers can
 * commit-or-discard their pending throttle-slot records.
 */
bool publishMQTTOnOff(const char* topic, bool value) {
  return sendMQTTData(topic, value ? "ON" : "OFF");
}

bool publishMQTTOnOff(const __FlashStringHelper* topic, bool value) {
  return sendMQTTData(topic, value ? "ON" : "OFF");
}

/**
 * Publish numeric value as string to MQTT topic
 * Reduces duplicate pattern of number-to-string conversion with static buffer
 */
void publishMQTTNumeric(const char* topic, float value, uint8_t decimals = 2) {
  static char buffer[16];
  dtostrf(value, 1, decimals, buffer);
  sendMQTTData(topic, buffer);
}

void publishMQTTNumeric(const __FlashStringHelper* topic, float value, uint8_t decimals = 2) {
  static char buffer[16];
  dtostrf(value, 1, decimals, buffer);
  sendMQTTData(topic, buffer);
}

/**
 * Publish integer value as string to MQTT topic
 */
void publishMQTTInt(const char* topic, int value) {
  static char buffer[12];
  snprintf_P(buffer, sizeof(buffer), PSTR("%d"), value);
  sendMQTTData(topic, buffer);
}

void publishMQTTInt(const __FlashStringHelper* topic, int value) {
  static char buffer[12];
  snprintf_P(buffer, sizeof(buffer), PSTR("%d"), value);
  sendMQTTData(topic, buffer);
}

// ADR-096 worldview routing + ADR-097 sibling-suffix topic shape.
//
// Each per-source topic represents what the named device sees on the OT bus,
// regardless of which side originated the frame. Topics are sibling leaves
// of the canonical topic (suffix shape per ADR-097, replacing the nested-
// children shape from 2.0.0-alpha):
//
//   <topic>            canonical (boiler-side worldview)
//   <topic>_thermostat value the thermostat sent (T) or received (A under
//                      answer-override, B under pass-through)
//   <topic>_boiler     value the boiler received (R under override, T under
//                      pass-through) or sent (B)
//
// Routing decisions per (rsptype, OTdata.bGatewaySubstituted, OTdata.bAnswerOverride):
//
//   T  no-override (bGS=false): _thermostat AND _boiler
//   T  with R-follow (bGS=true): _thermostat only (R wins _boiler)
//   R                          : _boiler only
//   B  no-override (bGS=false): _thermostat AND _boiler
//   B  with A-follow (bGS=true): _boiler only (A wins _thermostat)
//   A  answer-override (bAnswerOverride=true) : _thermostat only (genuine B owns _boiler)
//   A  proxy answer    (bAnswerOverride=false): _thermostat AND _boiler (no B exists)
//
// The bGatewaySubstituted flag is set on the OLDER frame in a (T,R) or (B,A)
// sequence by processOT() in OTGW-Core.ino. ADR-103: bAnswerOverride is set
// true only on an A that followed a B (answer-override); a proxy A (no
// preceding B — e.g. MaxTSet/57) keeps it false so its value reaches _boiler
// and canonical instead of being starved. There is no _gateway suffix;
// override visibility is achieved by divergence between _thermostat and
// _boiler, not by a third topic.
//
// The ADR-097 Write-Ack gate (bSlaveEchoesValue) suppresses real-boiler
// Write-Ack frames whose data byte is per-spec undefined; see the gate
// comment inside the function for the OTdata.type vs rsptype distinction.
void publishToSourceTopic(const char* topic, const char* json, byte rsptype)
{
  if (!settings.mqtt.bSeparateSources || !topic || !json) return;
  // ADR-097: skip the source subtopics for MsgIDs where the slave's Write-Ack
  // data byte is per-spec undefined. Without this gate, the _thermostat /
  // _boiler topics flap between the Write-Data value and the Ack's protocol-
  // zero (e.g. Tr, TrSet, MaxRelModLevelSetting). The bSlaveEchoesValue flag
  // is populated for every MsgID in OTmap[] per
  // docs/api/MQTT-message-id-echo-audit.md.
  //
  // The frame-type check MUST use OTdata.type (OpenThermMessageType, where
  // OT_WRITE_ACK==B101==5), NOT rsptype (OTGW_response_type, 0..5). The two
  // enum families collide numerically: rsptype==OT_WRITE_ACK is true only
  // for OTGW_UNDEF, never for a real boiler Write-Ack. We additionally
  // require rsptype==OTGW_BOILER so this only fires on real boiler frames
  // (B), not on gateway-faked Answer-Thermostat frames (A) where the value
  // is deliberately constructed.
  //
  // OTlookupitem and OTdata are set by processOT before each print_* call
  // and are valid here.
  if (OTdata.type == OT_WRITE_ACK
      && rsptype == OTGW_BOILER
      && !OTlookupitem.bSlaveEchoesValue) return;
  // Re-entrancy guard: sendMQTTData may yield via feedWatchDog, allowing
  // a second processOT call to overwrite the static buffer mid-publish.
  static bool inUse = false;
  if (inUse) return;
  inUse = true;

  // Worldview routing decision (ADR-096, refined by ADR-103 for proxy A).
  bool toThermostat = false;
  bool toBoiler = false;
  switch (rsptype) {
    case OTGW_THERMOSTAT:        // T: thermostat-sent write
      toThermostat = true;
      toBoiler = !OTdata.bGatewaySubstituted;  // R wins /boiler when override active
      break;
    case OTGW_BOILER:            // B: boiler-sent response
      toBoiler = true;
      toThermostat = !OTdata.bGatewaySubstituted;  // A wins /thermostat when answer-override active
      break;
    case OTGW_REQUEST_BOILER:    // R: gateway-substituted write (only the boiler sees this value)
      toBoiler = true;
      break;
    case OTGW_ANSWER_THERMOSTAT: // A: gateway-faked answer
      toThermostat = true;
      toBoiler = !OTdata.bAnswerOverride;  // ADR-103: proxy A (no B) → _boiler too; override A → _thermostat only
      break;
    default:                     // parity errors, unknown types
      inUse = false;
      return;
  }

  static char sourceTopic[MQTT_TOPIC_MAX_LEN];
  if (toThermostat) {
    snprintf_P(sourceTopic, sizeof(sourceTopic), PSTR("%s_thermostat"), topic);
    sendMQTTData(sourceTopic, json, false);
  }
  if (toBoiler) {
    snprintf_P(sourceTopic, sizeof(sourceTopic), PSTR("%s_boiler"), topic);
    sendMQTTData(sourceTopic, json, false);
  }
  inUse = false;
}

//===========================================================================================
bool getMQTTConfigDone(const uint8_t MSGid)
{
  return bitRead(MQTTautoConfigMap[MSGid >> 5], MSGid & 0x1F) != 0;
}
//===========================================================================================
void setMQTTConfigDone(const uint8_t MSGid)
{
  bitSet(MQTTautoConfigMap[MSGid >> 5], MSGid & 0x1F);
}
//===========================================================================================
void clearMQTTConfigDone()
{
  memset(MQTTautoConfigMap, 0, sizeof(MQTTautoConfigMap));
  // Reset published-topic counter so it stays in sync with the bitmap (ADR-062).
  // Stream helpers re-increment on each successful endPublish.
  state.discovery.iPublishedTopicCount = 0;
}
//===========================================================================================
// Pending-bitmap helpers for async drip-discovery (ADR-100).
// MQTTautoCfgPendingMap[8] mirrors MQTTautoConfigMap layout: 8 × uint32_t = 256 bits.
// Setting a bit means "this OT ID needs its discovery config (re-)published".
//===========================================================================================
void clearMQTTConfigPending()
{
  memset(MQTTautoCfgPendingMap, 0, sizeof(MQTTautoCfgPendingMap));
}
//===========================================================================================
// publishNonOTDiscoveryConfigs() — queue only the non-OT discovery configs for drip publish.
// Called at boot, top-topic change, and broker restart.
// OT ID configs are NOT queued here; they publish JIT as each MsgID arrives on the bus.
//===========================================================================================
void publishNonOTDiscoveryConfigs()
{
  if (!settings.mqtt.bEnable) return;
  setMQTTConfigPending(0);                  // climate: thermostat + DHW control
  setMQTTConfigPending(27);                 // number: outside temperature override
  setMQTTConfigPending(OTGWdallasdataid);   // Dallas temperature sensors
  setMQTTConfigPending(OTGWheapstatsid);    // heap / discovery statistics
  setMQTTConfigPending(OTGWfwinfoid);       // firmware info
  setMQTTConfigPending(OTGWpicinfoid);      // PIC info
  setMQTTConfigPending(OTGWpicsettingsid);  // PIC settings
  setMQTTConfigPending(OTGWpiccontrolsid);  // PIC controls: resetgateway button, GPIO/LED selects
  dripDeviceInfoPending = true;
  MQTTDebugTln(F("MQTT discovery: non-OT configs queued; OT IDs will publish JIT"));
}
//===========================================================================================
void setMQTTConfigPending(const uint8_t MSGid)
{
  uint8_t group = (MSGid >> 5) & 0x07;
  uint8_t bit   = MSGid & 0x1F;
  bitSet(MQTTautoCfgPendingMap[group], bit);
}
//===========================================================================================
//===========================================================================================
void markAllMQTTConfigPending()
{
  // Mark every OT ID that appears in the PROGMEM discovery tables as pending.
  // Also clears the "published" bitmap so drainOnePendingDiscovery re-publishes.
  clearMQTTConfigDone();
  memset(MQTTautoCfgPendingMap, 0, sizeof(MQTTautoCfgPendingMap));
  for (uint16_t i = 0; i < 256; i++) {
    uint16_t sIdx = readSensorIndex(static_cast<uint8_t>(i));
    uint16_t bIdx = readBinSensorIndex(static_cast<uint8_t>(i));
    if (sIdx != MQTT_HA_INDEX_NONE || bIdx != MQTT_HA_INDEX_NONE) {
      setMQTTConfigPending(static_cast<uint8_t>(i));
    }
  }
  // Mark climate (ID 0) and number (ID 27) as pending
  setMQTTConfigPending(0);   // climate thermostat + DHW
  setMQTTConfigPending(27);  // number Toutside override
  // Also mark the Dallas sensor pseudo-ID
  setMQTTConfigPending(OTGWdallasdataid);
  // Heap/discovery statistics discovery (TASK-346): 17 retained otgw-firmware/stats/* topics
  setMQTTConfigPending(OTGWheapstatsid);
  // Diagnostic discovery (TASK-540 / TASK-541): firmware info, PIC info, PIC settings.
  // PIC pseudo-IDs use MQTT_HA_FLAG_IS_PIC_ENTRY so they self-skip when isPICEnabled() is false.
  setMQTTConfigPending(OTGWfwinfoid);
  setMQTTConfigPending(OTGWpicinfoid);
  setMQTTConfigPending(OTGWpicsettingsid);
  // PIC control entities (pseudo-ID 244): resetgateway button + gpioa/gpiob/leda-f
  // selects. Discovery unconditional like the other PIC pseudo-IDs; the
  // set-commands and otgw-pic/ state topics are PIC-gated at their source.
  setMQTTConfigPending(OTGWpiccontrolsid);
  // 2.0.0-specific diagnostic discovery (TASK-541): OTDirect flame metrics + SAT BLE/pressure health.
  setMQTTConfigPending(OTGWdiag200id);
  // TASK-543: SAT user-facing discovery stays unconditional on this dual-target branch.
  // Platform/runtime-specific publishers decide whether entities show live state.
  setMQTTConfigPending(OTGWsatzoneid);
  dripDeviceInfoPending = true;
  MQTTDebugTln(F("MQTT discovery: all IDs marked pending for async drip publish"));
}
//===========================================================================================
// loopMQTTDiscovery() — call from the main loop on every iteration.
// Manages its own timer internally.  When the timer fires, finds the next
// pending OT ID, publishes its discovery config, clears its pending bit,
// and sets its "done" bit.  Publishes exactly ONE ID per timer tick to
// spread broker load over time.
//
// Adaptive interval: 2s when heap is healthy, 10s under heap pressure.
// The 2s cadence gives heap time to recover between discovery bursts so that
// a Status-frame fanout (9 sub-topic publishes in ~20ms) does not overlap
// with the next drip alloc. Pressure trigger is HEAP_LOW (not WARNING): the
// drip MUST back off before the publish gate starts dropping, otherwise we
// only mitigate drops at the gate instead of preventing them at the source.
//
// Three hysteresis layers gate normal<->slow mode transitions:
//  1. Time hysteresis (TASK-370): a mode holds for at least one full
//     timerDiscoveryDrip_interval before a switch is allowed in either
//     direction. Normal (2s) -> Slow: requires >=2s in normal; Slow (10s)
//     -> Normal: requires >=10s in slow.
//  2. Threshold hysteresis (TASK-553 / TASK-555): entry trigger uses the
//     existing discoveryDripHasHeapPressure() predicate; restore trigger
//     uses the companion discoveryDripIsHeapHealthyForRestore() predicate
//     with a deadband above the entry thresholds (1KB on ESP8266, 2KB on
//     ESP32-S3 freeHeap + 1KB maxBlock). Schmitt-trigger pattern prevents
//     a single post-recovery burst from immediately tipping back below
//     the entry threshold.
//  3. K-ticks hysteresis (TASK-553 / TASK-555): restore additionally
//     requires consecutiveHealthyTicks >= 2 — i.e. ~20s of confirmed-
//     healthy heap on the 10s slow-mode cadence — so a transient
//     burst-aligned recovery sample cannot drive a premature restore.
//     Counter is updated once per timer tick (post-DUE) and resets to 0
//     on any unhealthy read or on slow-mode entry.
//===========================================================================================
constexpr uint8_t DISCOVERY_INTERVAL_NORMAL = 2;   // seconds (heap recovery between bursts)
constexpr uint8_t DISCOVERY_INTERVAL_SLOW   = 10;  // seconds (heap pressure backoff)
constexpr uint8_t DRIP_RESTORE_K_TICKS      = 2;   // TASK-555: consecutive healthy ticks required to restore

static bool discoveryDripHasHeapPressure() {
#if defined(ESP32)
  // ESP32-S3 has far more DRAM than ESP8266, so the drip should only slow down
  // when both total free heap and the largest allocatable block are genuinely low.
  return (platformFreeHeap() < 16384U) && (platformMaxFreeBlock() < 8192U);
#else
  // ESP8266 keeps the existing HEAP_LOW-based backoff policy from TASK-370.
  return getHeapHealth() >= HEAP_LOW;
#endif
}

// Companion to discoveryDripHasHeapPressure(): returns true when heap is
// comfortably above the entry thresholds, with a deadband to prevent thrash
// (TASK-553 / TASK-555). Used together with the K-ticks counter to gate
// slow->normal restoration.
static bool discoveryDripIsHeapHealthyForRestore() {
#if defined(ESP32)
  // ~12-15% deadband above entry thresholds (16384/8192). Conservative
  // starting values; ESP32-S3 has ~300KB DRAM so thrash is unlikely in
  // field but pattern-symmetry with ESP8266 keeps the port clean. Tunable
  // post-validation.
  return (platformFreeHeap() >= 18432U) && (platformMaxFreeBlock() >= 9216U);
#else
  // ESP8266: 1KB deadband above HEAP_LOW_THRESHOLD (5120). See declaration
  // in OTGW-firmware.h for sizing rationale.
  return platformFreeHeap() >= HEAP_LOW_RESTORE_THRESHOLD;
#endif
}

void loopMQTTDiscovery()
{
  DECLARE_TIMER_SEC(timerDiscoveryDrip, DISCOVERY_INTERVAL_NORMAL, SKIP_MISSED_TICKS);
  static uint32_t modeEnteredMs = 0;  // millis() when current mode was entered; 0 = boot
  static uint8_t  consecutiveHealthyTicks = 0;  // TASK-555: K-ticks counter for restore decision
  sDripDueAtMs = timerDiscoveryDrip_due;  // expose due-time for dripDueWithinMs()

  if (!DUE(timerDiscoveryDrip)) return;

  // Tick-boundary K-ticks counter update: runs exactly once per timer tick.
  // Increments when heap is comfortably above entry thresholds (deadband),
  // resets on any unhealthy read. The restore decision below requires this
  // counter to reach DRIP_RESTORE_K_TICKS (TASK-555).
  if (discoveryDripIsHeapHealthyForRestore()) {
    if (consecutiveHealthyTicks < 0xFF) consecutiveHealthyTicks++;
  } else {
    consecutiveHealthyTicks = 0;
  }

  // Mode switch decision (per-tick). Entry trigger uses the existing
  // pressure predicate; restore trigger uses the threshold-hysteresis
  // predicate plus the K-ticks counter.
  bool heapPressure = discoveryDripHasHeapPressure();
  bool canSwitch = (modeEnteredMs == 0) ||
                   ((millis() - modeEnteredMs) >= timerDiscoveryDrip_interval);
  if (heapPressure && timerDiscoveryDrip_interval != DISCOVERY_INTERVAL_SLOW * 1000UL && canSwitch) {
    CHANGE_INTERVAL_SEC(timerDiscoveryDrip, DISCOVERY_INTERVAL_SLOW, SKIP_MISSED_TICKS);
    state.heapdiag.iDripSlowModeCount++;
    modeEnteredMs = millis();
    consecutiveHealthyTicks = 0;  // restore needs fresh K healthy ticks (TASK-555)
    MQTTDebugTf(PSTR("[drip] slowed to %ds (heap pressure)\r\n"), DISCOVERY_INTERVAL_SLOW);
  } else if (!heapPressure
             && consecutiveHealthyTicks >= DRIP_RESTORE_K_TICKS
             && timerDiscoveryDrip_interval != DISCOVERY_INTERVAL_NORMAL * 1000UL
             && canSwitch) {
    CHANGE_INTERVAL_SEC(timerDiscoveryDrip, DISCOVERY_INTERVAL_NORMAL, SKIP_MISSED_TICKS);
    modeEnteredMs = millis();
    MQTTDebugTf(PSTR("[drip] restored to %ds (heap healthy)\r\n"), DISCOVERY_INTERVAL_NORMAL);
  }

  if (!settings.mqtt.bEnable) return;
  if (!state.mqtt.bConnected) return;
  if (platformFreeHeap() < MQTT_DISCOVERY_HEAP_MIN) return;
  // Defer drip during Status-frame fanout AND for STATUS_BURST_COOLDOWN_MS afterwards.
  // Timer keeps running; next tick picks up as soon as the deferred window clears.
  // Track which of the two reasons triggered the skip for diagnostics.
  if (isStatusBurstActive()) {
    state.heapdiag.iDripActiveBurstSkipCount++;
    return;
  }
  if (isDripDeferred()) {
    state.heapdiag.iDripCooldownSkipCount++;
    return;
  }

  // Scan pending bitmap for the next set bit
  for (uint8_t group = 0; group < 8; group++) {
    if (MQTTautoCfgPendingMap[group] == 0) continue;
    for (uint8_t bit = 0; bit < 32; bit++) {
      if (!bitRead(MQTTautoCfgPendingMap[group], bit)) continue;

      uint8_t msgId = (group << 5) | bit;

      // Already published (e.g. by explicit doAutoConfigure while drip was pending)
      if (getMQTTConfigDone(msgId)) {
        bitClear(MQTTautoCfgPendingMap[group], bit);
        continue;  // check next pending bit in same call
      }

      // Dallas sensors use a separate path (configSensors)
      if (msgId == OTGWdallasdataid) {
        MQTTDebugTln(F("[drip] publishing Dallas sensor discovery"));
        configSensors();
        bitClear(MQTTautoCfgPendingMap[group], bit);
        return;  // one per tick
      }

      MQTTDebugTf(PSTR("[drip] publishing discovery for OT ID %d\r\n"), msgId);
      bool success = doAutoConfigureMsgid(msgId, dripDeviceInfoPending);
      if (success) {
        dripDeviceInfoPending = false;
        setMQTTConfigDone(msgId);
        bitClear(MQTTautoCfgPendingMap[group], bit);
        MQTTDebugTf(PSTR("[drip] OT ID %d published OK\r\n"), msgId);
      } else {
        // Leave pending bit set — next drip tick retries automatically.
        // Rate-limited by the drip timer itself (2s normal, 10s slow-mode under
        // heap pressure), so no busy-loop risk. Fixes the limbo where a failed
        // publish used to drop the msgid until an external markAllMQTTConfigPending
        // call arrived (TASK-348).
        MQTTDebugTf(PSTR("[drip] OT ID %d publish failed, retaining pending\r\n"), msgId);
      }
      return;  // one attempt per tick regardless of success
    }
  }
}
//===========================================================================================
// Build a discovery context from the current MQTT state.
// Caller sets ctx.isFirstEntity as appropriate.
static HaDiscoveryContext buildDiscoveryContext(bool isFirst = false) {
  HaDiscoveryContext ctx;
  ctx.nodeId = NodeId;
  ctx.hostname = CSTR(settings.sHostname);
  ctx.version = _VERSION;
  ctx.mqttPubTopic = MQTTPubNamespace;
  ctx.mqttSubTopic = MQTTSubNamespace;
  ctx.haPrefix = CSTR(settings.mqtt.sHaprefix);
  ctx.manufacturer = settings.device.sManufacturer;
  ctx.model = settings.device.sModel;
  ctx.isFirstEntity = isFirst;
  ctx.sourceSuffix = "";
  ctx.sourceName = "";
  ctx.sourceTopicSegment = "";
  return ctx;
}

static bool publishDiscoveryJson(PubSubClient &client,
                                 const char *topic,
                                 const char *payload,
                                 size_t payloadLen)
{
  if (!client.beginPublish(topic, payloadLen, true)) return false;
  if (client.write(reinterpret_cast<const uint8_t*>(payload), payloadLen) != payloadLen) {
    client.endPublish();
    return false;
  }
  if (!client.endPublish()) return false;
  incPublishedTopicCount();
  feedWatchDog();
  return true;
}

static bool buildDiscoveryDeviceBlock(char *dest, size_t destSize, HaDiscoveryContext &ctx)
{
  int n;
  if (ctx.isFirstEntity) {
    n = snprintf_P(dest, destSize,
                   PSTR("\"dev\":{\"identifiers\":\"%s\",\"manufacturer\":\"%s\",\"model\":\"%s\",\"name\":\"OpenTherm Gateway (%s)\",\"sw_version\":\"%s\"}"),
                   ctx.nodeId, ctx.manufacturer, ctx.model, ctx.hostname, ctx.version);
  } else {
    n = snprintf_P(dest, destSize,
                   PSTR("\"dev\":{\"identifiers\":\"%s\"}"),
                   ctx.nodeId);
  }
  return (n > 0 && static_cast<size_t>(n) < destSize);
}

// Forward declaration — defined just below streamSatZoneDiscovery (TASK-640).
static bool streamSatPvBoostDiscovery(PubSubClient &client, HaDiscoveryContext &ctx);

bool streamSatZoneDiscovery(PubSubClient &client, HaDiscoveryContext &ctx)
{
  if (!client.connected()) return false;
  if (!canPublishMQTT()) return false;

  const uint8_t maxZones = satGetMaxZones();

  char topic[160];
  char payload[768];
  char deviceBlock[256];

  auto publishZoneSensor = [&](uint8_t zoneNumber,
                               const char *metric,
                               const char *friendlySuffix,
                               const char *icon,
                               const char *unit,
                               const char *deviceClass,
                               const char *stateClass) -> bool {
    if (!buildDiscoveryDeviceBlock(deviceBlock, sizeof(deviceBlock), ctx)) return false;
    int topicLen = snprintf_P(topic, sizeof(topic),
                              PSTR("%s/sensor/%s/sat_zone_%u_%s/config"),
                              ctx.haPrefix, ctx.nodeId, zoneNumber, metric);
    if (topicLen <= 0 || static_cast<size_t>(topicLen) >= sizeof(topic)) return false;
    int payloadLen = snprintf_P(payload, sizeof(payload),
                                PSTR("{\"avty_t\":\"%s\",%s,\"uniq_id\":\"%s-sat_zone_%u_%s\",\"name\":\"%s_SAT_Zone_%u_%s\",\"stat_t\":\"%s/sat/zone/%u/%s\",\"device_class\":\"%s\",\"unit_of_measurement\":\"%s\",\"state_class\":\"%s\",\"icon\":\"mdi:%s\",\"value_template\":\"{{ value }}\",\"origin\":{\"name\":\"OTGW-firmware\",\"sw\":\"%s\",\"url\":\"https://github.com/rvdbreemen/OTGW-firmware\"}}"),
                                ctx.mqttPubTopic, deviceBlock, ctx.nodeId, zoneNumber, metric,
                                ctx.hostname, zoneNumber, friendlySuffix,
                                ctx.mqttPubTopic, zoneNumber, metric,
                                deviceClass, unit, stateClass, icon, ctx.version);
    if (payloadLen <= 0 || static_cast<size_t>(payloadLen) >= sizeof(payload)) return false;
    if (!publishDiscoveryJson(client, topic, payload, static_cast<size_t>(payloadLen))) return false;
    ctx.isFirstEntity = false;
    return true;
  };

  auto publishZoneBinary = [&](uint8_t zoneNumber,
                               const char *metric,
                               const char *friendlySuffix,
                               const char *icon) -> bool {
    if (!buildDiscoveryDeviceBlock(deviceBlock, sizeof(deviceBlock), ctx)) return false;
    int topicLen = snprintf_P(topic, sizeof(topic),
                              PSTR("%s/binary_sensor/%s/sat_zone_%u_%s/config"),
                              ctx.haPrefix, ctx.nodeId, zoneNumber, metric);
    if (topicLen <= 0 || static_cast<size_t>(topicLen) >= sizeof(topic)) return false;
    int payloadLen = snprintf_P(payload, sizeof(payload),
                                PSTR("{\"avty_t\":\"%s\",%s,\"uniq_id\":\"%s-sat_zone_%u_%s\",\"name\":\"%s_SAT_Zone_%u_%s\",\"stat_t\":\"%s/sat/zone/%u/%s\",\"pl_on\":\"true\",\"pl_off\":\"false\",\"stat_on\":\"true\",\"stat_off\":\"false\",\"icon\":\"mdi:%s\",\"origin\":{\"name\":\"OTGW-firmware\",\"sw\":\"%s\",\"url\":\"https://github.com/rvdbreemen/OTGW-firmware\"}}"),
                                ctx.mqttPubTopic, deviceBlock, ctx.nodeId, zoneNumber, metric,
                                ctx.hostname, zoneNumber, friendlySuffix,
                                ctx.mqttPubTopic, zoneNumber, metric,
                                icon, ctx.version);
    if (payloadLen <= 0 || static_cast<size_t>(payloadLen) >= sizeof(payload)) return false;
    if (!publishDiscoveryJson(client, topic, payload, static_cast<size_t>(payloadLen))) return false;
    ctx.isFirstEntity = false;
    return true;
  };

  // TASK-543 decision: publish zone discovery only for zones that are configured
  // now or still considered active from recent SAT runtime data.
  for (uint8_t i = 0; i < maxZones; i++) {
    if (!satShouldDiscoverZone(i)) continue;
    const uint8_t zoneNumber = i + 1;
    if (!publishZoneBinary(zoneNumber, "active", "Active", "thermostat")) return false;
    if (!publishZoneSensor(zoneNumber, "output", "Output", "thermometer", "°C", "temperature", "measurement")) return false;
    if (!publishZoneSensor(zoneNumber, "error", "Error", "thermometer", "°C", "temperature", "measurement")) return false;
  }

  // TASK-640: PV-surplus boost discovery. Chained into the zone discovery drip
  // slot so we don't have to allocate a new pseudo-ID. The switch + 5 number
  // entities are emitted unconditionally (so users can enable the feature from
  // HA); the sensor / binary_sensor entities are emitted only when enabled to
  // keep the dashboard clean for users who don't use it.
  if (!streamSatPvBoostDiscovery(client, ctx)) return false;

  return true;
}

// TASK-640: PV-surplus boost HA auto-discovery.
// Mirrors the local-lambda pattern of streamSatZoneDiscovery: small helpers
// stamp out sensor/binary_sensor/switch/number configs with the shared device
// block + origin block. All entities live under the unique-id namespace
// "<nodeId>-sat_pv_boost_*" so HA aggregates them under the OTGW device.
static bool streamSatPvBoostDiscovery(PubSubClient &client, HaDiscoveryContext &ctx)
{
  if (!client.connected()) return false;
  if (!canPublishMQTT()) return false;

  char topic[180];
  char payload[768];
  char deviceBlock[256];

  auto publishSensor = [&](const char *metric,
                           const char *friendlySuffix,
                           const char *unit,
                           const char *deviceClass,
                           const char *stateClass,
                           const char *icon) -> bool {
    if (!buildDiscoveryDeviceBlock(deviceBlock, sizeof(deviceBlock), ctx)) return false;
    int topicLen = snprintf_P(topic, sizeof(topic),
                              PSTR("%s/sensor/%s/sat_pv_%s/config"),
                              ctx.haPrefix, ctx.nodeId, metric);
    if (topicLen <= 0 || static_cast<size_t>(topicLen) >= sizeof(topic)) return false;
    int payloadLen = snprintf_P(payload, sizeof(payload),
                                PSTR("{\"avty_t\":\"%s\",%s,\"uniq_id\":\"%s-sat_pv_%s\",\"name\":\"%s_SAT_PV_%s\",\"stat_t\":\"%s/sat/pv_%s\",\"device_class\":\"%s\",\"unit_of_measurement\":\"%s\",\"state_class\":\"%s\",\"icon\":\"mdi:%s\",\"value_template\":\"{{ value }}\",\"origin\":{\"name\":\"OTGW-firmware\",\"sw\":\"%s\",\"url\":\"https://github.com/rvdbreemen/OTGW-firmware\"}}"),
                                ctx.mqttPubTopic, deviceBlock, ctx.nodeId, metric,
                                ctx.hostname, friendlySuffix,
                                ctx.mqttPubTopic, metric,
                                deviceClass, unit, stateClass, icon, ctx.version);
    if (payloadLen <= 0 || static_cast<size_t>(payloadLen) >= sizeof(payload)) return false;
    if (!publishDiscoveryJson(client, topic, payload, static_cast<size_t>(payloadLen))) return false;
    ctx.isFirstEntity = false;
    return true;
  };

  auto publishBinary = [&](const char *metric,
                           const char *friendlySuffix,
                           const char *icon) -> bool {
    if (!buildDiscoveryDeviceBlock(deviceBlock, sizeof(deviceBlock), ctx)) return false;
    int topicLen = snprintf_P(topic, sizeof(topic),
                              PSTR("%s/binary_sensor/%s/sat_pv_%s/config"),
                              ctx.haPrefix, ctx.nodeId, metric);
    if (topicLen <= 0 || static_cast<size_t>(topicLen) >= sizeof(topic)) return false;
    int payloadLen = snprintf_P(payload, sizeof(payload),
                                PSTR("{\"avty_t\":\"%s\",%s,\"uniq_id\":\"%s-sat_pv_%s\",\"name\":\"%s_SAT_PV_%s\",\"stat_t\":\"%s/sat/pv_%s\",\"pl_on\":\"true\",\"pl_off\":\"false\",\"icon\":\"mdi:%s\",\"origin\":{\"name\":\"OTGW-firmware\",\"sw\":\"%s\",\"url\":\"https://github.com/rvdbreemen/OTGW-firmware\"}}"),
                                ctx.mqttPubTopic, deviceBlock, ctx.nodeId, metric,
                                ctx.hostname, friendlySuffix,
                                ctx.mqttPubTopic, metric,
                                icon, ctx.version);
    if (payloadLen <= 0 || static_cast<size_t>(payloadLen) >= sizeof(payload)) return false;
    if (!publishDiscoveryJson(client, topic, payload, static_cast<size_t>(payloadLen))) return false;
    ctx.isFirstEntity = false;
    return true;
  };

  auto publishSwitch = [&](const char *metric,
                           const char *friendlySuffix,
                           const char *icon) -> bool {
    if (!buildDiscoveryDeviceBlock(deviceBlock, sizeof(deviceBlock), ctx)) return false;
    int topicLen = snprintf_P(topic, sizeof(topic),
                              PSTR("%s/switch/%s/sat_pv_%s/config"),
                              ctx.haPrefix, ctx.nodeId, metric);
    if (topicLen <= 0 || static_cast<size_t>(topicLen) >= sizeof(topic)) return false;
    int payloadLen = snprintf_P(payload, sizeof(payload),
                                PSTR("{\"avty_t\":\"%s\",%s,\"uniq_id\":\"%s-sat_pv_%s\",\"name\":\"%s_SAT_PV_%s\",\"stat_t\":\"%s/sat/%s\",\"cmd_t\":\"%s/sat/%s\",\"pl_on\":\"1\",\"pl_off\":\"0\",\"stat_on\":\"1\",\"stat_off\":\"0\",\"icon\":\"mdi:%s\",\"origin\":{\"name\":\"OTGW-firmware\",\"sw\":\"%s\",\"url\":\"https://github.com/rvdbreemen/OTGW-firmware\"}}"),
                                ctx.mqttPubTopic, deviceBlock, ctx.nodeId, metric,
                                ctx.hostname, friendlySuffix,
                                ctx.mqttPubTopic, metric,
                                ctx.mqttSubTopic, metric,
                                icon, ctx.version);
    if (payloadLen <= 0 || static_cast<size_t>(payloadLen) >= sizeof(payload)) return false;
    if (!publishDiscoveryJson(client, topic, payload, static_cast<size_t>(payloadLen))) return false;
    ctx.isFirstEntity = false;
    return true;
  };

  auto publishNumber = [&](const char *metric,
                           const char *friendlySuffix,
                           float minVal, float maxVal, float step,
                           const char *unit,
                           const char *icon) -> bool {
    if (!buildDiscoveryDeviceBlock(deviceBlock, sizeof(deviceBlock), ctx)) return false;
    int topicLen = snprintf_P(topic, sizeof(topic),
                              PSTR("%s/number/%s/sat_pv_%s/config"),
                              ctx.haPrefix, ctx.nodeId, metric);
    if (topicLen <= 0 || static_cast<size_t>(topicLen) >= sizeof(topic)) return false;
    char minBuf[12], maxBuf[12], stepBuf[12];
    dtostrf(minVal, 1, 2, minBuf);
    dtostrf(maxVal, 1, 2, maxBuf);
    dtostrf(step,   1, 2, stepBuf);
    int payloadLen = snprintf_P(payload, sizeof(payload),
                                PSTR("{\"avty_t\":\"%s\",%s,\"uniq_id\":\"%s-sat_pv_%s\",\"name\":\"%s_SAT_PV_%s\",\"stat_t\":\"%s/sat/%s\",\"cmd_t\":\"%s/sat/%s\",\"min\":%s,\"max\":%s,\"step\":%s,\"unit_of_measurement\":\"%s\",\"mode\":\"box\",\"icon\":\"mdi:%s\",\"origin\":{\"name\":\"OTGW-firmware\",\"sw\":\"%s\",\"url\":\"https://github.com/rvdbreemen/OTGW-firmware\"}}"),
                                ctx.mqttPubTopic, deviceBlock, ctx.nodeId, metric,
                                ctx.hostname, friendlySuffix,
                                ctx.mqttPubTopic, metric,
                                ctx.mqttSubTopic, metric,
                                minBuf, maxBuf, stepBuf, unit, icon, ctx.version);
    if (payloadLen <= 0 || static_cast<size_t>(payloadLen) >= sizeof(payload)) return false;
    if (!publishDiscoveryJson(client, topic, payload, static_cast<size_t>(payloadLen))) return false;
    ctx.isFirstEntity = false;
    return true;
  };

  // Always-on entities: switch + 5 numbers (so HA users can enable from UI).
  if (!publishSwitch("boost_enabled", "PV_Boost_Enabled", "solar-power-variant")) return false;
  if (!publishNumber("boost_threshold_w",      "PV_Boost_Threshold",     100, 10000, 100, "W",   "flash"))         return false;
  if (!publishNumber("boost_hold_s",           "PV_Boost_Hold",           30,   600,  10, "s",   "timer-sand"))    return false;
  if (!publishNumber("boost_delta_c",          "PV_Boost_Delta",         0.5,    5,  0.1, "°C",  "thermometer-plus")) return false;
  if (!publishNumber("boost_max_indoor_c",     "PV_Boost_Max_Indoor",     18,    28, 0.5, "°C",  "home-thermometer")) return false;
  if (!publishNumber("boost_max_duration_min", "PV_Boost_Max_Duration",   30,  1440,  10, "min", "timer"))         return false;

  // Telemetry entities: only when feature is enabled, keeps dashboard clean.
  if (settings.sat.bPvBoostEnabled) {
    if (!publishSensor("surplus_w",     "Surplus",        "W",  "power",       "measurement", "solar-power")) return false;
    if (!publishBinary("boost_active",  "Boost_Active",                                       "fire")) return false;
    if (!publishSensor("boost_applied_c","Boost_Applied", "°C", "temperature", "measurement", "thermometer-chevron-up")) return false;
  }

  return true;
}

// ADR-097 supersedes ADR-095: under sibling-suffix shape the canonical entity
// coexists with the source variants without semantic duplication, so the base-
// entity suppression bitmap (msgIdHasAnySourceEntry, originally added under
// TASK-528 / ADR-095) is removed. Source variants are now purely additive.

void doAutoConfigure(){
  // Force-publishes HA discovery configs for ALL entries asynchronously.
  // Routes through the drip publisher (markAllMQTTConfigPending +
  // loopMQTTDiscovery) instead of streaming the full set inline. This
  // eliminates the handleOTGW() stall during F-key force-discovery and
  // POST /api/v2/otgw/discovery; the REST endpoint already returns
  // 202 Accepted before this call, so the contract was always async.
  if (!settings.mqtt.bEnable) return;
  markAllMQTTConfigPending();
}
//===========================================================================================
bool doAutoConfigureMsgid(byte OTid, bool isFirst)
{
  // Dallas sensors have their own discovery path
  if (OTid == OTGWdallasdataid) {
    configSensors();
    return true;
  }

  MQTTAutoConfigSessionLock sessionLock;
  if (!sessionLock.locked) return false;
  if (!settings.mqtt.bEnable) return false;
  if (!MQTTclient.connected()) return false;
  if (!isValidIP(MQTTbrokerIP)) return false;
  if (platformFreeHeap() < MQTT_DISCOVERY_HEAP_MIN) return false;

  bool result = false;
  HaDiscoveryContext ctx = buildDiscoveryContext(isFirst);

  // Sensors
  uint16_t sIdx = readSensorIndex(OTid);
  if (sIdx != MQTT_HA_INDEX_NONE) {
    while (sIdx < MQTT_HA_SENSOR_COUNT) {
      MqttHaSensorCfg cfg = readSensorCfg(sIdx);
      if (cfg.id != OTid) break;
      if (cfg.flags & MQTT_HA_FLAG_ANY_SOURCE) {
        if (settings.mqtt.bSeparateSources) {
          if (expandAndStreamSensorSources(MQTTclient, cfg, ctx)) result = true;
        }
      } else {
        // ADR-097: base entity always emitted (ADR-095 suppression dropped).
        if (streamSensorDiscovery(MQTTclient, cfg, ctx)) result = true;
      }
      sIdx++;
      feedWatchDog();
    }
  }

  // Binary sensors — indexed range. ADR-106: filter by naming mode.
  // - new mode (default, bUseLegacyOtTopics=false): SKIP rows flagged
  //   MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS (they have an alias replacement).
  // - legacy mode (bUseLegacyOtTopics=true): publish all indexed rows.
  const bool useLegacy = settings.mqtt.bUseLegacyOtTopics;
  uint16_t bIdx = readBinSensorIndex(OTid);
  if (bIdx != MQTT_HA_INDEX_NONE) {
    while (bIdx < MQTT_HA_BINSENSOR_INDEXED_COUNT) {
      MqttHaBinSensorCfg cfg = readBinSensorCfg(bIdx);
      if (cfg.id != OTid) break;
      const bool skipReplaced = !useLegacy && (cfg.flags & MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS);
      if (!skipReplaced) {
        if (streamBinarySensorDiscovery(MQTTclient, cfg, ctx)) result = true;
      }
      bIdx++;
      feedWatchDog();
    }
  }
  // ADR-106: alias tail (non-contiguous; not covered by index). Walked only
  // in new mode — these are the new defaults. Legacy mode skips them entirely.
  if (!useLegacy) {
    for (uint16_t aIdx = MQTT_HA_BINSENSOR_INDEXED_COUNT; aIdx < MQTT_HA_BINSENSOR_COUNT; aIdx++) {
      MqttHaBinSensorCfg cfg = readBinSensorCfg(aIdx);
      if (cfg.id != OTid) continue;
      if (streamBinarySensorDiscovery(MQTTclient, cfg, ctx)) result = true;
      feedWatchDog();
    }
  }

  // Climate + SAT switches/select (OT ID 0 — TASK-284 piggyback)
  if (OTid == 0) {
    if (streamClimateDiscovery(MQTTclient, 0, ctx)) result = true;
    if (streamClimateDiscovery(MQTTclient, 1, ctx)) result = true;
    // TASK-516: idx 13 (dhw_enable) gated on MsgID 3 HB3=1 (storage tank).
    const bool dhwEnableSwitchAllowed =
        (OTcurrentSystemState.SlaveConfigMemberIDcode & 0x0800) != 0;
    for (uint8_t swIdx = 0; swIdx < 14; swIdx++) {
      if (swIdx == 13 && !dhwEnableSwitchAllowed) continue;
      feedWatchDog();
      if (streamSatSwitchDiscovery(MQTTclient, swIdx, ctx)) result = true;
    }
    feedWatchDog();
    if (streamSatSelectDiscovery(MQTTclient, 0, ctx)) result = true;
  }
  // Number (OT ID 27)
  if (OTid == 27) {
    if (streamNumberDiscovery(MQTTclient, ctx)) result = true;
  }

  if (OTid == OTGWsatzoneid) {
    if (streamSatZoneDiscovery(MQTTclient, ctx)) result = true;
  }

  // PIC control entities (pseudo-ID 244): resetgateway button + GPIO/LED selects.
  // Published unconditionally — like the other PIC pseudo-IDs (249/250) the entity
  // is always discovered; the set-commands and otgw-pic/ state topics are PIC-gated
  // at their source. Gating discovery on isPICEnabled() would leave result=false on
  // PIC-less devices and make loopMQTTDiscovery() retry this ID every drip tick
  // forever (the bug fixed on dev in PR #596).
  //
  // All-or-nothing: clear the pending bit only when every one of the nine configs
  // published this tick. If any single publish fails (transient MQTT/heap), result
  // stays false so loopMQTTDiscovery() retains the pending bit and retries the whole
  // set next tick (retained idempotent re-publish). Mirrors dev PR #596.
  if (OTid == OTGWpiccontrolsid) {
    bool allOk = streamButtonDiscovery(MQTTclient, ctx);
    feedWatchDog();
    for (uint8_t i = 0; i <= 7; i++) {
      if (!streamSelectDiscovery(MQTTclient, i, ctx)) allOk = false;
      feedWatchDog();
    }
    if (allOk) result = true;
  }

  return result;
}


void sensorAutoConfigure(byte dataid, bool finishflag, const char *cfgSensorId = nullptr) {
  // Dallas temperature sensor discovery via streaming API.
  // cfgSensorId is the Dallas device address string (e.g. "28FF1234567890").
  if (getMQTTConfigDone(dataid) && finishflag) return;
  if (!cfgSensorId || cfgSensorId[0] == '\0') return;

  HaDiscoveryContext ctx = buildDiscoveryContext();
  bool success = streamDallasSensorDiscovery(MQTTclient, cfgSensorId, ctx);
  if (success) {
    MQTTDebugTf(PSTR("Dallas discovery sent for [%s]\r\n"), cfgSensorId);
    if (finishflag) setMQTTConfigDone(dataid);
  } else {
    MQTTDebugTf(PSTR("Dallas discovery failed for [%s]\r\n"), cfgSensorId);
  }
}

#if HAS_SAT_BLE
//===========================================================================================
// TASK-488 BLE HA-discovery helpers (ESP32 / OTGW32 only).
// TASK-742: guarded by HAS_SAT_BLE (not raw defined(ESP32)); ESP8266 uses the
// inline no-op stubs in platform_esp8266.h.
//
// Caller wiring (lives in SATble.ino, finalised by TASK-487):
//   In satBLEPublishMQTT(), iterate _bleSensors[] valid slots:
//     1. Build macCompact via satBLEMacToCompact(slot.sMacAddress, ...).
//     2. If slot.bDiscoveryPublished is false:
//          - Call satBLEPublishHaDiscovery(macCompact, slot.sMacAddress)
//          - Set slot.bDiscoveryPublished = true on success path
//     3. Call satBLEPublishStateTopics(macCompact, slot.fTemperature, ...)
//   The bDiscoveryPublished flag must be added to BLESensorData by TASK-487.
//
// ADR-077 conformance: HA discovery configs are emitted via the existing
// streaming primitives (beginMqttPublish + writeMqttChunk + endPublish)
// using a SINGLE-buffer publish per the bounded-payload exception — the
// per-config payload is statically capped at 768 bytes and all four BLE
// configs comfortably fit. ADR-077 normally prescribes a two-pass
// MEASURE-then-WRITE for unbounded payloads; the bounded case is the
// addendum codified in ADR-077 (TASK-499 / 1B-M1).
// The 256-bit OT-ID drip bitmap (MQTTautoCfgPendingMap) does not apply
// here: BLE MACs are not OT IDs. Drip pacing is provided by the caller
// cadence (one BLE scan per iBleInterval, typically 30 s) plus the
// one-shot bDiscoveryPublished flag — there is no synchronous burst of
// N×4 retained configs per scan. canPublishMQTT() and
// MQTT_DISCOVERY_HEAP_MIN gate every publish, so heap pressure
// transparently defers via the existing tier machine without needing a
// separate queue.
//===========================================================================================

// Convert "AA:BB:CC:DD:EE:FF" into "aabbccddeeff" (lowercase, no colons).
// Bounded write; on malformed input (not exactly 17 chars / wrong colon
// positions) writes empty string. No String, no heap. Exported for
// SATble.ino which builds the compact MAC before calling
// satBLEPublishHaDiscovery / satBLEPublishStateTopics.
void satBLEMacToCompact(const char* macWithColons, char* out, size_t outSize)
{
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (!macWithColons) return;
  // Expect exactly 17 chars: "AA:BB:CC:DD:EE:FF"
  size_t inLen = strnlen(macWithColons, 18);
  if (inLen != 17) {
    MQTTDebugTf(PSTR("[ble-disc] malformed MAC '%s' (len=%u, want 17)\r\n"),
                macWithColons, (unsigned)inLen);
    return;
  }
  // Need 12 hex chars + NUL in output.
  if (outSize < BLE_MAC_COMPACT_SIZE) return;
  size_t o = 0;
  for (size_t i = 0; i < 17; i++) {
    char c = macWithColons[i];
    if ((i % 3) == 2) {
      if (c != ':') {
        MQTTDebugTf(PSTR("[ble-disc] malformed MAC '%s' (colon expected at %u)\r\n"),
                    macWithColons, (unsigned)i);
        out[0] = '\0';
        return;
      }
      continue;
    }
    if (!isxdigit(static_cast<unsigned char>(c))) {
      MQTTDebugTf(PSTR("[ble-disc] malformed MAC '%s' (non-hex at %u)\r\n"),
                  macWithColons, (unsigned)i);
      out[0] = '\0';
      return;
    }
    out[o++] = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  }
  out[o] = '\0';
}

// Publish 4 BLE state topics under <MQTTPubNamespace>/sat/ble/<mac>/{temp,rh,bat,rssi}.
// Not retained (state). Skips silently if MQTT is not connected.
//
// ADR-111: on-change + jittered heartbeat. The shadows below cover the
// "currently published MAC". When the selected MAC changes (user picks a
// different sensor) we reset all four shadows so values for the new MAC
// don't get suppressed by stale comparisons against the previous MAC.
void satBLEPublishStateTopics(const char* macCompact, float temp, float hum, uint8_t bat, int8_t rssi)
{
  if (!macCompact || macCompact[0] == '\0') return;
  if (!settings.mqtt.bEnable || !state.mqtt.bConnected) return;

  static SATShadowF s_ble_temp;
  static SATShadowF s_ble_rh;
  static SATShadowI s_ble_bat;
  static SATShadowI s_ble_rssi;
  static char       s_last_mac[BLE_MAC_COMPACT_SIZE] = {0};

  // MAC changed (or first call): wipe shadows so the new MAC publishes fresh.
  if (strncmp(macCompact, s_last_mac, sizeof(s_last_mac)) != 0) {
    s_ble_temp = SATShadowF{};
    s_ble_rh   = SATShadowF{};
    s_ble_bat  = SATShadowI{};
    s_ble_rssi = SATShadowI{};
    strlcpy(s_last_mac, macCompact, sizeof(s_last_mac));
  }

  char topic[64];

  snprintf_P(topic, sizeof(topic), PSTR("sat/ble/%s/temp"), macCompact);
  publishIfChangedF(topic, temp, s_ble_temp, SAT_EPS_TEMP,        2, false);

  snprintf_P(topic, sizeof(topic), PSTR("sat/ble/%s/rh"), macCompact);
  publishIfChangedF(topic, hum,  s_ble_rh,   SAT_EPS_TEMP_COARSE, 2, false);

  snprintf_P(topic, sizeof(topic), PSTR("sat/ble/%s/bat"), macCompact);
  publishIfChangedI(topic, (int32_t)bat,  s_ble_bat,  false);

  snprintf_P(topic, sizeof(topic), PSTR("sat/ble/%s/rssi"), macCompact);
  publishIfChangedI(topic, (int32_t)rssi, s_ble_rssi, false);
}

// Build one HA discovery config payload and publish it retained to
// <HaPrefix>/sensor/<uniqueId>_ble_<mac>_<kind>/config.
// Manual JSON via snprintf_P (no ArduinoJson per ADR). Returns true on
// successful endPublish.
//
// Single-buffer publish via the streaming primitives. Payload size is
// bounded at 768 bytes (the four discovery configs each fit comfortably
// under 600), so one stack-local allocation suffices and we skip the
// two-pass MEASURE-then-WRITE dance ADR-077 prescribes for unbounded
// payloads. Heap pressure is still bounded by the canPublishMQTT() and
// MQTT_DISCOVERY_HEAP_MIN gates per call, so behaviour is heap-safe.
// TASK-508: tiny JSON-escaper for user-supplied labels in HA discovery
// payloads. Local to MQTTstuff.ino to avoid the global cMsg scratch
// buffer used by escapeJsonStringTo() (which is unsafe to call from
// here while writeSettings() may be flushing — even though they
// currently share the loop task, this keeps the contract explicit).
// Handles the only characters JSON forbids in a quoted string: " \ and
// the ASCII control range 0x00-0x1F. Output is always NUL-terminated.
static void mqttJsonEscape(const char* src, char* dst, size_t dstSize)
{
  if (!dst || dstSize == 0) return;
  size_t di = 0;
  for (size_t si = 0; src && src[si] && di + 1 < dstSize; si++) {
    char c = src[si];
    if (c == '"' || c == '\\') {
      if (di + 2 >= dstSize) break;
      dst[di++] = '\\';
      dst[di++] = c;
    } else if ((unsigned char)c < 0x20) {
      if (di + 6 >= dstSize) break;
      di += snprintf(dst + di, dstSize - di, "\\u%04x", (unsigned)c);
    } else {
      dst[di++] = c;
    }
  }
  dst[di] = '\0';
}

static bool satBLEPublishOneDiscovery(const char* macCompact,
                                         const char* macWithColons,
                                         const char* kindKey,        // "temp" | "rh" | "bat" | "rssi"
                                         const __FlashStringHelper* friendlyName,
                                         const __FlashStringHelper* deviceClass,
                                         const __FlashStringHelper* unit,
                                         const __FlashStringHelper* valueTemplate,
                                         const char* labelOverride)  // TASK-508: optional, NULL = legacy "BLE %s %s" form
{
  if (!settings.mqtt.bEnable) return false;
  if (!MQTTclient.connected()) return false;
  if (!isValidIP(MQTTbrokerIP)) return false;
  if (!canPublishMQTT()) return false;
  if (platformFreeHeap() < MQTT_DISCOVERY_HEAP_MIN) return false;

  const char* haPrefix  = CSTR(settings.mqtt.sHaprefix);
  const char* uniqueId  = CSTR(settings.mqtt.sUniqueid);
  const char* topTopic  = CSTR(settings.mqtt.sTopTopic);

  char topic[MQTT_TOPIC_MAX_LEN];
  snprintf_P(topic, sizeof(topic),
             PSTR("%s/sensor/%s_ble_%s_%s/config"),
             haPrefix, uniqueId, macCompact, kindKey);

  char stateTopic[MQTT_TOPIC_MAX_LEN];
  snprintf_P(stateTopic, sizeof(stateTopic),
             PSTR("%s/value/%s/sat/ble/%s/%s"),
             topTopic, uniqueId, macCompact, kindKey);

  // TASK-508: when a user-set label is provided, render the entity name
  // as "<label> <Kind>" (e.g. "Woonkamer Temperature") and the device
  // name as "<label>". Both fields are JSON-escaped locally to handle
  // quotes/backslashes in user input without reaching for cMsg.
  bool useLabel = (labelOverride && labelOverride[0] != '\0');
  char labelEsc[64] = {0};
  if (useLabel) {
    mqttJsonEscape(labelOverride, labelEsc, sizeof(labelEsc));
  }

  // Compose the JSON payload into a single bounded RAM buffer.
  // Two snprintf_P branches (label vs legacy) keep the format strings
  // statically analysable and the headroom calculation explicit.
  char payload[768];
  int n;
  if (useLabel) {
    n = snprintf_P(payload, sizeof(payload),
      PSTR("{"
        "\"name\":\"%s %S\","
        "\"uniq_id\":\"%s_ble_%s_%s\","
        "\"stat_t\":\"%s\","
        "\"dev_cla\":\"%S\","
        "\"unit_of_meas\":\"%S\","
        "\"val_tpl\":\"%S\","
        "\"dev\":{"
          "\"ids\":[\"%s_ble_%s\"],"
          "\"name\":\"%s\","
          "\"mdl\":\"BLE Sensor\","
          "\"mf\":\"BLE\","
          "\"via_device\":\"%s\""
        "}"
      "}"),
      labelEsc, reinterpret_cast<PGM_P>(friendlyName),
      uniqueId, macCompact, kindKey,
      stateTopic,
      reinterpret_cast<PGM_P>(deviceClass),
      reinterpret_cast<PGM_P>(unit),
      reinterpret_cast<PGM_P>(valueTemplate),
      uniqueId, macCompact,
      labelEsc,
      uniqueId);
  } else {
    n = snprintf_P(payload, sizeof(payload),
      PSTR("{"
        "\"name\":\"BLE %s %s\","
        "\"uniq_id\":\"%s_ble_%s_%s\","
        "\"stat_t\":\"%s\","
        "\"dev_cla\":\"%S\","
        "\"unit_of_meas\":\"%S\","
        "\"val_tpl\":\"%S\","
        "\"dev\":{"
          "\"ids\":[\"%s_ble_%s\"],"
          "\"name\":\"BLE %s\","
          "\"mdl\":\"BLE Sensor\","
          "\"mf\":\"BLE\","
          "\"via_device\":\"%s\""
        "}"
      "}"),
      macWithColons, reinterpret_cast<PGM_P>(friendlyName),
      uniqueId, macCompact, kindKey,
      stateTopic,
      reinterpret_cast<PGM_P>(deviceClass),
      reinterpret_cast<PGM_P>(unit),
      reinterpret_cast<PGM_P>(valueTemplate),
      uniqueId, macCompact,
      macWithColons,
      uniqueId);
  }

  if (n <= 0 || (size_t)n >= sizeof(payload)) {
    MQTTDebugTf(PSTR("[ble-disc] payload truncated for %s/%s\r\n"), macCompact, kindKey);
    return false;
  }

  // TASK-496 (2B-H1): every return path that follows a network attempt feeds
  // the watchdog. PubSubClient sockets can stall up to ~15 s on a flaky broker,
  // and a 16-publish first-scan burst (4 sensors × 4 configs) can otherwise
  // traverse all branches without a single watchdog kick.
  if (!beginMqttPublish(topic, (size_t)n, /*retain=*/true)) { feedWatchDog(); return false; }
  if (!writeMqttChunk(payload, (size_t)n)) {
    MQTTclient.endPublish();
    feedWatchDog();
    return false;
  }
  if (!MQTTclient.endPublish()) { PrintMQTTError(); feedWatchDog(); return false; }
  feedWatchDog();
  return true;
}

// Publish 4 retained HA-discovery configs (temperature, humidity, battery,
// signal_strength) for one BLE sensor MAC. One-shot per MAC: the caller is
// responsible for tracking bDiscoveryPublished to avoid re-publishing on
// every publish cycle. This is intentional — the bitmap-drip in
// loopMQTTDiscovery() is keyed by OT message ID and does not have a slot
// for arbitrary MACs; the satBLEPublishMQTT cadence (every iBleInterval
// seconds, typically 30 s — post-TASK-494 the BLE scan itself is continuous,
// only the publish loop is throttled) already provides drip pacing.
// TASK-493 (1A-H1): returns true only when ALL four discovery configs were
// successfully published. The caller (satBLEPublishMQTT) gates the per-slot
// `bDiscoveryPublished` flag on this return value; a transient first-scan
// failure will retry on the next iBleInterval cycle instead of permanently
// suppressing HA discovery for that sensor.
bool satBLEPublishHaDiscovery(const char* macCompact, const char* macWithColons,
                              const char* label /* TASK-508: optional, NULL/"" = legacy */)
{
  if (!macCompact || macCompact[0] == '\0') return false;
  if (!macWithColons || macWithColons[0] == '\0') return false;
  if (!settings.mqtt.bEnable || !state.mqtt.bConnected) return false;

  // Each kind: short-name, device_class, unit, value_template. All PROGMEM.
  bool ok;
  ok = satBLEPublishOneDiscovery(macCompact, macWithColons,
        "temp",
        F("Temperature"),
        F("temperature"),
        F("°C"),
        F("{{ value | float }}"),
        label);
  if (!ok) {
    MQTTDebugTf(PSTR("[ble-disc] temp publish failed for %s\r\n"), macCompact);
    return false;
  }

  ok = satBLEPublishOneDiscovery(macCompact, macWithColons,
        "rh",
        F("Humidity"),
        F("humidity"),
        F("%"),
        F("{{ value | float }}"),
        label);
  if (!ok) {
    MQTTDebugTf(PSTR("[ble-disc] rh publish failed for %s\r\n"), macCompact);
    return false;
  }

  ok = satBLEPublishOneDiscovery(macCompact, macWithColons,
        "bat",
        F("Battery"),
        F("battery"),
        F("%"),
        F("{{ value | int }}"),
        label);
  if (!ok) {
    MQTTDebugTf(PSTR("[ble-disc] bat publish failed for %s\r\n"), macCompact);
    return false;
  }

  ok = satBLEPublishOneDiscovery(macCompact, macWithColons,
        "rssi",
        F("Signal Strength"),
        F("signal_strength"),
        F("dBm"),
        F("{{ value | int }}"),
        label);
  if (!ok) {
    MQTTDebugTf(PSTR("[ble-disc] rssi publish failed for %s\r\n"), macCompact);
    return false;
  }

  if (label && label[0] != '\0') {
    MQTTDebugTf(PSTR("[ble-disc] published 4 HA configs for MAC %s (label='%s')\r\n"),
                macCompact, label);
  } else {
    MQTTDebugTf(PSTR("[ble-disc] published 4 HA configs for MAC %s\r\n"), macCompact);
  }
  return true;
}

// TASK-508: wipe Home Assistant retained discovery configs for one BLE
// MAC. Publishes 4 zero-byte retained payloads to the same topics
// satBLEPublishOneDiscovery wrote — HA interprets empty retained
// discovery as device removal. Called from satBLERosterForget() in
// SATble.ino when the user drops the active selection.
void satBLEUnpublishDiscovery(const char* macCompact)
{
  if (!macCompact || macCompact[0] == '\0') return;
  if (!settings.mqtt.bEnable || !state.mqtt.bConnected) return;
  if (!isValidIP(MQTTbrokerIP)) return;
  if (!canPublishMQTT()) return;

  const char* haPrefix = CSTR(settings.mqtt.sHaprefix);
  const char* uniqueId = CSTR(settings.mqtt.sUniqueid);

  // Same kind keys as satBLEPublishHaDiscovery / satBLEPublishOneDiscovery.
  // PROGMEM kind names match the publish path; deviation here would leak
  // orphans on the broker.
  static const char* const kKinds[] = { "temp", "rh", "bat", "rssi" };

  for (int k = 0; k < 4; k++) {
    char topic[MQTT_TOPIC_MAX_LEN];
    snprintf_P(topic, sizeof(topic),
               PSTR("%s/sensor/%s_ble_%s_%s/config"),
               haPrefix, uniqueId, macCompact, kKinds[k]);
    // Zero-length retained payload via the streaming primitives — same
    // path as the publish side, so retain semantics are identical.
    if (beginMqttPublish(topic, 0, /*retain=*/true)) {
      // No writeMqttChunk needed for empty payload.
      if (!MQTTclient.endPublish()) {
        PrintMQTTError();
      }
    }
    feedWatchDog();
  }
  MQTTDebugTf(PSTR("[ble-disc] unpublished HA configs for MAC %s\r\n"),
              macCompact);
}
#endif // defined(ESP32)

// ============================================================================
// ADR-106: topic-naming-mode cleanup machinery
// ============================================================================
// When settings.mqtt.bUseLegacyOtTopics toggles, the firmware needs to clear
// the OTHER name set's retained discovery configs from the broker so HA
// removes those entities. We don't clear retained state-topic values — those
// stale out naturally via the LWT/availability fall.
//
// State: 1 byte mode + 5-byte bitmap, persisted to LittleFS '/topic_cleanup.bin'
//   mode 0  = idle (no cleanup pending)
//   mode 1  = clear 37 alias topics       (toggle was new→legacy)
//   mode 2  = clear 37 legacy-replaced    (toggle was legacy→new)
// Bitmap bit N = "the N-th topic in this mode is still pending".
//
// Drain is paced (1 publish per loop tick) and gated by canPublishMQTT().
// Reboot mid-drain: state is on flash, resumed lazily by runTopicCleanupStep.
// MQTT disconnect mid-drain: runTopicCleanupStep no-ops while not connected,
// auto-resumes when MQTT reconnects.
// ----------------------------------------------------------------------------

constexpr uint8_t TOPIC_CLEANUP_MODE_IDLE             = 0;
constexpr uint8_t TOPIC_CLEANUP_MODE_CLEAR_ALIASES    = 1;
constexpr uint8_t TOPIC_CLEANUP_MODE_CLEAR_LEGACY_REP = 2;
constexpr const char *kTopicCleanupFile = "/topic_cleanup.bin";

struct TopicCleanupState {
  uint8_t mode;        // see TOPIC_CLEANUP_MODE_* above
  uint8_t bitmap[5];   // 40 bits; up to 37 used (3 unused top bits cleared at arm)
};
static TopicCleanupState g_topicCleanup = {TOPIC_CLEANUP_MODE_IDLE, {0, 0, 0, 0, 0}};
static bool              g_topicCleanupLoaded = false;

static void writeTopicCleanupState() {
  File f = LittleFS.open(kTopicCleanupFile, "w");
  if (!f) return;
  f.write(&g_topicCleanup.mode, 1);
  f.write(g_topicCleanup.bitmap, sizeof(g_topicCleanup.bitmap));
  f.close();
}

static void deleteTopicCleanupFile() {
  if (LittleFS.exists(kTopicCleanupFile)) LittleFS.remove(kTopicCleanupFile);
}

static void readTopicCleanupStateOnce() {
  if (g_topicCleanupLoaded) return;
  g_topicCleanupLoaded = true;
  if (!LittleFS.exists(kTopicCleanupFile)) return;
  File f = LittleFS.open(kTopicCleanupFile, "r");
  if (!f) return;
  uint8_t buf[6] = {0};
  if (f.read(buf, sizeof(buf)) == (int)sizeof(buf)) {
    g_topicCleanup.mode = buf[0];
    memcpy(g_topicCleanup.bitmap, &buf[1], 5);
  }
  f.close();
  if (g_topicCleanup.mode != TOPIC_CLEANUP_MODE_IDLE) {
    DebugTf(PSTR("[ADR-106] topic cleanup state resumed from flash (mode=%u)\r\n"),
            (unsigned)g_topicCleanup.mode);
  }
}

// Arm cleanup: called from settingStuff.ino when bUseLegacyOtTopics flips.
// newUseLegacy=true  → we're entering legacy mode → clear the alias topics.
// newUseLegacy=false → we're entering new mode    → clear the legacy-replaced topics.
void armTopicCleanupOnLegacyToggle(bool newUseLegacy) {
  readTopicCleanupStateOnce();
  g_topicCleanup.mode = newUseLegacy ? TOPIC_CLEANUP_MODE_CLEAR_ALIASES
                                     : TOPIC_CLEANUP_MODE_CLEAR_LEGACY_REP;
  // Set all 37 bits (bits 0..36 = pending). Bits 37..39 stay 0.
  memset(g_topicCleanup.bitmap, 0xFF, 4);
  g_topicCleanup.bitmap[4] = 0x1F;  // 5 low bits set (bits 32..36); bits 37..39 cleared
  writeTopicCleanupState();
  DebugTf(PSTR("[ADR-106] armed topic cleanup mode=%u (37 topics pending)\r\n"),
          (unsigned)g_topicCleanup.mode);
}

// Find the N-th legacy-replaced row in mqttHaBinSensors[] (indexed range
// 0..MQTT_HA_BINSENSOR_INDEXED_COUNT-1). Returns -1 if not found.
static int16_t findNthLegacyReplacedRow(uint8_t n) {
  uint8_t count = 0;
  for (uint16_t i = 0; i < MQTT_HA_BINSENSOR_INDEXED_COUNT; i++) {
    MqttHaBinSensorCfg cfg = readBinSensorCfg(i);
    if (cfg.flags & MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS) {
      if (count == n) return (int16_t)i;
      count++;
    }
  }
  return -1;
}

// Publish an empty retained payload to the discovery topic for the given row.
// HA reads zero-length retained as "remove this entity".
static bool publishEmptyDiscoveryFor(const MqttHaBinSensorCfg &cfg) {
  if (!MQTTclient.connected()) return false;
  if (!canPublishMQTT()) return false;
  char labelBuf[48];
  strlcpy_P(labelBuf, cfg.label, sizeof(labelBuf));
  char topic[MQTT_TOPIC_MAX_LEN];
  snprintf_P(topic, sizeof(topic), PSTR("%s/binary_sensor/%s/%s/config"),
             CSTR(settings.mqtt.sHaprefix), CSTR(settings.mqtt.sUniqueid), labelBuf);
  // Zero-length retained payload via the streaming primitives — same path
  // as the deregister code in mqttSatBLEUnpublishHaDiscoveryForMac().
  if (!beginMqttPublish(topic, 0, /*retain=*/true)) return false;
  if (!MQTTclient.endPublish()) return false;
  return true;
}

// Drain one bit per call from doBackgroundTasks(). Total work = up to 37
// successful publishes per toggle, paced by the loop tick cadence.
void runTopicCleanupStep() {
  readTopicCleanupStateOnce();
  if (g_topicCleanup.mode == TOPIC_CLEANUP_MODE_IDLE) return;
  if (!MQTTclient.connected()) return;  // resume on next connect
  if (!canPublishMQTT()) return;

  // Find the next pending bit (lowest set bit).
  int8_t bitIdx = -1;
  for (uint8_t i = 0; i < 37; i++) {
    if (g_topicCleanup.bitmap[i >> 3] & (1 << (i & 7))) { bitIdx = i; break; }
  }
  if (bitIdx < 0) {
    // Drain complete.
    DebugTln(F("[ADR-106] topic cleanup complete; deleting state file"));
    g_topicCleanup.mode = TOPIC_CLEANUP_MODE_IDLE;
    memset(g_topicCleanup.bitmap, 0, sizeof(g_topicCleanup.bitmap));
    deleteTopicCleanupFile();
    return;
  }

  // Look up the row this bit refers to.
  int16_t rowIdx = -1;
  if (g_topicCleanup.mode == TOPIC_CLEANUP_MODE_CLEAR_ALIASES) {
    // Alias rows are at indices MQTT_HA_BINSENSOR_INDEXED_COUNT..COUNT-1 (37 entries).
    rowIdx = MQTT_HA_BINSENSOR_INDEXED_COUNT + bitIdx;
    if (rowIdx >= (int16_t)MQTT_HA_BINSENSOR_COUNT) rowIdx = -1;
  } else if (g_topicCleanup.mode == TOPIC_CLEANUP_MODE_CLEAR_LEGACY_REP) {
    rowIdx = findNthLegacyReplacedRow((uint8_t)bitIdx);
  }
  if (rowIdx < 0) {
    // Mapping failure (shouldn't happen) — clear the bit and persist to avoid
    // an infinite loop, then return.
    g_topicCleanup.bitmap[bitIdx >> 3] &= ~(1 << (bitIdx & 7));
    writeTopicCleanupState();
    return;
  }

  MqttHaBinSensorCfg cfg = readBinSensorCfg((uint16_t)rowIdx);
  if (publishEmptyDiscoveryFor(cfg)) {
    g_topicCleanup.bitmap[bitIdx >> 3] &= ~(1 << (bitIdx & 7));
    writeTopicCleanupState();
    feedWatchDog();
  }
  // On failure: leave bit set, retry next tick.
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
***************************************************************************/
