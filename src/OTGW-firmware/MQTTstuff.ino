/* 
***************************************************************************  
**  Program  : MQTTstuff
**  Version  : v1.5.0-beta.3
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
void loopMQTTDiscovery();

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
// ~200 bytes per chunk, so the historical 1200-byte floor is obsolete. Value
// aligned with HEAP_WARNING_THRESHOLD (3072) in canPublishMQTT(): if heap is
// already at WARNING the drip skips rather than competing with publish-gate
// throttling.
constexpr uint32_t MQTT_DISCOVERY_HEAP_MIN = 3000;  // Streaming needs ~200 bytes; aligned with WARNING tier

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

// pgm_strncmp_PP() and pgm_read_char() are in MQTTstuff.h (inline, shared with mqtt_configuratie.cpp)

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
// progressing). 2000ms is the chosen default: it fits comfortably between
// Status-frames (~3s cadence leaves ~1s of drip window per cycle) while still
// giving lwIP pbufs from the just-finished burst time to drain before the next
// heap allocation. If you see iDripCooldownSkipCount climb again without
// discovery progressing, consider lowering further; raising above ~2500ms
// re-introduces the overlap stall.
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
constexpr unsigned long STATUS_BURST_COOLDOWN_MS = 2000;   // TASK-353: 10000->2000; stays under the ~3s Status cadence so the drip gets a window per cycle

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
// public streamXxxDiscovery() API live in mqtt_configuratie.cpp to avoid
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
const char s_empty[] PROGMEM = "";

const char s_cmd_command[] PROGMEM = "command";
const char s_cmd_setpoint[] PROGMEM = "setpoint";
const char s_cmd_constant[] PROGMEM = "constant";
const char s_cmd_outside[] PROGMEM = "outside";
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

// Source variant strings for separate-source MQTT data publishing.
// Used by publishToSourceTopic() via copySourceTableEntry().
const char s_mqtt_src_key_thermostat[] PROGMEM = "thermostat";
const char s_mqtt_src_key_boiler[] PROGMEM = "boiler";
const char s_mqtt_src_key_gateway[] PROGMEM = "gateway";

const char* const mqttSourceKeys[] PROGMEM = {
  s_mqtt_src_key_thermostat,
  s_mqtt_src_key_boiler,
  s_mqtt_src_key_gateway
};

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
  wifiClient.setSync(true);
  wifiClient.setNoDelay(true);

  // Outbound publishes stream via beginPublish/write/endPublish.
  // Keep only enough client buffer for inbound subscribed topics and payloads.
  MQTTclient.setBufferSize(MQTT_CLIENT_BUFFER_SIZE);
  
  stateMQTT = MQTT_STATE_INIT;
  //setup for mqtt discovery — mark all IDs pending for async drip publish
  strlcpy(NodeId, CSTR(settings.mqtt.sUniqueid), sizeof(NodeId));
  buildNamespace(MQTTPubNamespace, sizeof(MQTTPubNamespace), CSTR(settings.mqtt.sTopTopic), "value", NodeId);
  buildNamespace(MQTTSubNamespace, sizeof(MQTTSubNamespace), CSTR(settings.mqtt.sTopTopic), "set", NodeId);
  markAllMQTTConfigPending();  // queue all discovery for async drip publish
  handleMQTT(); //initialize the MQTT statemachine
}

bool bHAcycle = false;

// handles MQTT subscribe incoming stuff
void handleMQTTcallback(char* topic, byte* payload, unsigned int length) {

  if (state.debug.bMQTT) {
    DebugT(F("Message arrived on topic [")); Debug(topic); Debug(F("] = ["));
    for (unsigned int i = 0; i < length; i++) {
      Debug((char)payload[i]);
    }
    Debug(F("] (")); Debug(length); Debug(F(")")); Debugln(); DebugFlush();
  }

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
      // HA restart does not affect the MQTT broker connection; no reconnect needed.
      // Mark all discovery pending for async drip re-publish.
      markAllMQTTConfigPending();
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
        if (!isPICEnabled()) {
          MQTTDebugln(F(" MQTT command ignored: no PIC detected"));
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
            addOTWGcmdtoqueue(otgwcmd, strlen(otgwcmd), true);
          } else {
            //all other commands are <otgwcmd>=<payload message>
            // Copy command string from Flash to temp buffer for snprintf
            char cmdBuf[10];
            strncpy_P(cmdBuf, pOtgwCmd, sizeof(cmdBuf));
            cmdBuf[sizeof(cmdBuf)-1] = 0; // Ensure null termination

            snprintf_P(otgwcmd, sizeof(otgwcmd), PSTR("%s=%s"), cmdBuf, msgPayload);
            MQTTDebugf(PSTR(" found command, sending payload [%s]\r\n"), otgwcmd);
            addOTWGcmdtoqueue(otgwcmd, strlen(otgwcmd), true);
          }
        } else {
          //no match found
          MQTTDebugln();
          MQTTDebugTf(PSTR("No match found for command: [%s]\r\n"), topicToken);
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
        MQTTclient.setSocketTimeout(15);  // Increased from 4 to 15 seconds for better stability
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
      DebugTf(PSTR("[HEAP] pre-connect: free=%u max_block=%u\r\n"), ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());

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
      DebugTf(PSTR("[HEAP] post-connect: free=%u max_block=%u\r\n"), ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());

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
        DebugTf(PSTR("[HEAP] post-birth: free=%u max_block=%u\r\n"), ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());

        // Force re-publish of all OT values so HA gets current state after reconnect.
        requestMQTTRepublishAll();
        DebugTf(PSTR("[HEAP] post-republish: free=%u max_block=%u\r\n"), ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());

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
        DebugTf(PSTR("[HEAP] post-subscribe: free=%u max_block=%u\r\n"), ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
        sendMQTTversioninfo();
        publishAllPICsettings();
        DebugTf(PSTR("[HEAP] post-versioninfo: free=%u max_block=%u\r\n"), ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
      }
      else
      { // no connection, try again, do a non-blocking wait for 3 seconds.
        MQTTDebugln(F(" .. \r"));
        MQTTDebugTf(PSTR("failed, retrycount=[%d], rc=[%d] ..  try again in 3 seconds\r\n"), reconnectAttempts, MQTTclient.state());
        RESTART_TIMER(timerMQTTwaitforretry);
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

/* 
  topic:  <string> , sensor topic, will be automatically prefixed with <mqtt topic>/value/<node_id>
  json:   <string> , payload to send
  retain: <bool> , retain mqtt message  
*/
void sendMQTTData(const char* topic, const char *json, const bool retain)
{
  if (!settings.mqtt.bEnable) return;
  if (!mqttPublishAllowed) return;
  if (!MQTTclient.connected()) { return; }  // handleMQTT() logs disconnect and manages reconnect
  if (!isValidIP(MQTTbrokerIP)) {DebugTln(F("Error: MQTT broker IP not valid.")); return;} 
  
  // Check heap health before publishing
  if (!canPublishMQTT()) {
    // Message dropped due to low heap - canPublishMQTT() handles logging
    return;
  }
  
  char full_topic[MQTT_TOPIC_MAX_LEN];
  snprintf_P(full_topic, sizeof(full_topic), PSTR("%s/"), MQTTPubNamespace);
  strlcat(full_topic, topic, sizeof(full_topic));
  MQTTDebugTf(PSTR("Sending MQTT: server %s:%d => TopicId [%s] --> Message [%s]\r\n"), settings.mqtt.sBroker, settings.mqtt.iBrokerPort, full_topic, json);
  const size_t payloadLen = strlen(json);
  if (!beginMqttPublish(full_topic, payloadLen, retain)) return;
  if (!writeMqttChunk(json, payloadLen)) {
    MQTTclient.endPublish();
    return;
  }
  if (!MQTTclient.endPublish()) { PrintMQTTError(); return; }
  // Publish succeeded — confirm any pending throttle slot updates
  confirmMQTTPublishSlot();
  confirmMQTTPublishBitSlot();
  confirmMQTTPublishByteSlot();
  feedWatchDog();//feed the dog
} // sendMQTTData()

