/* 
***************************************************************************  
**  Program  : MQTTstuff
**  Version  : v2.0.0-beta
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

// MQTT Streaming Mode - ALWAYS ENABLED
// Large auto-discovery messages are sent in 128-byte chunks instead of
// requiring a large buffer resize. This prevents heap fragmentation on ESP8266.
// Similar to ESPHome's chunked MQTT publishing strategy.

// MQTT_MAX_FREE_BLOCK() is ESP8266-only; ESP32 does not expose an
// equivalent. Use 0 as a sentinel on ESP32 so the heap-diagnostics format
// strings compile unchanged and users understand "not available on this
// platform" when the value shows as 0 in the trace.
#ifdef ARDUINO_ARCH_ESP8266
  #define MQTT_MAX_FREE_BLOCK() platformMaxFreeBlock()
#else
  #define MQTT_MAX_FREE_BLOCK() ((uint32_t)0)
#endif

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
// Streaming HA discovery (ADR-077) only needs ~200 bytes per chunk, so the
// historical 1200-byte floor is obsolete. Value aligned with HEAP_WARNING_THRESHOLD
// (3072) in canPublishMQTT(): if heap is already at WARNING the drip skips
// rather than competing with publish-gate throttling.
constexpr uint32_t MQTT_DISCOVERY_HEAP_MIN = 3000;  // Streaming needs ~200 bytes; aligned with WARNING tier

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
// CAUTION: at 10 seconds, the cooldown can overlap consecutive Status-frames
// (Crashevans log shows ~3s cadence). That means under heavy Status traffic
// the drip can stall. Tunable via STATUS_BURST_COOLDOWN_MS. If you see
// iDripCooldownSkipCount grow without discovery progressing, lower the value
// (candidates: 2500ms = fits between bursts, 5000ms = partial overlap).
//
// Timeout safety: if endStatusBurst() is never reached (exception path),
// isStatusBurstActive() auto-clears after STATUS_BURST_TIMEOUT_MS.
static bool            statusBurstActive     = false;
static unsigned long   statusBurstStartMs    = 0;
static uint16_t        statusBurstPublishCount = 0;
static unsigned long   burstCooldownUntilMs  = 0;
static uint32_t        sDripDueAtMs          = 0;   // updated by loopMQTTDiscovery(); read by dripDueWithinMs()
constexpr unsigned long STATUS_BURST_TIMEOUT_MS  = 500;
constexpr unsigned long STATUS_BURST_COOLDOWN_MS = 10000;

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
// Subscribe briefly to <haprefix>/+/<nodeId>/# and count retained configs
// delivered by the broker. If received < expected, trigger a full re-announce.
//
// RAM-tuned: PubSubClient RX buffer raised 384->1024 only during the 15s
// window, then restored. Peak transient delta: +640 bytes.
// Placed below MQTTclient+NodeId static declarations (both referenced here).
// =====================================================================
static bool            verifyActive          = false;
static unsigned long   verifyStartMs         = 0;
static uint16_t        verifyReceivedCount   = 0;
static uint16_t        verifyOrphanCount     = 0;
static bool            verifyBufferResized   = false;
// sHaprefix[41] + "/+/" + sUniqueid[41] + "/#" + NUL = 88 bytes worst case.
// Sized to 128 gives comfortable headroom for any future field-size bump.
static char            verifyWildcard[128]   = "";
// Cached once in startDiscoveryVerification() so the MQTT callback filter does
// not recompute strlen() on every incoming retained-config message during the
// 15s verify window (HIGH/Perf review finding).
static size_t          verifyPrefixLen       = 0;
static size_t          verifyNodeLen         = 0;

constexpr unsigned long VERIFICATION_WINDOW_MS      = 15000;
constexpr uint16_t      VERIFICATION_BUFFER_BYTES   = 1024;
constexpr uint32_t      VERIFICATION_MIN_HEAP_START = 6000;
constexpr uint32_t      VERIFICATION_MIN_HEAP_ABORT = 4500;
// Max single-segment length the node-id parse accepts. Caps DoS-window CPU
// from malicious broker flooding with long-segment topics (MEDIUM/Sec finding).
constexpr size_t        VERIFICATION_MAX_NODE_SEGMENT_LEN = 64;

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

// Begin a verify pass. Returns false if any precondition fails.
bool startDiscoveryVerification() {
  if (verifyActive) return false;
  if (!state.mqtt.bConnected) return false;
  if (isFlashing()) return false;
  if (countPendingDiscoveryIds() > 0) return false;                 // drip-race guard
  if (ESP.getFreeHeap() < VERIFICATION_MIN_HEAP_START) return false;
  // Max-block precheck: umm_malloc realloc of PubSubClient's buffer needs a
  // contiguous 1024-byte block. Avoid the realloc entirely when the heap is
  // fragmented (Perf review: setBufferSize grow/shrink fragments over long uptime).
  if (platformMaxFreeBlock() < (VERIFICATION_BUFFER_BYTES + 256U)) return false;

  const int wrote = snprintf_P(verifyWildcard, sizeof(verifyWildcard),
                               PSTR("%s/+/%s/#"), CSTR(settings.mqtt.sHaprefix), NodeId);
  if (wrote <= 0 || (size_t)wrote >= sizeof(verifyWildcard)) {
    // Arch review (HIGH): refuse to start on wildcard truncation, which would
    // subscribe to a malformed topic and yield zero matches → false-positive
    // full republish every run.
    DebugTf(PSTR("[verify] wildcard truncated (%d/%u), refusing start\r\n"),
            wrote, (unsigned)sizeof(verifyWildcard));
    return false;
  }

  // Cache segment lengths for callback fast-path.
  verifyPrefixLen = strlen(CSTR(settings.mqtt.sHaprefix));
  verifyNodeLen   = strlen(NodeId);

  // Raise RX buffer BEFORE subscribe so oversize configs fit.
  if (!MQTTclient.setBufferSize(VERIFICATION_BUFFER_BYTES)) {
    DebugTln(F("[verify] setBufferSize failed"));
    return false;
  }
  verifyBufferResized = true;

  if (!MQTTclient.subscribe(verifyWildcard, 0)) {
    DebugTln(F("[verify] subscribe failed"));
    MQTTclient.setBufferSize(MQTT_CLIENT_BUFFER_SIZE);
    verifyBufferResized = false;
    return false;
  }

  verifyReceivedCount = 0;
  verifyOrphanCount   = 0;
  verifyStartMs       = millis();
  verifyActive        = true;
  state.discovery.bVerificationActive = true;
  state.discovery.iVerifyRunCount++;
  DebugTf(PSTR("[verify] started: wildcard=%s expected=%lu\r\n"),
          verifyWildcard, (unsigned long)state.discovery.iPublishedTopicCount);
  return true;
}

// End the verify window, reconcile, trigger republish if missing.
void endDiscoveryVerification() {
  if (!verifyActive) return;
  verifyActive = false;
  state.discovery.bVerificationActive = false;
  state.discovery.iLastVerifyEpoch    = (uint32_t)time(nullptr);

  const uint16_t expected = (uint16_t)state.discovery.iPublishedTopicCount;
  const uint16_t missing  = (verifyReceivedCount >= expected) ? 0
                                                              : (uint16_t)(expected - verifyReceivedCount);
  state.discovery.iLastMissingCount = missing;
  state.discovery.iLastOrphanCount  = verifyOrphanCount;

  if (state.mqtt.bConnected) {
    MQTTclient.unsubscribe(verifyWildcard);
  }
  if (verifyBufferResized) {
    MQTTclient.setBufferSize(MQTT_CLIENT_BUFFER_SIZE);
    verifyBufferResized = false;
  }

  DebugTf(PSTR("[verify] done: expected=%u received=%u orphans=%u missing=%u\r\n"),
          expected, verifyReceivedCount, verifyOrphanCount, missing);

  if (missing > 0) {
    DebugTln(F("[verify] missing configs detected, triggering markAllMQTTConfigPending"));
    state.discovery.iRepublishTriggeredCount++;
    markAllMQTTConfigPending();
  }
}

// Polled from handleMQTT() to close the window on timeout / disconnect / heap-abort.
void tickDiscoveryVerification() {
  if (!verifyActive) return;
  if (!state.mqtt.bConnected) {
    // Fast-path close. Restore the buffer defensively even on a dead client:
    // setBufferSize is a local realloc, safe when not connected. Prevents the
    // 1024B allocation from leaking across a reconnect path that does not
    // re-size (Arch/Sec review finding).
    if (verifyBufferResized) {
      MQTTclient.setBufferSize(MQTT_CLIENT_BUFFER_SIZE);
      verifyBufferResized = false;
    }
    verifyActive = false;
    state.discovery.bVerificationActive = false;
    return;
  }
  const unsigned long now = millis();
  const uint16_t expected = (uint16_t)state.discovery.iPublishedTopicCount;

  // Heap-abort gate: avoid fighting for RAM mid-window.
  if (ESP.getFreeHeap() < VERIFICATION_MIN_HEAP_ABORT) {
    verifyReceivedCount = expected;   // suppress false-missing republish
    DebugTln(F("[verify] heap-abort: closing window early"));
    endDiscoveryVerification();
    return;
  }
  // Early-close when everything arrived (with tiny settling delay).
  if (verifyReceivedCount >= expected && (unsigned long)(now - verifyStartMs) > 500UL) {
    endDiscoveryVerification();
    return;
  }
  // Timeout.
  if ((unsigned long)(now - verifyStartMs) >= VERIFICATION_WINDOW_MS) {
    endDiscoveryVerification();
  }
}

bool isDiscoveryVerificationActive() { return verifyActive; }

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
  //setup for mqtt discovery — mark all IDs pending for async drip publish
  strlcpy(NodeId, CSTR(settings.mqtt.sUniqueid), sizeof(NodeId));
  buildNamespace(MQTTPubNamespace, sizeof(MQTTPubNamespace), CSTR(settings.mqtt.sTopTopic), "value", NodeId);
  buildNamespace(MQTTSubNamespace, sizeof(MQTTSubNamespace), CSTR(settings.mqtt.sTopTopic), "set", NodeId);
  markAllMQTTConfigPending();  // queue all discovery for async drip publish
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
// same convention as kV2Routes[] in restAPI.ino.
static const SatMqttCmdEntry kSatMqttCmds[] = {
  // --- Typed handlers (payload parsing lives in the handler) ---
  { "target",                 nullptr,                _satTargetTempCmd },
  { "indoor_temp",            nullptr,                _satExtTempCmd },
  { "outdoor_temp",           nullptr,                _satExtOutdoorCmd },
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

  { nullptr, nullptr, nullptr } // sentinel
};

// Returns true when the sub-command was found in the table and dispatched.
static bool dispatchSatMqttCmd(const char* cmd, const char* payload) {
  for (const SatMqttCmdEntry* e = kSatMqttCmds; e->cmd != nullptr; e++) {
    if (strcasecmp(cmd, e->cmd) == 0) {
      if (e->handler) {
        e->handler(payload);
      } else if (e->settingKey) {
        updateSetting(e->settingKey, payload);
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

  // Verify-window retained-config filter (ADR-062, TASK-349).
  // Route retained discovery configs from our subscribe-wildcard to the verify
  // counters without falling through to the command-topic dispatcher below.
  // Uses cached verifyPrefixLen/verifyNodeLen populated in startDiscoveryVerification
  // to avoid strlen() per message (Perf/Sec review).
  if (verifyActive && verifyPrefixLen > 0) {
    const char *prefix = CSTR(settings.mqtt.sHaprefix);
    if (strncmp(topic, prefix, verifyPrefixLen) == 0 && topic[verifyPrefixLen] == '/') {
      // Topic format: <haprefix>/<component>/<nodeId>/.../config
      const char *rest   = topic + verifyPrefixLen + 1;
      const char *slash1 = strchr(rest, '/');
      if (slash1) {
        const char *nodeStart = slash1 + 1;
        const char *slash2    = strchr(nodeStart, '/');
        if (slash2) {
          const size_t nodeLen = (size_t)(slash2 - nodeStart);
          // DoS-cap: refuse overly long node segments from malicious/misconfigured
          // brokers (Sec review). Anything beyond our own nodeid length window is
          // noise we don't need to strncmp.
          if (nodeLen > VERIFICATION_MAX_NODE_SEGMENT_LEN) {
            verifyOrphanCount++;
            return;
          }
          if (nodeLen == verifyNodeLen && strncmp(nodeStart, NodeId, nodeLen) == 0) {
            verifyReceivedCount++;
          } else {
            verifyOrphanCount++;
          }
          return;  // handled by verify — do not forward to command dispatcher
        }
      }
    }
  }

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
            addCommandToQueue(otgwcmd, strlen(otgwcmd), true);
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
      DebugTf(PSTR("[HEAP] pre-connect: free=%u max_block=%u\r\n"), ESP.getFreeHeap(), MQTT_MAX_FREE_BLOCK());

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
      DebugTf(PSTR("[HEAP] post-connect: free=%u max_block=%u\r\n"), ESP.getFreeHeap(), MQTT_MAX_FREE_BLOCK());

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
        DebugTf(PSTR("[HEAP] post-birth: free=%u max_block=%u\r\n"), ESP.getFreeHeap(), MQTT_MAX_FREE_BLOCK());

        // Force re-publish of all OT values so HA gets current state after reconnect.
        requestMQTTRepublishAll();
        DebugTf(PSTR("[HEAP] post-republish: free=%u max_block=%u\r\n"), ESP.getFreeHeap(), MQTT_MAX_FREE_BLOCK());

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
        DebugTf(PSTR("[HEAP] post-subscribe: free=%u max_block=%u\r\n"), ESP.getFreeHeap(), MQTT_MAX_FREE_BLOCK());
        sendMQTTversioninfo();
        publishAllPICsettings();
        DebugTf(PSTR("[HEAP] post-versioninfo: free=%u max_block=%u\r\n"), ESP.getFreeHeap(), MQTT_MAX_FREE_BLOCK());
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
  sendMQTTData("otgw-firmware/version", _SEMVER_FULL);
  sendMQTTData("otgw-firmware/reboot_count", rebootCountBuf);
  sendMQTTData("otgw-firmware/reboot_reason", lastReset);
  if (isPICEnabled()) {
    sendMQTTData("otgw-pic/version", state.pic.sFwversion);
    sendMQTTData("otgw-pic/deviceid", state.pic.sDeviceid);
    sendMQTTData("otgw-pic/firmwaretype", state.pic.sType);
    sendMQTTData(F("otgw-pic/designer"), F("Schelte Bron"));
  }
  sendMQTTData("otgw-pic/picavailable", CCONOFF(state.pic.bAvailable));
}

static void publishBoilerConnectedState()
{
  sendMQTTData(F("boiler_connected"), CCONOFF(state.otBus.bBoilerState));
  if (isPICEnabled()) {
    sendMQTTData(F("otgw-pic/boiler_connected"), CCONOFF(state.otBus.bBoilerState));
  }
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  if (isOTDirectEnabled()) {
    sendMQTTData(F("otgw-otdirect/boiler_connected"), CCONOFF(state.otBus.bBoilerState));
  }
#endif
}

static void publishThermostatConnectedState()
{
  sendMQTTData(F("thermostat_connected"), CCONOFF(state.otBus.bThermostatState));
  if (isPICEnabled()) {
    sendMQTTData(F("otgw-pic/thermostat_connected"), CCONOFF(state.otBus.bThermostatState));
  }
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  if (isOTDirectEnabled()) {
    sendMQTTData(F("otgw-otdirect/thermostat_connected"), CCONOFF(state.otBus.bThermostatState));
  }
#endif
}

static void publishOTGWConnectedState()
{
  sendMQTTData(F("otgw_connected"), CCONOFF(state.otBus.bOnline));
  if (isPICEnabled()) {
    sendMQTTData(F("otgw-pic/otgw_connected"), CCONOFF(state.otBus.bOnline));
  }
#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
  if (isOTDirectEnabled()) {
    sendMQTTData(F("otgw-otdirect/ot_online"), CCONOFF(state.otBus.bOnline));
  }
#endif
  sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(state.otBus.bOnline));
}

void sendMQTTheapdiag(){
  if (!settings.mqtt.bEnable) return;
  if (!state.mqtt.bConnected) return;
  state.heapdiag.iLastPublishedEpoch = (uint32_t)time(nullptr);
  char json[384];   // TASK-349: raised 256->384 to fit disc_* fields
  snprintf_P(json, sizeof(json),
    PSTR("{\"ws_drops\":%lu,\"mqtt_drops\":%lu,\"enter_low\":%lu,\"enter_warning\":%lu,"
         "\"enter_critical\":%lu,\"drip_burst_skip\":%lu,\"drip_cooldown_skip\":%lu,"
         "\"drip_slowmode\":%lu,\"free_heap\":%lu,\"max_block\":%lu,\"frag_pct\":%u,"
         "\"disc_verify_runs\":%lu,\"disc_republish_triggered\":%lu,"
         "\"disc_last_missing\":%u,\"disc_last_orphan\":%u,"
         "\"disc_published_topics\":%lu,\"disc_last_verify_epoch\":%lu}"),
    (unsigned long)state.heapdiag.iWsDropsTotal,
    (unsigned long)state.heapdiag.iMqttDropsTotal,
    (unsigned long)state.heapdiag.iEnteredLowCount,
    (unsigned long)state.heapdiag.iEnteredWarningCount,
    (unsigned long)state.heapdiag.iEnteredCriticalCount,
    (unsigned long)state.heapdiag.iDripActiveBurstSkipCount,
    (unsigned long)state.heapdiag.iDripCooldownSkipCount,
    (unsigned long)state.heapdiag.iDripSlowModeCount,
    (unsigned long)platformFreeHeap(),
    (unsigned long)platformMaxFreeBlock(),
    getHeapFragmentation());
  sendMQTTData(F("otgw-firmware/stats/heap"), json, true);   // retained
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
      bool success = doAutoConfigureMsgid(msgId);
      if (success) {
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
    // SAT switches + select (TASK-284, piggyback on climate pseudo-ID 0)
    for (uint8_t swIdx = 0; swIdx < 13; swIdx++) {
      feedWatchDog();
      streamSatSwitchDiscovery(MQTTclient, swIdx, ctx);
    }
    feedWatchDog();
    streamSatSelectDiscovery(MQTTclient, 0, ctx);
    // Mark climate/number IDs done
    setMQTTConfigDone(0);   // climate + SAT switch/select entries are OT ID 0
    setMQTTConfigDone(27);  // number entry is OT ID 27
  } // Lock released here

  // Trigger Dallas configuration separately as it requires specific sensor addresses.
  // Always run after force (clearMQTTConfigDone above cleared the done flag).
  if (settings.mqtt.bEnable) {
    configSensors();
  }
}
//===========================================================================================
bool doAutoConfigureMsgid(byte OTid)
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
  HaDiscoveryContext ctx = buildDiscoveryContext();

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

  // Climate + SAT switches/select (OT ID 0 — TASK-284 piggyback)
  if (OTid == 0) {
    if (streamClimateDiscovery(MQTTclient, 0, ctx)) result = true;
    if (streamClimateDiscovery(MQTTclient, 1, ctx)) result = true;
    for (uint8_t swIdx = 0; swIdx < 13; swIdx++) {
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