void sendMQTTData(const __FlashStringHelper *topic, const char *json, const bool retain)
{
  char topicBuf[MQTT_TOPIC_MAX_LEN];
  strncpy_P(topicBuf, reinterpret_cast<PGM_P>(topic), sizeof(topicBuf) - 1);
  topicBuf[sizeof(topicBuf) - 1] = '\0';
  sendMQTTData(topicBuf, json, retain);
}

void sendMQTTData(const __FlashStringHelper *topic, const __FlashStringHelper *json, const bool retain)
{
  if (!settings.mqtt.bEnable) return;
  if (!mqttPublishAllowed) return;
  if (!MQTTclient.connected()) { return; }  // handleMQTT() logs disconnect and manages reconnect
  if (!isValidIP(MQTTbrokerIP)) {DebugTln(F("Error: MQTT broker IP not valid.")); return;}
  if (!canPublishMQTT()) return;

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
  if (!beginMqttPublish(full_topic, payloadLen, retain)) return;
  if (!writeMqttProgmemChunk(payload, payloadLen)) {
    MQTTclient.endPublish();
    return;
  }
  if (!MQTTclient.endPublish()) { PrintMQTTError(); return; }
  confirmMQTTPublishSlot();
  confirmMQTTPublishBitSlot();
  confirmMQTTPublishByteSlot();
  feedWatchDog();
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
  if (isPICEnabled()) {
    sendMQTTDataPic(F("version"), state.pic.sFwversion);
    sendMQTTDataPic(F("deviceid"), state.pic.sDeviceid);
    sendMQTTDataPic(F("firmwaretype"), state.pic.sType);
    sendMQTTDataPic(F("designer"), F("Schelte Bron"));
  }
  sendMQTTDataPic(F("picavailable"), CCONOFF(state.pic.bAvailable));
}

/*
Publish cumulative heap-pressure and drop diagnostics as individual retained
topics under <topTopic>/value/<uniqueid>/otgw-firmware/stats/*. Each metric
lives on its own topic (not bundled in a JSON blob) so consumers can subscribe
to a single counter, expose it as a Home Assistant entity without JSON path
templating, or graph it in Grafana without parsing. sendMQTTData() prepends
MQTTPubNamespace, so uniqueid already scopes the topics per device - multiple
OTGWs on one broker never collide.

Called from doTaskMinuteChanged() under if (hourFlag) per ADR-064 (wall-clock
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
  publishStatU32(F("otgw-firmware/stats/free_heap"),             (unsigned long)ESP.getFreeHeap());
  publishStatU32(F("otgw-firmware/stats/max_block"),             (unsigned long)ESP.getMaxFreeBlockSize());
  publishStatU32(F("otgw-firmware/stats/frag_pct"),              (unsigned long)getHeapFragmentation());

  // Discovery verification telemetry (ADR-062)
  publishStatU32(F("otgw-firmware/stats/disc_verify_runs"),         (unsigned long)state.discovery.iVerifyRunCount);
  publishStatU32(F("otgw-firmware/stats/disc_republish_triggered"), (unsigned long)state.discovery.iRepublishTriggeredCount);
  publishStatU32(F("otgw-firmware/stats/disc_last_missing"),        (unsigned long)state.discovery.iLastMissingCount);
  publishStatU32(F("otgw-firmware/stats/disc_last_orphan"),         (unsigned long)state.discovery.iLastOrphanCount);
  publishStatU32(F("otgw-firmware/stats/disc_published_topics"),    (unsigned long)state.discovery.iPublishedTopicCount);
  publishStatU32(F("otgw-firmware/stats/disc_last_verify_epoch"),   (unsigned long)state.discovery.iLastVerifyEpoch);
}

/*
Publish state information of PIC firmware version information to MQTT broker.
*/
void sendMQTTstateinformation(){
  if (!isPICEnabled()) return;
  sendMQTTDataPic(F("boiler_connected"), CCONOFF(state.otgw.bBoilerState));
  sendMQTTDataPic(F("thermostat_connected"), CCONOFF(state.otgw.bThermostatState));
  if (state.otgw.bGatewayModeKnown) {
    sendMQTTDataPic(F("gateway_mode"), CCONOFF(state.otgw.bGatewayMode));
  }
  sendMQTTDataPic(F("otgw_connected"), CCONOFF(state.otgw.bOnline));
  sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(state.otgw.bOnline));
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
 * Publish ON/OFF value to MQTT topic
 * Reduces duplicate pattern of boolean-to-string conversion
 */
void publishMQTTOnOff(const char* topic, bool value) {
  sendMQTTData(topic, value ? "ON" : "OFF");
}

void publishMQTTOnOff(const __FlashStringHelper* topic, bool value) {
  sendMQTTData(topic, value ? "ON" : "OFF");
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
  snprintf(buffer, sizeof(buffer), "%d", value);
  sendMQTTData(topic, buffer);
}

void publishMQTTInt(const __FlashStringHelper* topic, int value) {
  static char buffer[12];
  snprintf(buffer, sizeof(buffer), "%d", value);
  sendMQTTData(topic, buffer);
}

// Shared source mapping for source-separated MQTT topics / HA discovery expansion.
// Returns false for parity errors / unknown types (caller should not publish).
static bool resolveSourceIndex(byte rsptype, uint8_t &sourceIndex) {
  switch (rsptype) {
    case OTGW_THERMOSTAT:        sourceIndex = 0; return true;
    case OTGW_BOILER:            sourceIndex = 1; return true;
    case OTGW_ANSWER_THERMOSTAT: sourceIndex = 1; return true;  // OTGW answers as boiler — value is boiler-side
    case OTGW_REQUEST_BOILER:    sourceIndex = 2; return true;  // OTGW requests for itself — gateway source
    default: return false;
  }
}

static bool copySourceTableEntry(const char* const table[], uint8_t sourceIndex, char *dest, size_t destSize)
{
  if (!dest || destSize == 0 || sourceIndex >= 3) return false;
  PGM_P pValue = (PGM_P)pgm_read_ptr(&table[sourceIndex]);
  if (!pValue) { dest[0] = '\0'; return false; }
  strncpy_P(dest, pValue, destSize - 1);
  dest[destSize - 1] = '\0';
  return true;
}

void publishToSourceTopic(const char* topic, const char* json, byte rsptype)
{
  if (!settings.mqtt.bSeparateSources || !topic || !json) return;
  // ADR-066: skip /boiler subtopic for MsgIDs where the slave's Write-Ack
  // data byte is per-spec undefined. Without this gate, /boiler shows
  // protocol-zero readings that are not measurements (e.g. Tr, TrSet,
  // MaxRelModLevelSetting). The bSlaveEchoesValue flag is populated for
  // every MsgID in OTmap[] per docs/api/MQTT-message-id-echo-audit.md.
  // OTlookupitem is set by processOT before each print_* call and is
  // therefore valid here.
  if (rsptype == OT_WRITE_ACK && !OTlookupitem.bSlaveEchoesValue) return;
  // Re-entrancy guard: sendMQTTData may yield via feedWatchDog, allowing
  // a second processOT call to overwrite the static buffer mid-publish.
  static bool inUse = false;
  if (inUse) return;
  inUse = true;
  uint8_t sourceIndex = 0;
  if (!resolveSourceIndex(rsptype, sourceIndex)) { inUse = false; return; }
  static char sourceTopic[MQTT_TOPIC_MAX_LEN];
  char sourceKeyBuf[16];
  if (!copySourceTableEntry(mqttSourceKeys, sourceIndex, sourceKeyBuf, sizeof(sourceKeyBuf))) { inUse = false; return; }
  snprintf(sourceTopic, sizeof(sourceTopic), "%s/%s", topic, sourceKeyBuf);
  sendMQTTData(sourceTopic, json, false);
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
// Pending-bitmap helpers for async drip-discovery (ADR-pending).
// MQTTautoCfgPendingMap[8] mirrors MQTTautoConfigMap layout: 8 × uint32_t = 256 bits.
// Setting a bit means "this OT ID needs its discovery config (re-)published".
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
// Mode hysteresis: once a mode is entered, it holds for at least one full
// timerDiscoveryDrip_interval before a switch is allowed in either direction.
// Normal (2s) -> Slow: requires >=2s in normal; Slow (10s) -> Normal: requires
// >=10s in slow. This prevents rapid oscillation when freeHeap hovers near
// HEAP_LOW_THRESHOLD (TASK-370).
//===========================================================================================
constexpr uint8_t DISCOVERY_INTERVAL_NORMAL = 2;   // seconds (heap recovery between bursts)
constexpr uint8_t DISCOVERY_INTERVAL_SLOW   = 10;  // seconds (heap pressure backoff)

void loopMQTTDiscovery()
{
  DECLARE_TIMER_SEC(timerDiscoveryDrip, DISCOVERY_INTERVAL_NORMAL, SKIP_MISSED_TICKS);
  static uint32_t modeEnteredMs = 0;  // millis() when current mode was entered; 0 = boot
  sDripDueAtMs = timerDiscoveryDrip_due;  // expose due-time for dripDueWithinMs()

  // Adaptive interval: back off BEFORE the publish gate engages.
  // canPublishMQTT() starts dropping at HEAP_LOW (<6KB); if we only trigger
  // slow-mode at HEAP_WARNING (<4KB) we are too late — drops have already
  // started. Trigger at HEAP_LOW so the drip quiets down first.
  bool heapPressure = (getHeapHealth() >= HEAP_LOW);
  // Hold current mode for at least one full interval before switching.
  // modeEnteredMs == 0 on first call: first switch is always allowed immediately.
  bool canSwitch = (modeEnteredMs == 0) ||
                   ((millis() - modeEnteredMs) >= timerDiscoveryDrip_interval);
  if (heapPressure && timerDiscoveryDrip_interval != DISCOVERY_INTERVAL_SLOW * 1000UL && canSwitch) {
    CHANGE_INTERVAL_SEC(timerDiscoveryDrip, DISCOVERY_INTERVAL_SLOW, SKIP_MISSED_TICKS);
    state.heapdiag.iDripSlowModeCount++;
    modeEnteredMs = millis();
    MQTTDebugTf(PSTR("[drip] slowed to %ds (heap pressure)\r\n"), DISCOVERY_INTERVAL_SLOW);
  } else if (!heapPressure && timerDiscoveryDrip_interval != DISCOVERY_INTERVAL_NORMAL * 1000UL && canSwitch) {
    CHANGE_INTERVAL_SEC(timerDiscoveryDrip, DISCOVERY_INTERVAL_NORMAL, SKIP_MISSED_TICKS);
    modeEnteredMs = millis();
    MQTTDebugTf(PSTR("[drip] restored to %ds (heap healthy)\r\n"), DISCOVERY_INTERVAL_NORMAL);
  }

  if (!DUE(timerDiscoveryDrip)) return;

  if (!settings.mqtt.bEnable) return;
  if (!state.mqtt.bConnected) return;
  if (ESP.getFreeHeap() < MQTT_DISCOVERY_HEAP_MIN) return;
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

void doAutoConfigure(){
  // Force-publishes HA discovery configs for ALL entries.
  // Clears the "done" bitmap first so everything is re-sent.
  // Lock scope is limited to the publish loop; configSensors() acquires its own lock.

  if (!settings.mqtt.bEnable) return;

  {
    MQTTAutoConfigSessionLock sessionLock;
    if (!sessionLock.locked) {
      MQTTDebugTln(F("MQTT autoconfig already running, skipping"));
      return;
    }

    clearMQTTConfigDone();

    HaDiscoveryContext ctx = buildDiscoveryContext(true);

    // Stream sensor discoveries
    for (uint16_t i = 0; i < MQTT_HA_SENSOR_COUNT; i++) {
      feedWatchDog();
      MqttHaSensorCfg cfg = readSensorCfg(i);

      if (!isPICEnabled() && (cfg.flags & MQTT_HA_FLAG_IS_PIC_ENTRY)) continue;
      if (cfg.id == OTGWdallasdataid) continue;  // Dallas handled separately

      if (cfg.flags & MQTT_HA_FLAG_ANY_SOURCE) {
        if (settings.mqtt.bSeparateSources) {
          expandAndStreamSensorSources(MQTTclient, cfg, ctx);
        }
        // Skip source-template entries when separate sources disabled
      } else {
        streamSensorDiscovery(MQTTclient, cfg, ctx);
      }
      setMQTTConfigDone(cfg.id);
      ctx.isFirstEntity = false;
    }

    // Stream binary sensor discoveries
    for (uint16_t i = 0; i < MQTT_HA_BINSENSOR_COUNT; i++) {
      feedWatchDog();
      MqttHaBinSensorCfg cfg = readBinSensorCfg(i);

      if (!isPICEnabled() && (cfg.flags & MQTT_HA_FLAG_IS_PIC_ENTRY)) continue;

      streamBinarySensorDiscovery(MQTTclient, cfg, ctx);
      setMQTTConfigDone(cfg.id);
      ctx.isFirstEntity = false;
    }

    // Climate and number entities (hardcoded, not in PROGMEM arrays)
    feedWatchDog();
    streamClimateDiscovery(MQTTclient, 0, ctx);  // Thermostat
    ctx.isFirstEntity = false;
    feedWatchDog();
    streamClimateDiscovery(MQTTclient, 1, ctx);  // DHW Control
    feedWatchDog();
    streamNumberDiscovery(MQTTclient, ctx);       // Toutside Override
    // Mark climate/number IDs done
    setMQTTConfigDone(0);   // climate entries are OT ID 0
    setMQTTConfigDone(27);  // number entry is OT ID 27
  } // Lock released here

  // Trigger Dallas configuration separately as it requires specific sensor addresses.
  // Always run after force (clearMQTTConfigDone above cleared the done flag).
  if (settings.mqtt.bEnable) {
    configSensors();
  }
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
  if (ESP.getFreeHeap() < MQTT_DISCOVERY_HEAP_MIN) return false;

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
        if (streamSensorDiscovery(MQTTclient, cfg, ctx)) result = true;
      }
      sIdx++;
      feedWatchDog();
    }
  }

  // Binary sensors
  uint16_t bIdx = readBinSensorIndex(OTid);
  if (bIdx != MQTT_HA_INDEX_NONE) {
    while (bIdx < MQTT_HA_BINSENSOR_COUNT) {
      MqttHaBinSensorCfg cfg = readBinSensorCfg(bIdx);
      if (cfg.id != OTid) break;
      if (streamBinarySensorDiscovery(MQTTclient, cfg, ctx)) result = true;
      bIdx++;
      feedWatchDog();
    }
  }

  // Climate (OT ID 0)
  if (OTid == 0) {
    if (streamClimateDiscovery(MQTTclient, 0, ctx)) result = true;
    if (streamClimateDiscovery(MQTTclient, 1, ctx)) result = true;
  }
  // Number (OT ID 27)
  if (OTid == 27) {
    if (streamNumberDiscovery(MQTTclient, ctx)) result = true;
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
