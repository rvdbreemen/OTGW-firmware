/* 
***************************************************************************  
**  Program  : OTGW-Core.ino
**  Version  : v2.0.0-alpha.328
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  Borrowed from OpenTherm library from: 
**      https://github.com/jpraus/arduino-opentherm
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

/*
***************************************************************************
**  OTGW-Core.ino — Section Map
**
**  Event / Log Helpers              ~ line  108
**  Global Data Arrays & Variables   ~ line  216
**  OT Spec Profile Helpers          ~ line  241
**  Send useful information to MQTT   ~ line  296
**  Reset OTGW                       ~ line  326
**  getpicfwversion                  ~ line  348
**  queryOTGWgatewaymode             ~ line  365
**  queryNextPICsetting              ~ line  580
**  publishAllPICsettings            ~ line  680
**  sendPICBootCommands                  ~ line  444
**  OTGW Command & Response          ~ line  463
**  Watchdog OTGW                    ~ line  531
**  OpenTherm Data Types             ~ line  598
**  Status Bit Query Helpers         ~ line  681
**  MQTT throttle helpers            ~ line  820
**  OT Message Field Formatters      ~ line 1037
**  Command Queue implementation      ~ line 1902
**  Send buffer to OTGW              ~ line 2095
**  PS=1 Summary Parsing             ~ line 2281
**  OT Message Processing            ~ line 2725
**  HandleOTGW                       ~ line 3256
**  functions for REST API           ~ line 3403
**  Upgrade PIC firmware             ~ line 3537
***************************************************************************
*/

#define OTDebugTln(...) ({ if (state.debug.bOTmsg) DebugTln(__VA_ARGS__);    })
#define OTDebugln(...)  ({ if (state.debug.bOTmsg) Debugln(__VA_ARGS__);    })
#define OTDebugTf(...)  ({ if (state.debug.bOTmsg) DebugTf(__VA_ARGS__);    })
#define OTDebugf(...)   ({ if (state.debug.bOTmsg) Debugf(__VA_ARGS__);    })
#define OTDebugT(...)   ({ if (state.debug.bOTmsg) DebugT(__VA_ARGS__);    })
#define OTDebug(...)    ({ if (state.debug.bOTmsg) Debug(__VA_ARGS__);    })
#define OTDebugFlush()  ({ if (state.debug.bOTmsg) DebugFlush();    })

// Pin aliases — sourced from boards.h (included via OTGW-firmware.h)
#define OTGW_BUTTON PIN_BUTTON
#define OTGW_LED1   PIN_LED1
#define OTGW_LED2   PIN_LED2
#if HAS_PIC
#define OTGW_RESET  PIN_PIC_RST
#endif

#if HAS_PIC_WATCHDOG
// External secondary I2C watchdog at 0x26 on the OTGW Classic PCB (esp32-classic
// + combo-in-PIC-mode). The ESP32 TWDT is the primary watchdog (ADR-135).
#define EXT_WD_I2C_ADDRESS 0x26
#endif

#if HAS_PIC
//used by update firmware functions (PIC self-programming over HTTP)
const char *hexheaders[] = {
  "Last-Modified",
  "X-Version"
};
#endif

/* --- PRINTF_BYTE_TO_BINARY macro's --- */
#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i)    \
    (((i) & 0x80ll) ? '1' : '0'), \
    (((i) & 0x40ll) ? '1' : '0'), \
    (((i) & 0x20ll) ? '1' : '0'), \
    (((i) & 0x10ll) ? '1' : '0'), \
    (((i) & 0x08ll) ? '1' : '0'), \
    (((i) & 0x04ll) ? '1' : '0'), \
    (((i) & 0x02ll) ? '1' : '0'), \
    (((i) & 0x01ll) ? '1' : '0')

#define PRINTF_BINARY_PATTERN_INT16     PRINTF_BINARY_PATTERN_INT8              PRINTF_BINARY_PATTERN_INT8
#define PRINTF_BYTE_TO_BINARY_INT16(i)  PRINTF_BYTE_TO_BINARY_INT8((i) >> 8),   PRINTF_BYTE_TO_BINARY_INT8(i)
#define PRINTF_BINARY_PATTERN_INT32     PRINTF_BINARY_PATTERN_INT16             PRINTF_BINARY_PATTERN_INT16
#define PRINTF_BYTE_TO_BINARY_INT32(i)  PRINTF_BYTE_TO_BINARY_INT16((i) >> 16), PRINTF_BYTE_TO_BINARY_INT16(i)
#define PRINTF_BINARY_PATTERN_INT64     PRINTF_BINARY_PATTERN_INT32             PRINTF_BINARY_PATTERN_INT32
#define PRINTF_BYTE_TO_BINARY_INT64(i)  PRINTF_BYTE_TO_BINARY_INT32((i) >> 32), PRINTF_BYTE_TO_BINARY_INT32(i)


/* --- Endf of macro's --- */

/* --- LOG marcro's ---*/

char ot_log_buffer[OT_LOG_BUFFER_SIZE];
size_t ot_log_pos = 0;  // tracked write position — eliminates O(n²) strlen per AddLog call

static uint32_t gOTGWStartupQuietStartMs = 0;
static bool     gOTGWStartupQuietActive  = false;
static const uint32_t OTGW_STARTUP_QUIET_PERIOD_MS = 15000;

/* --- End of LOG marcro's ---*/

//===================[ Event / Log Helpers ]===================
/* Send a single-line event to the WebSocket with a prefix character.
   Prefix conventions: '>' sent command, '<' command response, '!' error/warning, '*' system event.
   Uses ot_log_buffer; no additional buffers needed.
   sendEventToWebSocket   - msg is a DRAM string; len < 0 means null-terminated, else bounded by len.
   sendEventToWebSocket_P - msg_P is a PROGMEM string (pass with PSTR). */
static void sendEventToWebSocket(char prefix, const char *msg, int len = -1) {
  ClrLog();
  AddLog(getOTLogTimestamp());
  AddLogf_P(PSTR(" %c "), prefix);
  if (len < 0) AddLog(msg);
  else AddLogf_P(PSTR("%.*s"), len, msg);
  AddLogln();
  sendLogToWebSocket(ot_log_buffer);
  ClrLog();
}

static void sendEventToWebSocket_P(char prefix, PGM_P msg_P) {
  ClrLog();
  AddLog(getOTLogTimestamp());
  AddLogf_P(PSTR(" %c "), prefix);
  if (ot_log_pos < (OT_LOG_BUFFER_SIZE - 1)) {
    size_t _rem  = OT_LOG_BUFFER_SIZE - ot_log_pos;
    size_t _src  = strlen_P(msg_P);
    // strlcpy_P is absent from ESP8266 2.7.4; replicate with memcpy_P.
    size_t _copy = (_src < _rem - 1) ? _src : (_rem - 1);
    memcpy_P(ot_log_buffer + ot_log_pos, msg_P, _copy);
    ot_log_buffer[ot_log_pos + _copy] = '\0';
    ot_log_pos += _copy;
  }
  AddLogln();
  sendLogToWebSocket(ot_log_buffer);
  ClrLog();
}

static void scheduleOTGWStartupQuietPeriod()
{
  gOTGWStartupQuietStartMs = millis();
  gOTGWStartupQuietActive  = true;
}

static bool isOTGWStartupQuietPeriodActive()
{
  if (!gOTGWStartupQuietActive) return false;
  if ((millis() - gOTGWStartupQuietStartMs) >= OTGW_STARTUP_QUIET_PERIOD_MS) {
    gOTGWStartupQuietActive = false;
    return false;
  }
  return true;
}

static bool canFanOutOTGWEvent()
{
  return (settings.mqtt.bEnable && MQTTclient.connected() && isValidIP(MQTTbrokerIP)) || hasWebSocketClients();
}

static void reportOTGWEvent(const char *eventMsg, char prefix, bool suppressDuringStartup = false)
{
  if (eventMsg == nullptr) return;
  if (suppressDuringStartup && isOTGWStartupQuietPeriodActive()) return;
  if (!canFanOutOTGWEvent()) return;

  if (settings.mqtt.bEnable && MQTTclient.connected() && isValidIP(MQTTbrokerIP)) {
    sendMQTTData(F("event_report"), eventMsg);
  }
  if (hasWebSocketClients()) {
    sendEventToWebSocket(prefix, eventMsg);
  }
}

static void reportOTGWEvent_P(PGM_P eventMsg_P, char prefix, bool suppressDuringStartup = false)
{
  if (eventMsg_P == nullptr) return;
  if (suppressDuringStartup && isOTGWStartupQuietPeriodActive()) return;
  if (!canFanOutOTGWEvent()) return;

  if (settings.mqtt.bEnable && MQTTclient.connected() && isValidIP(MQTTbrokerIP)) {
    sendMQTTData(F("event_report"), reinterpret_cast<const __FlashStringHelper*>(eventMsg_P));
  }
  if (hasWebSocketClients()) {
    sendEventToWebSocket_P(prefix, eventMsg_P);
  }
}

static const char* skipOTLogTimestamp(const char* logLine)
{
  if (!logLine) return logLine;
  if (strlen(logLine) < 16) return logLine;

  const bool hasTimestamp =
      (logLine[0] >= '0' && logLine[0] <= '9') &&
      (logLine[1] >= '0' && logLine[1] <= '9') &&
      (logLine[2] == ':') &&
      (logLine[3] >= '0' && logLine[3] <= '9') &&
      (logLine[4] >= '0' && logLine[4] <= '9') &&
      (logLine[5] == ':') &&
      (logLine[6] >= '0' && logLine[6] <= '9') &&
      (logLine[7] >= '0' && logLine[7] <= '9') &&
      (logLine[8] == '.') &&
      (logLine[9] >= '0' && logLine[9] <= '9') &&
      (logLine[10] >= '0' && logLine[10] <= '9') &&
      (logLine[11] >= '0' && logLine[11] <= '9') &&
      (logLine[12] >= '0' && logLine[12] <= '9') &&
      (logLine[13] >= '0' && logLine[13] <= '9') &&
      (logLine[14] >= '0' && logLine[14] <= '9') &&
      (logLine[15] == ' ');

  return hasTimestamp ? (logLine + 16) : logLine;
}


//===================[ Global Data Arrays & Variables ]================
//some variable's
OpenthermData_t OTdata, delayedOTdata, tmpOTdata;

// ADR-118: gateway-override visibility store (see OTGW-Core.h). ~132 bytes static.
OTOverrideEntry_t otOverrideStore[OVERRIDE_STORE_MAX] = {};

static constexpr uint8_t MQTT_TRACKED_RESPONSE_ID_COUNT = 128; // linear msgid slots for IDs 0-127
static constexpr uint16_t MQTT_TRACKED_SLOT_COUNT = MQTT_TRACKED_RESPONSE_ID_COUNT * 2; // response + request view

// TASK-400: Status-bit specific heartbeat interval. Hardcoded to 60 seconds
// so the msgId 0 status_master / status_slave fan-out (and the msgId 70
// Status VH equivalent) publishes at least once a minute as a state-snapshot
// for HA reconnect recovery, INDEPENDENT of settings.mqtt.iInterval (which
// governs all OTHER topic throttles). The 60s cadence is a compromise: long
// enough to eliminate per-frame publish spam under steady-state boiler
// conditions (drops 160 publishes/sec → ~0.27/sec), short enough that HA
// regains full state within one minute after any MQTT broker reconnect.
static constexpr uint16_t STATUS_HEARTBEAT_INTERVAL_SEC = 60;

// Global state arrays — defined here (one definition rule), declared extern in OTGW-Core.h. (ADR-044)
uint32_t mqttlastsent[MQTT_TRACKED_SLOT_COUNT] = {0}; // packed throttle for msgids 0-127: bits31-16=last published u16, bits15-0=seconds-since-boot
uint16_t mqttlastsentstatusbit[16] = {0}; // per-bit publish timers for OT_Statusflags (slots 0-7=master, 8-15=slave)
bool     mqttPublishAllowed        = true; // MQTT interval gate — managed via OTPublishGate (OTGW-Core.h)
static uint16_t mqttlastsentstatusbyte[2] = {0}; // combined status_master/status_slave publish timers
static uint16_t mqttlastsentstatusvhbit[16] = {0}; // per-bit publish timers for OT_StatusVH (slots 0-7=master, 8-15=slave)
static uint16_t mqttlastsentstatusvhbyte[2] = {0}; // combined status_vh_master/status_vh_slave publish timers
static bool mqttForceNextMasterStatusPublish = true;
static bool mqttForceNextSlaveStatusPublish  = true;
static bool mqttForceNextMasterStatusVHPublish = true;
static bool mqttForceNextSlaveStatusVHPublish  = true;

// TASK-401: per-bit / per-byte publish timers for non-Status fan-out sites
// that fire frequently enough to matter for heap pressure. Same first-seen +
// change + STATUS_HEARTBEAT_INTERVAL_SEC heartbeat gating as msgId 0 Status.
// Reset to TRACKED_TIME_UNSEEN in resetMqttTrackedState() so the first frame
// after boot publishes all bits/bytes once.
static uint16_t mqttlastsentASFbit[8]   = {0};  // msgId 5 ASF bits 0-5 (slots 6-7 reserved)
static uint16_t mqttlastsentASFbyte[1]  = {0};  // msgId 5 ASF_flags byte-topic
static uint16_t mqttlastsentRBPbit[4]   = {0};  // msgId 6 RBP: 0=dhw_sp, 1=max_ch_sp, 2=rw_dhw_sp, 3=rw_max_ch_sp
static uint16_t mqttlastsentRBPbyte[2]  = {0};  // msgId 6 RBP: 0=transfer_enable, 1=read_write
static uint16_t mqttlastsentRObit[2]    = {0};  // msgId 100 Remote Override: 0=manual, 1=program
static uint16_t mqttlastsentRObyte[1]   = {0};  // msgId 100 Remote Override LB flag8

// ADR-104 (2.0.0 sibling of dev's ADR-076): a global rate-gate (TASK-402's
// MQTT_GATED_PUBLISH_SPACING_MS) used to live here and was meant to spread the
// 60s heartbeat storm across slots. Instead it starved per-bit slots inside
// the same fan-out (the combined byte always grabbed the token first, the
// bits lost it for the next heartbeat too). Removed under ADR-104 — heap-tier
// back-pressure via canPublishMQTT() (ADR-030 / ADR-089) is now the sole
// publish throttle.

// Pending MQTT throttle slot update — applied only after successful publish.
// Prevents the throttle from "burning" a slot when sendMQTTData fails silently.
struct MQTTPendingSlotUpdate {
  uint8_t  idx;         // tracked slot index
  uint16_t rawValue;    // value to record
  uint16_t trackedTime; // timestamp to record
  bool     pending;     // true = waiting for confirmation
  bool     isTimeOnly;  // true = PS=1 mode (update time only, preserve value bits)
};
static MQTTPendingSlotUpdate mqttPendingSlot = {0, 0, 0, false, false};

struct MQTTPendingTrackedUpdate {
  uint16_t *trackedSlots; // pointer to the tracked array
  uint8_t   slot;         // slot index
  uint16_t  trackedTime;  // timestamp to record
  bool      pending;
};
static MQTTPendingTrackedUpdate mqttPendingBitSlot  = {nullptr, 0, 0, false};
static MQTTPendingTrackedUpdate mqttPendingByteSlot = {nullptr, 0, 0, false};

// TRACKED_TIME_UNSEEN must be a sentinel that currentTrackedSeconds() can never produce.
// currentTrackedSeconds() returns values in [0, TRACKED_TIME_MODULUS-1] = [0, 65534].
// 0xFFFF (65535) is therefore never produced, making it a safe "not yet seen" marker.
// TRACKED_TIME_MODULUS = 65535 keeps the rolling window at ~18.2 hours.
static constexpr uint16_t TRACKED_TIME_UNSEEN  = 0xFFFFU; // sentinel: never produced by currentTrackedSeconds()
static constexpr uint32_t TRACKED_TIME_MODULUS = 65535UL; // modulus → valid range [0, 65534]

enum RestLastUpdatedSlot : uint8_t {
  REST_UPDATED_STATUSFLAGS = 0,
  REST_UPDATED_ASFFLAGS,
  REST_UPDATED_TOUTSIDE,
  REST_UPDATED_TR,
  REST_UPDATED_TRSET,
  REST_UPDATED_TROVERRIDE,
  REST_UPDATED_TSET,
  REST_UPDATED_RELMODLEVEL,
  REST_UPDATED_MAXRELMODLEVELSETTING,
  REST_UPDATED_TBOILER,
  REST_UPDATED_TRET,
  REST_UPDATED_TDHW,
  REST_UPDATED_TDHWSET,
  REST_UPDATED_MAXTSET,
  REST_UPDATED_CHPRESSURE,
  REST_UPDATED_OEMDIAGNOSTICCODE,
  REST_UPDATED_COUNT
};

static uint16_t restLastUpdated[REST_UPDATED_COUNT] = {
  TRACKED_TIME_UNSEEN, TRACKED_TIME_UNSEEN, TRACKED_TIME_UNSEEN, TRACKED_TIME_UNSEEN,
  TRACKED_TIME_UNSEEN, TRACKED_TIME_UNSEEN, TRACKED_TIME_UNSEEN, TRACKED_TIME_UNSEEN,
  TRACKED_TIME_UNSEEN, TRACKED_TIME_UNSEEN, TRACKED_TIME_UNSEEN, TRACKED_TIME_UNSEEN,
  TRACKED_TIME_UNSEEN, TRACKED_TIME_UNSEEN, TRACKED_TIME_UNSEEN, TRACKED_TIME_UNSEEN
};

static uint16_t currentTrackedSeconds()
{
  return static_cast<uint16_t>((millis() / 1000UL) % TRACKED_TIME_MODULUS);
}

static uint16_t elapsedTrackedSeconds(uint16_t now, uint16_t lastTime)
{
  return (now >= lastTime)
           ? static_cast<uint16_t>(now - lastTime)
           : static_cast<uint16_t>((TRACKED_TIME_MODULUS - lastTime) + now);
}

static int8_t restLastUpdatedSlotForMsgId(uint8_t msgId)
{
  switch (msgId) {
    case OT_Statusflags: return REST_UPDATED_STATUSFLAGS;
    case OT_ASFflags: return REST_UPDATED_ASFFLAGS;
    case OT_Toutside: return REST_UPDATED_TOUTSIDE;
    case OT_Tr: return REST_UPDATED_TR;
    case OT_TrSet: return REST_UPDATED_TRSET;
    case OT_TrOverride: return REST_UPDATED_TROVERRIDE;
    case OT_TSet: return REST_UPDATED_TSET;
    case OT_RelModLevel: return REST_UPDATED_RELMODLEVEL;
    case OT_MaxRelModLevelSetting: return REST_UPDATED_MAXRELMODLEVELSETTING;
    case OT_Tboiler: return REST_UPDATED_TBOILER;
    case OT_Tret: return REST_UPDATED_TRET;
    case OT_Tdhw: return REST_UPDATED_TDHW;
    case OT_TdhwSet: return REST_UPDATED_TDHWSET;
    case OT_MaxTSet: return REST_UPDATED_MAXTSET;
    case OT_CHPressure: return REST_UPDATED_CHPRESSURE;
    case OT_OEMDiagnosticCode: return REST_UPDATED_OEMDIAGNOSTICCODE;
    default: return -1;
  }
}

static void clearMsgLastUpdated()
{
  for (uint8_t i = 0; i < REST_UPDATED_COUNT; i++) {
    restLastUpdated[i] = TRACKED_TIME_UNSEEN;
  }
}

uint16_t getMsgLastUpdated(uint8_t msgId)
{
  int8_t slot = restLastUpdatedSlotForMsgId(msgId);
  if (slot < 0) return 0;
  uint16_t tracked = restLastUpdated[slot];
  return (tracked == TRACKED_TIME_UNSEEN) ? 0 : tracked;
}

static void setMsgLastUpdated(uint8_t msgId, uint16_t trackedNow)
{
  int8_t slot = restLastUpdatedSlotForMsgId(msgId);
  if (slot >= 0) {
    restLastUpdated[slot] = trackedNow;
  }
}

static bool tryGetTrackedSlotIndex(uint8_t id, byte masterslave, uint8_t &trackedSlot)
{
  if (id > 127) return false;
  trackedSlot = (masterslave == OT_MSGTYPE_REQUEST)
                ? static_cast<uint8_t>(id + MQTT_TRACKED_RESPONSE_ID_COUNT)
                : id;
  return true;
}

static void resetMqttTrackedState()
{
  for (uint16_t i = 0; i < MQTT_TRACKED_SLOT_COUNT; i++) {
    mqttlastsent[i] = TRACKED_TIME_UNSEEN;
  }
  for (uint8_t i = 0; i < 16; i++) {
    mqttlastsentstatusbit[i] = TRACKED_TIME_UNSEEN;
    mqttlastsentstatusvhbit[i] = TRACKED_TIME_UNSEEN;
  }
  mqttlastsentstatusbyte[0] = TRACKED_TIME_UNSEEN;
  mqttlastsentstatusbyte[1] = TRACKED_TIME_UNSEEN;
  mqttlastsentstatusvhbyte[0] = TRACKED_TIME_UNSEEN;
  mqttlastsentstatusvhbyte[1] = TRACKED_TIME_UNSEEN;
  // TASK-401: non-Status fan-out trackers (ASF / RBP / Remote Override)
  for (uint8_t i = 0; i < 8; i++) mqttlastsentASFbit[i] = TRACKED_TIME_UNSEEN;
  mqttlastsentASFbyte[0] = TRACKED_TIME_UNSEEN;
  for (uint8_t i = 0; i < 4; i++) mqttlastsentRBPbit[i]  = TRACKED_TIME_UNSEEN;
  mqttlastsentRBPbyte[0] = TRACKED_TIME_UNSEEN;
  mqttlastsentRBPbyte[1] = TRACKED_TIME_UNSEEN;
  mqttlastsentRObit[0]  = TRACKED_TIME_UNSEEN;
  mqttlastsentRObit[1]  = TRACKED_TIME_UNSEEN;
  mqttlastsentRObyte[0] = TRACKED_TIME_UNSEEN;
}

struct TrackingStateInitializer {
  TrackingStateInitializer()
  {
    clearMsgLastUpdated();
    resetMqttTrackedState();
  }
};

static TrackingStateInitializer gTrackingStateInitializer;
struct OT_cmd_t cmdqueue[CMDQUEUE_MAX];
int cmdQueueSize = 0;  // fill-pointer: entries are 0..cmdQueueSize-1, left-shift on deletion

// ===== ADR-123 Phase-1 concurrency foundation (TASK-865.5) ================
// Definitions for the OT-frame producer/consumer queue and the OTGWState mutex
// declared in OTGW-Core.h. Both handles are created ONCE in setupOTConcurrency()
// (called from setup(), ADR-044). Until then they are nullptr and the shims
// degrade gracefully (enqueue drops, lock is a no-op).
PlatformQueue otFrameQueue = nullptr;   // OTFrameMsg producer->consumer queue
PlatformMutex otStateMutex = nullptr;   // guards the decoded OTGWState snapshot

// Diagnostic counter: OTFrameMsg producers that hit a full queue. Should stay 0
// in Phase 1 (producer+consumer co-resident per loop iteration). A non-zero
// value means the depth/drain assumption broke and frames were lost — surfaced
// for the field-soak AC, never used as a fall-back trigger.
static uint32_t otFrameQueueDrops = 0;

// setupOTConcurrency — create the mutex and frame queue exactly once (ADR-044).
// Safe to call before WiFi/MQTT are up; pure FreeRTOS object allocation.
void setupOTConcurrency() {
  if (otStateMutex == nullptr) otStateMutex = platformMutexCreate();
  if (otFrameQueue == nullptr) {
    otFrameQueue = platformQueueCreate(OT_FRAME_QUEUE_DEPTH, sizeof(OTFrameMsg));
  }
  // TASK-865.6: PIC-UART TX queue (loop-side producers -> dedicated task writer).
  if (otTxQueue == nullptr) {
    otTxQueue = platformQueueCreate(OT_TX_QUEUE_DEPTH, sizeof(OTTxMsg));
  }
}

// enqueueOTFrame — producer-side. Copy the line (null-terminated), enqueue by
// value. The ONLY two callers are the frame producers (dispatchOTGWInputLine,
// bridgeFrameToParser). Returns false on a full queue (diagnostic drop).
bool enqueueOTFrame(const char *buf, size_t len, bool suppressOutput, uint8_t source) {
  if (otFrameQueue == nullptr || buf == nullptr || len == 0) return false;
  OTFrameMsg msg;
  if (len > sizeof(msg.line) - 1) len = sizeof(msg.line) - 1;  // clamp (defensive)
  memcpy(msg.line, buf, len);
  msg.line[len] = '\0';                       // processOT's non-OT branches need a terminator
  msg.len = (uint16_t)len;
  msg.suppressOutput = suppressOutput;
  msg.source = source;
  if (!platformQueueSend(otFrameQueue, &msg)) {
    otFrameQueueDrops++;
    return false;
  }
  return true;
}

// Forward decl: reportPendingPICRxErrors() is defined after picSerialDrainOnce
// but called from drainOTFrameQueue below (static fns are not auto-prototyped by
// the Arduino preprocessor, so the prototype must precede the first use).
#if HAS_PIC
static void reportPendingPICRxErrors();
#endif

// drainOTFrameQueue — consumer-side. Dequeue every pending frame and parse it
// under the OTStateLock (writer side). Called from loop() AFTER
// doBackgroundTasks() so it never nests with the re-entrant
// doAutoConfigure->handleOTGW enqueue path.
void drainOTFrameQueue() {
#if HAS_PIC
  // TASK-865.6: report the RX-error signals the PIC task detected (it only sets
  // plain volatile flags/counters — no MQTT/WS/OTGWState writes happen in task
  // context). Done here, loop-side, so all network + OTGWState writes stay on the
  // single (loop) thread.
  reportPendingPICRxErrors();
#endif
  if (otFrameQueue == nullptr) return;
  OTFrameMsg msg;
  while (platformQueueReceive(otFrameQueue, &msg, 0)) {
    // PIC-only loop-side side-effects (moved off the PIC task, TASK-865.6):
    // the LED blink and the ser2net 25238 mirror used to run producer-side in
    // dispatchOTGWInputLine. OTDirect mirrors producer-side itself, so gate on
    // source to avoid double-emitting OTDirect frames to 25238.
    if (msg.source == OTFRAME_SRC_PIC) {
      blinkLEDnow(LED2);
      if (settings.mqtt.bLegacyPort25238Enabled) {
        OTGWstream.write(reinterpret_cast<const uint8_t*>(msg.line), msg.len);
        OTGWstream.write('\r');
        OTGWstream.write('\n');
      }
    }
    // processOT() acquires OTStateLock internally (writer side), covering all
    // five processOT call sites uniformly — not just this consumer. Do NOT wrap
    // here too: the lock is non-recursive and would self-deadlock.
    processOT(msg.line, msg.len, msg.suppressOutput);
    feedWatchDog();                            // bound worst-case drain time
  }
}

// ===== ADR-123 Phase-1 PIC-UART dedicated task (TASK-865.6) ================
// Definitions for the TX queue (declared in OTGW-Core.h) and the dedicated
// FreeRTOS PIC-UART task. See the header for the full design rationale.
PlatformQueue otTxQueue = nullptr;            // raw-byte loop->task TX queue

// TASK-865.15: loop-side park request for the resetPic() direct UART write.
// resetPic() (vendored OTGWSerial.cpp) writes "GW=R\r" straight to the UART and
// toggles the reset GPIO with a delay(100); it bypasses otTxQueue and is not
// gated by any park condition. resetOTGW() raises this flag so the PIC task
// parks (releases the UART) for the duration of the direct write, avoiding a
// second concurrent UART owner. Declared UNCONDITIONALLY (not under #if HAS_PIC)
// because picSerialTaskShouldPark() — which reads it — is itself compiled on
// every build (incl. HAS_PIC==0), and resetOTGW() is a no-op there anyway.
static volatile bool g_picResetInProgress = false;

#if HAS_PIC
static PlatformTask    g_picSerialTask  = nullptr;  // dedicated PIC-UART task handle
static volatile bool   g_picTaskParked  = false;    // task->loop park acknowledgement
static uint32_t        otTxQueueDrops   = 0;         // diagnostic: full-TX-queue drops

// TASK-865.6: RX-error signals DETECTED in the PIC task but REPORTED loop-side.
// The task only sets these plain volatile flags/counters; the loop-side
// reportPendingPICRxErrors() does the actual OTGWState write + MQTT/WS publish
// under OTStateLock, keeping OTGWState single-writer and the network stack
// single-threaded. The read/clear race is benign for diagnostics (no atomics).
static volatile bool     g_picRxOverrunPending  = false;  // UART HW overrun seen
static volatile bool     g_picRxErrorPending    = false;  // UART HW framing/parity error seen
static volatile uint32_t g_picRxOverflowPending = 0;      // line-buffer overflows since last report
#endif

// enqueuePICTx — producer-side. Copy the bytes verbatim and queue by value for
// the PIC task to write. Returns false on a full queue (diagnostic drop) or when
// the TX path is not available (no PIC build / queue not yet created).
bool enqueuePICTx(const uint8_t *buf, size_t len) {
#if HAS_PIC
  if (otTxQueue == nullptr || buf == nullptr || len == 0) return false;
  OTTxMsg msg;
  if (len > sizeof(msg.bytes)) len = sizeof(msg.bytes);   // clamp (defensive)
  memcpy(msg.bytes, buf, len);
  msg.len = (uint16_t)len;
  if (!platformQueueSend(otTxQueue, &msg)) {
    otTxQueueDrops++;
    return false;
  }
  return true;
#else
  (void)buf; (void)len;
  return false;
#endif
}

#if HAS_PIC
// picSerialFlushRx — OWNER FN. Discard any pending RX bytes from the PIC UART.
// Used by the OTGW-replay simulation path (which feeds the parser from a file and
// must not also ingest live PIC bytes). Runs only while the dedicated task is
// parked (simulation implies no concurrent task drain). Byte-I/O owner: sole
// place the simulation flush touches OTGWSerial.read()/available().
void picSerialFlushRx() {
  while (OTGWSerial.available()) {
    OTGWSerial.read();
    feedWatchDog();
  }
}

// picSerialPumpUpgrade — OWNER FN. While a PIC/ESP flash is in progress the task
// is parked and the loop-side flash FSM owns the UART. The OTGWSerial upgrade
// state machine is driven from inside its byte-I/O methods (available()/read()
// short-circuit to upgradeTick() while busy()), so a single available() poll per
// loop keeps the upgrade ticking. Byte-I/O owner: the only loop-side UART touch
// during a flash.
void picSerialPumpUpgrade() {
  // available() internally calls upgradeEvent()->upgradeTick() when an upgrade
  // is active; the returned byte count is irrelevant (and 0 during an upgrade).
  (void)OTGWSerial.available();
}

// picSerialDrainOnce — OWNER FN. The task body's single work unit: drain the RX
// FIFO into complete CR/LF lines (enqueued onto the OTFrame queue with
// source=PIC) and drain the TX queue out to the UART. This is the SOLE runtime
// owner of OTGWSerial.read()/.write()/.availableForWrite()/.flush(). No per-call
// line cap (kMaxLinesPerDrain is gone): the task runs on its own core slice, so
// it can fully empty the FIFO each pass without starving the network/web stack.
//
// Strict byte-I/O-only mandate (TASK-865.6): this runs in TASK context, so it
// performs NO network/MQTT/WebSocket I/O and NO OTGWState writes. RX errors are
// DETECTED here (UART access) but only flagged via plain volatile vars; the
// loop-side reportPendingPICRxErrors() (called from drainOTFrameQueue) does the
// actual reporting + OTGWState write under OTStateLock. The LED blink and the
// ser2net 25238 mirror likewise moved to the loop-side consumer.
void picSerialDrainOnce() {
  // ---- RX: read bytes, assemble lines, enqueue each complete line ----------
  static char    sRead[MAX_BUFFER_READ];
  static size_t  bytes_read = 0;
  static bool    discardCurrentReadLine = false;
  static uint32_t droppedReadLines = 0;

  // Detect-only (no reporting in task context): set flags, report loop-side.
  if (platformSerialHasOverrun(OTGWSerial)) g_picRxOverrunPending = true;
  if (platformSerialHasRxError(OTGWSerial)) g_picRxErrorPending   = true;

  while (OTGWSerial.available()) {
    uint8_t outByte = OTGWSerial.read();
    if (outByte == '\r' || outByte == '\n') {
      if ((bytes_read == 0) && !discardCurrentReadLine) continue;

      if (discardCurrentReadLine) {
        droppedReadLines++;
        DebugTf(PSTR("Serial line dropped after overflow. Dropped lines total: %lu\r\n"),
                static_cast<unsigned long>(droppedReadLines));
      }

      if (!discardCurrentReadLine) {
        sRead[bytes_read] = '\0';
        // Pure byte->frame seam: enqueue directly (value-copy queue, thread-safe).
        // LED + 25238 mirror are applied loop-side by the consumer (source=PIC).
        enqueueOTFrame(sRead, bytes_read, false, OTFRAME_SRC_PIC);
      }

      bytes_read = 0;
      discardCurrentReadLine = false;
    } else if (bytes_read < (MAX_BUFFER_READ - 1)) {
      if (!discardCurrentReadLine) {
        sRead[bytes_read++] = outByte;
      }
    } else {
      // Buffer overflow detected - discard this complete line. Flag only; the
      // loop-side reporter does the OTGWState bump + MQTT/WS publish.
      // (non-compound assign: volatile++ is deprecated in C++20 -Wvolatile)
      g_picRxOverflowPending = g_picRxOverflowPending + 1;
      DebugTf(PSTR("Serial Buffer Overflow! Discarding %d bytes.\r\n"), bytes_read);
      // Drop this line until next CR/LF to avoid forwarding partial/corrupted data
      bytes_read = 0;
      discardCurrentReadLine = true;
    }
    feedWatchDog();
  }

  // ---- TX: drain the command/relay byte queue out to the UART --------------
  OTTxMsg tx;
  while (platformQueueReceive(otTxQueue, &tx, 0)) {
    // availableForWrite() is the only flow-control we have; if the UART TX
    // buffer is full this pass, the bytes wait in the queue for the next tick.
    if (OTGWSerial.availableForWrite() >= tx.len) {
      OTGWSerial.write(tx.bytes, tx.len);
      OTGWSerial.flush();
    } else {
      // TX buffer can't take this chunk this pass. Re-queue it to the FRONT and
      // stop draining: the loop-side ser2net relay (enqueuePICTx, 1 byte each)
      // can enqueue concurrently between our pop and re-queue, so a re-queue to
      // the BACK would let a relay byte jump ahead of this popped chunk and
      // interleave a command's bytes (TASK-865.15). Sending to front keeps the
      // popped chunk as the next thing written, preserving strict FIFO order.
      platformQueueSendToFront(otTxQueue, &tx);
      break;
    }
    feedWatchDog();
  }
}

// reportPendingPICRxErrors — loop-side. Consume the RX-error signals the PIC
// task flagged and do the actual reporting: OTGWState write (single-writer,
// loop-side) + telnet + MQTT/WS publish. Read-and-clear; the tiny race with the
// task setting a flag again is acceptable for diagnostics.
#if HAS_PIC
static void reportPendingPICRxErrors() {
  if (g_picRxOverrunPending) {
    g_picRxOverrunPending = false;
    DebugT(F("Serial Overrun\r\n"));
    reportOTGWEvent_P(PSTR("Serial Overrun"), '!', true);
  }
  if (g_picRxErrorPending) {
    g_picRxErrorPending = false;
    DebugT(F("Serial Rx Error\r\n"));
    reportOTGWEvent_P(PSTR("Serial Rx Error"), '!', true);
  }
  uint32_t overflows = g_picRxOverflowPending;
  if (overflows) {
    g_picRxOverflowPending = 0;
    OTcurrentSystemState.errorBufferOverflow += overflows;   // single-writer (loop)
    snprintf_P(cMsg, sizeof(cMsg), PSTR("Serial overflow [%u]"), OTcurrentSystemState.errorBufferOverflow);
    sendEventToWebSocket('!', cMsg);
    char overflowCountBuf[12] = {0};
    utoa(OTcurrentSystemState.errorBufferOverflow, overflowCountBuf, 10);
    sendMQTTData(F("Error_BufferOverflow"), overflowCountBuf);
  }
}
#endif

// The dedicated PIC-UART task body. Blocks between drains via platformTaskDelay()
// (the cooperative yield) so it never busy-polls available() — a tight no-yield
// poll would starve the core and trip the Task Watchdog. While parked it raises
// the ack flag and sleeps longer; the loop-side flash FSM waits on that ack
// before driving the UART (waitForPICTaskParked).
static void picSerialTaskBody(void *arg) {
  (void)arg;
  for (;;) {
    if (picSerialTaskShouldPark()) {
      g_picTaskParked = true;
      platformTaskDelay(20);            // parked: idle, do not touch the UART
      continue;
    }
    g_picTaskParked = false;
    picSerialDrainOnce();
    platformTaskDelay(2);               // yield ~1 tick; FIFO refills meanwhile
  }
}
#endif // HAS_PIC

// picSerialTaskShouldPark — see header. true => task releases the UART. Defined
// unconditionally (references only isOTDirectEnabled/isFlashing/simulation, all
// available on every build) so the symbol exists regardless of HAS_PIC.
// Also parks during the OTGW replay simulation: the simulation feeds the parser
// from a file and flushes the live UART loop-side (picSerialFlushRx), so the
// task must not drain it concurrently.
bool picSerialTaskShouldPark() {
  // g_picResetInProgress parks the task during resetOTGW()'s direct GW=R write
  // (TASK-865.15), so the loop-side reset is never a second concurrent UART
  // owner alongside the task drain.
  return isOTDirectEnabled() || isFlashing() || state.debug.bOTGWSimulation
         || g_picResetInProgress;
}

// startPICSerialTask — create the dedicated PIC task once (ADR-044). On a no-PIC
// build the body is empty; on a combo that boots into OTDirect the task is still
// created but immediately parks (picSerialTaskShouldPark()==true), so it never
// touches the closed UART — it just sleeps.
void startPICSerialTask() {
#if HAS_PIC
  if (g_picSerialTask != nullptr) return;        // create exactly once
  g_picSerialTask = platformTaskCreatePinned(
      picSerialTaskBody, "picSerial", 4096, nullptr, 1);
  if (g_picSerialTask == nullptr) {
    DebugTln(F("ERROR: failed to create PIC serial task"));
  } else {
    DebugTln(F("PIC serial task started (pinned to app core)"));
  }
#endif
}

// waitForPICTaskParked — loop-side handshake before the flash FSM drives the UART.
void waitForPICTaskParked() {
#if HAS_PIC
  if (g_picSerialTask == nullptr) return;        // task never created => nothing to wait for
  // Bounded spin: the task tick is 2 ms, so it parks within a few ms once
  // shouldPark flips. Cap at ~200 ms so a wedged task can never hang the flash.
  for (uint16_t i = 0; i < 100 && !g_picTaskParked; i++) {
    feedWatchDog();
    delay(2);
  }
#endif
}

#define OTGW_BANNER "OpenTherm Gateway"

enum OTSpecCompatMode : uint8_t {
  OT_SPEC_COMPAT_AUTO = 0,
  OT_SPEC_COMPAT_V4X_STRICT,
  OT_SPEC_COMPAT_PRE_V42_LEGACY
};

// Default behavior:
// - AUTO keeps pre-v4.2 compatibility until a 4.x OpenTherm version is detected,
//   then applies v4.x reserved-ID rules (notably IDs 50-63).
static OTSpecCompatMode gOTSpecCompatMode = OT_SPEC_COMPAT_AUTO;

//===================[ OT Spec Profile Helpers ]====================
static bool isLegacyPreV42CompatibilityId(uint8_t msgid)
{
  return (msgid >= 50U && msgid <= 63U);
}

static bool useV4xReservedIdRules()
{
  switch (gOTSpecCompatMode) {
    case OT_SPEC_COMPAT_V4X_STRICT:
      return true;
    case OT_SPEC_COMPAT_PRE_V42_LEGACY:
      return false;
    case OT_SPEC_COMPAT_AUTO:
    default:
      return (OTcurrentSystemState.OpenThermVersionSlave >= 4.0f) ||
             (OTcurrentSystemState.OpenThermVersionMaster >= 4.0f);
  }
}

static bool isMsgIdReservedInActiveProfile(uint8_t msgid)
{
  return isLegacyPreV42CompatibilityId(msgid) && useV4xReservedIdRules();
}

static PGM_P activeOTSpecProfileName_P()
{
  if (useV4xReservedIdRules()) return PSTR("OpenTherm v4.x");
  return PSTR("pre-v4.2 legacy");
}

static void copyProgmemString(char *dst, size_t dstSize, PGM_P src)
{
  if (dst == nullptr || dstSize == 0) return;
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  strncpy_P(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

static inline const __FlashStringHelper* toFlashStringHelper(PGM_P p)
{
  return reinterpret_cast<const __FlashStringHelper*>(p);
}

static void appendProgmemSuffix(char *dst, size_t dstSize, PGM_P suffix)
{
  if (dst == nullptr || dstSize == 0 || suffix == nullptr) return;
  size_t len = strlen(dst);
  if (len >= (dstSize - 1)) return;
  strncat_P(dst, suffix, dstSize - len - 1);
}

//===================[ Reset OTGW ]===============================
#if HAS_PIC
void resetOTGW() {
  if (!isPICEnabled()) return;
  scheduleOTGWStartupQuietPeriod();
  // TASK-865.15: resetPic() does a direct UART write of "GW=R\r" + a reset-GPIO
  // toggle (delay(100)), bypassing otTxQueue. The ser2net GW=R path and the MQTT
  // resetgateway command both call this loop-side WHILE THE PIC TASK IS UNPARKED
  // (reset flips no other park condition), making the loop a second concurrent
  // UART owner. Park the task for the duration so the direct write has the UART
  // to itself. The GPIO toggle + delay live in the vendored resetPic(), so we
  // cannot queue it; parking the task is the in-scope fix. At boot resetOTGW()
  // runs before startPICSerialTask(), so waitForPICTaskParked() returns
  // immediately (no task yet) and this adds no startup latency.
  g_picResetInProgress = true;
  waitForPICTaskParked();
  OTGWSerial.resetPic();
  g_picResetInProgress = false;
}

/*
  To detect the pic, reset the pic, then find ETX in the response after reset (within 1 second).
  The ETX response is send by the bootload, when received it also means you have a pic connected.
*/
void detectPIC(){
  OTGWSerial.registerFirmwareCallback(fwreportinfo); //register the callback to report version, type en device ID
  scheduleOTGWStartupQuietPeriod();
  // TASK-865.15: detectPIC drives the UART directly twice in a row outside the
  // task-owner functions — resetPic() (direct GW=R write) and find(ETX) (an
  // inherited Stream read loop = concurrent UART byte-I/O). At boot both run
  // before startPICSerialTask() so there is no task to race, but the manual 'p'
  // telnet command reaches detectPIC() at runtime while the PIC task is unparked,
  // making the loop a second concurrent UART owner for the whole reset+probe.
  // Park the task for the entire span (same flag resetOTGW() uses) so the direct
  // write AND the ETX read have the UART to themselves. waitForPICTaskParked()
  // returns immediately at boot (no task yet), so this adds no startup latency.
  g_picResetInProgress = true;
  waitForPICTaskParked();
  OTGWSerial.resetPic(); // make sure it the firmware is detected
  state.pic.bAvailable = OTGWSerial.find(ETX);
  g_picResetInProgress = false;
  if (state.pic.bAvailable) {
      state.hw.eMode = HW_MODE_PIC;
      DebugTln(F("ETX found after reset: Pic detected!"));
  } else {
      state.hw.eMode = HW_MODE_DEGRADED;
      DebugTln(F("No ETX found after reset: no Pic detected!"));
      DebugTln(F("All PIC-related functions are disabled (no PIC-based OTGW detected)"));
  }
}
#else
void resetOTGW() {}   // no-op on OTGW32
void detectPIC() {}   // no-op on OTGW32
#endif

//===================[ getpicfwversion ]===========================
/*
Get the information of the pic firmware: version  number, device type and firmware type. 
This is done by sending a PR=A command, requesting a banner from the PIC. This will trigger detection of version.
*/
void getpicfwversion(){
  // Non-blocking: queues PR=A via the command queue.
  // The banner response is processed by handlePRresponse() which copies
  // OTGWSerial version fields into state.pic.* and publishes MQTT.
  addCommandToQueue("PR=A", 4, true);
}
//===================[ queryOTGWgatewaymode ]======================
/*
Query the actual gateway mode setting from the PIC firmware using PR=M command.
Non-blocking: queues PR=M via the command queue. The response is processed
asynchronously by handlePRresponse() when it arrives via processOT().
*/
void queryOTGWgatewaymode(){
  static uint32_t lastGatewayModeQueryMs = 0;
  constexpr uint32_t GATEWAY_MODE_QUERY_MIN_INTERVAL_MS = 60000; // max one PR=M per minute

  if (!state.pic.bAvailable) {
    OTDebugTln(F("queryOTGWgatewaymode: PIC not available"));
    return;
  }

  const uint32_t now = millis();
  if (state.otBus.bGatewayModeKnown && ((uint32_t)(now - lastGatewayModeQueryMs) < GATEWAY_MODE_QUERY_MIN_INTERVAL_MS)) {
    OTDebugTf(PSTR("queryOTGWgatewaymode: throttled\r\n"));
    return;
  }

  lastGatewayModeQueryMs = now;
  addCommandToQueue("PR=M", 4, true);  // forceQueue=true; response handled by handlePRresponse()
}

//===================[ PIC settings readout control ]=============
/*
  On-demand PIC settings readout.
  Call triggerPICsettingsReadout() to start a full cycle.
  pollPICsettings() is called from the main loop every iteration;
  it spaces out PR= commands every 3 seconds when a cycle is active.

  A cycle runs automatically at boot. Subsequent cycles are triggered
  by the REST API (GET /api/v2/pic/settings) or after any command
  is sent to the PIC via addCommandToQueue().

  Multiple rapid triggers are coalesced: while a cycle is in
  progress, additional triggers are silently ignored.
*/
bool            picSettingsCycleActive = false;
static uint8_t  picSettingsQueryIdx    = 0;
static constexpr uint8_t kPICSettingsCount = 15;

void triggerPICsettingsReadout() {
  if (!isPICEnabled()) return;
  if (picSettingsCycleActive) {
    return;  // cycle already in progress — ignore until it completes
  }
  picSettingsQueryIdx    = 0;
  picSettingsCycleActive = true;
  OTDebugTln(F("PIC settings readout cycle triggered"));
}

//===================[ queryNextPICsetting ]======================
/*
  Polls one PIC setting via a PR= command and advances to the next.
  Called by pollPICsettings() every 3 seconds during an active cycle.
  A full cycle of 15 settings completes in ~45 seconds.

  PR= command reference (Schelte Bron, https://otgw.tclcode.com/firmware.html):
    PR=A and PR=M are handled separately (getpicfwversion / queryOTGWgatewaymode).

    PR=O  -> setpoint override: "O=T20.5" (TT active), "O=C20.5" (TC active), or "O=N" (none)
    PR=S  -> setback temperature: "S=15.0"
    PR=W  -> DHW (hot water) override: "W=0" (off), "W=1" (on), or "W=A" (auto)
    PR=G  -> GPIO A+B function codes: "G=05" (two digits, function per pin)
    PR=I  -> GPIO A+B current input states: "I=00"
    PR=L  -> LED A-F function chars: "L=RFFTTT" (six chars)
    PR=T  -> Tweaks: "T=NM" (ignore_transitions + ovrd_high_byte, two chars)
    PR=D  -> External temp sensor function: "D=O" (outside) or "D=R" (return water); v5+ only
    PR=P  -> Smart power mode: "P=L" (low), "P=M" (medium), "P=H" (high), or "P=N" (off)
    PR=R  -> Thermostat detection setting
    PR=B  -> Firmware build date/time: "B=17:52 12-03-2023"
    PR=C  -> PIC clock speed in MHz: "C=4"
    PR=Q  -> Last reset cause: "Q=W" (watchdog), "Q=B" (brownout), "Q=P" (power-on)
    PR=N  -> Message interval in standalone mode: "N=30"
    PR=V  -> Voltage reference setting: "V=1"

  Values are stored in state.picSettings and published to MQTT when they change.
  NG/SE/TO responses are silently ignored (keeps previous cached value).
  Skips all queries when PIC is unavailable, offline, or flashing is in progress.
*/
void queryNextPICsetting() {
  if (!isPICEnabled() || !isGatewayFirmware()) return;
  if (state.flash.bESPactive || state.flash.bPICactive) return;
  // Defer PR= queries during Status-burst fanouts and when a drip tick is imminent.
  // Each PR= response triggers 2 MQTT publishes; landing these inside a burst or
  // drip window amplifies heap pressure. Deferred queries retry on the next 3s tick.
  if (isStatusBurstActive()) return;
  if (dripDueWithinMs(500)) return;

  const uint8_t idx = picSettingsQueryIdx;
  picSettingsQueryIdx++;
  if (picSettingsQueryIdx >= kPICSettingsCount) {
    picSettingsCycleActive = false;
    OTDebugTln(F("PIC settings readout cycle complete"));
  }

  // Map index → register letter. Response parsing is handled asynchronously
  // by handlePRresponse() when the PR: response arrives via processOT().
  char letter = 0;
  switch (idx) {
    case  0: letter = 'O'; break;  // Setpoint override
    case  1: letter = 'S'; break;  // Setback temperature
    case  2: letter = 'W'; break;  // DHW override
    case  3: letter = 'G'; break;  // GPIO configuration
    case  4: letter = 'I'; break;  // GPIO states
    case  5: letter = 'L'; break;  // LED configuration
    case  6: letter = 'T'; break;  // Tweaks
    case  7: letter = 'D'; break;  // Temperature sensor
    case  8: letter = 'P'; break;  // Smart power
    case  9: letter = 'R'; break;  // Thermostat detect
    case 10: letter = 'B'; break;  // Build date
    case 11: letter = 'C'; break;  // Clock MHz
    case 12: letter = 'Q'; break;  // Reset cause
    case 13: letter = 'N'; break;  // Standalone interval
    case 14: letter = 'V'; break;  // Voltage reference
    default: return;
  }

  char cmd[5];
  snprintf_P(cmd, sizeof(cmd), PSTR("PR=%c"), letter);
  addCommandToQueue(cmd, 4, true);  // forceQueue=true: bypass PR prefix dedup
}

//===================[ handlePRresponse ]==========================
/*
  Asynchronous handler for PR: responses from the PIC.
  Called from processOT() when a line starting with "PR:" is received.
  Dispatches based on register letter: updates state, publishes MQTT.
  Special cases: 'M' (gateway mode), 'A' (firmware banner).
*/
static void handlePRresponse(const char* buf, size_t len) {
  // buf = "PR: X=value" or "PR: OpenTherm Gateway x.x"
  if (len < 4 || buf[0] != 'P' || buf[1] != 'R' || buf[2] != ':') return;

  // Skip "PR:" prefix and trim leading whitespace
  const char* payload = buf + 3;
  while (*payload == ' ') payload++;
  size_t plen = strlen(payload);
  if (plen == 0) return;

  // Note: PR=A banner ("OpenTherm Gateway x.x") never reaches here because
  // the banner has no ":" at position 2. It is handled in processOT() directly.

  // --- Register response: "X=value" ---
  if (plen < 3 || payload[1] != '=') return;  // need at least "X=v"
  const char  reg   = payload[0];
  const char* value = payload + 2;

  // --- Gateway mode (register 'M'): "M=G" or "M=M" ---
  if (reg == 'M') {
    static bool prevGatewayMode = false;
    static bool prevGatewayKnown = false;
    char modeVal = value[0];
    bool isGateway;
    if (modeVal == 'G' || modeVal == 'g') {
      isGateway = true;
    } else if (modeVal == 'M' || modeVal == 'm') {
      isGateway = false;
    } else {
      OTDebugTf(PSTR("handlePRresponse: PR=M unexpected value [%c]\r\n"), modeVal);
      return;
    }
    state.otBus.bGatewayMode = isGateway;
    state.otBus.bGatewayModeKnown = true;
    if (isGateway != prevGatewayMode || !prevGatewayKnown) {
      sendMQTTDataPic(F("gateway_mode"), CCONOFF(isGateway));
      prevGatewayMode = isGateway;
      prevGatewayKnown = true;
      OTDebugTf(PSTR("handlePRresponse: gateway mode = %s\r\n"), CCONOFF(isGateway));
    }
    return;
  }

  // --- PIC settings registers ---
  char*       stateField = nullptr;
  size_t      fieldSize  = 0;
  const __FlashStringHelper* mqttTopic = nullptr;

  switch (reg) {
    case 'O': stateField = state.picSettings.sSetpointOverride;
              fieldSize = sizeof(state.picSettings.sSetpointOverride);
              mqttTopic = F("otgw-pic/settings/setpoint_override"); break;
    case 'S': stateField = state.picSettings.sSetback;
              fieldSize = sizeof(state.picSettings.sSetback);
              mqttTopic = F("otgw-pic/settings/setback");           break;
    case 'W': stateField = state.picSettings.sDhwOverride;
              fieldSize = sizeof(state.picSettings.sDhwOverride);
              mqttTopic = F("otgw-pic/settings/dhw_override");      break;
    case 'G': stateField = state.picSettings.sGpio;
              fieldSize = sizeof(state.picSettings.sGpio);
              mqttTopic = F("otgw-pic/settings/gpio");              break;
    case 'I': stateField = state.picSettings.sGpioStates;
              fieldSize = sizeof(state.picSettings.sGpioStates);
              mqttTopic = F("otgw-pic/settings/gpio_states");       break;
    case 'L': stateField = state.picSettings.sLed;
              fieldSize = sizeof(state.picSettings.sLed);
              mqttTopic = F("otgw-pic/settings/led");               break;
    case 'T': stateField = state.picSettings.sTweaks;
              fieldSize = sizeof(state.picSettings.sTweaks);
              mqttTopic = F("otgw-pic/settings/tweaks");            break;
    case 'D': stateField = state.picSettings.sTempSensor;
              fieldSize = sizeof(state.picSettings.sTempSensor);
              mqttTopic = F("otgw-pic/settings/temp_sensor");       break;
    case 'P': stateField = state.picSettings.sSmartPower;
              fieldSize = sizeof(state.picSettings.sSmartPower);
              mqttTopic = F("otgw-pic/settings/smart_power");       break;
    case 'R': stateField = state.picSettings.sThermostatDetect;
              fieldSize = sizeof(state.picSettings.sThermostatDetect);
              mqttTopic = F("otgw-pic/settings/thermostat_detect"); break;
    case 'B': stateField = state.picSettings.sBuilddate;
              fieldSize = sizeof(state.picSettings.sBuilddate);
              mqttTopic = F("otgw-pic/settings/builddate");         break;
    case 'C': stateField = state.picSettings.sClockMHz;
              fieldSize = sizeof(state.picSettings.sClockMHz);
              mqttTopic = F("otgw-pic/settings/clock_mhz");         break;
    case 'Q': stateField = state.picSettings.sResetCause;
              fieldSize = sizeof(state.picSettings.sResetCause);
              mqttTopic = F("otgw-pic/settings/reset_cause");       break;
    case 'N': stateField = state.picSettings.sStandaloneInterval;
              fieldSize = sizeof(state.picSettings.sStandaloneInterval);
              mqttTopic = F("otgw-pic/settings/standalone_interval"); break;
    case 'V': stateField = state.picSettings.sVoltageRef;
              fieldSize = sizeof(state.picSettings.sVoltageRef);
              mqttTopic = F("otgw-pic/settings/voltage_ref");       break;
    default:
      OTDebugTf(PSTR("handlePRresponse: unknown register [%c]\r\n"), reg);
      return;
  }

  bool changed = (strcmp(stateField, value) != 0);
  if (changed) {
    strlcpy(stateField, value, fieldSize);
    OTDebugTf(PSTR("handlePRresponse: PR=%c updated to [%s]\r\n"), reg, stateField);
    sendMQTTData(mqttTopic, stateField);
  }

  if (hasWebSocketClients()) {
    char eventBuf[80];
    snprintf_P(eventBuf, sizeof(eventBuf), PSTR("PIC setting PR=%c = %s"), reg, stateField);
    sendEventToWebSocket('*', eventBuf);
  }
}

//===================[ publishAllPICsettings ]=====================
/*
  Publishes all currently cached PIC settings to MQTT.
  Call on reconnect or periodic refresh to sync all topics.
  Only publishes fields that have been queried (non-empty).
*/
void publishAllPICsettings() {
  if (!isPICEnabled()) return;
  // Active settings
  if (state.picSettings.sSetpointOverride[0]   != '\0') sendMQTTData(F("otgw-pic/settings/setpoint_override"),   state.picSettings.sSetpointOverride);
  if (state.picSettings.sSetback[0]            != '\0') sendMQTTData(F("otgw-pic/settings/setback"),             state.picSettings.sSetback);
  if (state.picSettings.sDhwOverride[0]        != '\0') sendMQTTData(F("otgw-pic/settings/dhw_override"),        state.picSettings.sDhwOverride);
  // Hardware configuration
  if (state.picSettings.sGpio[0]               != '\0') sendMQTTData(F("otgw-pic/settings/gpio"),                state.picSettings.sGpio);
  if (state.picSettings.sGpioStates[0]         != '\0') sendMQTTData(F("otgw-pic/settings/gpio_states"),         state.picSettings.sGpioStates);
  if (state.picSettings.sLed[0]                != '\0') sendMQTTData(F("otgw-pic/settings/led"),                 state.picSettings.sLed);
  if (state.picSettings.sTweaks[0]             != '\0') sendMQTTData(F("otgw-pic/settings/tweaks"),              state.picSettings.sTweaks);
  if (state.picSettings.sTempSensor[0]         != '\0') sendMQTTData(F("otgw-pic/settings/temp_sensor"),         state.picSettings.sTempSensor);
  if (state.picSettings.sSmartPower[0]         != '\0') sendMQTTData(F("otgw-pic/settings/smart_power"),         state.picSettings.sSmartPower);
  if (state.picSettings.sThermostatDetect[0]   != '\0') sendMQTTData(F("otgw-pic/settings/thermostat_detect"),   state.picSettings.sThermostatDetect);
  // Diagnostics
  if (state.picSettings.sBuilddate[0]          != '\0') sendMQTTData(F("otgw-pic/settings/builddate"),           state.picSettings.sBuilddate);
  if (state.picSettings.sClockMHz[0]           != '\0') sendMQTTData(F("otgw-pic/settings/clock_mhz"),           state.picSettings.sClockMHz);
  if (state.picSettings.sResetCause[0]         != '\0') sendMQTTData(F("otgw-pic/settings/reset_cause"),         state.picSettings.sResetCause);
  if (state.picSettings.sStandaloneInterval[0] != '\0') sendMQTTData(F("otgw-pic/settings/standalone_interval"), state.picSettings.sStandaloneInterval);
  if (state.picSettings.sVoltageRef[0]         != '\0') sendMQTTData(F("otgw-pic/settings/voltage_ref"),         state.picSettings.sVoltageRef);
}

//===================[ sendPICBootCommands ]=====================
void sendPICBootCommands(){
  if (!isPICEnabled()) return;
  if (!settings.picBoot.bEnable) return;
  OTDebugTf(PSTR("OTGW boot message = [%s]\r\n"), CSTR(settings.picBoot.sCommands));

  // parse and execute commands
  char bootcmds[129];
  strlcpy(bootcmds, settings.picBoot.sCommands, sizeof(bootcmds));
  
  char* cmd;
  int i = 0;
  cmd = strtok(bootcmds, ";");
  while (cmd != NULL) {
    size_t cmdLen = strlen(cmd);
    // Validate alphabetic prefix (same check as handleCommandSubmit)
    if (cmdLen >= 3 && cmd[2] == '=' &&
        isalpha((unsigned char)cmd[0]) && isalpha((unsigned char)cmd[1])) {
      OTDebugTf(PSTR("Boot command[%d]: %s\r\n"), i, cmd);
      addCommandToQueue(cmd, cmdLen, true);
    } else {
      OTDebugTf(PSTR("Boot command[%d]: skipped invalid [%s]\r\n"), i, cmd);
    }
    i++;
    cmd = strtok(NULL, ";");
  }
}

//===================[ Watchdog OTGW ]===============================
// Two shapes after the ESP8266 drop (ADR-128) + TWDT re-scope (ADR-135):
//   1. HAS_PIC_WATCHDOG (esp32-classic + combo): the ESP32 Task Watchdog Timer
//      (TWDT) is the ALWAYS-ON primary safety net; the external 0x26 I2C
//      watchdog on the OTGW Classic PCB is an OPTIONAL SECONDARY layer.
//      Original directive (2026-06-14) was "arm + feed UNCONDITIONALLY", on the
//      premise that writing to an absent 0x26 is a harmless NACKed no-op. That
//      premise broke on the ESP32-S3 esp32-hal-i2c-ng driver: a non-responding
//      0x26 logs an [E] i2c_master_transmit error on EVERY write, so the 100ms
//      feed became a continuous log storm (TASK-945, observed on a classic-S3
//      with no 0x26). TASK-946 resolves this by SYMMETRIC presence-gating instead
//      of dropping back to unconditional: initWatchDog probes 0x26 once (retried,
//      below) into s_extWdPresent, and BOTH the arm (WatchDogEnabled) and the
//      feed (feedWatchDog) are gated on it. The asymmetry the directive warned
//      against (feed gated, arm unconditional -> armed-but-unfed -> reset loop)
//      is therefore avoided: a chip we did not detect is never armed, so it can
//      never bite, and the TWDT remains the always-on primary safety net (so a
//      false-negative only loses the optional secondary for that boot, per
//      ADR-135). The DISARM is still sent unconditionally (safe; boot disarm runs
//      before detection). The only PIC-presence probe is reset->ETX in detectPIC().
//   2. !HAS_PIC_WATCHDOG (OTGW32): ESP32 TWDT only — no external chip.
// The TWDT subscribes the loop task only (esp_task_wdt_add(NULL) from the loop
// context); see ADR-135 §"PIC task subscription" for the explicit decision not
// to subscribe the dedicated PIC-UART task.
#if HAS_PIC_WATCHDOG
// esp32-classic + combo: TWDT primary, external 0x26 optional secondary.
#include <esp_task_wdt.h>

// True once the loop task is registered with the TWDT (initWatchDog ran).
// feedWatchDog() is called all through early setup() — long BEFORE initWatchDog()
// (which sits after the network bring-up) — so resetting the TWDT before the
// loop task is subscribed spams "esp_task_wdt_reset: task not found". Guard on
// this flag: no spam, and the TWDT simply isn't armed until it's set up.
static bool s_twdtReady = false;

// TASK-945: runtime presence of the external 0x26 I2C watchdog. The secondary
// feed is gated on this — on the ESP32-S3 esp32-hal-i2c-ng driver a non-responding
// 0x26 logs an [E] i2c_master_transmit error per write instead of NACKing
// silently, so feeding an absent chip every 100ms is a continuous log storm. An
// absent chip cannot bite and the TWDT is primary (ADR-135), so skipping it is safe.
static bool s_extWdPresent = false;

void initWatchDog(char* reasonBuf, size_t reasonSize) {
  if (reasonSize > 0) reasonBuf[0] = '\0';
  OTDebugTln(F("Setup ESP32 Task Watchdog (primary) + external 0x26 (secondary)"));
  Wire.begin(activeI2cSda(), activeI2cScl());  // I2C for OLED/sensors/0x26 (resolved pins)
  // ---- PRIMARY: arm the ESP32 TWDT (loop task) -----------------------------
  const esp_task_wdt_config_t twdtConfig = {
    .timeout_ms = 30000,
    .idle_core_mask = 0,
    .trigger_panic = true,
  };
  // Arduino-ESP32 may have already initialized the TWDT before setup() runs.
  // Try reconfigure first; fall back to init if not yet running.
  esp_err_t err = esp_task_wdt_reconfigure(&twdtConfig);
  if (err == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_init(&twdtConfig);
  }
  // Subscribe the loop task only if not already subscribed (ADR-135).
  if (esp_task_wdt_status(NULL) != ESP_OK) {
    esp_task_wdt_add(NULL);
  }
  s_twdtReady = true;  // from here feedWatchDog() may safely reset the TWDT

  // ---- SECONDARY: probe + read the external 0x26 boot-status (reset-by-WD flag) -
  // Hardware WatchDog is based on:
  // https://github.com/rvdbreemen/ESPEasySlaves/tree/master/TinyI2CWatchdog
  // TASK-945: this same transaction doubles as the presence probe —
  // endTransmission()==0 means the 0x26 ACKed (present). On the ESP32-S3 i2c-ng
  // driver an absent 0x26 does NOT NACK silently; it logs an [E] error. So we
  // detect presence ONCE here and gate the periodic feed (feedWatchDog) on it.
  delay(100);
  // TASK-946: retry the probe a few times. A single-shot probe can false-negative
  // if the I2C bus is briefly busy at init, and a false-negative on a PRESENT
  // chip is the one dangerous case — WatchDogEnabled gates the ARM and feedWatchDog
  // gates the FEED on this flag (symmetric), so a wrong "absent" verdict leaves a
  // real 0x26 disarmed (safe, cannot bite) but loses the secondary WD for that
  // boot. Retries keep that rare.
  s_extWdPresent = false;
  for (uint8_t attempt = 0; attempt < 3 && !s_extWdPresent; attempt++) {
    Wire.beginTransmission(EXT_WD_I2C_ADDRESS);   // OTGW WD address
    Wire.write(0x83);             // command to set pointer
    Wire.write(17);               // pointer value to status byte
    s_extWdPresent = (Wire.endTransmission() == 0);
    if (!s_extWdPresent) delay(5);
  }
  if (s_extWdPresent) {
    Wire.requestFrom((uint8_t)EXT_WD_I2C_ADDRESS, (uint8_t)1);
    if (Wire.available()) {
      byte status = Wire.read();
      if (status & 0x1) {
        OTDebugTln(F("INIT : Reset by external WD!"));
        strlcpy(reasonBuf, "Reset by External WD\r\n", reasonSize);
      }
    }
    OTDebugTln(F("INIT : external 0x26 watchdog present (secondary feed enabled)"));
  } else {
    OTDebugTln(F("INIT : no external 0x26 watchdog responding (secondary feed disabled; TWDT is primary)"));
  }
}

void WatchDogEnabled(byte stateWatchdog) {
  // SECONDARY only (TWDT is primary, ADR-135). Arms/disarms the external 0x26 chip.
  // TASK-946: the ARM is gated on s_extWdPresent. feedWatchDog() likewise only
  // feeds a detected 0x26, so arming a chip we did NOT detect would leave it
  // armed-but-unfed -> it bites -> spurious ESP reset (the exact asymmetry the
  // 2026-06-14 directive warned against). Gating arm AND feed together keeps it
  // safe: an undetected (or false-negative) 0x26 is simply never armed.
  // The DISARM (stateWatchdog==0) is ALWAYS sent — turning a present chip off is
  // safe, an absent chip NACKs harmlessly, and the boot-time disarm must run
  // before detection. (One boot-time NACK is not the 100ms feed spam.)
  if (stateWatchdog && !s_extWdPresent) return;   // never arm an undetected 0x26
  Wire.beginTransmission(EXT_WD_I2C_ADDRESS);   //Nodoshop design uses the hardware WD on I2C, address 0x26
  Wire.write(7);                                //Write to register 7, the action register
  Wire.write(stateWatchdog);                    //1 = armed to reset, 0 = turned off
  Wire.endTransmission();                       //That's all there is...
}

//===[ Feed the WatchDog before it bites! ]===
void feedWatchDog() {
  // Rate-limit to at most every 100ms: bounds I2C bus traffic to the 0x26 chip
  // and the TWDT reset rate during long blocking operations.
  DECLARE_TIMER_MS(timerWD, 100, SKIP_MISSED_TICKS);
  if (DUE(timerWD)) {
    // PRIMARY: reset the ESP32 TWDT (loop task).
    if (s_twdtReady) esp_task_wdt_reset();
    // SECONDARY: feed the external 0x26 chip — ONLY if it was detected at init
    // (TASK-945). Feeding an absent/non-responding 0x26 every 100ms spams the
    // ESP32-S3 i2c-ng [E] log; an absent chip cannot bite anyway.
    if (s_extWdPresent) {
      Wire.beginTransmission(EXT_WD_I2C_ADDRESS);   //Nodoshop design uses the hardware WD on I2C, address 0x26
      Wire.write(0xA5);                             //Feed the dog, before it bites.
      Wire.endTransmission();                       //That's all there is...
    }
  }
  //==== blink LED1 once a second to show we are alive ====
  DECLARE_TIMER_MS(timerWDBlink, 1000, SKIP_MISSED_TICKS);
  if DUE(timerWDBlink) {
    blinkLEDnow(LED1);
  }
}

#else  // !HAS_PIC_WATCHDOG — OTGW32 (ESP32-S3): use ESP32 Task Watchdog Timer
#include <esp_task_wdt.h>

// True once the loop task is registered with the TWDT (initWatchDog ran).
// feedWatchDog() is called all through early setup() — long BEFORE initWatchDog()
// (which sits after the network bring-up) — so resetting the TWDT before the
// task is subscribed spams "esp_task_wdt_reset: task not found" on the console.
// Guard on this flag: no spam, and the WD simply isn't armed until it's set up.
static bool s_twdtReady = false;

void initWatchDog(char* reasonBuf, size_t reasonSize) {
  if (reasonSize > 0) reasonBuf[0] = '\0';
  OTDebugTln(F("Setup ESP32 Task Watchdog"));
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);  // I2C bus for OLED/sensors
  const esp_task_wdt_config_t twdtConfig = {
    .timeout_ms = 30000,
    .idle_core_mask = 0,
    .trigger_panic = true,
  };
  // Arduino-ESP32 may have already initialized the TWDT before setup() runs.
  // Try reconfigure first; fall back to init if not yet running.
  esp_err_t err = esp_task_wdt_reconfigure(&twdtConfig);
  if (err == ESP_ERR_INVALID_STATE) {
    // TWDT not yet initialized — initialize it
    esp_task_wdt_init(&twdtConfig);
  }
  // Subscribe the loop task only if not already subscribed
  if (esp_task_wdt_status(NULL) != ESP_OK) {
    esp_task_wdt_add(NULL);
  }
  s_twdtReady = true;  // from here feedWatchDog() may safely reset the TWDT
}

void WatchDogEnabled(byte stateWatchdog) {
  // ESP32 TWDT is always running; enable/disable is a no-op
  (void)stateWatchdog;
}

void feedWatchDog() {
  DECLARE_TIMER_MS(timerWD, 100, SKIP_MISSED_TICKS);
  if (DUE(timerWD) && s_twdtReady) {
    esp_task_wdt_reset();
  }
  DECLARE_TIMER_MS(timerWDBlink, 1000, SKIP_MISSED_TICKS);
  if DUE(timerWDBlink) {
    blinkLEDnow(LED1);
  }
}
#endif // watchdog shape dispatch (ADR-127 / ADR-135 TWDT re-scope)

//===================[ END Watchdog OTGW ]===============================

//===================[ OpenTherm Data Types & Protocol Helpers ]=========
float OpenthermData_t::f88() {
  float value = (int8_t) valueHB;
  return value + (float)valueLB / 256.0f;
}

void OpenthermData_t::f88(float value) {
  // f8.8 format: signed high byte + unsigned fractional low byte (two's complement)
  int16_t fixed = (int16_t)(value * 256.0f);
  valueHB = (uint8_t)((fixed >> 8) & 0xFF);
  valueLB = (uint8_t)(fixed & 0xFF);
}

uint16_t OpenthermData_t::u16() {
  uint16_t value = valueHB;
  return ((value << 8) + valueLB);
}

void OpenthermData_t::u16(uint16_t value) {
  valueLB = value & 0xFF;
  valueHB = (value >> 8) & 0xFF;
}

int16_t OpenthermData_t::s16() {
  int16_t value = valueHB;
  return ((value << 8) + valueLB);
}

void OpenthermData_t::s16(int16_t value) {
  valueLB = value & 0xFF;
  valueHB = (value >> 8) & 0xFF;
}

//parsing helpers
const char *messageTypeToString(OTLibMessageType message_type)
{
	switch (message_type) {
		case OT_READ_DATA:        return "Read-Data";
		case OT_WRITE_DATA:       return "Write-Data";
		case OT_INVALID_DATA:     return "Invalid-Data";
		case OT_RESERVED:         return "Reserved";
		case OT_READ_ACK:         return "Read-Ack";
		case OT_WRITE_ACK:        return "Write-Ack";
		case OT_DATA_INVALID:     return "Data-Invalid";
		case OT_UNKNOWN_DATA_ID:  return "Unknown-Data-Id";
		default:                  return "Unknown";
	}
}

const char *messageIDToString(OTLibMessageID message_id){
  if (message_id <= OT_MSGID_MAX) {
    PROGMEM_readAnything (&OTmap[message_id], OTlookupitem);
    return OTlookupitem.label;
  } else return "Undefined";}

OTLibMessageType getMessageType(unsigned long message)
{
    OTLibMessageType msg_type = static_cast<OTLibMessageType>((message >> 28) & 7);
    return msg_type;
}

OTLibMessageID getDataID(unsigned long frame)
{
    return (OTLibMessageID)((frame >> 16) & 0xFF);
}

//===================[ Status Bit Query Helpers ]========================
//parsing responses - helper functions
// bit: description [ clear/0, set/1]
// 0: CH enable [ CH is disabled, CH is enabled]
// 1: DHW enable [ DHW is disabled, DHW is enabled]
// 2: Cooling enable [ Cooling is disabled, Cooling is enabled]
// 3: OTC active [OTC not active, OTC is active]
// 4: CH2 enable [CH2 is disabled, CH2 is enabled]
// 5: reserved
// 6: reserved
// 7: reserved

bool isCentralHeatingEnabled() {
	return OTcurrentSystemState.MasterStatus & 0x01;
}

bool isDomesticHotWaterEnabled() {
	return OTcurrentSystemState.MasterStatus & 0x02;
}

bool isCoolingEnabled() {
	return OTcurrentSystemState.MasterStatus & 0x04;
}

bool isOutsideTemperatureCompensationActive() {
	return OTcurrentSystemState.MasterStatus & 0x08;
}

bool isCentralHeating2enabled() {
	return OTcurrentSystemState.MasterStatus & 0x10;
}

//Slave
// bit: description [ clear/0, set/1]
// 0: fault indication [ no fault, fault ]
// 1: CH mode [CH not active, CH active]
// 2: DHW mode [ DHW not active, DHW active]
// 3: Flame status [ flame off, flame on ]
// 4: Cooling status [ cooling mode not active, cooling mode active ]
// 5: CH2 mode [CH2 not active, CH2 active]
// 6: diagnostic indication [no diagnostics, diagnostic event]
// 7: reserved

bool isFaultIndicator() {
	return OTcurrentSystemState.SlaveStatus & 0x01;
}

bool isCentralHeatingActive() {
	return OTcurrentSystemState.SlaveStatus & 0x02;
}

bool isDomesticHotWaterActive() {
	return OTcurrentSystemState.SlaveStatus & 0x04;
}

bool isFlameStatus() {
	return OTcurrentSystemState.SlaveStatus & 0x08;
}

bool isCoolingActive() {
	return OTcurrentSystemState.SlaveStatus & 0x10;
}

bool isCentralHeating2Active() {
	return OTcurrentSystemState.SlaveStatus & 0x20;
}

bool isDiagnosticIndicator() {
	return OTcurrentSystemState.SlaveStatus & 0x40;
}

  //bit: [clear/0, set/1]
  //0: Service request [service not req’d, service required]
  //1: Lockout-reset [ remote reset disabled, rr enabled]
  //2: Low water press [ no WP fault, water pressure fault]
  //3: Gas/flame fault [ no G/F fault, gas/flame fault ]
  //4: Air press fault [ no AP fault, air pressure fault ]
  //5: Water over-temp[ no OvT fault, over-temperat. Fault]
  //6: reserved
  //7: reserved

bool isServiceRequest() {
	return OTcurrentSystemState.ASFflags & 0x0100;
}

bool isLockoutReset() {
	return OTcurrentSystemState.ASFflags & 0x0200;
}

bool isLowWaterPressure() {
	return OTcurrentSystemState.ASFflags & 0x0400;
}

bool isGasFlameFault() {
	return OTcurrentSystemState.ASFflags & 0x0800;
}

bool isAirTemperature() {
	return OTcurrentSystemState.ASFflags & 0x1000;
}

bool isWaterOverTemperature() {
	return OTcurrentSystemState.ASFflags & 0x2000;
}

const char *byte_to_binary(int x)
{
    static char b[9];
    b[0] = '\0';

    int z;
    for (z = 128; z > 0; z >>= 1) {
        strlcat(b, ((x & z) == z) ? "1" : "0", sizeof(b));
    }

    return b;
}  //byte_to_binary


/*
  This determines if the value in the OpenTherm message is valid and can be used in the data object, MQTT or REST API.
  Rules are:
  - if the message is overriden (R and A messages override B and T messages), then the value is not valid for use.
  - if the OT message is a READ message, and the received OT msg is being read and acknowledged, then the value is valid.
  - if the OT message is a WRITE message, and the received OT msg is being written (OT_WRITE_DATA), then the value is valid.
  - if the OT message is a READ/WRITE message, and receive OT msg is being read and ackownledge, or, is being written, then the value is valid.
  - if the OT message is a status message (from Heating, HAVC or Solar), then the message is always valid.
*/
bool is_value_valid(OpenthermData_t OT, OTlookup_t OTlookup) {
  if (OT.skipthis) return false;
  if (isMsgIdReservedInActiveProfile(OT.id)) return false;
  bool _valid = false;
  _valid = _valid || (OTlookup.msgcmd==OT_READ && OT.type==OT_READ_ACK);
  _valid = _valid || (OTlookup.msgcmd==OT_WRITE && (OT.type==OT_WRITE_DATA || OT.type==OT_WRITE_ACK));
  _valid = _valid || (OTlookup.msgcmd==OT_RW && (OT.type==OT_READ_ACK || OT.type==OT_WRITE_DATA || OT.type==OT_WRITE_ACK));
  _valid = _valid || (OT.id==OT_Statusflags) || (OT.id==OT_StatusVH) || (OT.id==OT_SolarStorageMaster);;
  return _valid;
}

// ADR-097: Master-topic validity check. Mirrors is_value_valid but excludes
// WRITE-ACK for OT_WRITE / OT_RW messages.
//
// ADR-096 refines the canonical interpretation from "thermostat-side intent"
// to "boiler-side worldview" (= the value that was actually transmitted to
// the boiler, including any gateway override). Two additional gates implement
// that shift:
//   - OTGW_ANSWER_THERMOSTAT (A) frames are gateway-faked answers TO the
//     thermostat; they never reach the boiler-side. Suppress canonical for A.
//   - OTGW_THERMOSTAT (T) frames flagged bGatewaySubstituted=true did not
//     reach the boiler (R replaced them). Suppress canonical for those T's;
//     the corresponding R frame will populate canonical.
// B frames (boiler responses) always publish to canonical regardless of
// answer-substitution: B IS the boiler-side reality even when the gateway
// fakes a different answer to the thermostat.
//
// Source-separated subtopics still use the broader is_value_valid; routing
// across /thermostat vs /boiler is decided inside publishToSourceTopic() per
// the ADR-096 worldview rules.
//
// See ADR-097 + docs/api/MQTT-message-id-echo-audit.md for the per-MsgID
// Write-Ack classification rationale (preserved by this ADR).
bool is_value_valid_for_master_topic(OpenthermData_t OT, OTlookup_t OTlookup) {
  if (OT.skipthis) return false;
  if (isMsgIdReservedInActiveProfile(OT.id)) return false;
  // ADR-096/ADR-103 canonical = boiler-side worldview gates:
  // ADR-103: only an answer-override A (a genuine B owns canonical) is blocked. A proxy A
  // (no preceding B — e.g. MaxTSet/57) IS the boiler-side value and reaches canonical.
  if (OT.rsptype == OTGW_ANSWER_THERMOSTAT && OT.bAnswerOverride) return false;
  if (OT.rsptype == OTGW_THERMOSTAT && OT.bGatewaySubstituted) return false;
  bool _valid = false;
  _valid = _valid || (OTlookup.msgcmd==OT_READ && OT.type==OT_READ_ACK);
  _valid = _valid || (OTlookup.msgcmd==OT_WRITE && OT.type==OT_WRITE_DATA);
  _valid = _valid || (OTlookup.msgcmd==OT_RW && (OT.type==OT_READ_ACK || OT.type==OT_WRITE_DATA));
  _valid = _valid || (OT.id==OT_Statusflags) || (OT.id==OT_StatusVH) || (OT.id==OT_SolarStorageMaster);
  return _valid;
}

// ADR-097 (PS=1 amendment, TASK-483 ACs #8-#13): The PS=1 summary stream emits
// one value per MsgID, chosen by the PIC from its most recent observation.
// For OT_WRITE / OT_RW MsgIDs whose slave Write-Ack data byte is per-spec
// undefined (bSlaveEchoesValue=false in OTmap[]), the PIC may have captured
// either the meaningful Write-Data or the undefined Write-Ack byte; the PS=1
// stream cannot distinguish these at this layer. For those MsgIDs we suppress
// base-topic publication and state-write so the master-topic invariant holds
// across both the live OT-bus path and the PS=1 path. READ messages are
// always meaningful (slave's Read-Ack carries the value). Status-flag MsgIDs
// (Statusflags / StatusVH) are handled inside ot_flag8flag8 with their own
// per-MsgID switch and are not gated here.
static bool is_msgid_valid_for_master_topic_in_ps_summary(const OTlookup_t &lookup)
{
  if (lookup.msgcmd == OT_READ) return true;
  return lookup.bSlaveEchoesValue;
}

// =====================[ MQTT throttle helpers ]==================
#define CoreMQTTDebugTf(...) ({ if (state.debug.bMQTTGate) DebugTf(__VA_ARGS__); })

static char mqttPublishSourceTag(byte masterslave)
{
  return (masterslave == OT_MSGTYPE_REQUEST) ? 'M' : 'S';
}

static void logMQTTValueGateDecision(byte id,
                                     byte masterslave,
                                     uint8_t idx,
                                     uint16_t previousValue,
                                     uint16_t currentValue,
                                     bool firstSeen,
                                     bool valueChanged,
                                     bool intervalElapsed,
                                     uint16_t lastTime,
                                     uint16_t now,
                                     bool allowPublish,
                                     const __FlashStringHelper *reason)
{
  if (!state.debug.bMQTTGate) return;
  DebugTf(PSTR("MQTT gate id=%u src=%c slot=%u prev=0x%04X curr=0x%04X first=%s changed=%s interval=%s last=%u now=%u => %s [%S]\r\n"),
                  id,
                  mqttPublishSourceTag(masterslave),
                  idx,
                  previousValue,
                  currentValue,
                  CBOOLEAN(firstSeen),
                  CBOOLEAN(valueChanged),
                  CBOOLEAN(intervalElapsed),
                  lastTime,
                  now,
                  allowPublish ? "publish" : "skip",
                  reason);
}

static void logMQTTStatusBitDecision(uint8_t bitSlot,
                                     const char *topic,
                                     bool previousValue,
                                     bool currentValue,
                                     bool forcePublish,
                                     bool allowPublish)
{
  if (!state.debug.bMQTTGate) return;
  if (!allowPublish) return;  // skips are not interesting
  if ((!settings.mqtt.bOnChangePublishing || settings.mqtt.iInterval == 0) && previousValue == currentValue && !forcePublish) return;  // legacy always-publish: only log actual changes
  const char *reason = forcePublish                    ? "force"
                     : (previousValue != currentValue) ? "changed"
                     :                                   "first-seen";
  DebugTf(PSTR("MQTT bit[%u] %s %s->%s [%s]\r\n"),
                  bitSlot,
                  topic,
                  CBOOLEAN(previousValue),
                  CBOOLEAN(currentValue),
                  reason);
}

static bool shouldForceMasterStatusPublish()
{
  return mqttForceNextMasterStatusPublish;
}

static bool shouldForceSlaveStatusPublish()
{
  return mqttForceNextSlaveStatusPublish;
}

static bool shouldForceMasterStatusVHPublish()
{
  return mqttForceNextMasterStatusVHPublish;
}

static bool shouldForceSlaveStatusVHPublish()
{
  return mqttForceNextSlaveStatusVHPublish;
}

static bool hasTrackedTime(uint16_t trackedTime)
{
  return trackedTime != TRACKED_TIME_UNSEEN;
}

static uint16_t getPackedSlotTime(uint32_t packed)
{
  return static_cast<uint16_t>(packed & 0xFFFFU);
}

// ADR-116: on-change publishing is active when the flag is set AND a heartbeat
// interval is configured. Flag false (or interval 0) => legacy always-publish.
// This gates the value-topic publish paths only; the TASK-400 status-bit
// heartbeat (STATUS_HEARTBEAT_INTERVAL_SEC) is independent and unaffected.
static bool mqttOnChangePublishingActive()
{
  return settings.mqtt.bOnChangePublishing && settings.mqtt.iInterval > 0;
}

static void setPackedSlot(uint8_t idx, uint16_t rawValue, uint16_t trackedNow)
{
  mqttlastsent[idx] = (static_cast<uint32_t>(rawValue) << 16) | trackedNow;
}

// Confirm pending throttle slot updates after successful MQTT publish.
// Called from sendMQTTData() on success so the slot is only marked
// "published" when the data actually reached the broker.
void confirmMQTTPublishSlot()
{
  if (!mqttPendingSlot.pending) return;
  if (mqttPendingSlot.isTimeOnly) {
    mqttlastsent[mqttPendingSlot.idx] =
      (mqttlastsent[mqttPendingSlot.idx] & 0xFFFF0000UL) |
      static_cast<uint32_t>(mqttPendingSlot.trackedTime);
  } else {
    setPackedSlot(mqttPendingSlot.idx, mqttPendingSlot.rawValue, mqttPendingSlot.trackedTime);
  }
  mqttPendingSlot.pending = false;
}

void confirmMQTTPublishBitSlot()
{
  if (!mqttPendingBitSlot.pending || !mqttPendingBitSlot.trackedSlots) return;
  mqttPendingBitSlot.trackedSlots[mqttPendingBitSlot.slot] = mqttPendingBitSlot.trackedTime;
  mqttPendingBitSlot.pending = false;
}

void confirmMQTTPublishByteSlot()
{
  if (!mqttPendingByteSlot.pending || !mqttPendingByteSlot.trackedSlots) return;
  mqttPendingByteSlot.trackedSlots[mqttPendingByteSlot.slot] = mqttPendingByteSlot.trackedTime;
  mqttPendingByteSlot.pending = false;
}

void requestMQTTRepublishAll()
{
  resetMqttTrackedState();
  requestMQTTStatusRepublish();
}

void requestMQTTStatusRepublish()
{
  mqttForceNextMasterStatusPublish = true;
  mqttForceNextSlaveStatusPublish = true;
  mqttForceNextMasterStatusVHPublish = true;
  mqttForceNextSlaveStatusVHPublish = true;
}

// shouldPublishMQTTForID - returns true if this OT slot's value should be
// published now (value changed OR interval elapsed). Takes explicit rawValue
// so the caller controls which value is compared — normal OT mode passes
// OTdata.value; PS=1 mode passes 0 to rely on interval-only gating. (ADR-006)
bool shouldPublishMQTTForID(byte id, byte masterslave, uint16_t rawValue) {
  if (id == OT_Statusflags || id == OT_StatusVH) {
    CoreMQTTDebugTf(PSTR("MQTT gate id=%u src=%c curr=0x%04X => publish [delegated to status-byte/bit gates]\r\n"),
                    id,
                    mqttPublishSourceTag(masterslave),
                    rawValue);
    return true; // status uses dedicated combined-byte and per-bit gates
  }
  // IDs 128-255 (manufacturer-specific/Remeha) wrap when offset +128, aliasing
  // with critical RESPONSE slots (Status flags, TSet…). Always publish to avoid
  // cross-slot throttle contamination. (ADR-006)
  if (id > 127) {
    CoreMQTTDebugTf(PSTR("MQTT gate id=%u src=%c prev=%s curr=0x%04X => publish [passthrough id>127]\r\n"),
                    id,
                    mqttPublishSourceTag(masterslave),
                    "untracked",
                    rawValue);
    return true;
  }
  uint8_t idx = 0;
  if (!tryGetTrackedSlotIndex(id, masterslave, idx)) return true;
  uint32_t packed = mqttlastsent[idx];
  const bool firstSeen = !hasTrackedTime(getPackedSlotTime(packed));
  uint16_t lastVal  = (uint16_t)(packed >> 16);             // bits 31-16: last published u16
  uint16_t lastTime = getPackedSlotTime(packed);            // bits 15-0: rolling seconds-since-boot
  uint16_t now      = currentTrackedSeconds();
  if (!mqttOnChangePublishingActive()) {
    logMQTTValueGateDecision(id, masterslave, idx, lastVal, rawValue, firstSeen, rawValue != lastVal, false, lastTime, now, true,
                             settings.mqtt.bOnChangePublishing ? F("interval=0") : F("on-change disabled"));
    return true;   // legacy: always publish
  }
  bool valueChanged    = (rawValue != lastVal);
  bool intervalElapsed = !firstSeen && (elapsedTrackedSeconds(now, lastTime) >= settings.mqtt.iInterval);
  const bool allowPublish = firstSeen || valueChanged || intervalElapsed;
  logMQTTValueGateDecision(id, masterslave, idx, lastVal, rawValue, firstSeen, valueChanged, intervalElapsed, lastTime, now, allowPublish,
                           allowPublish ? F("tracked update") : F("suppressed by interval"));
  if (allowPublish) {
    // Defer slot update until sendMQTTData confirms successful publish.
    // If publish fails, the slot retains its previous state so the value
    // will be retried on the next observation.
    mqttPendingSlot = {idx, rawValue, now, true, false};
    return true;
  }
  return false;
}

// shouldPublishMQTTForPSField - interval-only gate for PS=1 summary fields.
// PS=1 mode does not carry a raw OT uint16 (values are pre-decoded ASCII),
// so change-detection uses only the time dimension. Shares the mqttlastsent[]
// array with normal OT mode (response slot, masterslave=0) so both paths
// respect the same per-ID interval regardless of which mode is active. (ADR-006)
bool shouldPublishMQTTForPSField(byte id) {
  if (!mqttOnChangePublishingActive()) {
    CoreMQTTDebugTf(PSTR("MQTT gate PS id=%u => publish [%s]\r\n"),
                    id, settings.mqtt.bOnChangePublishing ? "interval=0" : "on-change disabled");
    return true;
  }
  if (id > 127) {
    CoreMQTTDebugTf(PSTR("MQTT gate PS id=%u => publish [passthrough id>127]\r\n"), id);
    return true;
  }
  if (id == OT_Statusflags || id == OT_StatusVH) {
    CoreMQTTDebugTf(PSTR("MQTT gate PS id=%u => publish [delegated to status-byte/bit gates]\r\n"), id);
    return true; // status uses dedicated combined-byte and per-bit gates
  }
  uint8_t idx = 0;
  if (!tryGetTrackedSlotIndex(id, 0, idx)) return true;
  uint16_t lastTime = getPackedSlotTime(mqttlastsent[idx]);
  const bool firstSeen = !hasTrackedTime(lastTime);
  uint16_t now      = currentTrackedSeconds();
  const bool intervalElapsed = !firstSeen && (elapsedTrackedSeconds(now, lastTime) >= settings.mqtt.iInterval);
  const bool allowPublish = firstSeen || intervalElapsed;
  CoreMQTTDebugTf(PSTR("MQTT gate PS id=%u slot=%u first=%s interval=%s last=%u now=%u => %s\r\n"),
                  id,
                  idx,
                  CBOOLEAN(firstSeen),
                  CBOOLEAN(intervalElapsed),
                  lastTime,
                  now,
                  allowPublish ? "publish" : "skip");
  if (allowPublish) {
    mqttPendingSlot = {idx, 0, now, true, true}; // isTimeOnly for PS=1
    return true;
  }
  return false;
}

// shouldPublishStatusBit - per-bit publish decision for OT_Statusflags.
// TASK-400: pure change-detection with a fixed 60-second heartbeat.
//   - forcePublish (boot reset-flag): publish once to establish state
//   - firstSeen (per-bit sentinel): publish the very first value per bit
//   - valueChanged: publish only when the bit actually flipped
//   - 60-second heartbeat (STATUS_HEARTBEAT_INTERVAL_SEC): republish if no
//     change for ≥60s, so HA has recent state after MQTT broker reconnect
// Behaviour change vs pre-TASK-400: previously settings.mqtt.iInterval
// governed the heartbeat AND iInterval==0 forced publish on every frame
// (causing 160 MQTT publishes/sec on Status frames). Status bits now use
// a dedicated 60s constant, independent of iInterval — other topic throttles
// continue to honour iInterval.
static bool shouldPublishTrackedStatusBit(uint16_t *trackedSlots, uint8_t bitSlot, bool newVal, bool prevVal, bool forcePublish) {
  mqttPendingBitSlot.pending = false; // discard unconfirmed pending from prior call
  const uint16_t lastTime = trackedSlots[bitSlot];
  const bool firstSeen = !hasTrackedTime(lastTime);
  const uint16_t now = currentTrackedSeconds();
  // Change-detect has absolute priority — bit-flips publish immediately.
  // Excludes firstSeen (handled below as a first-seen publish, not a "flip").
  const bool valueChanged = !firstSeen && (newVal != prevVal);
  if (valueChanged) {
    mqttPendingBitSlot = {trackedSlots, bitSlot, now, true};
    return true;
  }
  const bool intervalElapsed = !firstSeen
                             && (elapsedTrackedSeconds(now, lastTime) >= STATUS_HEARTBEAT_INTERVAL_SEC);
  if (firstSeen || forcePublish || intervalElapsed) {
    // ADR-104 (2.0.0 sibling of dev's ADR-076): per-slot heartbeat only. No
    // global cross-slot spacing — that was TASK-402's MQTT_GATED_PUBLISH_
    // SPACING_MS and it starved the per-bit slots when the combined byte won
    // the token at every tick.
    mqttPendingBitSlot = {trackedSlots, bitSlot, now, true};
    return true;
  }
  return false;
}

bool shouldPublishStatusBit(uint8_t bitSlot, bool newVal, bool prevVal, bool forcePublish) {
  return shouldPublishTrackedStatusBit(mqttlastsentstatusbit, bitSlot, newVal, prevVal, forcePublish);
}

static bool shouldPublishStatusVHBit(uint8_t bitSlot, bool newVal, bool prevVal, bool forcePublish)
{
  return shouldPublishTrackedStatusBit(mqttlastsentstatusvhbit, bitSlot, newVal, prevVal, forcePublish);
}

// TASK-400: same rules as shouldPublishTrackedStatusBit — the status_master
// and status_slave combined-byte topics (and Status VH equivalents) also use
// the hardcoded 60-second heartbeat, independent of settings.mqtt.iInterval.
static bool shouldPublishTrackedStatusByte(uint16_t *trackedSlots, uint8_t byteSlot, uint8_t newVal, uint8_t prevVal, bool forcePublish)
{
  mqttPendingByteSlot.pending = false; // discard unconfirmed pending from prior call
  const uint16_t lastTime = trackedSlots[byteSlot];
  const bool firstSeen = !hasTrackedTime(lastTime);
  const uint16_t now = currentTrackedSeconds();
  // Change-detect priority — byte changed -> immediate publish.
  const bool valueChanged = !firstSeen && (newVal != prevVal);
  if (valueChanged) {
    mqttPendingByteSlot = {trackedSlots, byteSlot, now, true};
    return true;
  }
  const bool intervalElapsed = !firstSeen
                             && (elapsedTrackedSeconds(now, lastTime) >= STATUS_HEARTBEAT_INTERVAL_SEC);
  if (firstSeen || forcePublish || intervalElapsed) {
    // ADR-104 (2.0.0 sibling of dev's ADR-076): per-slot heartbeat only — see
    // shouldPublishTrackedStatusBit() for the rationale on removing the global
    // rate-gate.
    mqttPendingByteSlot = {trackedSlots, byteSlot, now, true};
    return true;
  }
  return false;
}

static bool shouldPublishStatusByte(uint8_t byteSlot, uint8_t newVal, uint8_t prevVal, bool forcePublish)
{
  return shouldPublishTrackedStatusByte(mqttlastsentstatusbyte, byteSlot, newVal, prevVal, forcePublish);
}

static bool shouldPublishStatusVHByte(uint8_t byteSlot, uint8_t newVal, uint8_t prevVal, bool forcePublish)
{
  return shouldPublishTrackedStatusByte(mqttlastsentstatusvhbyte, byteSlot, newVal, prevVal, forcePublish);
}

// publishStatusBitMQTT - publish a status bit with per-bit change-detect + interval.
// Uses OTPublishGate (RAII) so the outer gate state is always restored even if
// publishMQTTOnOff() or any callee throws or early-returns. (ADR-006)
void publishStatusBitMQTT(uint8_t bitSlot, const char* topic, bool newVal, bool prevVal,
                          bool forcePublish, uint8_t previousBits, uint8_t currentBits,
                          const char* aliasLabel = nullptr) {
  const bool allowPublish = shouldPublishStatusBit(bitSlot, newVal, prevVal, forcePublish);
  // ADR-106: pick exactly one label — alias in new mode (default), legacy in legacy mode.
  // If aliasLabel is nullptr the bit has no alias replacement and topic always wins.
  const char* labelToPublish = (aliasLabel && !settings.mqtt.bUseLegacyOtTopics) ? aliasLabel : topic;
  logMQTTStatusBitDecision(bitSlot, labelToPublish, prevVal, newVal, forcePublish, allowPublish);
  OTPublishGate gate(allowPublish);
  if (allowPublish) incrementStatusBurstPublishCount();  // TASK-347: arm cooldown only for real sends
  // ADR-104: commit pending bit-slot only when sendMQTTData confirms success.
  if (publishMQTTOnOff(labelToPublish, newVal)) confirmMQTTPublishBitSlot();
  else                                          mqttPendingBitSlot.pending = false;
}

static void publishStatusVHBitMQTT(uint8_t bitSlot, const char* topic, bool newVal, bool prevVal,
                                   bool forcePublish, uint8_t previousBits, uint8_t currentBits,
                                   const char* aliasLabel = nullptr)
{
  const bool allowPublish = shouldPublishStatusVHBit(bitSlot, newVal, prevVal, forcePublish);
  // ADR-106: pick exactly one label.
  const char* labelToPublish = (aliasLabel && !settings.mqtt.bUseLegacyOtTopics) ? aliasLabel : topic;
  logMQTTStatusBitDecision(bitSlot, labelToPublish, prevVal, newVal, forcePublish, allowPublish);
  OTPublishGate gate(allowPublish);
  if (allowPublish) incrementStatusBurstPublishCount();  // TASK-354: arm cooldown only for real sends
  if (publishMQTTOnOff(labelToPublish, newVal)) confirmMQTTPublishBitSlot();
  else                                          mqttPendingBitSlot.pending = false;
}

// TASK-401: generic gate-wrapped publish helpers for non-Status fan-out
// (msgId 5 ASF, msgId 6 RBP, msgId 100 Remote Override). Reuse the Status
// gate (shouldPublishTrackedStatusBit/Byte) so heartbeat semantics match
// msgId 0. No status-burst cooldown here — that mechanism is scoped to the
// high-frequency msgId 0 flow and these sites publish an order of magnitude
// less often.
static void publishGatedBitMQTT(uint16_t *trackedSlots, uint8_t bitSlot,
                                const __FlashStringHelper *topic,
                                bool newVal, bool prevVal,
                                const __FlashStringHelper *aliasLabel = nullptr)
{
  const bool allowPublish = shouldPublishTrackedStatusBit(trackedSlots, bitSlot, newVal, prevVal, /*forcePublish=*/false);
  // ADR-106: pick exactly one label — alias in new mode (default), legacy in legacy mode.
  const __FlashStringHelper *labelToPublish =
      (aliasLabel && !settings.mqtt.bUseLegacyOtTopics) ? aliasLabel : topic;
  OTPublishGate gate(allowPublish);
  // ADR-104: commit pending only when sendMQTTData confirms success.
  if (publishMQTTOnOff(labelToPublish, newVal)) confirmMQTTPublishBitSlot();
  else                                          mqttPendingBitSlot.pending = false;
}

static void publishGatedByteMQTT(uint16_t *trackedSlots, uint8_t byteSlot,
                                 const __FlashStringHelper *topic, const char *payload,
                                 uint8_t newVal, uint8_t prevVal)
{
  const bool allowPublish = shouldPublishTrackedStatusByte(trackedSlots, byteSlot, newVal, prevVal, /*forcePublish=*/false);
  OTPublishGate gate(allowPublish);
  // ADR-104: commit pending only when sendMQTTData confirms success.
  if (sendMQTTData(topic, payload)) confirmMQTTPublishByteSlot();
  else                              mqttPendingByteSlot.pending = false;
}

// Overload for dynamically-built char* topics (e.g. "<msgid>_flag8").
static void publishGatedByteMQTT(uint16_t *trackedSlots, uint8_t byteSlot,
                                 const char *topic, const char *payload,
                                 uint8_t newVal, uint8_t prevVal)
{
  const bool allowPublish = shouldPublishTrackedStatusByte(trackedSlots, byteSlot, newVal, prevVal, /*forcePublish=*/false);
  OTPublishGate gate(allowPublish);
  // ADR-104: commit pending only when sendMQTTData confirms success.
  if (sendMQTTData(topic, payload)) confirmMQTTPublishByteSlot();
  else                              mqttPendingByteSlot.pending = false;
}

static void copyBinaryByteString(uint8_t value, char *dest, size_t destSize)
{
  if (!dest || destSize == 0) return;
  strlcpy(dest, byte_to_binary(value), destSize);
}

static void buildStatusMasterText(uint8_t valueHB, char *statusText, size_t statusTextSize)
{
  if (!statusText || statusTextSize < 9) return;
  statusText[0] = ((valueHB & 0x01) ? 'C' : '-');
  statusText[1] = ((valueHB & 0x02) ? 'D' : '-');
  statusText[2] = ((valueHB & 0x04) ? 'C' : '-');
  statusText[3] = ((valueHB & 0x08) ? 'O' : '-');
  statusText[4] = ((valueHB & 0x10) ? '2' : '-');
  statusText[5] = ((valueHB & 0x20) ? 'S' : 'W');
  statusText[6] = ((valueHB & 0x40) ? 'B' : '-');
  statusText[7] = ((valueHB & 0x80) ? '.' : '-');
  statusText[8] = '\0';
}

static void buildStatusSlaveText(uint8_t valueLB, char *statusText, size_t statusTextSize)
{
  if (!statusText || statusTextSize < 9) return;
  statusText[0] = ((valueLB & 0x01) ? 'E' : '-');
  statusText[1] = ((valueLB & 0x02) ? 'C' : '-');
  statusText[2] = ((valueLB & 0x04) ? 'W' : '-');
  statusText[3] = ((valueLB & 0x08) ? 'F' : '-');
  statusText[4] = ((valueLB & 0x10) ? 'C' : '-');
  statusText[5] = ((valueLB & 0x20) ? '2' : '-');
  statusText[6] = ((valueLB & 0x40) ? 'D' : '-');
  statusText[7] = ((valueLB & 0x80) ? 'P' : '-');
  statusText[8] = '\0';
}

// GH #665 / TASK-939 (port of 1.x 7b2d3cdd): HA-climate hvac_mode + hvac_action
// for a unified heat/cool/off thermostat entity. hvac_mode follows the MASTER
// enable bits (the thermostat's intent), hvac_action the SLAVE actual bits (what
// the appliance is doing). Mode is reflective: the thermostat owns heat/cool
// switching, the OTGW only mirrors it. Both report "off" when the thermostat is
// disconnected. Published on change, and on force (reconnect/interval).
static int8_t mqttLastHvacMode   = -1;  // -1 unset, 0 off, 1 heat, 2 cool
static int8_t mqttLastHvacAction = -1;  // -1 unset, 0 off, 1 idle, 2 heating, 3 cooling

static void publishHvacMode(bool forcePublish)
{
  // mode is the thermostat's standing intent, not its momentary demand: a
  // connected heating thermostat stays "heat" even while satisfied/idle (gas
  // boilers clear ch_enable between calls). Only the master's cooling_enable bit
  // flips it to "cool"; "off" is reserved for a disconnected thermostat. (GH #665)
  const uint8_t hb = OTcurrentSystemState.MasterStatus;
  const int8_t mode = !state.otBus.bThermostatState ? 0   // off when no thermostat
                    : (hb & 0x04)                   ? 2   // cooling_enable -> cool
                    :                                 1;  // connected, not cooling -> heat
  if (forcePublish || mode != mqttLastHvacMode) {
    sendMQTTData("hvac_mode", mode == 2 ? "cool" : mode == 1 ? "heat" : "off");
    mqttLastHvacMode = mode;
  }
}

static void publishHvacAction(bool forcePublish)
{
  const uint8_t lb = OTcurrentSystemState.SlaveStatus;
  const int8_t action = !state.otBus.bThermostatState ? 0   // off when no thermostat
                      : (lb & 0x10)                   ? 3   // cooling        -> cooling
                      : (lb & 0x02)                   ? 2   // centralheating -> heating
                      :                                 1;  // else           -> idle
  if (forcePublish || action != mqttLastHvacAction) {
    sendMQTTData("hvac_action",
                 action == 3 ? "cooling" : action == 2 ? "heating" : action == 1 ? "idle" : "off");
    mqttLastHvacAction = action;
  }
}

static void publishMasterStatusState(uint8_t valueHB, const char *statusText)
{
  const uint8_t previousStatus = OTcurrentSystemState.MasterStatus;
  const bool forcePublish = shouldForceMasterStatusPublish();
  const bool publishCombined = shouldPublishStatusByte(0, valueHB, previousStatus, forcePublish);
  if (state.debug.bMQTTGate) {
    const bool logWorthy = forcePublish || (valueHB != previousStatus) || mqttOnChangePublishingActive();
    if (logWorthy) {
      char previousBitsText[9] {0};
      char currentBitsText[9] {0};
      copyBinaryByteString(previousStatus, previousBitsText, sizeof(previousBitsText));
      copyBinaryByteString(valueHB, currentBitsText, sizeof(currentBitsText));
      const char *reason = forcePublish                ? "force"
                         : (valueHB != previousStatus) ? "changed"
                         : publishCombined             ? "interval"
                         :                              "no-change";
      DebugTf(PSTR("MQTT gate status_master 0x%02X[%s]->0x%02X[%s] => %s[%s]\r\n"),
              previousStatus, previousBitsText, valueHB, currentBitsText,
              publishCombined ? "publish" : "skip", reason);
    }
  }
  OTcurrentSystemState.MasterStatus = valueHB;
  mqttForceNextMasterStatusPublish = false;
  // Suppress MQTT discovery drip during this sub-topic fanout (TASK-342)
  // and arm post-burst cooldown on real sends (TASK-347).
  beginStatusBurst();
  {
    OTPublishGate gate(publishCombined);
    if (publishCombined) incrementStatusBurstPublishCount();
    // ADR-104: commit pending byte-slot only when sendMQTTData confirms success.
    if (sendMQTTData("status_master", statusText)) confirmMQTTPublishByteSlot();
    else                                           mqttPendingByteSlot.pending = false;
  }
  publishStatusBitMQTT(0, "ch_enable",        (valueHB & 0x01), (previousStatus & 0x01), forcePublish, previousStatus, valueHB);
  publishStatusBitMQTT(1, "dhw_enable",       (valueHB & 0x02), (previousStatus & 0x02), forcePublish, previousStatus, valueHB);
  publishStatusBitMQTT(2, "cooling_enable",   (valueHB & 0x04), (previousStatus & 0x04), forcePublish, previousStatus, valueHB);
  publishStatusBitMQTT(3, "otc_active",       (valueHB & 0x08), (previousStatus & 0x08), forcePublish, previousStatus, valueHB);
  publishStatusBitMQTT(4, "ch2_enable",       (valueHB & 0x10), (previousStatus & 0x10), forcePublish, previousStatus, valueHB);
  publishStatusBitMQTT(5, "summerwintertime", (valueHB & 0x20), (previousStatus & 0x20), forcePublish, previousStatus, valueHB);
  publishStatusBitMQTT(6, "dhw_blocking",     (valueHB & 0x40), (previousStatus & 0x40), forcePublish, previousStatus, valueHB);
  publishHvacMode(forcePublish);   // GH #665: HA-climate hvac_mode from the master enable bits
  endStatusBurst();
}

static void publishSlaveStatusState(uint8_t valueLB, const char *statusText)
{
  const uint8_t previousStatus = OTcurrentSystemState.SlaveStatus;
  const bool forcePublish = shouldForceSlaveStatusPublish();
  const bool publishCombined = shouldPublishStatusByte(1, valueLB, previousStatus, forcePublish);
  if (state.debug.bMQTTGate) {
    const bool logWorthy = forcePublish || (valueLB != previousStatus) || mqttOnChangePublishingActive();
    if (logWorthy) {
      char previousBitsText[9] {0};
      char currentBitsText[9] {0};
      copyBinaryByteString(previousStatus, previousBitsText, sizeof(previousBitsText));
      copyBinaryByteString(valueLB, currentBitsText, sizeof(currentBitsText));
      const char *reason = forcePublish                ? "force"
                         : (valueLB != previousStatus) ? "changed"
                         : publishCombined             ? "interval"
                         :                              "no-change";
      DebugTf(PSTR("MQTT gate status_slave 0x%02X[%s]->0x%02X[%s] => %s[%s]\r\n"),
              previousStatus, previousBitsText, valueLB, currentBitsText,
              publishCombined ? "publish" : "skip", reason);
    }
  }
  OTcurrentSystemState.SlaveStatus = valueLB;
  mqttForceNextSlaveStatusPublish = false;
  // Suppress MQTT discovery drip during this sub-topic fanout (TASK-342)
  // and arm post-burst cooldown on real sends (TASK-347).
  beginStatusBurst();
  {
    OTPublishGate gate(publishCombined);
    if (publishCombined) incrementStatusBurstPublishCount();
    // ADR-104: commit pending byte-slot only when sendMQTTData confirms success.
    if (sendMQTTData("status_slave", statusText)) confirmMQTTPublishByteSlot();
    else                                          mqttPendingByteSlot.pending = false;
  }
  publishStatusBitMQTT(8,  "fault",                (valueLB & 0x01), (previousStatus & 0x01), forcePublish, previousStatus, valueLB, "fault_indication");
  publishStatusBitMQTT(9,  "centralheating",       (valueLB & 0x02), (previousStatus & 0x02), forcePublish, previousStatus, valueLB, "central_heating");
  publishStatusBitMQTT(10, "domestichotwater",     (valueLB & 0x04), (previousStatus & 0x04), forcePublish, previousStatus, valueLB, "hot_water");
  publishStatusBitMQTT(11, "flame",                (valueLB & 0x08), (previousStatus & 0x08), forcePublish, previousStatus, valueLB);
  publishStatusBitMQTT(12, "cooling",              (valueLB & 0x10), (previousStatus & 0x10), forcePublish, previousStatus, valueLB);
  publishStatusBitMQTT(13, "centralheating2",      (valueLB & 0x20), (previousStatus & 0x20), forcePublish, previousStatus, valueLB, "central_heating_2");
  publishStatusBitMQTT(14, "diagnostic_indicator", (valueLB & 0x40), (previousStatus & 0x40), forcePublish, previousStatus, valueLB, "diagnostic_indication");
  publishStatusBitMQTT(15, "electric_production",  (valueLB & 0x80), (previousStatus & 0x80), forcePublish, previousStatus, valueLB);
  publishHvacAction(forcePublish);   // GH #665: HA-climate hvac_action from the slave actual bits
  endStatusBurst();
}

static uint16_t publishCombinedStatusState(uint8_t valueHB, uint8_t valueLB)
{
  char masterStatus[9] {0};
  char slaveStatus[9] {0};
  buildStatusMasterText(valueHB, masterStatus, sizeof(masterStatus));
  buildStatusSlaveText(valueLB, slaveStatus, sizeof(slaveStatus));
  // Status-burst quiesce is inside publishMasterStatusState/publishSlaveStatusState
  // so the individual-side callers (lines 1871 and 1899 of this file) also benefit.
  publishMasterStatusState(valueHB, masterStatus);
  publishSlaveStatusState(valueLB, slaveStatus);
  return (OTcurrentSystemState.MasterStatus << 8) | OTcurrentSystemState.SlaveStatus;
}

static void buildStatusVHMasterText(uint8_t valueHB, char *statusText, size_t statusTextSize)
{
  if (!statusText || statusTextSize < 9) return;
  statusText[0] = ((valueHB & 0x01) ? 'V' : '-');
  statusText[1] = ((valueHB & 0x02) ? 'P' : '-');
  statusText[2] = ((valueHB & 0x04) ? 'M' : '-');
  statusText[3] = ((valueHB & 0x08) ? 'F' : '-');
  statusText[4] = ((valueHB & 0x10) ? '.' : '-');
  statusText[5] = ((valueHB & 0x20) ? '.' : '-');
  statusText[6] = ((valueHB & 0x40) ? '.' : '-');
  statusText[7] = ((valueHB & 0x80) ? '.' : '-');
  statusText[8] = '\0';
}

static void buildStatusVHSlaveText(uint8_t valueLB, char *statusText, size_t statusTextSize)
{
  if (!statusText || statusTextSize < 9) return;
  statusText[0] = ((valueLB & 0x01) ? 'F' : '-');
  statusText[1] = ((valueLB & 0x02) ? 'V' : '-');
  statusText[2] = ((valueLB & 0x04) ? 'P' : '-');
  statusText[3] = ((valueLB & 0x08) ? 'A' : '-');
  statusText[4] = ((valueLB & 0x10) ? 'F' : '-');
  statusText[5] = ((valueLB & 0x20) ? '.' : '-');
  statusText[6] = ((valueLB & 0x40) ? 'D' : '-');
  statusText[7] = ((valueLB & 0x80) ? '.' : '-');
  statusText[8] = '\0';
}

static void publishMasterStatusVHState(uint8_t valueHB, const char *statusText)
{
  const uint8_t previousStatus = OTcurrentSystemState.MasterStatusVH;
  const bool forcePublish = shouldForceMasterStatusVHPublish();
  const bool publishCombined = shouldPublishStatusVHByte(0, valueHB, previousStatus, forcePublish);
  if (state.debug.bMQTTGate) {
    const bool logWorthy = forcePublish || (valueHB != previousStatus) || mqttOnChangePublishingActive();
    if (logWorthy) {
      char previousBitsText[9] {0};
      char currentBitsText[9] {0};
      copyBinaryByteString(previousStatus, previousBitsText, sizeof(previousBitsText));
      copyBinaryByteString(valueHB, currentBitsText, sizeof(currentBitsText));
      const char *reason = forcePublish                ? "force"
                         : (valueHB != previousStatus) ? "changed"
                         : publishCombined             ? "interval"
                         :                              "no-change";
      DebugTf(PSTR("MQTT gate status_vh_master 0x%02X[%s]->0x%02X[%s] => %s[%s]\r\n"),
              previousStatus, previousBitsText, valueHB, currentBitsText,
              publishCombined ? "publish" : "skip", reason);
    }
  }
  OTcurrentSystemState.MasterStatusVH = valueHB;
  mqttForceNextMasterStatusVHPublish = false;
  // Suppress MQTT discovery drip during this sub-topic fanout (TASK-342/354)
  // and arm post-burst cooldown on real sends (TASK-347/354).
  beginStatusBurst();
  {
    OTPublishGate gate(publishCombined);
    if (publishCombined) incrementStatusBurstPublishCount();
    // ADR-104: commit pending byte-slot only when sendMQTTData confirms success.
    if (sendMQTTData(F("status_vh_master"), statusText)) confirmMQTTPublishByteSlot();
    else                                                 mqttPendingByteSlot.pending = false;
  }
  publishStatusVHBitMQTT(0, "vh_ventilation_enabled",    (valueHB & 0x01), (previousStatus & 0x01), forcePublish, previousStatus, valueHB, "ventilation_enabled");
  publishStatusVHBitMQTT(1, "vh_bypass_position",        (valueHB & 0x02), (previousStatus & 0x02), forcePublish, previousStatus, valueHB, "ventilation_bypass_position");
  publishStatusVHBitMQTT(2, "vh_bypass_mode",            (valueHB & 0x04), (previousStatus & 0x04), forcePublish, previousStatus, valueHB, "ventilation_bypass_mode");
  publishStatusVHBitMQTT(3, "vh_free_ventilation_mode", (valueHB & 0x08), (previousStatus & 0x08), forcePublish, previousStatus, valueHB, "ventilation_free_mode");
  endStatusBurst();
}

static void publishSlaveStatusVHState(uint8_t valueLB, const char *statusText)
{
  const uint8_t previousStatus = OTcurrentSystemState.SlaveStatusVH;
  const bool forcePublish = shouldForceSlaveStatusVHPublish();
  const bool publishCombined = shouldPublishStatusVHByte(1, valueLB, previousStatus, forcePublish);
  if (state.debug.bMQTTGate) {
    const bool logWorthy = forcePublish || (valueLB != previousStatus) || mqttOnChangePublishingActive();
    if (logWorthy) {
      char previousBitsText[9] {0};
      char currentBitsText[9] {0};
      copyBinaryByteString(previousStatus, previousBitsText, sizeof(previousBitsText));
      copyBinaryByteString(valueLB, currentBitsText, sizeof(currentBitsText));
      const char *reason = forcePublish                ? "force"
                         : (valueLB != previousStatus) ? "changed"
                         : publishCombined             ? "interval"
                         :                              "no-change";
      DebugTf(PSTR("MQTT gate status_vh_slave 0x%02X[%s]->0x%02X[%s] => %s[%s]\r\n"),
              previousStatus, previousBitsText, valueLB, currentBitsText,
              publishCombined ? "publish" : "skip", reason);
    }
  }
  OTcurrentSystemState.SlaveStatusVH = valueLB;
  mqttForceNextSlaveStatusVHPublish = false;
  // Suppress MQTT discovery drip during this sub-topic fanout (TASK-342/354)
  // and arm post-burst cooldown on real sends (TASK-347/354).
  beginStatusBurst();
  {
    OTPublishGate gate(publishCombined);
    if (publishCombined) incrementStatusBurstPublishCount();
    // ADR-104: commit pending byte-slot only when sendMQTTData confirms success.
    if (sendMQTTData(F("status_vh_slave"), statusText)) confirmMQTTPublishByteSlot();
    else                                                mqttPendingByteSlot.pending = false;
  }
  publishStatusVHBitMQTT(0, "vh_fault",                   (valueLB & 0x01), (previousStatus & 0x01), forcePublish, previousStatus, valueLB, "ventilation_fault");
  publishStatusVHBitMQTT(1, "vh_ventilation_mode",        (valueLB & 0x02), (previousStatus & 0x02), forcePublish, previousStatus, valueLB, "ventilation_active");
  publishStatusVHBitMQTT(2, "vh_bypass_status",           (valueLB & 0x04), (previousStatus & 0x04), forcePublish, previousStatus, valueLB, "ventilation_bypass_status");
  publishStatusVHBitMQTT(3, "vh_bypass_automatic_status", (valueLB & 0x08), (previousStatus & 0x08), forcePublish, previousStatus, valueLB, "ventilation_bypass_automatic");
  publishStatusVHBitMQTT(4, "vh_free_ventliation_status", (valueLB & 0x10), (previousStatus & 0x10), forcePublish, previousStatus, valueLB, "ventilation_free_status");
  publishStatusVHBitMQTT(6, "vh_diagnostic_indicator",    (valueLB & 0x40), (previousStatus & 0x40), forcePublish, previousStatus, valueLB, "ventilation_diagnostic");
  endStatusBurst();
}

static uint16_t publishCombinedStatusVHState(uint8_t valueHB, uint8_t valueLB)
{
  char masterStatus[9] {0};
  char slaveStatus[9] {0};
  buildStatusVHMasterText(valueHB, masterStatus, sizeof(masterStatus));
  buildStatusVHSlaveText(valueLB, slaveStatus, sizeof(slaveStatus));
  publishMasterStatusVHState(valueHB, masterStatus);
  publishSlaveStatusVHState(valueLB, slaveStatus);
  return (OTcurrentSystemState.MasterStatusVH << 8) | OTcurrentSystemState.SlaveStatusVH;
}

// TASK-401: gated RBP publish. `prevTransfer` and `prevReadWrite` come from the
// previously stored OTcurrentSystemState.RBPflags (HB<<8 | LB) so the gate can
// compute per-bit change vs last frame. First-seen + change-only + 60s heartbeat.
static uint16_t publishRBPFlagsState(uint8_t transferEnableFlags, uint8_t readWriteFlags,
                                     uint8_t prevTransfer, uint8_t prevReadWrite)
{
  char transferEnableText[9] {0};
  char readWriteText[9] {0};
  copyBinaryByteString(transferEnableFlags, transferEnableText, sizeof(transferEnableText));
  copyBinaryByteString(readWriteFlags, readWriteText, sizeof(readWriteText));

  publishGatedByteMQTT(mqttlastsentRBPbyte, 0, F("RBP_flags_transfer_enable"), transferEnableText,
                       transferEnableFlags, prevTransfer);
  publishGatedByteMQTT(mqttlastsentRBPbyte, 1, F("RBP_flags_read_write"), readWriteText,
                       readWriteFlags, prevReadWrite);
  publishGatedBitMQTT(mqttlastsentRBPbit, 0, F("rbp_dhw_setpoint"),
                      (transferEnableFlags & 0x01), (prevTransfer & 0x01),
                      F("supports_hot_water_setpoint_transfer"));
  publishGatedBitMQTT(mqttlastsentRBPbit, 1, F("rbp_max_ch_setpoint"),
                      (transferEnableFlags & 0x02), (prevTransfer & 0x02),
                      F("supports_central_heating_setpoint_transfer"));
  publishGatedBitMQTT(mqttlastsentRBPbit, 2, F("rbp_rw_dhw_setpoint"),
                      (readWriteFlags & 0x01), (prevReadWrite & 0x01),
                      F("supports_hot_water_setpoint_writing"));
  publishGatedBitMQTT(mqttlastsentRBPbit, 3, F("rbp_rw_max_ch_setpoint"),
                      (readWriteFlags & 0x02), (prevReadWrite & 0x02),
                      F("supports_central_heating_setpoint_writing"));

  return ((uint16_t)transferEnableFlags << 8) | readWriteFlags;
}

//===================[ ADR-118 Gateway-Override Store ]=========
// Additive visibility for gateway-injected overrides that the boiler-side-worldview
// gate (ADR-096/103) drops from canonical. Pure RAM; no MQTT publish or yield in the
// capture path (publication happens on the periodic MQTT path, REST reads on request).

// Find-or-allocate: refresh an existing entry for `id`, else claim the first free
// or stalest slot. Linear scan over <=11 entries (cold path). lastSeen is updated
// ONLY here, so an active override is never cleared by a subsequent valid frame.
void recordOTOverride(uint8_t id, uint8_t kind, float value) {
  const uint32_t now = millis();
  int slot = -1;
  int freeSlot = -1;
  int stalestSlot = 0;
  uint32_t stalestAge = 0;
  for (int i = 0; i < OVERRIDE_STORE_MAX; i++) {
    if (otOverrideStore[i].lastSeen != 0 && otOverrideStore[i].id == id) { slot = i; break; }
    if (otOverrideStore[i].lastSeen == 0 && freeSlot < 0) freeSlot = i;
    const uint32_t age = now - otOverrideStore[i].lastSeen;
    if (age >= stalestAge) { stalestAge = age; stalestSlot = i; }
  }
  if (slot < 0) slot = (freeSlot >= 0) ? freeSlot : stalestSlot;
  otOverrideStore[slot].id       = id;
  otOverrideStore[slot].kind     = kind;
  otOverrideStore[slot].value    = value;
  otOverrideStore[slot].lastSeen = (now == 0) ? 1 : now;  // 0 reserved as "empty" sentinel
}

bool isOTOverrideActive(uint8_t id) {
  const uint32_t now = millis();
  for (int i = 0; i < OVERRIDE_STORE_MAX; i++) {
    if (otOverrideStore[i].lastSeen != 0 && otOverrideStore[i].id == id) {
      return (now - otOverrideStore[i].lastSeen) < OVERRIDE_ACTIVE_TIMEOUT;
    }
  }
  return false;
}

const __FlashStringHelper* overrideKindLabel(uint8_t kind) {
  return (kind == OT_OVERRIDE_ANSWER) ? F("answer") : F("substituted");
}

// Periodic publisher (called from the MQTT periodic path, NOT from the decode hook).
// Publishes one retained `<label>/override` topic per active override entry so HA
// and dashboards reflect the injected value. Canonical topics are untouched.
void publishActiveOverrides() {
  if (!settings.mqtt.bEnable) return;
  const uint32_t now = millis();
  char leaf[64];
  char val[15];
  for (int i = 0; i < OVERRIDE_STORE_MAX; i++) {
    if (otOverrideStore[i].lastSeen == 0) continue;
    if ((now - otOverrideStore[i].lastSeen) >= OVERRIDE_ACTIVE_TIMEOUT) continue;
    const char* label = messageIDToString(static_cast<OTLibMessageID>(otOverrideStore[i].id));
    snprintf_P(leaf, sizeof(leaf), PSTR("%s/override"), label);
    dtostrf(otOverrideStore[i].value, 3, 2, val);
    sendMQTTData(leaf, val, /*retain=*/true);
    feedWatchDog();
  }
}

//===================[ OT Message Field Formatters ]=========

void print_f88(float& value)
{
  //function to print data
  float _value = roundf(OTdata.f88()*100.0f) / 100.0f; // round float 2 digits, like this: x.xx
  // AddLog("%s = %3.2f %s", OTlookupitem.label, _value , OTlookupitem.unit);
  char _msg[15] {0};
  dtostrf(_value, 3, 2, _msg);

  // ADR-097: gate log decode + state write on master-topic validity. The protocol
  // event stays visible (timestamp/source/msgid/type/indicator are added in processOT);
  // only the per-spec-undefined Write-Ack data byte is suppressed from log + REST state.
  const bool validForMaster = is_value_valid_for_master_topic(OTdata, OTlookupitem);
  if (validForMaster) {
    AddLogf("%s = %s %s", OTlookupitem.label, _msg, OTlookupitem.unit);
  } else {
    AddLogf("%s", OTlookupitem.label);
  }

  // ADR-118: capture gateway-injected override (additive; does NOT affect canonical).
  // Gated on the explicit suppression flags only, NOT !validForMaster (which is also
  // false for non-override reasons). Pure RAM write — no MQTT publish, no yield.
  if ((OTdata.rsptype == OTGW_ANSWER_THERMOSTAT) && OTdata.bAnswerOverride) {
    recordOTOverride(OTdata.id, OT_OVERRIDE_ANSWER, _value);
  } else if ((OTdata.rsptype == OTGW_THERMOSTAT) && OTdata.bGatewaySubstituted) {
    recordOTOverride(OTdata.id, OT_OVERRIDE_SUBSTITUTED, _value);
  }

  //SendMQTT
  if (is_value_valid(OTdata, OTlookupitem)){
    const char* topic = messageIDToString(static_cast<OTLibMessageID>(OTdata.id));
    if (validForMaster) sendMQTTData(topic, _msg);
    publishToSourceTopic(topic, _msg, OTdata.rsptype);
    if (validForMaster) value = _value;
  }
}


void print_s16(int16_t& value)
{
  int16_t _value = OTdata.s16();
  // AddLogf("%s = %5d %s", OTlookupitem.label, _value, OTlookupitem.unit);
  //Build string for MQTT
  char _msg[15] {0};
  itoa(_value, _msg, 10);

  // ADR-097: gate log decode + state write on master-topic validity (see print_f88).
  const bool validForMaster = is_value_valid_for_master_topic(OTdata, OTlookupitem);
  if (validForMaster) {
    AddLogf("%s = %s %s", OTlookupitem.label, _msg, OTlookupitem.unit);
  } else {
    AddLogf("%s", OTlookupitem.label);
  }

  //SendMQTT
  if (is_value_valid(OTdata, OTlookupitem)){
    const char* topic = messageIDToString(static_cast<OTLibMessageID>(OTdata.id));
    if (validForMaster) sendMQTTData(topic, _msg);
    publishToSourceTopic(topic, _msg, OTdata.rsptype);
    if (validForMaster) value = _value;
  }
}

void print_s8s8(uint16_t& value)
{
  // ADR-097: gate log decode + state write on master-topic validity (see print_f88).
  const bool validForMaster = is_value_valid_for_master_topic(OTdata, OTlookupitem);
  if (validForMaster) {
    AddLogf("%s = %3d / %3d %s", OTlookupitem.label, (int8_t)OTdata.valueHB, (int8_t)OTdata.valueLB, OTlookupitem.unit);
  } else {
    AddLogf("%s", OTlookupitem.label);
  }

  //Build string for MQTT
  char _msg[15] {0};
  otTopic[0] = '\0';
  itoa((int8_t)OTdata.valueHB, _msg, 10);
  strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
  strlcat(otTopic, "_value_hb", sizeof(otTopic));
  //AddLogf("%s = %s %s", OTlookupitem.label, _msg, OTlookupitem.unit);
  const bool _valid = is_value_valid(OTdata, OTlookupitem);
  if (_valid){
    if (validForMaster) sendMQTTData(otTopic, _msg);
    publishToSourceTopic(otTopic, _msg, OTdata.rsptype);
  }
  //Build string for MQTT
  itoa((int8_t)OTdata.valueLB, _msg, 10);
  strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
  strlcat(otTopic, "_value_lb", sizeof(otTopic));
  //AddLogf("%s = %s %s", OTlookupitem.label, _msg, OTlookupitem.unit);
  if (_valid){
    if (validForMaster) sendMQTTData(otTopic, _msg);
    publishToSourceTopic(otTopic, _msg, OTdata.rsptype);
    if (validForMaster) value = OTdata.u16();
  }
}

void print_u16(uint16_t& value)
{
  uint16_t _value = OTdata.u16();
  //Build string for MQTT
  char _msg[15] {0};
  utoa(_value, _msg, 10);

  // ADR-097: gate log decode + state write on master-topic validity (see print_f88).
  const bool validForMaster = is_value_valid_for_master_topic(OTdata, OTlookupitem);
  if (validForMaster) {
    AddLogf("%s = %s %s", OTlookupitem.label, _msg, OTlookupitem.unit);
  } else {
    AddLogf("%s", OTlookupitem.label);
  }

  //SendMQTT
  if (is_value_valid(OTdata, OTlookupitem)){
    const char* topic = messageIDToString(static_cast<OTLibMessageID>(OTdata.id));
    if (validForMaster) sendMQTTData(topic, _msg);
    publishToSourceTopic(topic, _msg, OTdata.rsptype);
    if (validForMaster) value = _value;
  }
}

void print_status(uint16_t& value)
{
  char _flag8_master[9] {0};
  char _flag8_slave[9] {0};
  
  if (OTdata.masterslave == 0) {
    // Parse master bits
    //bit: [clear/0, set/1]
    //  0: CH enable [ CH is disabled, CH is enabled]
    //  1: DHW enable [ DHW is disabled, DHW is enabled]
    //  2: Cooling enable [ Cooling is disabled, Cooling is enabled]]
    //  3: OTC active [OTC not active, OTC is active]
    //  4: CH2 enable [CH2 is disabled, CH2 is enabled]
    //  5: Summer/winter mode [Summertime, Wintertime]
    //  6: DHW blocking [ DHW not blocking, DHW blocking ]
    //  7: reserved
    _flag8_master[0] = (((OTdata.valueHB) & 0x01) ? 'C' : '-');
    _flag8_master[1] = (((OTdata.valueHB) & 0x02) ? 'D' : '-');
    _flag8_master[2] = (((OTdata.valueHB) & 0x04) ? 'C' : '-'); 
    _flag8_master[3] = (((OTdata.valueHB) & 0x08) ? 'O' : '-');
    _flag8_master[4] = (((OTdata.valueHB) & 0x10) ? '2' : '-'); 
    _flag8_master[5] = (((OTdata.valueHB) & 0x20) ? 'S' : 'W'); 
    _flag8_master[6] = (((OTdata.valueHB) & 0x40) ? 'B' : '-'); 
    _flag8_master[7] = (((OTdata.valueHB) & 0x80) ? '.' : '-');
    _flag8_master[8] = '\0';

    AddLog(" ");
    AddLog(OTlookupitem.label);
    AddLogf(" = Master [%s]", _flag8_master);

    //Master Status — ADR-069 boiler-side worldview: suppress canonical for
    //gateway-substituted T-frames so the HW override is reflected in dhw_enable.
    if (is_value_valid_for_master_topic(OTdata, OTlookupitem)){
      publishMasterStatusState(OTdata.valueHB, _flag8_master);
    }
  } else {
    // Parse slave bits
    //  0: fault indication [ no fault, fault ]
    //  1: CH mode [CH not active, CH active]
    //  2: DHW mode [ DHW not active, DHW active]
    //  3: Flame status [ flame off, flame on ]
    //  4: Cooling status [ cooling mode not active, cooling mode active ]
    //  5: CH2 mode [CH2 not active, CH2 active]
    //  6: diagnostic indication [no diagnostics, diagnostic event]
    //  7: Electricity production [no electric production, electric production]
    _flag8_slave[0] = (((OTdata.valueLB) & 0x01) ? 'E' : '-');
    _flag8_slave[1] = (((OTdata.valueLB) & 0x02) ? 'C' : '-'); 
    _flag8_slave[2] = (((OTdata.valueLB) & 0x04) ? 'W' : '-'); 
    _flag8_slave[3] = (((OTdata.valueLB) & 0x08) ? 'F' : '-'); 
    _flag8_slave[4] = (((OTdata.valueLB) & 0x10) ? 'C' : '-'); 
    _flag8_slave[5] = (((OTdata.valueLB) & 0x20) ? '2' : '-'); 
    _flag8_slave[6] = (((OTdata.valueLB) & 0x40) ? 'D' : '-'); 
    _flag8_slave[7] = (((OTdata.valueLB) & 0x80) ? 'P' : '-');
    _flag8_slave[8] = '\0';

    AddLog(" ");
    AddLog(OTlookupitem.label);
    AddLogf(" = Slave  [%s]", _flag8_slave);
    
    //Slave Status — ADR-069 boiler-side worldview: suppress canonical for
    //gateway-faked A-frames so the boiler's true DHW mode is what we publish.
    if (is_value_valid_for_master_topic(OTdata, OTlookupitem)){
      publishSlaveStatusState(OTdata.valueLB, _flag8_slave);
    }
  }

  if (is_value_valid_for_master_topic(OTdata, OTlookupitem)){
    // AddLogf("Status u16 [%04x] _value [%04x] hb [%02x] lb [%02x]", OTdata.u16(), _value, OTdata.valueHB, OTdata.valueLB);
    value = (OTcurrentSystemState.MasterStatus<<8) | OTcurrentSystemState.SlaveStatus;
  }
}

void print_solar_storage_status(uint16_t& value)
{ 
  char _msg[15] {0};

  if (OTdata.masterslave == 0) {
    // Master Solar Storage 
    // ID101:HB012: Master Solar Storage: Solar mode
    uint8_t MasterSolarMode = (OTdata.valueHB) & 0x7;
    AddLogf("%s = Solar Storage Master Mode [%d] ", OTlookupitem.label, MasterSolarMode);
    if (is_value_valid(OTdata, OTlookupitem)){
      sendMQTTData(F("solar_storage_master_mode"), itoa(MasterSolarMode, _msg, 10));  //delayms(5);
      OTcurrentSystemState.SolarMasterStatus = OTdata.valueHB;
    }
  } else { 
    //Slave
    // ID101:LB0: Slave Solar Storage: Fault indication
    uint8_t SlaveSolarFaultIndicator =  (OTdata.valueLB) & 0x01;
    // ID101:LB123: Slave Solar Storage: Solar mode status
    uint8_t SlaveSolarModeStatus = (OTdata.valueLB>>1) & 0x07;
    // ID101:LB45: Slave Solar Storage: Solar status
    uint8_t SlaveSolarStatus = (OTdata.valueLB>>4)& 0x03;
    AddLogf("\r\n%s = Slave Solar Fault Indicator [%d] ", OTlookupitem.label, SlaveSolarFaultIndicator);
    AddLogf("\r\n%s = Slave Solar Mode Status [%d] ", OTlookupitem.label, SlaveSolarModeStatus);
    AddLogf("\r\n%s = Slave Solar Status [%d] ", OTlookupitem.label, SlaveSolarStatus);
    if (is_value_valid(OTdata, OTlookupitem)){
      // ADR-106: pick legacy vs new label.
      sendMQTTData(settings.mqtt.bUseLegacyOtTopics ? F("solar_storage_slave_fault_indicator")
                                                    : F("solar_storage_fault"),
                   ((SlaveSolarFaultIndicator) ? "ON" : "OFF"));
      sendMQTTData(F("solar_storage_mode_status"), itoa(SlaveSolarModeStatus, _msg, 10));  
      sendMQTTData(F("solar_storage_slave_status"), itoa(SlaveSolarStatus, _msg, 10));  
      OTcurrentSystemState.SolarSlaveStatus = OTdata.valueLB;
    }
  }
  if (is_value_valid(OTdata, OTlookupitem)){
    //OTDebugTf(PSTR("Solar Storage Master / Slave Mode u16 [%04x] _value [%04x] hb [%02x] lb [%02x]"), OTdata.u16(), _value, OTdata.valueHB, OTdata.valueLB);
    value = (OTcurrentSystemState.SolarMasterStatus<<8) | OTcurrentSystemState.SolarSlaveStatus;
  }
}

void print_statusVH(uint16_t& value)
{ 
  char _flag8_master[9] {0};
  char _flag8_slave[9] {0};

  if (OTdata.masterslave == 0){
      
    // Parse master bits
    //bit: [clear/0, set/1]
    // ID70:HB0: Master status ventilation / heat-recovery: Ventilation enable
    // ID70:HB1: Master status ventilation / heat-recovery: Bypass postion
    // ID70:HB2: Master status ventilation / heat-recovery: Bypass mode
    // ID70:HB3: Master status ventilation / heat-recovery: Free ventilation mode
    //  4: reserved
    //  5: reserved
    //  6: reserved
    //  7: reserved
    _flag8_master[0] = (((OTdata.valueHB) & 0x01) ? 'V' : '-');
    _flag8_master[1] = (((OTdata.valueHB) & 0x02) ? 'P' : '-');
    _flag8_master[2] = (((OTdata.valueHB) & 0x04) ? 'M' : '-'); 
    _flag8_master[3] = (((OTdata.valueHB) & 0x08) ? 'F' : '-');
    _flag8_master[4] = (((OTdata.valueHB) & 0x10) ? '.' : '-'); 
    _flag8_master[5] = (((OTdata.valueHB) & 0x20) ? '.' : '-'); 
    _flag8_master[6] = (((OTdata.valueHB) & 0x40) ? '.' : '-'); 
    _flag8_master[7] = (((OTdata.valueHB) & 0x80) ? '.' : '-');
    _flag8_master[8] = '\0';

    
    AddLogf("%s = VH Master [%s]", OTlookupitem.label, _flag8_master);
    //Master Status
    if (is_value_valid(OTdata, OTlookupitem)){
      publishMasterStatusVHState(OTdata.valueHB, _flag8_master);
    }
  } else {
    // Parse slave bits
    // ID70:LB0: Slave status ventilation / heat-recovery: Fault indication
    // ID70:LB1: Slave status ventilation / heat-recovery: Ventilation mode
    // ID70:LB2: Slave status ventilation / heat-recovery: Bypass status
    // ID70:LB3: Slave status ventilation / heat-recovery: Bypass automatic status
    // ID70:LB4: Slave status ventilation / heat-recovery: Free ventilation status
    // ID70:LB6: Slave status ventilation / heat-recovery: Diagnostic indication
    _flag8_slave[0] = (((OTdata.valueLB) & 0x01) ? 'F' : '-');
    _flag8_slave[1] = (((OTdata.valueLB) & 0x02) ? 'V' : '-'); 
    _flag8_slave[2] = (((OTdata.valueLB) & 0x04) ? 'P' : '-'); 
    _flag8_slave[3] = (((OTdata.valueLB) & 0x08) ? 'A' : '-'); 
    _flag8_slave[4] = (((OTdata.valueLB) & 0x10) ? 'F' : '-'); 
    _flag8_slave[5] = (((OTdata.valueLB) & 0x20) ? '.' : '-');
    _flag8_slave[6] = (((OTdata.valueLB) & 0x40) ? 'D' : '-'); 
    _flag8_slave[7] = (((OTdata.valueLB) & 0x80) ? '.' : '-');
    _flag8_slave[8] = '\0';

    
    AddLogf("%s = VH Slave  [%s]", OTlookupitem.label, _flag8_slave);

    //Slave Status
    if (is_value_valid(OTdata, OTlookupitem)){
      publishSlaveStatusVHState(OTdata.valueLB, _flag8_slave);
    }
  }

  if (is_value_valid(OTdata, OTlookupitem)){
    //OTDebugTf(PSTR("Status u16 [%04x] _value [%04x] hb [%02x] lb [%02x]"), OTdata.u16(), _value, OTdata.valueHB, OTdata.valueLB);
    value = (OTcurrentSystemState.MasterStatusVH<<8) | OTcurrentSystemState.SlaveStatusVH;
  }
}


void print_ASFflags(uint16_t& value)
{
  AddLogf("%s = ASF flags[%s] OEM faultcode [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);

  if (is_value_valid(OTdata, OTlookupitem)){
    // TASK-401: gate ASF_flags byte-topic + 6 bit-topics on first-seen + change + 60s heartbeat.
    // Previous byte value lives in `value` (uint16_t& ref to OTcurrentSystemState.ASFflags).
    const uint8_t prevHB = (uint8_t)((value >> 8) & 0xFF);
    const uint8_t newHB  = OTdata.valueHB;
    //Application Specific Fault (byte, gated)
    publishGatedByteMQTT(mqttlastsentASFbyte, 0, F("ASF_flags"), byte_to_binary(newHB), newHB, prevHB);
    //OEM fault code — numeric value, not a bit; left raw (low publish cost vs HA wants fresh code)
    char _msg[15] {0};
    utoa(OTdata.valueLB, _msg, 10);
    sendMQTTData(F("OEMFaultCode"), _msg);

    //bit: [clear/0, set/1]
    //0: Service request [service not req’d, service required]
    //1: Lockout-reset [ remote reset disabled, rr enabled]
    //2: Low water press [ no WP fault, water pressure fault]
    //3: Gas/flame fault [ no G/F fault, gas/flame fault ]
    //4: Air press fault [ no AP fault, air pressure fault ]
    //5: Water over-temp[ no OvT fault, over-temperat. Fault]
    //6: reserved
    //7: reserved
    publishGatedBitMQTT(mqttlastsentASFbit, 0, F("service_request"),        (newHB & 0x01), (prevHB & 0x01), F("service_required"));
    publishGatedBitMQTT(mqttlastsentASFbit, 1, F("lockout_reset"),          (newHB & 0x02), (prevHB & 0x02), F("supports_lockout_reset"));
    publishGatedBitMQTT(mqttlastsentASFbit, 2, F("low_water_pressure"),     (newHB & 0x04), (prevHB & 0x04));
    publishGatedBitMQTT(mqttlastsentASFbit, 3, F("gas_flame_fault"),        (newHB & 0x08), (prevHB & 0x08), F("gas_fault"));
    publishGatedBitMQTT(mqttlastsentASFbit, 4, F("air_pressure_fault"),     (newHB & 0x10), (prevHB & 0x10));
    publishGatedBitMQTT(mqttlastsentASFbit, 5, F("water_over_temperature"), (newHB & 0x20), (prevHB & 0x20), F("water_overtemperature"));
    value = OTdata.u16();
  }
}

void print_RBPflags(uint16_t& value)
{
  AddLogf("%s = M[%s] OEM fault code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  if (is_value_valid(OTdata, OTlookupitem)){
    // TASK-401: extract previous HB/LB so publishRBPFlagsState can gate per-bit vs last frame.
    const uint8_t prevTransfer  = (uint8_t)((value >> 8) & 0xFF);
    const uint8_t prevReadWrite = (uint8_t)(value & 0xFF);
    value = publishRBPFlagsState(OTdata.valueHB, OTdata.valueLB, prevTransfer, prevReadWrite);
  }
}

void print_slavememberid(uint16_t& value)
{
  AddLogf("%s = Slave Config[%s] MemberID code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for SendMQTT
    sendMQTTData(F("slave_configuration"), byte_to_binary(OTdata.valueHB));
    char _msg[15] {0};
    utoa(OTdata.valueLB, _msg, 10);
    sendMQTTData(F("slave_memberid_code"), _msg);

    
    // bit: description  [ clear/0, set/1] 
    // 0:  DHW present  [ dhw not present, dhw is present ] 
    // 1:  Control type  [ modulating, on/off ] 
    // 2:  Cooling config  [ cooling not supported,  
    //     cooling supported] 
    // 3:  DHW config  [instantaneous or not-specified, 
    //     storage tank] 
    // 4:  Master low-off&pump control function [allowed, 
    //     not allowed] 
    // 5:  CH2 present  [CH2 not present, CH2 present]
    // 6:  Remote water filling function
    //     NOTE: pyotgw / HA core's opentherm_gw integration names this bit
    //     DATA_SLAVE_REMOTE_RESET. The OpenTherm 2.2 spec and this firmware's
    //     label call it "remote_water_filling_function". Both refer to the
    //     same wire bit (MsgID 3, HB bit 6); HA-side discovery parity is
    //     audited in docs/audits/2026-05-21-ha-capability-flags-feature-2.0.0.md (TASK-650).
    // 7:  Heat/cool mode control

    // ADR-106: pick legacy vs new label per bit. Heat_cool_mode_control (HB7) has no alias and always publishes its legacy name.
    {
      const bool useLegacy = settings.mqtt.bUseLegacyOtTopics;
      sendMQTTData(useLegacy ? F("dhw_present")                          : F("supports_hot_water"),     (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));
      sendMQTTData(useLegacy ? F("control_type_modulation")              : F("control_type"),           (((OTdata.valueHB) & 0x02) ? "ON" : "OFF"));
      sendMQTTData(useLegacy ? F("cooling_config")                       : F("supports_cooling"),       (((OTdata.valueHB) & 0x04) ? "ON" : "OFF"));
      sendMQTTData(useLegacy ? F("dhw_config")                           : F("hot_water_config"),       (((OTdata.valueHB) & 0x08) ? "ON" : "OFF"));
      sendMQTTData(useLegacy ? F("master_low_off_pump_control_function") : F("supports_pump_control"),  (((OTdata.valueHB) & 0x10) ? "ON" : "OFF"));
      sendMQTTData(useLegacy ? F("ch2_present")                          : F("supports_ch_2"),          (((OTdata.valueHB) & 0x20) ? "ON" : "OFF"));
      sendMQTTData(useLegacy ? F("remote_water_filling_function")        : F("supports_remote_reset"),  (((OTdata.valueHB) & 0x40) ? "ON" : "OFF"));
      sendMQTTData(F("heat_cool_mode_control"),                                                          (((OTdata.valueHB) & 0x80) ? "ON" : "OFF"));
    }
    // SAT heating-source detect (TASK-943): MsgID 3 HB bit2 = "cooling supported".
    // NON-CONTROL telemetry hint only — surfaced as heating_source_detected for the UI.
    // Control authority is the manual satsource setting; satGetEffectiveHeatingSource()
    // resolves AUTO to a safe gas-boiler default. OpenTherm has no source-class field
    // (spec v4.2:1606), so cooling-capable is a lossy proxy (heating-only heat pumps do
    // not set it; hybrid is invisible on one OT bus) — never drive control off this bit.
    state.sat.iDetectedHeatingSource = ((OTdata.valueHB) & 0x04) ? SAT_SRC_HEAT_PUMP : SAT_SRC_GAS_BOILER;
    // SAT: auto-detect manufacturer from slave MemberID code
    satDetectManufacturer(OTdata.valueLB);
    value = OTdata.u16();
  }
}

void print_mastermemberid(uint16_t& value)
{
  AddLogf("%s = Master Config[%s] MemberID code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    char _msg[15] {0};
    sendMQTTData(F("master_configuration"), byte_to_binary(OTdata.valueHB));
    // ADR-106: pick legacy vs new label.
    sendMQTTData(settings.mqtt.bUseLegacyOtTopics ? F("master_configuration_smart_power")
                                                  : F("supports_master_smart_power"),
                 (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  
    
    utoa(OTdata.valueLB, _msg, 10);
    sendMQTTData(F("master_memberid_code"), _msg);
    value = OTdata.u16();
  }
}

void print_vh_configmemberid(uint16_t& value)
{
  AddLogf("%s = VH Config[%s] MemberID code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    char _msg[15] {0};
    sendMQTTData(F("vh_configuration"), byte_to_binary(OTdata.valueHB)); 
    // ADR-106: pick legacy vs new label per bit.
    {
      const bool useLegacy = settings.mqtt.bUseLegacyOtTopics;
      sendMQTTData(useLegacy ? F("vh_configuration_system_type")   : F("ventilation_system_type"),         (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));
      sendMQTTData(useLegacy ? F("vh_configuration_bypass")        : F("supports_ventilation_bypass"),     (((OTdata.valueHB) & 0x02) ? "ON" : "OFF"));
      sendMQTTData(useLegacy ? F("vh_configuration_speed_control") : F("ventilation_speed_control_type"),  (((OTdata.valueHB) & 0x04) ? "ON" : "OFF"));
    }
    // NOTE (TASK-943): heating-source detection was previously (and wrongly) keyed off
    // this MsgID 74 ventilation bit0. It now lives in the MsgID 3 slave-config handler
    // (cooling bit2) as a non-control hint. This handler decodes ventilation config only.
    utoa(OTdata.valueLB, _msg, 10);
    sendMQTTData(F("vh_memberid_code"), _msg);
    value = OTdata.u16();
  }
}

void print_solarstorage_slavememberid(uint16_t& value)
{
  AddLogf("%s = Solar Storage Slave Config[%s] MemberID code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  if (is_value_valid(OTdata, OTlookupitem)){
     //Build string for SendMQTT
    sendMQTTData(F("solar_storage_slave_configuration"), byte_to_binary(OTdata.valueHB));
    char _msg[15] {0};
    utoa(OTdata.valueLB, _msg, 10);
    sendMQTTData(F("solar_storage_slave_memberid_code"), _msg);

    //ID103:HB0: Slave Configuration Solar Storage: System type1
    sendMQTTData(F("solar_storage_system_type"),    (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  
    value = OTdata.u16();
  }
}

void print_remoteoverridefunction(uint16_t& value)
{
// MsdID 100 Remote override room setpoint 
// LB: Remote override function 
// bit: description  [ clear/0, set/1] 
// 0:  Manual change priority [disable overruling remote 
//     setpoint by manual setpoint change, enable overruling 
//     remote setpoint by manual setpoint change ] 
// 1:  Program change priority [disable overruling remote 
//     setpoint by program setpoint change, enable overruling 
//     remote setpoint by program setpoint change ] 
// 2:  reserved  
// 3:  reserved 
// 4:  reserved 
// 5:  reserved 
// 6:  reserved 
// 7:  reserved 
// HB: reserved 
  
  AddLogf("%s = flag8 = [%s] - decimal = [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueLB), OTdata.valueLB);

  if (is_value_valid(OTdata, OTlookupitem)){
    // TASK-401: gate <msgid>_flag8 byte + 2 bit-topics. Remote Override msgId 100
    // stores full u16 in `value`; LB holds the flag byte (HB is reserved).
    const uint8_t prevLB = (uint8_t)(value & 0xFF);
    const uint8_t newLB  = OTdata.valueLB;
    //Build string for MQTT
    otTopic[0] = '\0';
    //flag8 value
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_flag8", sizeof(otTopic));
    publishGatedByteMQTT(mqttlastsentRObyte, 0, otTopic, byte_to_binary(newLB), newLB, prevLB);
    //report remote override flags to MQTT
    publishGatedBitMQTT(mqttlastsentRObit, 0, F("remote_override_manual_change_priority"),
                        (newLB & 0x01), (prevLB & 0x01),
                        F("override_manual_change_prio"));
    publishGatedBitMQTT(mqttlastsentRObit, 1, F("remote_override_program_change_priority"),
                        (newLB & 0x02), (prevLB & 0x02),
                        F("override_program_change_prio"));
    value = OTdata.u16();
  }
}

void print_flag8u8(uint16_t& value)
{
  AddLogf("%s = M[%s] - [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);

  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    otTopic[0] = '\0';
    //flag8 value
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_flag8", sizeof(otTopic));
    sendMQTTData(otTopic, byte_to_binary(OTdata.valueHB));
    //u8 value
    char _msg[15] {0};
    utoa(OTdata.valueLB, _msg, 10);
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_code", sizeof(otTopic));
    sendMQTTData(otTopic, _msg);
    value = OTdata.u16(); 
  }
}

void print_flag8(uint16_t& value)
{
  
  AddLogf("%s = flag8 = [%s] - decimal = [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueLB), OTdata.valueLB);

   if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    otTopic[0] = '\0';
    //flag8 value
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_flag8", sizeof(otTopic));
    sendMQTTData(otTopic, byte_to_binary(OTdata.valueLB));
    value = OTdata.u16();
  }
}


void print_flag8flag8(uint16_t& value)
{ 
  //Build string for MQTT
  otTopic[0] = '\0';
  //flag8 valueHB
  
  AddLogf("%s = HB flag8[%s] -[%3d] ", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueHB);

  if (is_value_valid(OTdata, OTlookupitem)){
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_hb_flag8", sizeof(otTopic));
    sendMQTTData(otTopic, byte_to_binary(OTdata.valueHB));
  }
  //flag8 valueLB
  AddLogf("%s = LB flag8[%s] - [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueLB), OTdata.valueLB);
  if (is_value_valid(OTdata, OTlookupitem)){
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_lb_flag8", sizeof(otTopic));
    sendMQTTData(otTopic, byte_to_binary(OTdata.valueLB));
    value = OTdata.u16();
  }
}

void print_vh_remoteparametersetting(uint16_t& value)
{ 
  //Build string for MQTT
  otTopic[0] = '\0';
  //flag8 valueHB
  
  AddLogf("%s = HB flag8[%s] -[%3d] ", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueHB);
  if (is_value_valid(OTdata, OTlookupitem)){
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_hb_flag8", sizeof(otTopic));
    sendMQTTData(otTopic, byte_to_binary(OTdata.valueHB));
    sendMQTTData(F("vh_transfer_enable_nominal_ventilation_value"),    (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));
  }
  //flag8 valueLB
  AddLogf("%s = LB flag8[%s] - [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueLB), OTdata.valueLB);
  if (is_value_valid(OTdata, OTlookupitem)){
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_lb_flag8", sizeof(otTopic));
    sendMQTTData(otTopic, byte_to_binary(OTdata.valueLB));
    sendMQTTData(F("vh_rw_nominal_ventilation_value"),    (((OTdata.valueLB) & 0x01) ? "ON" : "OFF"));
    value = OTdata.u16();
  }
}

void print_command(uint16_t& value)
{ 
  //Known Commands
  // ID4 (HB=1): Remote Request Boiler Lockout-reset
  // ID4 (HB=2): Remote Request Water filling
  // ID4 (HB=10): Remote Request Service request reset
  
  AddLogf("%s = %3d / %3d %s", OTlookupitem.label, (uint8_t)OTdata.valueHB, (uint8_t)OTdata.valueLB, OTlookupitem.unit);
  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    otTopic[0] = '\0';
    char _msg[10] {0};
    //flag8 valueHB
    utoa((OTdata.valueHB), _msg, 10);
    //AddLogf("%s = HB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueHB);
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_hb_u8", sizeof(otTopic));
    sendMQTTData(otTopic, _msg);
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_remote_command", sizeof(otTopic));
    switch (OTdata.valueHB) {
      case 1: sendMQTTData(otTopic, "Remote Request Boiler Lockout-reset");  AddLogf("\r\n%s = remote command [%s]", OTlookupitem.label, "Remote Request Boiler Lockout-reset"); break;
      case 2: sendMQTTData(otTopic, "Remote Request Water filling"); AddLogf("\r\n%s = remote command [%s]", OTlookupitem.label, "Remote Request Water filling"); break;
      case 10: sendMQTTData(otTopic, "Remote Request Service request reset");  AddLogf("\r\n%s = remote command [%s]", OTlookupitem.label, "Remote Request Service request reset");break;
      default: sendMQTTData(otTopic, "Unknown command"); AddLogf("\r\n%s = remote command [%s]", OTlookupitem.label, "Unknown command");break;
    } 

    //flag8 valueLB
    utoa((OTdata.valueLB), _msg, 10);
    //AddLogf("%s = LB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueLB);
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_lb_u8", sizeof(otTopic));
    sendMQTTData(otTopic, _msg);
    value = OTdata.u16();
  }
}

void print_u8u8(uint16_t& value)
{ 
  
  AddLogf("%s = %3d / %3d %s", OTlookupitem.label, (uint8_t)OTdata.valueHB, (uint8_t)OTdata.valueLB, OTlookupitem.unit);

  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    otTopic[0] = '\0';
    char _msg[10] {0};
    //flag8 valueHB
    utoa((OTdata.valueHB), _msg, 10);
    //AddLogf("%s = HB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueHB);
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_hb_u8", sizeof(otTopic));
    sendMQTTData(otTopic, _msg);
    //flag8 valueLB
    utoa((OTdata.valueLB), _msg, 10);
    //AddLogf("%s = LB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueLB);
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_lb_u8", sizeof(otTopic));
    sendMQTTData(otTopic, _msg);
    value = OTdata.u16();
  }
}

static void publish_u8_alias_topics(const char* baseTopic)
{
  char _msg[10] {0};

  utoa(OTdata.valueHB, _msg, 10);
  strlcpy(otTopic, baseTopic, sizeof(otTopic));
  appendProgmemSuffix(otTopic, sizeof(otTopic), PSTR("_hb_u8"));
  if (is_value_valid_for_master_topic(OTdata, OTlookupitem)) sendMQTTData(otTopic, _msg);
  publishToSourceTopic(otTopic, _msg, OTdata.rsptype);

  utoa(OTdata.valueLB, _msg, 10);
  strlcpy(otTopic, baseTopic, sizeof(otTopic));
  appendProgmemSuffix(otTopic, sizeof(otTopic), PSTR("_lb_u8"));
  if (is_value_valid_for_master_topic(OTdata, OTlookupitem)) sendMQTTData(otTopic, _msg);
  publishToSourceTopic(otTopic, _msg, OTdata.rsptype);
}

static void print_u8_single(uint16_t& value, bool useHB)
{
  const uint8_t activeByte = useHB ? OTdata.valueHB : OTdata.valueLB;
  const uint8_t reservedByte = useHB ? OTdata.valueLB : OTdata.valueHB;
  const char activeByteName0 = useHB ? 'H' : 'L';
  const char activeByteName1 = 'B';
  const char reservedByteName0 = useHB ? 'L' : 'H';
  const char reservedByteName1 = 'B';

  AddLogf_P(PSTR("%s = %3u %s (%c%c used, %c%c=%u)"),
            OTlookupitem.label,
            activeByte,
            OTlookupitem.unit,
            activeByteName0, activeByteName1,
            reservedByteName0, reservedByteName1,
            reservedByte);

  if (is_value_valid(OTdata, OTlookupitem)){
    char _msg[10] {0};
    const char* baseTopic = messageIDToString(static_cast<OTLibMessageID>(OTdata.id));
    utoa(activeByte, _msg, 10);
    if (is_value_valid_for_master_topic(OTdata, OTlookupitem)) sendMQTTData(baseTopic, _msg);
    publishToSourceTopic(baseTopic, _msg, OTdata.rsptype);

    // Backward compatibility for earlier generic u8u8 decoding.
    publish_u8_alias_topics(baseTopic);
    value = activeByte;
  }
}

void print_u8_hb(uint16_t& value)
{
  print_u8_single(value, true);
}

void print_u8_lb(uint16_t& value)
{
  print_u8_single(value, false);
}

static PGM_P rfSensorTypeToString_P(uint8_t code)
{
  switch (code) {
    case 0x0: return PSTR("room_temperature_controller");
    case 0x1: return PSTR("room_temperature_sensor");
    case 0x2: return PSTR("outside_temperature_sensor");
    case 0xF: return PSTR("not_defined_type");
    default:  return PSTR("reserved");
  }
}

static PGM_P rfBatteryIndicationToString_P(uint8_t code)
{
  switch (code) {
    case 0x0: return PSTR("no_indication");
    case 0x1: return PSTR("low_battery");
    case 0x2: return PSTR("nearly_low_battery");
    case 0x3: return PSTR("battery_ok");
    default:  return PSTR("reserved");
  }
}

static PGM_P rfSignalStrengthToString_P(uint8_t code)
{
  switch (code) {
    case 0x0: return PSTR("no_indication");
    case 0x1: return PSTR("strength_1_weak");
    case 0x2: return PSTR("strength_2");
    case 0x3: return PSTR("strength_3");
    case 0x4: return PSTR("strength_4");
    case 0x5: return PSTR("strength_5_perfect");
    default:  return PSTR("reserved");
  }
}

static PGM_P heatingOverrideModeToString_P(uint8_t code)
{
  switch (code) {
    case 0: return PSTR("no_override");
    case 1: return PSTR("auto");
    case 2: return PSTR("comfort");
    case 3: return PSTR("precomfort");
    case 4: return PSTR("reduced");
    case 5: return PSTR("protection");
    case 6: return PSTR("off");
    default: return PSTR("reserved");
  }
}

static PGM_P dhwOverrideModeToString_P(uint8_t code)
{
  switch (code) {
    case 0: return PSTR("no_override");
    case 1: return PSTR("auto");
    case 2: return PSTR("anti_legionella");
    case 3: return PSTR("comfort");
    case 4: return PSTR("reduced");
    case 5: return PSTR("protection");
    case 6: return PSTR("off");
    default: return PSTR("reserved");
  }
}

static PGM_P onOffToString_P(bool isOn)
{
  return isOn ? PSTR("ON") : PSTR("OFF");
}

static void publish_current_message_u8_alias_topics()
{
  publish_u8_alias_topics(messageIDToString(static_cast<OTLibMessageID>(OTdata.id)));
}

static void publish_mqtt_u8_value_topic(const __FlashStringHelper *topic, uint8_t value)
{
  char msg[4] {0};
  utoa(value, msg, 10);
  sendMQTTData(topic, msg);
}

static void publish_mqtt_pgm_payload_topic(const __FlashStringHelper *topic, PGM_P payload)
{
  sendMQTTData(topic, toFlashStringHelper(payload));
}

static void publish_mqtt_u8_code_and_text_topics(const __FlashStringHelper *codeTopic,
                                                 const __FlashStringHelper *textTopic,
                                                 uint8_t code,
                                                 PGM_P text)
{
  publish_mqtt_u8_value_topic(codeTopic, code);
  publish_mqtt_pgm_payload_topic(textTopic, text);
}

void print_rf_sensor_status_information(uint16_t& value)
{
  const uint8_t sensorIndex = OTdata.valueHB & 0x0F;
  const uint8_t sensorType = (OTdata.valueHB >> 4) & 0x0F;
  const uint8_t batteryInd = OTdata.valueLB & 0x03;
  const uint8_t signalStrength = (OTdata.valueLB >> 2) & 0x07;
  char sensorTypeText[32] {0};
  char signalStrengthText[24] {0};
  char batteryIndText[24] {0};

  copyProgmemString(sensorTypeText, sizeof(sensorTypeText), rfSensorTypeToString_P(sensorType));
  copyProgmemString(signalStrengthText, sizeof(signalStrengthText), rfSignalStrengthToString_P(signalStrength));
  copyProgmemString(batteryIndText, sizeof(batteryIndText), rfBatteryIndicationToString_P(batteryInd));

  AddLogf_P(PSTR("%s = sensor_type[%u:%s] sensor_index[%u] signal[%u:%s] battery[%u:%s]"),
            OTlookupitem.label,
            sensorType, sensorTypeText,
            sensorIndex,
            signalStrength, signalStrengthText,
            batteryInd, batteryIndText);

  if (is_value_valid(OTdata, OTlookupitem)){
    publish_current_message_u8_alias_topics();

    publish_mqtt_u8_value_topic(F("RFSensorStatusInformation_sensor_index"), sensorIndex);
    publish_mqtt_u8_code_and_text_topics(F("RFSensorStatusInformation_sensor_type_code"),
                                         F("RFSensorStatusInformation_sensor_type"),
                                         sensorType,
                                         rfSensorTypeToString_P(sensorType));
    publish_mqtt_u8_code_and_text_topics(F("RFSensorStatusInformation_signal_strength_code"),
                                         F("RFSensorStatusInformation_signal_strength"),
                                         signalStrength,
                                         rfSignalStrengthToString_P(signalStrength));
    publish_mqtt_u8_code_and_text_topics(F("RFSensorStatusInformation_battery_indication_code"),
                                         F("RFSensorStatusInformation_battery_indication"),
                                         batteryInd,
                                         rfBatteryIndicationToString_P(batteryInd));

    value = OTdata.u16();
  }
}

void print_remote_override_operating_mode(uint16_t& value)
{
  const uint8_t hc1Mode = OTdata.valueLB & 0x0F;
  const uint8_t hc2Mode = (OTdata.valueLB >> 4) & 0x0F;
  const uint8_t dhwMode = OTdata.valueHB & 0x0F;
  const bool manualDhwPush = (OTdata.valueHB & 0x10) != 0;
  char dhwModeText[20] {0};
  char hc1ModeText[16] {0};
  char hc2ModeText[16] {0};
  char manualDhwPushText[4] {0};

  copyProgmemString(dhwModeText, sizeof(dhwModeText), dhwOverrideModeToString_P(dhwMode));
  copyProgmemString(hc1ModeText, sizeof(hc1ModeText), heatingOverrideModeToString_P(hc1Mode));
  copyProgmemString(hc2ModeText, sizeof(hc2ModeText), heatingOverrideModeToString_P(hc2Mode));
  copyProgmemString(manualDhwPushText, sizeof(manualDhwPushText), onOffToString_P(manualDhwPush));

  AddLogf_P(PSTR("%s = DHW[%u:%s push:%s] HC1[%u:%s] HC2[%u:%s]"),
            OTlookupitem.label,
            dhwMode, dhwModeText, manualDhwPushText,
            hc1Mode, hc1ModeText,
            hc2Mode, hc2ModeText);

  if (is_value_valid(OTdata, OTlookupitem)){
    publish_current_message_u8_alias_topics();

    publish_mqtt_u8_code_and_text_topics(F("RemoteOverrideOperatingMode_dhw_mode_code"),
                                         F("RemoteOverrideOperatingMode_dhw_mode"),
                                         dhwMode,
                                         dhwOverrideModeToString_P(dhwMode));
    publish_mqtt_pgm_payload_topic(F("RemoteOverrideOperatingMode_manual_dhw_push"), onOffToString_P(manualDhwPush));

    publish_mqtt_u8_code_and_text_topics(F("RemoteOverrideOperatingMode_hc1_mode_code"),
                                         F("RemoteOverrideOperatingMode_hc1_mode"),
                                         hc1Mode,
                                         heatingOverrideModeToString_P(hc1Mode));
    publish_mqtt_u8_code_and_text_topics(F("RemoteOverrideOperatingMode_hc2_mode_code"),
                                         F("RemoteOverrideOperatingMode_hc2_mode"),
                                         hc2Mode,
                                         heatingOverrideModeToString_P(hc2Mode));

    value = OTdata.u16();
  }
}

void print_date(uint16_t& value)
{ 
  
  AddLogf("%s = %3d / %3d %s", OTlookupitem.label, (uint8_t)OTdata.valueHB, (uint8_t)OTdata.valueLB, OTlookupitem.unit);
  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    otTopic[0] = '\0';
    char _msg[10] {0};
    //flag8 valueHB
    utoa((OTdata.valueHB), _msg, 10);
    //AddLogf("%s = HB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueHB);
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_month", sizeof(otTopic));
    sendMQTTData(otTopic, _msg);
    //flag8 valueLB
    utoa((OTdata.valueLB), _msg, 10);
    //AddLogf("%s = LB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueLB);
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_day_of_month", sizeof(otTopic));
    sendMQTTData(otTopic, _msg);
    value = OTdata.u16();
  }
}

void print_daytime(uint16_t& value)
{
  //function to print data
  static const char str_unknown[] PROGMEM = "Unknown";
  static const char str_monday[] PROGMEM = "Monday";
  static const char str_tuesday[] PROGMEM = "Tuesday";
  static const char str_wednesday[] PROGMEM = "Wednesday";
  static const char str_thursday[] PROGMEM = "Thursday";
  static const char str_friday[] PROGMEM = "Friday";
  static const char str_saturday[] PROGMEM = "Saturday";
  static const char str_sunday[] PROGMEM = "Sunday";
  static const char* const dayOfWeekName[] PROGMEM = { str_unknown, str_monday, str_tuesday, str_wednesday, str_thursday, str_friday, str_saturday, str_sunday, str_unknown };
  
  uint8_t dayIdx = (OTdata.valueHB >> 5) & 0x7;
  char dayName[15];
  strcpy_P(dayName, (PGM_P)pgm_read_ptr(&dayOfWeekName[dayIdx]));
  AddLogf("%s = %s - %.2d:%.2d", OTlookupitem.label, dayName, (OTdata.valueHB & 0x1F), OTdata.valueLB); 
  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    otTopic[0] = '\0';
    char _msg[10] {0};
    //dayofweek
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_dayofweek", sizeof(otTopic));
    sendMQTTData(otTopic, dayName); 
    //hour
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_hour", sizeof(otTopic));
    sendMQTTData(otTopic, itoa((OTdata.valueHB & 0x1F), _msg, 10)); 
    //min
    strlcpy(otTopic, messageIDToString(static_cast<OTLibMessageID>(OTdata.id)), sizeof(otTopic));
    strlcat(otTopic, "_minutes", sizeof(otTopic));
    sendMQTTData(otTopic, itoa((OTdata.valueLB), _msg, 10)); 
    value = OTdata.u16();
  }
}

//===================[ Command Queue implementation ]============================

#define OTGW_CMD_RETRY 5
#define OTGW_CMD_INTERVAL_MS 5000
#define SER2NET_QUIET_MS 2000       // queue pauses after ser2net activity
static uint32_t lastSer2netCmdMs = 0 - SER2NET_QUIET_MS;  // expired at boot
#define OTGW_DELAY_SEND_MS 1000
#define MAX_QUEUE_MSGSIZE 127

// Remove entry at index from the command queue, shifting remaining entries down.
static void removeFromCmdQueue(int index) {
  for (int j = index; j < (cmdQueueSize - 1); j++) {
    strlcpy(cmdqueue[j].cmd, cmdqueue[j+1].cmd, sizeof(cmdqueue[j].cmd));
    cmdqueue[j].cmdlen = cmdqueue[j+1].cmdlen;
    cmdqueue[j].retrycnt = cmdqueue[j+1].retrycnt;
    cmdqueue[j].due = cmdqueue[j+1].due;
  }
  cmdQueueSize--;
  cmdqueue[cmdQueueSize].cmd[0] = '\0';
  cmdqueue[cmdQueueSize].cmdlen = 0;
  cmdqueue[cmdQueueSize].retrycnt = 0;
  cmdqueue[cmdQueueSize].due = 0;
}

/*
  addCommandToQueue adds a command to the queue.
  First it checks the queue, if the command is in the queue, it's updated.
  Otherwise it's simply added to the queue, unless there are no free queue slots.
*/

//void addCommandToQueue(const char* buf, const int len, const bool forceQueue = false, const int16_t delay = OTGW_DELAY_SEND_MS);
void addCommandToQueue(const char* buf, const int len, const bool forceQueue, const int16_t delay){
  if (!isPICEnabled()) {
#if HAS_DIRECT_OT
    // On OTGW32, route commands directly to OT-direct handler
    if (isOTDirectEnabled()) {
      handleOTDirectCommand(buf, len);
      return;
    }
#endif
    OTDebugTln(F("CmdQueue: No PIC or OT-direct detected - command ignored"));
    return;
  }

  constexpr int kMaxCmdLen = (int)(sizeof(cmdqueue[0].cmd) - 1);

  if (buf == nullptr) {
    OTDebugTln(F("CmdQueue: Error: null command"));
    return;
  }
  if ((len < 3) || (buf[2] != '=')){ 
    // Not a valid command
    OTDebugT(F("CmdQueue: Error: Not a valid command=["));
    for (int i = 0; i < len; i++) {
      OTDebug((char)buf[i]);
    }
    OTDebugf(PSTR("] (%d)\r\n"), len);
    return;
  }
  if (len > kMaxCmdLen) {
    OTDebugTf(PSTR("CmdQueue: Error: command too long (%d > %d)\r\n"), len, kMaxCmdLen);
    OTDebugFlush();
    return;
  }

  //check to see if the cmd is in queue
  bool foundcmd = false;
  int8_t insertptr = cmdQueueSize; //set insertptr to next empty slot
  if (!forceQueue){
    char cmd[3];
    memset(cmd, 0, sizeof(cmd));
    memcpy(cmd, buf, 2);
    for (int i=0; i<cmdQueueSize; i++){
      if (cmdqueue[i].cmd[0] == cmd[0] && cmdqueue[i].cmd[1] == cmd[1]) {
        //found cmd exists, set the inertptr to found slot
        foundcmd = true;
        insertptr = i;
        break;
      }
    } 
  } 
  if (foundcmd) OTDebugTf(PSTR("CmdQueue: Found cmd exists in slot [%d]\r\n"), insertptr);
  else OTDebugTf(PSTR("CmdQueue: Adding cmd end of queue, slot [%d]\r\n"), insertptr);

  if (!foundcmd && cmdQueueSize >= CMDQUEUE_MAX) {
    OTDebugTln(F("CmdQueue: Error: Reached max queue"));
    OTDebugFlush();
    return;
  }
  if (insertptr < 0 || insertptr >= CMDQUEUE_MAX) {
    OTDebugTf(PSTR("CmdQueue: Error: Invalid insert slot [%d]\r\n"), insertptr);
    OTDebugFlush();
    return;
  }

  //insert to the queue
  OTDebugTf(PSTR("CmdQueue: Insert queue in slot[%d]:"), insertptr);
  OTDebug(F("cmd["));
  for (int i = 0; i < len; i++) {
    OTDebug((char)buf[i]);
  }
  OTDebugf(PSTR("] (%d)\r\n"), len); 

  //copy the command into the queue
  int cmdlen = min((int)len , (int)(sizeof(cmdqueue[insertptr].cmd)-1));
  memset(cmdqueue[insertptr].cmd, 0, cmdlen+1);
  memcpy(cmdqueue[insertptr].cmd, buf, cmdlen);
  cmdqueue[insertptr].cmdlen = cmdlen;
  cmdqueue[insertptr].retrycnt = 0;
  const uint32_t safeDelay = (delay <= 0) ? 0u : (uint32_t)delay;
  cmdqueue[insertptr].due = millis() + safeDelay;

  //if not found
  if (!foundcmd) {
    //if not reached max of queue
    if (cmdQueueSize < CMDQUEUE_MAX) {
      cmdQueueSize++; //next free slot
      OTDebugTf(PSTR("CmdQueue: Next free queue slot: [%d]\r\n"), cmdQueueSize);
    } else {
      // Should be prevented above; keep as defensive fallback.
      OTDebugTln(F("CmdQueue: Error: Reached max queue"));
    }
  } else OTDebugTf(PSTR("CmdQueue: Found command at: [%d] - [%d]\r\n"), insertptr, cmdQueueSize);

  // Trigger PIC settings re-read only for commands that modify readable PIC settings (PR=).
  // GW→PR=M, SB→PR=S, VR→PR=V, TS→PR=D, IT/OH→PR=T, GA/GB→PR=G, LA-LF→PR=L
  if (len >= 2) {
    char cmd[3] = { buf[0], buf[1], '\0' };
    static const char kSettingsCmds[] = "GW GA GB SB VR TS IT OH LA LB LC LD LE LF";
    if (strstr(kSettingsCmds, cmd)) {
      triggerPICsettingsReadout();
    }
  }

  OTDebugFlush();
}

/*
  handleCommandQueue should be called every second from main loop. 
  This checks the queue for message are due to be resent.
  If retry max is reached the cmd is delete from the queue
*/
void handleCommandQueue(){
  // Pause queue processing briefly after ser2net activity to avoid collisions
  if ((millis() - lastSer2netCmdMs) < SER2NET_QUIET_MS) return;
  const uint32_t now = millis();
  for (int i = 0; i < cmdQueueSize; i++) {
    // OTDebugTf(PSTR("CmdQueue: Checking due in queue slot[%d]:[%lu]=>[%lu]\r\n"), (int)i, (unsigned long)millis(), (unsigned long)cmdqueue[i].due);
    if ((int32_t)(now - cmdqueue[i].due) >= 0) {
      OTDebugTf(PSTR("CmdQueue: Queue slot [%d] due\r\n"), i);
      sendPICSerial(cmdqueue[i].cmd, cmdqueue[i].cmdlen);
      cmdqueue[i].retrycnt++;
      cmdqueue[i].due = now + OTGW_CMD_INTERVAL_MS;
      if (cmdqueue[i].retrycnt >= OTGW_CMD_RETRY){
        //max retry reached, so delete command from queue
        OTDebugTf(PSTR("CmdQueue: Delete [%d] from queue\r\n"), i);
        snprintf_P(cMsg, sizeof(cMsg), PSTR("%s [dropped]"), cmdqueue[i].cmd);
        sendEventToWebSocket('!', cMsg);
        removeFromCmdQueue(i);
        i--; // re-check current index after shift
      }
      // //exit queue handling, after 1 command
      // return;
    }
  }
  OTDebugFlush();
}

/*
  checkCommandResponse (buf, len)
  This takes response from otgw and checks to see if the command was accepted.
  Checks the response, and finds the command (if it's there).
  Then checks if incoming response matches what was to be set.
  Only then it's deleted from the queue.
*/
void checkCommandResponse(const char *buf, unsigned int len){
  if ((len<3) || (buf[2]!=':')) {
    OTDebugT(F("CmdQueue: Error: Not a command response ["));
    for (unsigned int i = 0; i < len; i++) {
      OTDebug((char)buf[i]);
    }
    OTDebugf(PSTR("] (len=%d)\r\n"), len);
    return; //not a valid command response
  }

  OTDebugT(F("CmdQueue: Checking if command is in queue ["));
  for (unsigned int i = 0; i < len; i++) {
    OTDebug((char)buf[i]);
  }
  OTDebugf(PSTR("] (len=%d)\r\n"), len);

  char cmd[3]; memset( cmd, 0, sizeof(cmd));
  char value[11]; memset( value, 0, sizeof(value));
  memcpy(cmd, buf, 2);
  memcpy(value, buf+3, ((len-3)<(sizeof(value)-1))?(len-3):(sizeof(value)-1));
  for (int i=0; i<cmdQueueSize; i++){
      OTDebugTf(PSTR("CmdQueue: Checking [%2s]==>[%d]:[%s] from queue\r\n"), cmd, i, cmdqueue[i].cmd);
    if (cmdqueue[i].cmd[0] == cmd[0] && cmdqueue[i].cmd[1] == cmd[1]){
      // For PR commands, also match the register letter
      // (e.g., response "PR: S=16.00" must match "PR=S" in queue, not "PR=O")
      // value starts at buf+3, which may have a leading space (e.g., " S=16.00")
      if (cmd[0] == 'P' && cmd[1] == 'R' && cmdqueue[i].cmdlen >= 4) {
        const char* reg = value;
        while (*reg == ' ') reg++;
        if (cmdqueue[i].cmd[3] != *reg) continue;
      }
      //command found
      OTDebugTf(PSTR("CmdQueue: Found cmd [%2s]==>[%d]:[%s]\r\n"), cmd, i, cmdqueue[i].cmd);
        OTDebugTf(PSTR("CmdQueue: Found value [%s]==>[%d]:[%s]\r\n"), value, i, cmdqueue[i].cmd);
        OTDebugTf(PSTR("CmdQueue: Remove from queue [%d]:[%s] from queue\r\n"), i, cmdqueue[i].cmd);
        removeFromCmdQueue(i);
        break;
      // } else OTDebugTf(PSTR("Error: Did not find value [%s]==>[%d]:[%s]\r\n"), value, i, cmdqueue[i].cmd); 
    }
  }
  OTDebugFlush();
}


//===================[ Send buffer to OTGW ]=============================
/*
  sendPICSerial(const char* buf, int len) sends a string to the serial OTGW device.
  The buffer is send out to OTGW on the serial device instantly, as long as there is space in the buffer.
*/
void sendPICSerial(const char* buf, int len)
{
  if (!isPICEnabled()) {
#if HAS_DIRECT_OT
    // On OTGW32, route commands through OT-direct command handler
    handleOTDirectCommand(buf, len);
#else
    OTDebugTln(F("sendPICSerial: No PIC detected - command ignored"));
#endif
    return;
  }
#if HAS_PIC
  // TASK-795 §4.1 path (a): block PIC serial when EITHER the legacy OTGW
  // serial-replay sim (bOTGWSimulation) OR the SAT boiler sim (bSimulation)
  // is active. The shared helper also captures the would-be command for the
  // §4.3 trace. buf may not be NUL-terminated at len, so copy bounded.
  {
    char picCmd[24];
    int  n = (len < (int)sizeof(picCmd) - 1) ? len : (int)sizeof(picCmd) - 1;
    if (n < 0) n = 0;
    memcpy(picCmd, buf, n);
    picCmd[n] = '\0';
    // strip a trailing CR/LF so the trace line stays single-line
    while (n > 0 && (picCmd[n - 1] == '\r' || picCmd[n - 1] == '\n')) picCmd[--n] = '\0';
    if (satSimulationBlocksBusTx(picCmd, F("pic-serial"))) return;
  }

  // TASK-865.6: the dedicated PIC task is the sole UART writer. Build the full
  // command (payload + CR/LF) into one chunk and enqueue it; the task pops it
  // and calls OTGWSerial.write()+flush(). The availableForWrite() flow-control
  // moves into the task (picSerialDrainOnce). Keep the WS '>' echo loop-side —
  // it is not byte I/O and preserves the existing OT-log surface.
  {
    uint8_t txbuf[OT_TX_CHUNK_MAX];
    int n = len;
    if (n > (int)sizeof(txbuf) - 2) n = (int)sizeof(txbuf) - 2;   // clamp (defensive)
    if (n < 0) n = 0;
    memcpy(txbuf, buf, n);
    txbuf[n]   = '\r';
    txbuf[n+1] = '\n';
    OTDebugT(F("Sending to Serial ["));
    for (int i = 0; i < len; i++) {
      OTDebug((char)buf[i]);
    }
    OTDebug(F("] (")); OTDebug(len); OTDebug(F(")")); OTDebugln();

    if (enqueuePICTx(txbuf, n + 2)) {
      sendEventToWebSocket('>', buf, len);
    } else {
      OTDebugln(F("Error: PIC TX queue full!"));
    }
  }
#endif
}

// dispatchOTGWInputLine — loop-side PIC-line enqueue helper. Used by the OTGW
// replay simulation path (the live PIC reader now enqueues directly from the
// dedicated task). Tags source=PIC; the consumer (drainOTFrameQueue) applies the
// LED blink + ser2net 25238 mirror, so both the live and simulated PIC paths get
// identical side-effects without the task touching the network/LED (TASK-865.6).
static void dispatchOTGWInputLine(const char* buf, size_t len)
{
  if (len == 0) return;
  enqueueOTFrame(buf, len, false, OTFRAME_SRC_PIC);
}

static bool readOTGWSimulationLine(File& replayFile, char* buffer, size_t bufferSize, size_t& lineLen)
{
  lineLen = 0;
  bool discardCurrentLine = false;

  while (replayFile.available()) {
    int inByte = replayFile.read();
    if (inByte < 0) break;

    char inChar = static_cast<char>(inByte);
    if (inChar == '\r' || inChar == '\n') {
      if (discardCurrentLine) {
        discardCurrentLine = false;
        lineLen = 0;
        continue;
      }
      if (lineLen == 0) continue;

      buffer[lineLen] = '\0';
      return true;
    }

    if (!discardCurrentLine) {
      if (lineLen < (bufferSize - 1)) {
        buffer[lineLen++] = inChar;
      } else {
        discardCurrentLine = true;
        lineLen = 0;
        DebugTln(F("OTGW simulation line too long, discarding line"));
      }
    }
    feedWatchDog();
  }

  if (!discardCurrentLine && lineLen > 0) {
    buffer[lineLen] = '\0';
    return true;
  }

  lineLen = 0;
  return false;
}

static void resetOTGWLineBuffers(size_t& bytesRead, size_t& bytesWrite, bool& discardCurrentReadLine)
{
  bytesRead = 0;
  bytesWrite = 0;
  discardCurrentReadLine = false;
}

static bool reopenOTGWSimulationFile(File& otgwSimulationFile)
{
  if (otgwSimulationFile) otgwSimulationFile.close();
  otgwSimulationFile = LittleFS.open(F("/otgw_simulation.log"), "r");
  return static_cast<bool>(otgwSimulationFile);
}

static bool replayNextOTGWSimulationLine(File& otgwSimulationFile, char* sReplay, size_t replaySize)
{
  size_t replayLen = 0;
  bool haveReplayLine = false;
  uint8_t replayPass = 0;

  while (!haveReplayLine && replayPass < 2) {
    haveReplayLine = readOTGWSimulationLine(otgwSimulationFile, sReplay, replaySize, replayLen);
    if (haveReplayLine) break;

    replayPass++;
    if (!reopenOTGWSimulationFile(otgwSimulationFile)) break;
  }

  if (haveReplayLine) {
    dispatchOTGWInputLine(sReplay, replayLen);
    state.debug.iOTGWSimulationNextDueMs = millis() + state.debug.iOTGWSimulationIntervalMs;
    return true;
  }

  return false;
}

static bool handlePICSerialSimulation(File& otgwSimulationFile,
                                 bool& otgwSimulationWasEnabled,
                                 size_t& bytesRead,
                                 size_t& bytesWrite,
                                 bool& discardCurrentReadLine,
                                 char* sReplay,
                                 size_t replaySize)
{
  if (!state.debug.bOTGWSimulation && otgwSimulationWasEnabled) {
    if (otgwSimulationFile) otgwSimulationFile.close();
    otgwSimulationWasEnabled = false;
    resetOTGWLineBuffers(bytesRead, bytesWrite, discardCurrentReadLine);
  }

  if (!state.debug.bOTGWSimulation) return false;

  if (!otgwSimulationWasEnabled) {
    if (otgwSimulationFile) otgwSimulationFile.close();
    otgwSimulationWasEnabled = true;
    resetOTGWLineBuffers(bytesRead, bytesWrite, discardCurrentReadLine);
    state.debug.iOTGWSimulationNextDueMs = 0;
  }

  #if HAS_PIC
  // TASK-865.6: discard live PIC RX while the replay simulation feeds the parser
  // from a file. Routed through the owner fn so all UART byte-I/O stays confined.
  // TASK-865.15: park the dedicated PIC task BEFORE the loop-side flush. On the
  // sim-mode entry tick bOTGWSimulation has only just flipped true, so the task
  // may still be mid-drain; without this handshake the loop-side flush and the
  // task drain race on OTGWSerial.read() for one tick. bOTGWSimulation is already
  // in picSerialTaskShouldPark(), so once parked waitForPICTaskParked() returns
  // on the first poll at zero cost on later ticks.
  waitForPICTaskParked();
  picSerialFlushRx();
  #endif

  if (!LittleFSmounted) {
    DebugTln(F("OTGW simulation disabled: LittleFS not mounted"));
    sendEventToWebSocket_P('!', PSTR("OTGW simulation disabled [LittleFS unavailable]"));
    state.debug.bOTGWSimulation = false;
    if (otgwSimulationFile) otgwSimulationFile.close();
    otgwSimulationWasEnabled = false;
    return true;
  }

  if (!otgwSimulationFile && !reopenOTGWSimulationFile(otgwSimulationFile)) {
    DebugTln(F("OTGW simulation disabled: /otgw_simulation.log not found"));
    sendEventToWebSocket_P('!', PSTR("OTGW simulation disabled [/otgw_simulation.log missing]"));
    state.debug.bOTGWSimulation = false;
    otgwSimulationWasEnabled = false;
    return true;
  }

  if ((state.debug.iOTGWSimulationNextDueMs == 0) || (static_cast<int32_t>(millis() - state.debug.iOTGWSimulationNextDueMs) >= 0)) {
    if (!replayNextOTGWSimulationLine(otgwSimulationFile, sReplay, replaySize) && !otgwSimulationFile) {
      DebugTln(F("OTGW simulation disabled: replay file reopen failed"));
      sendEventToWebSocket_P('!', PSTR("OTGW simulation disabled [replay file reopen failed]"));
      state.debug.bOTGWSimulation = false;
      otgwSimulationWasEnabled = false;
      return true;
    }
  }

  return false;
}

//===================[ PS=1 Summary Parsing ]===============

/*
  PS=1 (Print Summary) mode field-to-MsgID mapping tables.
  When in PS=1 mode, the OTGW PIC firmware outputs a single comma-separated summary
  line per OpenTherm cycle. Two formats exist:
    - Old firmware (< v5): 25 comma-separated fields (24 commas)
    - New firmware (v5+) : 34 comma-separated fields (33 commas)
  Each entry is the OpenTherm MsgID for the corresponding field position.
*/
static const uint8_t PSSUMMARY_MSGIDS_OLD[25] PROGMEM = {
  /*  0 */ 0,   // Status flags         (flag8/flag8)
  /*  1 */ 1,   // TSet                 (f88)
  /*  2 */ 6,   // RBPflags             (flag8/flag8)
  /*  3 */ 14,  // MaxRelModLevelSetting(f88)
  /*  4 */ 15,  // MaxCapacityMinModLevel (u8/u8)
  /*  5 */ 16,  // TrSet                (f88)
  /*  6 */ 17,  // RelModLevel          (f88)
  /*  7 */ 18,  // CHPressure           (f88)
  /*  8 */ 24,  // Tr                   (f88)
  /*  9 */ 25,  // Tboiler              (f88)
  /* 10 */ 26,  // Tdhw                 (f88)
  /* 11 */ 27,  // Toutside             (f88)
  /* 12 */ 28,  // Tret                 (f88)
  /* 13 */ 48,  // TdhwSetUBTdhwSetLB   (s8/s8)
  /* 14 */ 49,  // MaxTSetUBMaxTSetLB   (s8/s8)
  /* 15 */ 56,  // TdhwSet              (f88)
  /* 16 */ 57,  // MaxTSet              (f88)
  /* 17 */ 116, // BurnerStarts         (u16)
  /* 18 */ 117, // CHPumpStarts         (u16)
  /* 19 */ 118, // DHWPumpValveStarts   (u16)
  /* 20 */ 119, // DHWBurnerStarts      (u16)
  /* 21 */ 120, // BurnerOperationHours (u16)
  /* 22 */ 121, // CHPumpOperationHours (u16)
  /* 23 */ 122, // DHWPumpValveOperationHours (u16)
  /* 24 */ 123  // DHWBurnerOperationHours    (u16)
};

static const uint8_t PSSUMMARY_MSGIDS_NEW[34] PROGMEM = {
  /*  0 */ 0,   // Status flags              (flag8/flag8)
  /*  1 */ 1,   // TSet                      (f88)
  /*  2 */ 6,   // RBPflags                  (flag8/flag8)
  /*  3 */ 7,   // CoolingControl            (f88)     [new in v5+]
  /*  4 */ 8,   // TsetCH2                   (f88)     [new in v5+]
  /*  5 */ 14,  // MaxRelModLevelSetting     (f88)
  /*  6 */ 15,  // MaxCapacityMinModLevel    (u8/u8)
  /*  7 */ 16,  // TrSet                     (f88)
  /*  8 */ 17,  // RelModLevel               (f88)
  /*  9 */ 18,  // CHPressure                (f88)
  /* 10 */ 19,  // DHWFlowRate               (f88)     [new in v5+]
  /* 11 */ 23,  // TrSetCH2                  (f88)     [new in v5+]
  /* 12 */ 24,  // Tr                        (f88)
  /* 13 */ 25,  // Tboiler                   (f88)
  /* 14 */ 26,  // Tdhw                      (f88)
  /* 15 */ 27,  // Toutside                  (f88)
  /* 16 */ 28,  // Tret                      (f88)
  /* 17 */ 31,  // TflowCH2                  (f88)     [new in v5+]
  /* 18 */ 33,  // Texhaust                  (s16)     [new in v5+]
  /* 19 */ 48,  // TdhwSetUBTdhwSetLB        (s8/s8)
  /* 20 */ 49,  // MaxTSetUBMaxTSetLB        (s8/s8)
  /* 21 */ 56,  // TdhwSet                   (f88)
  /* 22 */ 57,  // MaxTSet                   (f88)
  /* 23 */ 70,  // StatusVH                  (flag8/flag8) [new in v5+]
  /* 24 */ 71,  // ControlSetpointVH         (u8)      [new in v5+]
  /* 25 */ 77,  // RelativeVentilation       (u8)      [new in v5+]
  /* 26 */ 116, // BurnerStarts              (u16)
  /* 27 */ 117, // CHPumpStarts              (u16)
  /* 28 */ 118, // DHWPumpValveStarts        (u16)
  /* 29 */ 119, // DHWBurnerStarts           (u16)
  /* 30 */ 120, // BurnerOperationHours      (u16)
  /* 31 */ 121, // CHPumpOperationHours      (u16)
  /* 32 */ 122, // DHWPumpValveOperationHours(u16)
  /* 33 */ 123  // DHWBurnerOperationHours   (u16)
};

static void enterPSMode(PGM_P debugMessage, PGM_P eventMessage, bool resetMsgLastUpdated)
{
  if (!state.otBus.bPSmode && debugMessage) {
    OTDebugTln(reinterpret_cast<const __FlashStringHelper*>(debugMessage));
  }

  state.otBus.bPSmode = true;
  state.statusMessage = StatusMessage::PSModeActive;

  if (resetMsgLastUpdated) {
    clearMsgLastUpdated();
  }

  if (eventMessage) {
    sendEventToWebSocket_P('*', eventMessage);
  }
}

static void leavePSMode(PGM_P debugMessage, PGM_P eventMessage)
{
  if (state.otBus.bPSmode && debugMessage) {
    OTDebugTln(reinterpret_cast<const __FlashStringHelper*>(debugMessage));
  }

  state.otBus.bPSmode = false;
  if (state.statusMessage == StatusMessage::PSModeActive) {
    state.statusMessage = StatusMessage::None;
  }

  if (eventMessage) {
    sendEventToWebSocket_P('*', eventMessage);
  }
}

static bool parseStrictSignedLong(const char *text, long minValue, long maxValue, long &value)
{
  if (!text || *text == '\0') return false;

  char *endPtr = nullptr;
  long parsedValue = strtol(text, &endPtr, 10);
  if ((endPtr == text) || (*endPtr != '\0') || (parsedValue < minValue) || (parsedValue > maxValue)) {
    return false;
  }

  value = parsedValue;
  return true;
}

static bool parseStrictUnsignedLong(const char *text, unsigned long maxValue, unsigned long &value)
{
  if (!text || *text == '\0' || *text == '-') return false;

  char *endPtr = nullptr;
  unsigned long parsedValue = strtoul(text, &endPtr, 10);
  if ((endPtr == text) || (*endPtr != '\0') || (parsedValue > maxValue)) {
    return false;
  }

  value = parsedValue;
  return true;
}

static bool parseStrictFloat(const char *text, float &value)
{
  if (!text || *text == '\0') return false;

  char *endPtr = nullptr;
  double parsedValue = strtod(text, &endPtr);
  if ((endPtr == text) || (*endPtr != '\0')) {
    return false;
  }

  value = static_cast<float>(parsedValue);
  return true;
}

static bool splitPSSummaryPair(const char *text, char separator,
                               char *left, size_t leftSize,
                               char *right, size_t rightSize)
{
  if (!text || !left || !right || leftSize == 0 || rightSize == 0) return false;

  const char *separatorPos = strchr(text, separator);
  if (!separatorPos || strchr(separatorPos + 1, separator)) return false;

  const size_t leftLen = static_cast<size_t>(separatorPos - text);
  const size_t rightLen = strlen(separatorPos + 1);
  if (leftLen == 0 || rightLen == 0 || leftLen >= leftSize || rightLen >= rightSize) return false;

  memcpy(left, text, leftLen);
  left[leftLen] = '\0';
  strlcpy(right, separatorPos + 1, rightSize);
  return true;
}

static bool parsePSSummaryS8S8(const char *text, int8_t &upperByte, int8_t &lowerByte)
{
  char left[12] {0};
  char right[12] {0};
  long leftValue = 0;
  long rightValue = 0;
  if (!splitPSSummaryPair(text, '/', left, sizeof(left), right, sizeof(right))) return false;
  if (!parseStrictSignedLong(left, -128, 127, leftValue)) return false;
  if (!parseStrictSignedLong(right, -128, 127, rightValue)) return false;
  upperByte = static_cast<int8_t>(leftValue);
  lowerByte = static_cast<int8_t>(rightValue);
  return true;
}

static bool parsePSSummaryU8U8(const char *text, uint8_t &upperByte, uint8_t &lowerByte)
{
  char left[12] {0};
  char right[12] {0};
  unsigned long leftValue = 0;
  unsigned long rightValue = 0;
  if (!splitPSSummaryPair(text, '/', left, sizeof(left), right, sizeof(right))) return false;
  if (!parseStrictUnsignedLong(left, 255UL, leftValue)) return false;
  if (!parseStrictUnsignedLong(right, 255UL, rightValue)) return false;
  upperByte = static_cast<uint8_t>(leftValue);
  lowerByte = static_cast<uint8_t>(rightValue);
  return true;
}

static bool parseBinaryOctet(const char *text, uint8_t &value)
{
  if (!text || strlen(text) != 8) return false;

  value = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (text[i] != '0' && text[i] != '1') return false;
    value = static_cast<uint8_t>((value << 1) | (text[i] - '0'));
  }
  return true;
}

static bool parsePSSummaryFlag8Flag8(const char *text, uint8_t &upperByte, uint8_t &lowerByte)
{
  char left[9] {0};
  char right[9] {0};
  if (!splitPSSummaryPair(text, '/', left, sizeof(left), right, sizeof(right))) return false;
  if (!parseBinaryOctet(left, upperByte)) return false;
  if (!parseBinaryOctet(right, lowerByte)) return false;
  return true;
}

static void updatePSSummaryFloatState(uint8_t msgid, float fval)
{
  switch (msgid) {
    case  1: OTcurrentSystemState.TSet                  = fval; break;
    case  7: OTcurrentSystemState.CoolingControl        = fval; break;
    case  8: OTcurrentSystemState.TsetCH2               = fval; break;
    case 14: OTcurrentSystemState.MaxRelModLevelSetting = fval; break;
    case 16: OTcurrentSystemState.TrSet                 = fval; break;
    case 17: OTcurrentSystemState.RelModLevel           = fval; break;
    case 18: OTcurrentSystemState.CHPressure            = fval; break;
    case 19: OTcurrentSystemState.DHWFlowRate           = fval; break;
    case 23: OTcurrentSystemState.TrSetCH2              = fval; break;
    case 24: OTcurrentSystemState.Tr                    = fval; break;
    case 25: OTcurrentSystemState.Tboiler               = fval; break;
    case 26: OTcurrentSystemState.Tdhw                  = fval; break;
    case 27: OTcurrentSystemState.Toutside              = fval; break;
    case 28: OTcurrentSystemState.Tret                  = fval; break;
    case 31: OTcurrentSystemState.TflowCH2              = fval; break;
    case 56: OTcurrentSystemState.TdhwSet               = fval; break;
    case 57: OTcurrentSystemState.MaxTSet               = fval; break;
    default: break;
  }
}

static void updatePSSummaryU16State(uint8_t msgid, uint16_t value)
{
  switch (msgid) {
    case 116: OTcurrentSystemState.BurnerStarts               = value; break;
    case 117: OTcurrentSystemState.CHPumpStarts               = value; break;
    case 118: OTcurrentSystemState.DHWPumpValveStarts         = value; break;
    case 119: OTcurrentSystemState.DHWBurnerStarts            = value; break;
    case 120: OTcurrentSystemState.BurnerOperationHours       = value; break;
    case 121: OTcurrentSystemState.CHPumpOperationHours       = value; break;
    case 122: OTcurrentSystemState.DHWPumpValveOperationHours = value; break;
    case 123: OTcurrentSystemState.DHWBurnerOperationHours    = value; break;
    default:  break;
  }
}

static void publishPSSummarySplitBytes(const char *label, const char *hbSuffix, const char *lbSuffix,
                                       const char *hbValue, const char *lbValue)
{
  char topicBuf[MQTT_TOPIC_MAX_LEN];
  strlcpy(topicBuf, label, sizeof(topicBuf));
  strlcat(topicBuf, hbSuffix, sizeof(topicBuf));
  sendMQTTData(topicBuf, hbValue);
  strlcpy(topicBuf, label, sizeof(topicBuf));
  strlcat(topicBuf, lbSuffix, sizeof(topicBuf));
  sendMQTTData(topicBuf, lbValue);
}

static void ensurePSSummaryDiscovery(uint8_t msgid)
{
  // Non-blocking: just mark pending; drainOnePendingDiscovery() publishes later.
  if (settings.mqtt.bEnable && !getMQTTConfigDone(msgid)) {
    setMQTTConfigPending(msgid);
  }
}

static void logPSSummaryField(const char *label, const char *rawField)
{
  ClrLog();
  AddLogf_P(PSTR("PS1 %-20s = %s"), label, rawField);
  AddLogln();
  sendLogToWebSocket(ot_log_buffer);
  ClrLog();
}

static bool publishPSSummaryFieldValue(uint8_t msgid, uint8_t valueType, const char *label, const char *rawField)
{
  char valueBuf[12] {0};
  const uint16_t trackedNow = currentTrackedSeconds();

  // ADR-097 (PS=1 amendment): Caller already populated the global OTlookupitem
  // via PROGMEM_readAnything(&OTmap[msgid], ...) before invoking us. Use it to
  // gate base-topic publication and OTcurrentSystemState updates so the PS=1
  // path matches the live OT-bus master-topic invariant. setMsgLastUpdated is
  // intentionally left ungated (cosmetic epoch tick, consistent with
  // OTGW-Core.ino:4034 in the live-bus path). The ot_flag8flag8 case keeps its
  // own per-MsgID handling (status-flag semantics) and is not gated here.
  const bool validForMaster = is_msgid_valid_for_master_topic_in_ps_summary(OTlookupitem);
  if (!validForMaster) {
    DebugTf(PSTR("PS=1 master-topic gate suppressed MsgID %u (%s): bSlaveEchoesValue=false\r\n"),
            msgid, label);
  }

  switch (valueType) {
    case ot_f88: {
      float value = 0.0f;
      if (!parseStrictFloat(rawField, value)) return false;
      dtostrf(value, 3, 2, valueBuf);
      if (validForMaster) sendMQTTData(label, valueBuf);
      setMsgLastUpdated(msgid, trackedNow);
      if (validForMaster) updatePSSummaryFloatState(msgid, value);
      return true;
    }

    case ot_s16: {
      long parsedValue = 0;
      if (!parseStrictSignedLong(rawField, -32768L, 32767L, parsedValue)) return false;
      itoa(static_cast<int16_t>(parsedValue), valueBuf, 10);
      if (validForMaster) sendMQTTData(label, valueBuf);
      setMsgLastUpdated(msgid, trackedNow);
      if (validForMaster && msgid == 33) OTcurrentSystemState.Texhaust = static_cast<int16_t>(parsedValue);
      return true;
    }

    case ot_u16: {
      unsigned long parsedValue = 0;
      if (!parseStrictUnsignedLong(rawField, 65535UL, parsedValue)) return false;
      utoa(static_cast<uint16_t>(parsedValue), valueBuf, 10);
      if (validForMaster) sendMQTTData(label, valueBuf);
      setMsgLastUpdated(msgid, trackedNow);
      if (validForMaster) updatePSSummaryU16State(msgid, static_cast<uint16_t>(parsedValue));
      return true;
    }

    case ot_s8s8: {
      int8_t upperByte = 0;
      int8_t lowerByte = 0;
      if (!parsePSSummaryS8S8(rawField, upperByte, lowerByte)) return false;
      char lowerValueBuf[12] {0};
      itoa(upperByte, valueBuf, 10);
      itoa(lowerByte, lowerValueBuf, 10);
      if (validForMaster) publishPSSummarySplitBytes(label, "_value_hb", "_value_lb", valueBuf, lowerValueBuf);
      setMsgLastUpdated(msgid, trackedNow);
      if (validForMaster) {
        if (msgid == 48) OTcurrentSystemState.TdhwSetUBTdhwSetLB = ((uint8_t)upperByte << 8) | (uint8_t)lowerByte;
        else if (msgid == 49) OTcurrentSystemState.MaxTSetUBMaxTSetLB = ((uint8_t)upperByte << 8) | (uint8_t)lowerByte;
      }
      return true;
    }

    case ot_u8u8: {
      uint8_t upperByte = 0;
      uint8_t lowerByte = 0;
      if (!parsePSSummaryU8U8(rawField, upperByte, lowerByte)) return false;
      char lowerValueBuf[12] {0};
      utoa(upperByte, valueBuf, 10);
      utoa(lowerByte, lowerValueBuf, 10);
      if (validForMaster) publishPSSummarySplitBytes(label, "_value_hb", "_value_lb", valueBuf, lowerValueBuf);
      setMsgLastUpdated(msgid, trackedNow);
      if (validForMaster && msgid == 15) OTcurrentSystemState.MaxCapacityMinModLevel = ((uint16_t)upperByte << 8) | lowerByte;
      return true;
    }

    case ot_u8: {
      unsigned long parsedValue = 0;
      if (!parseStrictUnsignedLong(rawField, 255UL, parsedValue)) return false;
      utoa(static_cast<uint8_t>(parsedValue), valueBuf, 10);
      if (validForMaster) sendMQTTData(label, valueBuf);
      setMsgLastUpdated(msgid, trackedNow);
      if (validForMaster) {
        if (msgid == 71) OTcurrentSystemState.ControlSetpointVH = static_cast<uint8_t>(parsedValue);
        else if (msgid == 77) OTcurrentSystemState.RelativeVentilation = static_cast<uint8_t>(parsedValue);
      }
      return true;
    }

    case ot_flag8flag8: {
      uint8_t upperByte = 0;
      uint8_t lowerByte = 0;
      if (!parsePSSummaryFlag8Flag8(rawField, upperByte, lowerByte)) return false;
      setMsgLastUpdated(msgid, trackedNow);
      switch (msgid) {
        case 0:
          OTcurrentSystemState.Statusflags = publishCombinedStatusState(upperByte, lowerByte);
          break;
        case 6: {
          // TASK-401: feed previous HB/LB into publishRBPFlagsState so per-bit gate works via PS summary path too.
          const uint16_t prevCombined = OTcurrentSystemState.RBPflags;
          const uint8_t  prevTransfer  = (uint8_t)((prevCombined >> 8) & 0xFF);
          const uint8_t  prevReadWrite = (uint8_t)(prevCombined & 0xFF);
          OTcurrentSystemState.RBPflags = publishRBPFlagsState(upperByte, lowerByte, prevTransfer, prevReadWrite);
          break;
        }
        case 70:
          OTcurrentSystemState.StatusVH = publishCombinedStatusVHState(upperByte, lowerByte);
          break;
        default:
          sendMQTTData(label, rawField);
          break;
      }
      return true;
    }

    default:
      return false;
  }
}

/*
  Process a PS=1 (Print Summary) comma-separated summary line from the OTGW PIC firmware.
  Parses each field, updates OTcurrentSystemState, and publishes to MQTT.
  Old firmware (< v5): 25 fields / 24 commas.
  New firmware (v5+) : 34 fields / 33 commas.
*/
void processPSSummary(const char *buf, int len) {
  int commaCount = 0;
  for (int i = 0; i < len; i++) {
    if (buf[i] == ',') commaCount++;
  }

  const bool bFW5 = (commaCount == 33);
  if (commaCount != 24 && commaCount != 33) return;

  enterPSMode(PSTR("PS mode auto-detected as ON (comma-separated summary)"), nullptr, false);

  const uint8_t *msgIdTable = bFW5 ? PSSUMMARY_MSGIDS_NEW : PSSUMMARY_MSGIDS_OLD;
  const uint8_t tableSize   = bFW5 ? 34 : 25;
  const char *p = buf;
  const char *end = buf + len;
  int idx = 0;
  char fBuf[22];

  while (p <= end && idx < tableSize) {
    const char *comma = (const char*)memchr(p, ',', end - p);
    const int fieldLen = (comma != nullptr) ? (int)(comma - p) : (int)(end - p);

    if (fieldLen > 0 && fieldLen < (int)sizeof(fBuf)) {
      memcpy(fBuf, p, fieldLen);
      fBuf[fieldLen] = '\0';

      const uint8_t msgid = pgm_read_byte(&msgIdTable[idx]);
      if (msgid <= OT_MSGID_MAX) {
        PROGMEM_readAnything(&OTmap[msgid], OTlookupitem);
        const char *label = OTlookupitem.label;
        // ADR-104 Decision item 7: scope mqttPendingSlot commit to this frame.
        // shouldPublishMQTTForPSField installs pending; capture the pre-publish
        // success count, run the publish path, then commit if any sendMQTTData
        // succeeded — else clear the pending so a later unrelated publish
        // cannot silently commit it.
        const uint32_t preSuccessCount = mqttSendSuccessCount;
        OTPublishGate psGate(shouldPublishMQTTForPSField(msgid));

        if (publishPSSummaryFieldValue(msgid, OTlookupitem.type, label, fBuf)) {
          ensurePSSummaryDiscovery(msgid);
          logPSSummaryField(label, fBuf);
        }

        if (mqttPendingSlot.pending) {
          if (mqttSendSuccessCount > preSuccessCount) confirmMQTTPublishSlot();
          else                                        mqttPendingSlot.pending = false;
        }
      }
    }

    if (comma == nullptr) break;
    p = comma + 1;
    idx++;
  }

  OTDebugTf(PSTR("PS=1 summary parsed: %d fields (%s firmware)\r\n"), idx + 1, bFW5 ? "v5+" : "<v5");
}

//===================[ OT Message Processing ]===============

/*
  This function checks if the string received is a valid "raw OT message".
  Raw OTmessages are 9 chars long and start with TBARE when talking to OTGW PIC.
  Message is not an OTmessage if length is not 9 long OR 3th char is ':' (= OTGW command response)
*/
bool isvalidotmsg(const char *buf, int len){
  bool _ret =  (len==9);    //check 9 chars long
  _ret &= (buf[2]!=':');    //not a otgw command response 
  char c = buf[0];
  _ret &= (c=='T' || c=='B' || c=='A' || c=='R' || c=='E'); //1 char matches any of 'B', 'T', 'A', 'R' or 'E'
  return _ret;
}

static bool decodeAndPublishStatusAndConfigValue(OTLibMessageID msgId)
{
  switch (msgId) {
    case OT_Statusflags:                            print_status(OTcurrentSystemState.Statusflags); return true;
    case OT_ASFflags:                               print_ASFflags(OTcurrentSystemState.ASFflags); return true;
    case OT_MasterConfigMemberIDcode:               print_mastermemberid(OTcurrentSystemState.MasterConfigMemberIDcode); return true;
    case OT_SlaveConfigMemberIDcode:                print_slavememberid(OTcurrentSystemState.SlaveConfigMemberIDcode); return true;
    case OT_Command:                                print_command(OTcurrentSystemState.Command );  return true;
    case OT_RBPflags:                               print_RBPflags(OTcurrentSystemState.RBPflags); return true;
    case OT_TSP:                                    print_u8u8(OTcurrentSystemState.TSP); return true;
    case OT_TSPindexTSPvalue:                       print_u8u8(OTcurrentSystemState.TSPindexTSPvalue); return true;
    case OT_FHBsize:                                print_u8u8(OTcurrentSystemState.FHBsize); return true;
    case OT_FHBindexFHBvalue:                       print_u8u8(OTcurrentSystemState.FHBindexFHBvalue); return true;
    case OT_MaxCapacityMinModLevel:                 print_u8u8(OTcurrentSystemState.MaxCapacityMinModLevel); return true;
    case OT_Date:                                   print_date(OTcurrentSystemState.Date); return true;
    case OT_Year:                                   print_u16(OTcurrentSystemState.Year); return true;
    case OT_TdhwSetUBTdhwSetLB:                     print_s8s8(OTcurrentSystemState.TdhwSetUBTdhwSetLB ); return true;
    case OT_MaxTSetUBMaxTSetLB:                     print_s8s8(OTcurrentSystemState.MaxTSetUBMaxTSetLB); return true;
    case OT_HcratioUBHcratioLB:                     print_s8s8(OTcurrentSystemState.HcratioUBHcratioLB); return true;
    case OT_Remoteparameter4boundaries:             print_s8s8(OTcurrentSystemState.Remoteparameter4boundaries); return true;
    case OT_Remoteparameter5boundaries:             print_s8s8(OTcurrentSystemState.Remoteparameter5boundaries); return true;
    case OT_Remoteparameter6boundaries:             print_s8s8(OTcurrentSystemState.Remoteparameter6boundaries); return true;
    case OT_Remoteparameter7boundaries:             print_s8s8(OTcurrentSystemState.Remoteparameter7boundaries); return true;
    case OT_Remoteparameter8boundaries:             print_s8s8(OTcurrentSystemState.Remoteparameter8boundaries); return true;
    case OT_RemoteOverrideFunction:                 print_remoteoverridefunction(OTcurrentSystemState.RemoteOverrideFunction); return true;
    case OT_OEMDiagnosticCode:                      print_u16(OTcurrentSystemState.OEMDiagnosticCode); return true;
    case OT_OpenThermVersionMaster:                 print_f88(OTcurrentSystemState.OpenThermVersionMaster); return true;
    case OT_OpenThermVersionSlave:                  print_f88(OTcurrentSystemState.OpenThermVersionSlave); return true;
    case OT_MasterVersion:                          print_u8u8(OTcurrentSystemState.MasterVersion ); return true;
    case OT_SlaveVersion:                           print_u8u8(OTcurrentSystemState.SlaveVersion); return true;
    case OT_Brand:                                  print_u8u8(OTcurrentSystemState.Brand); return true;
    case OT_BrandVersion:                           print_u8u8(OTcurrentSystemState.BrandVersion); return true;
    case OT_BrandSerialNumber:                      print_u8u8(OTcurrentSystemState.BrandSerialNumber); return true;
    default:
      return false;
  }
}

static bool decodeAndPublishTemperatureAndSensorValue(OTLibMessageID msgId)
{
  switch (msgId) {
    case OT_TSet:                                   print_f88(OTcurrentSystemState.TSet); return true;
    case OT_CoolingControl:                         print_f88(OTcurrentSystemState.CoolingControl); return true;
    case OT_TsetCH2:                                print_f88(OTcurrentSystemState.TsetCH2); return true;
    case OT_TrOverride:                             print_f88(OTcurrentSystemState.TrOverride); return true;
    case OT_MaxRelModLevelSetting:                  print_f88(OTcurrentSystemState.MaxRelModLevelSetting); return true;
    case OT_TrSet:                                  print_f88(OTcurrentSystemState.TrSet); return true;
    case OT_TrSetCH2:                               print_f88(OTcurrentSystemState.TrSetCH2); return true;
    case OT_RelModLevel:                            print_f88(OTcurrentSystemState.RelModLevel); return true;
    case OT_CHPressure:                             print_f88(OTcurrentSystemState.CHPressure); return true;
    case OT_DHWFlowRate:                            print_f88(OTcurrentSystemState.DHWFlowRate); return true;
    case OT_Tr:                                     print_f88(OTcurrentSystemState.Tr); return true;
    case OT_Tboiler:                                print_f88(OTcurrentSystemState.Tboiler);return true;
    case OT_Tdhw:                                   print_f88(OTcurrentSystemState.Tdhw); return true;
    case OT_Toutside:                               print_f88(OTcurrentSystemState.Toutside); return true;
    case OT_Tret:                                   print_f88(OTcurrentSystemState.Tret); return true;
    case OT_Tsolarstorage:                          print_f88(OTcurrentSystemState.Tsolarstorage); return true;
    case OT_Tsolarcollector:                        print_s16(OTcurrentSystemState.Tsolarcollector); return true;
    case OT_TflowCH2:                               print_f88(OTcurrentSystemState.TflowCH2); return true;
    case OT_Tdhw2:                                  print_f88(OTcurrentSystemState.Tdhw2 ); return true;
    case OT_Texhaust:                               print_s16(OTcurrentSystemState.Texhaust); return true;
    case OT_Theatexchanger:                         print_f88(OTcurrentSystemState.Theatexchanger); return true;
    case OT_TdhwSet:                                print_f88(OTcurrentSystemState.TdhwSet); return true;
    case OT_MaxTSet:                                print_f88(OTcurrentSystemState.MaxTSet); return true;
    case OT_Hcratio:                                print_f88(OTcurrentSystemState.Hcratio); return true;
    case OT_Remoteparameter4:                       print_f88(OTcurrentSystemState.Remoteparameter4); return true;
    case OT_Remoteparameter5:                       print_f88(OTcurrentSystemState.Remoteparameter5); return true;
    case OT_Remoteparameter6:                       print_f88(OTcurrentSystemState.Remoteparameter6); return true;
    case OT_Remoteparameter7:                       print_f88(OTcurrentSystemState.Remoteparameter7); return true;
    case OT_Remoteparameter8:                       print_f88(OTcurrentSystemState.Remoteparameter8); return true;
    case OT_BurnerStarts:                           print_u16(OTcurrentSystemState.BurnerStarts); return true;
    case OT_CHPumpStarts:                           print_u16(OTcurrentSystemState.CHPumpStarts); return true;
    case OT_DHWPumpValveStarts:                     print_u16(OTcurrentSystemState.DHWPumpValveStarts); return true;
    case OT_DHWBurnerStarts:                        print_u16(OTcurrentSystemState.DHWBurnerStarts); return true;
    case OT_BurnerOperationHours:                   print_u16(OTcurrentSystemState.BurnerOperationHours); return true;
    case OT_CHPumpOperationHours:                   print_u16(OTcurrentSystemState.CHPumpOperationHours); return true;
    case OT_DHWPumpValveOperationHours:             print_u16(OTcurrentSystemState.DHWPumpValveOperationHours); return true;
    case OT_DHWBurnerOperationHours:                print_u16(OTcurrentSystemState.DHWBurnerOperationHours); return true;
    case OT_FanSpeed:                               print_u8u8(OTcurrentSystemState.FanSpeed); return true;
    case OT_ElectricalCurrentBurnerFlame:           print_f88(OTcurrentSystemState.ElectricalCurrentBurnerFlame); return true;
    case OT_TRoomCH2:                               print_f88(OTcurrentSystemState.TRoomCH2); return true;
    case OT_RelativeHumidity:                       print_f88(OTcurrentSystemState.RelativeHumidity); return true;
    case OT_TrOverride2:                            print_f88(OTcurrentSystemState.TrOverride2); return true;
    case OT_CoolingOperationHours:                  print_u16(OTcurrentSystemState.CoolingOperationHours); return true;
    case OT_PowerCycles:                            print_u16(OTcurrentSystemState.PowerCycles); return true;
    case OT_ElectricityProducerStarts:              print_u16(OTcurrentSystemState.ElectricityProducerStarts); return true;
    case OT_ElectricityProducerHours:               print_u16(OTcurrentSystemState.ElectricityProducerHours); return true;
    case OT_ElectricityProduction:                  print_u16(OTcurrentSystemState.ElectricityProduction); return true;
    case OT_CumulativeElectricityProduction:        print_u16(OTcurrentSystemState.CumulativeElectricityProduction); return true;
    case OT_BurnerUnsuccessfulStarts:               print_u16(OTcurrentSystemState.BurnerUnsuccessfulStarts); return true;
    case OT_FlameSignalTooLow:                      print_u16(OTcurrentSystemState.FlameSignalTooLow); return true;
    default:
      return false;
  }
}

static bool decodeAndPublishVentilationValue(OTLibMessageID msgId)
{
  switch (msgId) {
    case OT_StatusVH:                               print_statusVH(OTcurrentSystemState.StatusVH); return true;
    case OT_ControlSetpointVH:                      print_u8_lb(OTcurrentSystemState.ControlSetpointVH); return true;
    case OT_ASFFaultCodeVH:                         print_flag8u8(OTcurrentSystemState.ASFFaultCodeVH); return true;
    case OT_DiagnosticCodeVH:                       print_u16(OTcurrentSystemState.DiagnosticCodeVH); return true;
    case OT_ConfigMemberIDVH:                       print_vh_configmemberid(OTcurrentSystemState.ConfigMemberIDVH); return true;
    case OT_OpenthermVersionVH:                     print_f88(OTcurrentSystemState.OpenthermVersionVH); return true;
    case OT_VersionTypeVH:                          print_u8u8(OTcurrentSystemState.VersionTypeVH ); return true;
    case OT_RelativeVentilation:                    print_u8_lb(OTcurrentSystemState.RelativeVentilation); return true;
    case OT_RelativeHumidityExhaustAir:             print_u8_lb(OTcurrentSystemState.RelativeHumidityExhaustAir); return true;
    case OT_CO2LevelExhaustAir:                     print_u16(OTcurrentSystemState.CO2LevelExhaustAir); return true;
    case OT_SupplyInletTemperature:                 print_f88(OTcurrentSystemState.SupplyInletTemperature); return true;
    case OT_SupplyOutletTemperature:                print_f88(OTcurrentSystemState.SupplyOutletTemperature); return true;
    case OT_ExhaustInletTemperature:                print_f88(OTcurrentSystemState.ExhaustInletTemperature); return true;
    case OT_ExhaustOutletTemperature:               print_f88(OTcurrentSystemState.ExhaustOutletTemperature); return true;
    case OT_ActualExhaustFanSpeed:                  print_u16(OTcurrentSystemState.ActualExhaustFanSpeed); return true;
    case OT_ActualSupplyFanSpeed:                   print_u16(OTcurrentSystemState.ActualSupplyFanSpeed); return true;
    case OT_RemoteParameterSettingVH:               print_vh_remoteparametersetting(OTcurrentSystemState.RemoteParameterSettingVH); return true;
    case OT_NominalVentilationValue:                print_u8_hb(OTcurrentSystemState.NominalVentilationValue); return true;
    case OT_TSPNumberVH:                            print_u8u8(OTcurrentSystemState.TSPNumberVH); return true;
    case OT_TSPEntryVH:                             print_u8u8(OTcurrentSystemState.TSPEntryVH); return true;
    case OT_FaultBufferSizeVH:                      print_u8u8(OTcurrentSystemState.FaultBufferSizeVH); return true;
    case OT_FaultBufferEntryVH:                     print_u8u8(OTcurrentSystemState.FaultBufferEntryVH); return true;
    default:
      return false;
  }
}

static bool decodeAndPublishSpecialValue(OTLibMessageID msgId)
{
  switch (msgId) {
    case OT_DayTime:                                print_daytime(OTcurrentSystemState.DayTime); return true;
    case OT_RFstrengthbatterylevel:                 print_rf_sensor_status_information(OTcurrentSystemState.RFstrengthbatterylevel); return true;
    case OT_OperatingMode_HC1_HC2_DHW:              print_remote_override_operating_mode(OTcurrentSystemState.OperatingMode_HC1_HC2_DHW ); return true;
    default:
      return false;
  }
}

static bool decodeAndPublishSolarStorageValue(OTLibMessageID msgId)
{
  switch (msgId) {
    case OT_SolarStorageMaster:                     print_solar_storage_status(OTcurrentSystemState.SolarStorageStatus ); return true;
    case OT_SolarStorageASFflags:                   print_flag8u8(OTcurrentSystemState.SolarStorageASFflags); return true;
    case OT_SolarStorageSlaveConfigMemberIDcode:    print_solarstorage_slavememberid(OTcurrentSystemState.SolarStorageSlaveConfigMemberIDcode); return true;
    case OT_SolarStorageVersionType:                print_u8u8(OTcurrentSystemState.SolarStorageVersionType); return true;
    case OT_SolarStorageTSP:                        print_u8u8(OTcurrentSystemState.SolarStorageTSP ); return true;
    case OT_SolarStorageTSPindexTSPvalue:           print_u8u8(OTcurrentSystemState.SolarStorageTSPindexTSPvalue ); return true;
    case OT_SolarStorageFHBsize:                    print_u8u8(OTcurrentSystemState.SolarStorageFHBsize ); return true;
    case OT_SolarStorageFHBindexFHBvalue:           print_u8u8(OTcurrentSystemState.SolarStorageFHBindexFHBvalue ); return true;
    default:
      return false;
  }
}

static bool decodeAndPublishVendorValue(OTLibMessageID msgId)
{
  switch (msgId) {
    case OT_RemehadFdUcodes:                        print_u8u8(OTcurrentSystemState.RemehadFdUcodes); return true;
    case OT_RemehaServicemessage:                   print_u8u8(OTcurrentSystemState.RemehaServicemessage); return true;
    case OT_RemehaDetectionConnectedSCU:            print_u8u8(OTcurrentSystemState.RemehaDetectionConnectedSCU); return true;
    default:
      return false;
  }
}
static void decodeAndPublishOTValue()
{
  if (isMsgIdReservedInActiveProfile(OTdata.id)) {
    char activeProfileName[20] {0};
    copyProgmemString(activeProfileName, sizeof(activeProfileName), activeOTSpecProfileName_P());
    AddLogf_P(PSTR("Reserved in %s profile (legacy pre-v4.2 ID %u ignored)"),
              activeProfileName,
              (unsigned)OTdata.id);
    return;
  }

  const OTLibMessageID msgId = static_cast<OTLibMessageID>(OTdata.id);

  if (decodeAndPublishStatusAndConfigValue(msgId)) return;
  if (decodeAndPublishTemperatureAndSensorValue(msgId)) return;
  if (decodeAndPublishVentilationValue(msgId)) return;
  if (decodeAndPublishSpecialValue(msgId)) return;
  if (decodeAndPublishSolarStorageValue(msgId)) return;
  if (decodeAndPublishVendorValue(msgId)) return;

  AddLogf("Unknown message [%02d] value [%04X] f8.8 [%3.2f] u16 [%d] s16 [%d]",
          OTdata.id,
          OTdata.value,
          OTdata.f88(),
          OTdata.u16(),
          OTdata.s16());
}

// PIC status / error tokens emitted by OTGW firmware as bare lines on the
// serial bus. Each entry collapses an "else if (strcmp_P) { Debugln + report }"
// branch into a single table row; the lookup loop in handlePICStatusToken()
// replaces 12 hand-written branches with one walk. All strings live in PROGMEM.
namespace {
  const char picTok_NG[]    PROGMEM = "NG";
  const char picTok_SE[]    PROGMEM = "SE";
  const char picTok_BV[]    PROGMEM = "BV";
  const char picTok_OR[]    PROGMEM = "OR";
  const char picTok_NS[]    PROGMEM = "NS";
  const char picTok_NF[]    PROGMEM = "NF";
  const char picTok_OE[]    PROGMEM = "OE";
  const char picTok_TDis[]  PROGMEM = "Thermostat disconnected";
  const char picTok_TCon[]  PROGMEM = "Thermostat connected";
  const char picTok_PwrL[]  PROGMEM = "Low power";
  const char picTok_PwrM[]  PROGMEM = "Medium power";
  const char picTok_PwrH[]  PROGMEM = "High power";

  const char picMsg_NG[]    PROGMEM = "NG - No Good. The command code is unknown.";
  const char picMsg_SE[]    PROGMEM = "SE - Syntax Error. The command contained an unexpected character or was incomplete.";
  const char picMsg_BV[]    PROGMEM = "BV - Bad Value. The command contained a data value that is not allowed.";
  const char picMsg_OR[]    PROGMEM = "OR - Out of Range. A number was specified outside of the allowed range.";
  const char picMsg_NS[]    PROGMEM = "NS - No Space. The alternative Data-ID could not be added because the table is full.";
  const char picMsg_NF[]    PROGMEM = "NF - Not Found. The specified alternative Data-ID could not be removed because it does not exist in the table.";
  const char picMsg_OE[]    PROGMEM = "OE - Overrun Error. The processor was busy and failed to process all received characters.";

  struct PICStatusEntry {
    const char *token;     // PROGMEM
    const char *message;   // PROGMEM (may equal token for short events)
    char        severity;  // '!' for errors, '*' for state events
  };

  const PICStatusEntry kPICStatusTokens[] PROGMEM = {
    { picTok_NG,   picMsg_NG,   '!' },
    { picTok_SE,   picMsg_SE,   '!' },
    { picTok_BV,   picMsg_BV,   '!' },
    { picTok_OR,   picMsg_OR,   '!' },
    { picTok_NS,   picMsg_NS,   '!' },
    { picTok_NF,   picMsg_NF,   '!' },
    { picTok_OE,   picMsg_OE,   '!' },
    { picTok_TDis, picTok_TDis, '*' },
    { picTok_TCon, picTok_TCon, '*' },
    { picTok_PwrL, picTok_PwrL, '*' },
    { picTok_PwrM, picTok_PwrM, '*' },
    { picTok_PwrH, picTok_PwrH, '*' },
  };
  const size_t kPICStatusTokenCount = sizeof(kPICStatusTokens) / sizeof(kPICStatusTokens[0]);
}

// Returns true if buf matches one of the table entries and the event has been
// reported. Callers fall through to the next branch when this returns false.
static bool handlePICStatusToken(const char *buf) {
  for (size_t i = 0; i < kPICStatusTokenCount; i++) {
    PGM_P tok = (PGM_P) pgm_read_ptr(&kPICStatusTokens[i].token);
    if (strcmp_P(buf, tok) == 0) {
      PGM_P msg = (PGM_P) pgm_read_ptr(&kPICStatusTokens[i].message);
      char sev = (char) pgm_read_byte(&kPICStatusTokens[i].severity);
      Debugln(reinterpret_cast<const __FlashStringHelper*>(msg));
      reportOTGWEvent_P(msg, sev, true);
      return true;
    }
  }
  return false;
}

/*
  Process OTGW messages coming from the PIC.
  It knows about:
  - raw OTmsg format
  - error format
  - ...
*/
// TASK-691 / TASK-692 / TASK-693 port (dev TASK-685/686/688): per-msgID bitmaps
// recording each side of the OT bus's observed capability. Populated in
// processOT (idempotent set on every observation). File scope so REST/MQTT/
// file-persistence layers can read them through the accessors below.
//
// Memory: 6 * 32 = 192 B bitmaps + 3 B dirty flags + 32 B scratch = 227 B.
//
// Persistence (TASK-693): the *FileDirty flags drive 15-min debounced atomic
// writes to /ot-thermo.json and /ot-boiler.json (saveOtSupportFilesIfDirty).
// boilerUnsupportedDirty stays independent because the MQTT republish has its
// own (1-min) cadence and consumes only the unsupported subset.
static uint8_t boilerLastMasterWasWrite[32] = {0};  // scratch — not persisted
static uint8_t boilerUnsupportedRead[32]    = {0};
static uint8_t boilerUnsupportedWrite[32]   = {0};
static uint8_t boilerAckedRead[32]          = {0};
static uint8_t boilerAckedWrite[32]         = {0};
static uint8_t thermostatSentRead[32]       = {0};
static uint8_t thermostatSentWrite[32]      = {0};
static bool boilerUnsupportedDirty = false;  // MQTT CSV republish gate (1-min cadence)
static bool boilerFileDirty        = false;  // /ot-boiler.json   write gate (15-min cadence)
static bool thermostatFileDirty    = false;  // /ot-thermo.json   write gate (15-min cadence)

bool isBoilerMsgIdUnsupportedRead(uint8_t id) {
  return (boilerUnsupportedRead[id >> 3] & (uint8_t)(1u << (id & 7))) != 0;
}
bool isBoilerMsgIdUnsupportedWrite(uint8_t id) {
  return (boilerUnsupportedWrite[id >> 3] & (uint8_t)(1u << (id & 7))) != 0;
}
bool isBoilerMsgIdAckedRead(uint8_t id) {
  return (boilerAckedRead[id >> 3] & (uint8_t)(1u << (id & 7))) != 0;
}
bool isBoilerMsgIdAckedWrite(uint8_t id) {
  return (boilerAckedWrite[id >> 3] & (uint8_t)(1u << (id & 7))) != 0;
}
bool isThermostatMsgIdSentRead(uint8_t id) {
  return (thermostatSentRead[id >> 3] & (uint8_t)(1u << (id & 7))) != 0;
}
bool isThermostatMsgIdSentWrite(uint8_t id) {
  return (thermostatSentWrite[id >> 3] & (uint8_t)(1u << (id & 7))) != 0;
}
bool getBoilerUnsupportedDirty()   { return boilerUnsupportedDirty; }
void clearBoilerUnsupportedDirty() { boilerUnsupportedDirty = false; }

void processOT(const char *buf, int len, bool suppressOutput){
  // TASK-865.5 (ADR-123 Phase-1): processOT() is THE writer of the decoded
  // OTGWState snapshot (OTcurrentSystemState.*, state.otBus.*). Acquire the
  // OTStateLock here — covering ALL processOT call sites uniformly: the queue
  // consumer (drainOTFrameQueue) AND the four OTDirect command-response
  // synthesis sites (otDirectBridgeProcessStatus, stats-line builder,
  // synthesizeResponse, otDirectBridgeProcessPRResponse) that call processOT()
  // directly. In seq6 the consumer runs on the PIC task while those synthesis
  // calls run on the loop task, so processOT executes from two tasks — the lock
  // serialises them against each other and against the restAPI reader.
  // Non-recursive: processOT's callees must NOT re-take OTStateLock (verified:
  // only sendOTmonitorV2 acquires it, and processOT does not call it).
  OTStateLock stateLock;

  // suppressOutput (TASK-293): when true, skip per-frame output paths and the
  // auto-leave-PS-mode heuristic. State updates, decoded value publishing,
  // and OT state flag writes still run so MQTT/SAT/WebUI values stay fresh.
  // Set by bridgeFrameToParser() on ESP32 OT-direct when PS=1 is active; the
  // PIC does the equivalent internally on ESP8266.
  // Per-link last-seen now lives in state.otBus (tBoilerLastSeen/tThermostatLastSeen)
  // so /api/v2/health can emit per-link recency for the v2 connectivity degraded/stale
  // state (ADR-155). Same single source of truth feeds the 30 s connected window below.
  static bool bOTGWboilerpreviousstate = false;
  static bool bOTGWthermostatpreviousstate = false;
  static bool bOTGWpreviousstate = false;
  time_t now = time(nullptr);

  if (isvalidotmsg(buf, len)) {
    // Raw OT frames normally indicate PS=0 (streaming resumed). Skip this
    // auto-leave path when the caller explicitly suppresses output: in
    // OT-direct PS=1 we synthesise raw frames ourselves, so seeing them
    // does not mean the PIC/firmware left PS mode.
    if (state.otBus.bPSmode && !suppressOutput) {
      leavePSMode(PSTR("PS mode auto-detected as OFF (raw OT stream resumed)"),
                  PSTR("PS=0 [auto-detected, raw mode resumed]"));
    }

    // Update LED heartbeat timestamp — resets the "no OT" warning
    lastOTmsgMs = millis();

    //OT protocol messages are 9 chars long
    if (!suppressOutput && settings.mqtt.bOTmessage) sendMQTTData(F("otmessage"), buf);

    // counter of number of OT messages processed
    static int32_t cntOTmessagesprocessed = 0;
    cntOTmessagesprocessed++;
    // char _msg[15] {0};
    // sendMQTTData(F("otmsg_count"), itoa(cntOTmessagesprocessed, _msg, 10)); 

    // source of otmsg
    if (buf[0]=='B'){
      state.otBus.tBoilerLastSeen = now;
      OTdata.rsptype = OTGW_BOILER;
      // TASK-795 §4.2: a real boiler frame arrived on the PIC bus. If SAT
      // simulation is active, trip the edge hook (deferred auto-disable).
      satNotifyBoilerFrameSeen();
    } else if (buf[0]=='T'){
      state.otBus.tThermostatLastSeen = now;
      OTdata.rsptype = OTGW_THERMOSTAT;
    } else if (buf[0]=='R')    {
      OTdata.rsptype = OTGW_REQUEST_BOILER;
    } else if (buf[0]=='A')    {
      OTdata.rsptype = OTGW_ANSWER_THERMOSTAT;
    } else if (buf[0]=='E')    {
      OTdata.rsptype = OTGW_PARITY_ERROR;
    } 

    //If the Boiler messages have not been seen for 30 seconds, then set the state to false.
    state.otBus.bBoilerState = (now < (state.otBus.tBoilerLastSeen+30));
    if ((state.otBus.bBoilerState != bOTGWboilerpreviousstate) || (cntOTmessagesprocessed==1)) {
      publishBoilerConnectedState();
      bOTGWboilerpreviousstate = state.otBus.bBoilerState;
    }

    //If the Thermostat messages have not been seen for 30 seconds, then set the state to false.
    state.otBus.bThermostatState = (now < (state.otBus.tThermostatLastSeen+30));
    if ((state.otBus.bThermostatState != bOTGWthermostatpreviousstate) || (cntOTmessagesprocessed==1)){
      publishThermostatConnectedState();
      publishHvacMode(false);    // GH #665: re-evaluate hvac_mode/action on thermostat connect/disconnect (off when gone)
      publishHvacAction(false);
      bOTGWthermostatpreviousstate = state.otBus.bThermostatState;
    }

    //OpenTherm is active when at least one side (boiler or thermostat) is communicating on the bus.
    state.otBus.bOnline = state.otBus.bBoilerState || state.otBus.bThermostatState;
    if ((state.otBus.bOnline != bOTGWpreviousstate) || (cntOTmessagesprocessed==1)){
      publishOTGWConnectedState();
      // nodeMCU online/offline zelf naar 'otgw-firmware/' pushen
      bOTGWpreviousstate = state.otBus.bOnline; //remember state, so we can detect statechanges
    }

    //clear ot log buffer
    ClrLog();
    // Start log with timestamp
    AddLog(getOTLogTimestamp());
    AddLog(" ");
    
    //process the OTGW message
    const char *bufval = buf + 1; //skip the first char
    uint32_t value = 0;
    //split 32bit value into the relevant OT protocol parts
    memset(OTdata.buf, 0, sizeof(OTdata.buf));        // clear buffer
    memcpy(OTdata.buf, buf, len);                     // copy the raw message to the buffer
    OTdata.len = len;                                 // set the length of the message  
    if (sscanf(bufval, "%8x", &value) != 1) return;    // extract the value, abort on parse failure
    OTdata.value = value;                             // store the value
    OTdata.type = (value >> 28) & 0x7;                // byte 1 = take 3 bits that define msg msgType
    OTdata.masterslave = (OTdata.type >> 2) & 0x1;    // MSB from type --> 0 = master and 1 = slave
    OTdata.id = (value >> 16) & 0xFF;                 // byte 2 = message id 8 bits 
    OTdata.valueHB = (value >> 8) & 0xFF;             // byte 3 = high byte
    OTdata.valueLB = value & 0xFF;                    // byte 4 = low byte
    OTdata.time = millis();                           // time of reception    
    OTdata.skipthis = false;                          // default: do not skip this message (parity errors only set this true)
    OTdata.bGatewaySubstituted = false;               // default: not substituted by gateway (ADR-096)
    OTdata.bAnswerOverride = false;                   // ADR-103: default proxy A (no preceding B)

    if (cntOTmessagesprocessed == 1) {       //first message needs to be put in the buffer
      // Boot-time one-shot: the very first OT frame has no prior delayed frame to pair
      // against, so the (B,A) and (T,R) substitution-detection logic below cannot run.
      // We store the raw frame with bAnswerOverride=false / bGatewaySubstituted=false
      // initialised above. Worst case: if the first frame happens to be an A that was
      // already an answer-override on the bus, it would reach _boiler/canonical once;
      // the next (B,A) pair recomputes correctly and behaviour self-corrects. Bounded,
      // intentional, one-shot drift — port from dev TASK-665.
      delayedOTdata = OTdata;       //store current msg
      OTDebugln(F("delaying first message!"));
    } else {                              //any other message will be processed
      // ADR-096 worldview semantics: when the gateway substitutes the bus traffic for an OT id
      // (T → R on the master-side, or B → A on the slave-side response), the older (delayed)
      // frame did not reach the *opposite* side. Earlier code marked the older frame as
      // skipthis=true, which silently dropped the thermostat-side (or boiler-side) value
      // entirely — the cause of the data-loss bug fixed by ADR-096. We now flag the older
      // frame as bGatewaySubstituted=true; the publish-time worldview routing in
      // publishToSourceTopic() then sends the value to the same-side subtopic only and
      // suppresses canonical / opposite-side publication. The OT-bus log decoration ("<ignored>")
      // is preserved as a diagnostic marker (see processOT log section).
      // Pattern detection is unchanged from the original skipthis logic:
      //   if T (master write) is followed within 500 ms by R (gateway-substituted write)
      //     → T did not reach the boiler; R replaces it on canonical and /boiler.
      //   if B (slave response) is followed within 500 ms by A (gateway-substituted answer)
      //     → A reaches the thermostat instead of B; B still represents boiler-side reality.
      bool bGatewaySubstituted = (delayedOTdata.id == OTdata.id) && (OTdata.time - delayedOTdata.time < 500) &&
           (((OTdata.rsptype == OTGW_ANSWER_THERMOSTAT) && (delayedOTdata.rsptype == OTGW_BOILER)) ||
            ((OTdata.rsptype == OTGW_REQUEST_BOILER) && (delayedOTdata.rsptype == OTGW_THERMOSTAT)));

      //delay message processing by 1 message, to make sure detection of value decoding is done correctly with R and A message.
      tmpOTdata = delayedOTdata;          //fetch delayed msg
      delayedOTdata = OTdata;             //store current msg
      // ADR-103: mark the incoming A (now the delayed frame) as an answer-override A iff a
      // (B,A) pair was just detected. It rides the struct copy to the cycle that publishes
      // it. A proxy A (no preceding B) keeps the init default 0 → reaches _boiler/canonical.
      delayedOTdata.bAnswerOverride = bGatewaySubstituted && (delayedOTdata.rsptype == OTGW_ANSWER_THERMOSTAT);
      OTdata = tmpOTdata;                 //then process delayed msg
      OTdata.bGatewaySubstituted = bGatewaySubstituted;  //flag substitution if needed (ADR-096)

      //when parity error in OTGW then skip data to MQTT nor store it local in data object
      OTdata.skipthis = (OTdata.rsptype == OTGW_PARITY_ERROR);

      //Read information from this OT message ready for use...
      if (OTdata.id <= OT_MSGID_MAX) {
        PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
      } else {
        //unknown message id, set safe defaults to prevent OTmap OOB read
        OTlookupitem.id = OTdata.id;
        OTlookupitem.msgcmd = OT_UNDEF;
        OTlookupitem.type = ot_undef;
        OTlookupitem.label = "Unknown";
        OTlookupitem.friendlyname = "Unknown";
        OTlookupitem.unit = "";
      }

      // TASK-691 / TASK-692 / TASK-693 port (dev TASK-685/686/688): maintain
      // the six per-msgID bitmaps that describe each side of the OT bus.
      // Dirty flags fire only on 0->1 transitions so the periodic publishers
      // (MQTT every minute, file every 15 min) do work exactly once per
      // newly-discovered (id, direction).
      {
        const uint8_t idx  = OTdata.id >> 3;
        const uint8_t mask = (uint8_t)(1u << (OTdata.id & 7));
        if (OTdata.masterslave == 0) {
          // Master frame — track thermostat-side requests.
          if (OTdata.type == OT_WRITE_DATA) {
            boilerLastMasterWasWrite[idx] |= mask;
            if ((thermostatSentWrite[idx] & mask) == 0) {
              thermostatSentWrite[idx] |= mask;
              thermostatFileDirty = true;
            }
          } else if (OTdata.type == OT_READ_DATA) {
            boilerLastMasterWasWrite[idx] &= ~mask;
            if ((thermostatSentRead[idx] & mask) == 0) {
              thermostatSentRead[idx] |= mask;
              thermostatFileDirty = true;
            }
          }
        } else {
          // Slave frame — track boiler-side response classification.
          if (OTdata.type == OT_READ_ACK) {
            if ((boilerAckedRead[idx] & mask) == 0) {
              boilerAckedRead[idx] |= mask;
              boilerFileDirty = true;
            }
          } else if (OTdata.type == OT_WRITE_ACK) {
            if ((boilerAckedWrite[idx] & mask) == 0) {
              boilerAckedWrite[idx] |= mask;
              boilerFileDirty = true;
            }
          } else if (OTdata.type == OT_UNKNOWN_DATA_ID) {
            // Master direction is read from boilerLastMasterWasWrite (set on
            // the preceding master frame). The slave's type-7 alone doesn't
            // carry intent.
            const bool isWriteCtx = (boilerLastMasterWasWrite[idx] & mask) != 0;
            uint8_t * const bitmap = isWriteCtx ? boilerUnsupportedWrite : boilerUnsupportedRead;
            if ((bitmap[idx] & mask) == 0) {
              bitmap[idx] |= mask;
              boilerUnsupportedDirty = true;  // MQTT republish (1-min cadence)
              boilerFileDirty        = true;  // file write     (15-min cadence)
            }
          }
        }
      }

      //keep track of last update time — only for valid responses
      if (is_value_valid(OTdata, OTlookupitem)) {
        setMsgLastUpdated(OTdata.id, currentTrackedSeconds());
      }

      // Queue MQTT HA discovery for this OT message ID if not yet published.
      // Non-blocking: just sets the pending bit; drainOnePendingDiscovery()
      // (3-second timer in main loop) handles the actual publish.
      if (is_value_valid(OTdata, OTlookupitem) && settings.mqtt.bEnable) {
        if (!getMQTTConfigDone(OTdata.id)) {
          setMQTTConfigPending(OTdata.id);
        }
      }


      // Decode and print OpenTherm Gateway Message
      switch (OTdata.rsptype){
        case OTGW_BOILER:
          AddLog("Boiler            ");
          break;
        case OTGW_THERMOSTAT:
          AddLog("Thermostat        ");
          break;
        case OTGW_REQUEST_BOILER:
          AddLog("Request Boiler    ");
          break;
        case OTGW_ANSWER_THERMOSTAT:
          AddLog("Answer Thermostat ");
          break;
        case OTGW_PARITY_ERROR:
          AddLog("Parity Error      ");
          break;
        default:
          AddLog("Unknown           ");
          break;
      }

      //print message Type and ID
      AddLogf(" %s %3d", OTdata.buf, OTdata.id);
      AddLogf(" %-16s", messageTypeToString(static_cast<OTLibMessageType>(OTdata.type)));
      //OTDebugf("[%-30s]", messageIDToString(static_cast<OTLibMessageID>(OTdata.id)));
      //OTDebugf("[M=%d]",OTdata.master);

      //Add indicators for parity error, gateway-substituted frame, or valid value (ADR-096)
      if (OTdata.rsptype == OTGW_PARITY_ERROR) AddLog("P");
      else if (OTdata.skipthis || OTdata.bGatewaySubstituted) AddLog("-");
      else if (is_value_valid(OTdata, OTlookupitem)) AddLog(">");
      else AddLog(" ");  //placeholder for alignment
      
      AddLog(" ");  // Space before payload for readability

      //next step interpret the OT protocol
      // OTPublishGate RAII: gate closes for this OT slot's throttle decision and
      // is guaranteed to reopen (restore true) when the scope exits, even on early
      // return. Non-OT sends (event_report, etc.) that follow are not affected. (ADR-006)
      // ADR-104 Decision item 7: scope mqttPendingSlot commit to this OT frame.
      // shouldPublishMQTTForID installs pending; capture the pre-publish success
      // count, run decodeAndPublishOTValue, then commit if any sendMQTTData
      // succeeded — else clear the pending so a later unrelated publish cannot
      // silently commit it.
      {
        const uint32_t preSuccessCount = mqttSendSuccessCount;
        OTPublishGate gate(shouldPublishMQTTForID(OTdata.id, OTdata.masterslave, OTdata.value));
        decodeAndPublishOTValue();
        if (mqttPendingSlot.pending) {
          if (mqttSendSuccessCount > preSuccessCount) confirmMQTTPublishSlot();
          else                                        mqttPendingSlot.pending = false;
        }
      }

      if (OTdata.skipthis || OTdata.bGatewaySubstituted) AddLog(" <ignored> ");
      // TASK-691 / TASK-692 port (dev TASK-685/686): plain-English direction-
      // aware suffix on slave Unknown-Data-Id. Emitted on every occurrence so
      // a tester who opens telnet after the first such frame still sees the
      // diagnostic context. The same suffix reaches the WebSocket OT Monitor
      // via the shared ot_log_buffer.
      if (OTdata.masterslave == 1 && OTdata.type == OT_UNKNOWN_DATA_ID) {
        const uint8_t idx  = OTdata.id >> 3;
        const uint8_t mask = (uint8_t)(1u << (OTdata.id & 7));
        const bool isWriteCtx = (boilerLastMasterWasWrite[idx] & mask) != 0;
        AddLog(isWriteCtx ? " (boiler rejected write)" : " (boiler does not implement)");
      }
      AddLogln();
      OTDebugT(skipOTLogTimestamp(ot_log_buffer));

      // Send log buffer directly to WebSocket (no JSON, no queue)
      sendLogToWebSocket(ot_log_buffer);

      // Throttle TCP flush to once per second instead of per-message (~10/sec).
      // debugTelnet (SimpleTelnet) buffers output; flushing just forces a TCP push.
      // At 10 msg/sec the per-message flush was the single largest TCP cost.
      { static unsigned long lastOTFlushMs = 0;
        unsigned long now = millis();
        if ((uint32_t)(now - lastOTFlushMs) >= 1000) {
          OTDebugFlush();
          lastOTFlushMs = now;
        }
      }
      ClrLog();
    } 
  } else if (buf[2]==':') { //seems to be a response to a command, so check to verify if it was
    checkCommandResponse(buf, len);
    if (buf[0] == 'P' && buf[1] == 'R') {
      handlePRresponse(buf, len);  // process PR: responses (PIC settings, gateway mode, banner)
    }
    Debugln(buf);
    reportOTGWEvent(buf, '<', true);
  } else if (handlePICStatusToken(buf)) {
    // handled inside lookup table — see kPICStatusTokens above
  // cMsg SAFETY NOTE: snprintf_P(cMsg,...) → sendEventToWebSocket('!', cMsg) is safe here.
  // sendEventToWebSocket copies cMsg into ot_log_buffer synchronously (AddLog call) before
  // any yield. So even if doBackgroundTasks re-enters via feedWatchDog, cMsg is no longer
  // needed by the time any yield can occur. Exception: reportOTGWEvent (below) calls both
  // sendMQTTData AND sendEventToWebSocket; feedWatchDog in the MQTT path can yield between
  // the two calls, so callers of reportOTGWEvent must use a local buffer (see evtBuf below).
  } else if (strstr_P(buf, PSTR("\r\nError 01"))!= NULL) {
    char errorBuf[12];
    OTcurrentSystemState.error01++;
    OTDebugTf(PSTR("\r\nError 01 = %d\r\n"),OTcurrentSystemState.error01);
    snprintf_P(errorBuf, sizeof(errorBuf), PSTR("%u"), OTcurrentSystemState.error01);
    sendMQTTData(F("Error 01"), errorBuf);
    snprintf_P(cMsg, sizeof(cMsg), PSTR("Error 01 [%u]"), OTcurrentSystemState.error01);
    sendEventToWebSocket('!', cMsg);
  } else if (strstr_P(buf, PSTR("Error 02"))!= NULL) {
    char errorBuf[12];
    OTcurrentSystemState.error02++;
    OTDebugTf(PSTR("\r\nError 02 = %d\r\n"),OTcurrentSystemState.error02);
    snprintf_P(errorBuf, sizeof(errorBuf), PSTR("%u"), OTcurrentSystemState.error02);
    sendMQTTData(F("Error 02"), errorBuf);
    snprintf_P(cMsg, sizeof(cMsg), PSTR("Error 02 [%u]"), OTcurrentSystemState.error02);
    sendEventToWebSocket('!', cMsg);
  } else if (strstr_P(buf, PSTR("Error 03"))!= NULL) {
    char errorBuf[12];
    OTcurrentSystemState.error03++;
    OTDebugTf(PSTR("\r\nError 03 = %d\r\n"),OTcurrentSystemState.error03);
    snprintf_P(errorBuf, sizeof(errorBuf), PSTR("%u"), OTcurrentSystemState.error03);
    sendMQTTData(F("Error 03"), errorBuf);
    snprintf_P(cMsg, sizeof(cMsg), PSTR("Error 03 [%u]"), OTcurrentSystemState.error03);
    sendEventToWebSocket('!', cMsg);
  } else if (strstr_P(buf, PSTR("Error 04"))!= NULL){
    char errorBuf[12];
    OTcurrentSystemState.error04++;
    OTDebugTf(PSTR("\r\nError 04 = %d\r\n"),OTcurrentSystemState.error04);
    snprintf_P(errorBuf, sizeof(errorBuf), PSTR("%u"), OTcurrentSystemState.error04);
    sendMQTTData(F("Error 04"), errorBuf);
    snprintf_P(cMsg, sizeof(cMsg), PSTR("Error 04 [%u]"), OTcurrentSystemState.error04);
    sendEventToWebSocket('!', cMsg);
#if HAS_PIC
  } else if (strstr(buf, OTGW_BANNER)!=NULL){
    //found a banner, so get the version of PIC
    // Re-enable PIC functions if boot-time detection missed it (transient startup failure).
    if (!state.pic.bAvailable) {
      state.pic.bAvailable = true;
      state.hw.eMode = HW_MODE_PIC;
      DebugTln(F("PIC detected via banner — PIC functions re-enabled"));
#if HAS_RUNTIME_HW_DETECT
      // ADR-127 re-detect safety net: a degraded combo boot that recovers via
      // the banner is by definition a Classic board — persist that so the next
      // boot skips the (previously missed) probe and starts in PIC mode.
      if (settings.iBoardMode != 1) {
        settings.iBoardMode = 1;
        writeSettings(false);
      }
#endif
    }
    strlcpy(state.pic.sFwversion, OTGWSerial.firmwareVersion(), sizeof(state.pic.sFwversion));
    OTDebugTf(PSTR("Current firmware version: %s\r\n"), state.pic.sFwversion);
    strlcpy(state.pic.sDeviceid, OTGWSerial.processorToString().c_str(), sizeof(state.pic.sDeviceid));
    OTDebugTf(PSTR("Current device id: %s\r\n"), state.pic.sDeviceid);
    strlcpy(state.pic.sType, OTGWSerial.firmwareToString().c_str(), sizeof(state.pic.sType));
    OTDebugTf(PSTR("Current firmware type: %s\r\n"), state.pic.sType);
    sendMQTTversioninfo();
    // Banner is the response to PR=A — remove it directly from the command queue
    for (int qi = 0; qi < cmdQueueSize; qi++) {
      if (cmdqueue[qi].cmd[0] == 'P' && cmdqueue[qi].cmd[1] == 'R' &&
          cmdqueue[qi].cmdlen >= 4 && cmdqueue[qi].cmd[3] == 'A') {
        removeFromCmdQueue(qi);
        break;
      }
    }
    { char evtBuf[60]; snprintf_P(evtBuf, sizeof(evtBuf), PSTR("OTGW PIC restarted [%s]"), state.pic.sFwversion); reportOTGWEvent(evtBuf, '*', true); }
#endif
  } else if (strchr(buf, ',') != nullptr) {
    // Comma-separated line: handle PS=1 summary (25 or 34 comma-separated fields).
    // processPSSummary() validates the field count and returns silently if not a PS=1 line.
    // Individual decoded field lines are forwarded to WebSocket inside processPSSummary().
    if (!isOTGWStartupQuietPeriodActive()) {
      processPSSummary(buf, len);
    }
  } else if ((strchr(buf, '=') != nullptr) && (strchr(buf, ':') == nullptr)) {
    // Lines containing '=' but no ':' are echoed commands or command responses in PS=1 mode
    // (e.g. "PS=0" after sending PS=0 to exit PS mode, "TT=20.0" for a setpoint command echo).
    // Forward to WebSocket and MQTT so the OT Monitor tab remains usable in PS=1 mode and
    // the user can see the result of commands like "PS=0". (ADR-038)
    Debugln(buf);
    reportOTGWEvent(buf, '<', true);
    // PS=0 echo: the PIC is exiting summary mode — update state accordingly.
    // All other XX=value lines (PS=1, TT=20.0, etc.) indicate PS=1 mode is active.
    if (strcasecmp_P(buf, PSTR("PS=0")) == 0) {
      leavePSMode(PSTR("PS=0 echo: exiting PS=1 mode"), nullptr);
    } else {
      enterPSMode(PSTR("PS mode auto-detected as ON (summary key=value stream)"), nullptr, false);
    }
  } else {
    OTDebugTf(PSTR("Not processed, received from OTGW => (%s) [%d]\r\n"), buf, len);
    reportOTGWEvent(buf, '<', true);
  }
}


//====================[ HandleOTGW ]====================
/*  
** This is the core of the OTGW firmware. 
** This code basically reads from serial, connected to the OTGW hardware, and
** processes each OT message coming. It can also be used to send data into the
** OpenTherm Gateway. 
**
** The main purpose is to read each OT Msg (9 bytes), or anything that comes 
** from the OTGW hardware firmware. Default it assumes raw OT messages, however
** it can handle the other messages to, like PS=1/PS=0 etc.
** 
** Also, this code bit implements the serial 2 network (port 25238). The serial port 
** is forwarded to port 25238, and visavera. So you can use it with OTmonitor (the 
** original OpenTherm program that comes with the hardware). The serial port and 
** ser2net port 25238 are both "line read" into the read buffer (coming from OTGW 
** thru serial) and write buffer  (coming from 25238 going to serial).
**
** The write buffer (incoming from port 25238) is also line printed to the Debug (port 23).
** The read line buffer is per line parsed by the proces OT parser code (processOT (buf, len)).
*/
void handlePICSerial()
{
#if HAS_PIC
  // TASK-865.6: the RX drain + UART read loop now lives in the dedicated PIC
  // task (picSerialDrainOnce). handlePICSerial() keeps only the loop-side work
  // that does NOT touch the UART directly: the OTGW replay simulation and the
  // ser2net (net->serial) relay. The latter reads from the TCP stream and now
  // ENQUEUES bytes onto otTxQueue for the task to write (no direct OTGWSerial
  // write here). The old kMaxLinesPerDrain per-call cap is gone — the task owns
  // a full FIFO drain on its own core slice, so the network/web stack can no
  // longer starve the reader.
  //
  // ADR-127: on a combo running OTDirect the UART is closed and OTDirect owns
  // the OT path — skip entirely. Compile-time false on fixed PIC boards.
  if (isOTDirectEnabled()) return;
  static char sWrite[MAX_BUFFER_WRITE];
  static size_t bytes_read = 0;   // unused by the relay; kept for the sim signature
  static size_t bytes_write = 0;
  static bool discardCurrentReadLine = false;
  static uint8_t outByte;
  static File otgwSimulationFile;
  static bool otgwSimulationWasEnabled = false;
  static char sReplay[MAX_BUFFER_READ];

  if (handlePICSerialSimulation(otgwSimulationFile,
                           otgwSimulationWasEnabled,
                           bytes_read,
                           bytes_write,
                           discardCurrentReadLine,
                           sReplay,
                           sizeof(sReplay))) {
    return;
  }

  if (isPICEnabled() && settings.mqtt.bLegacyPort25238Enabled) {
    //handle incoming data from network (port 25238) sent to serial port OTGW (WRITE BUFFER)
    while (OTGWstream.available()){
      outByte = OTGWstream.read();  // read from port 25238
      if (!state.debug.bOTGWSimulation) {
        // TASK-865.6: enqueue for the PIC task to write (no direct UART write).
        enqueuePICTx(&outByte, 1);
      }
      if (outByte == '\r')
      { //on CR, do something...
        sWrite[bytes_write] = 0;
        if (state.debug.bOTGWSimulation) {
          OTDebugTf(PSTR("Net2Ser blocked by simulation mode: [%s] (%d)\r\n"), sWrite, bytes_write);
          if (bytes_write > 0) {
            snprintf_P(cMsg, sizeof(cMsg), PSTR("Simulation blocked cmd [%s]"), sWrite);
            sendEventToWebSocket('!', cMsg);
          }
        } else {
          OTDebugTf(PSTR("Net2Ser: Sending to OTGW: [%s] (%d)\r\n"), sWrite, bytes_write);
          if (bytes_write > 0) sendEventToWebSocket('>', sWrite); // log every ser2net command
          // Track ser2net activity and remove conflicting queue entries
          if (bytes_write >= 3 && sWrite[2] == '=') {
            lastSer2netCmdMs = millis();
            // Remove matching command from queue to prevent override
            for (int qi = 0; qi < cmdQueueSize; qi++) {
              if (cmdqueue[qi].cmd[0] == sWrite[0] && cmdqueue[qi].cmd[1] == sWrite[1]) {
                // For PR commands, also match the register letter (e.g., ser2net PR=S must not remove PR=O)
                if (sWrite[0] == 'P' && sWrite[1] == 'R' && bytes_write >= 4 && cmdqueue[qi].cmdlen >= 4) {
                  if (cmdqueue[qi].cmd[3] != sWrite[3]) continue;
                }
                OTDebugTf(PSTR("Ser2net: Removing [%s] from queue (overridden by ser2net)\r\n"), cmdqueue[qi].cmd);
                removeFromCmdQueue(qi);
                break;
              }
            }
          }
          //check for reset command
          if (strcmp_P(sWrite, PSTR("GW=R"))==0){
            //detected [GW=R], then reset the gateway the gpio way
            OTDebugTln(F("Detected: GW=R. Reset gateway command executed."));
            sendEventToWebSocket_P('!', PSTR("GW=R [reset]"));
            resetOTGW();
          } else if (strcasecmp_P(sWrite, PSTR("PS=1"))==0) {
            //detected [PS=1], then PrintSummary mode = true --> From this point on you need to ask for summary.
            enterPSMode(nullptr, PSTR("PS=1 [print summary mode]"), true);
          } else if (strcasecmp_P(sWrite, PSTR("PS=0"))==0) {
            //detected [PS=0], then PrintSummary mode = OFF --> Raw mode is turned on again.
            leavePSMode(nullptr, PSTR("PS=0 [raw mode]"));
          }
        }
        bytes_write = 0; //start next line
      } else if  (outByte == '\n')
      {
        // on LF, skip 
      } 
      else 
      {
        if (bytes_write < (MAX_BUFFER_WRITE-1))
          sWrite[bytes_write++] = outByte;
      }
    }
  }
#endif // HAS_PIC
}// END of handlePICSerial

//====================[ functions for REST API ]====================
const char* getOTGWValue(int msgid)
{
  static char buffer[32];
  
  switch (static_cast<OTLibMessageID>(msgid)) { 
    case OT_TSet:                              dtostrf(OTcurrentSystemState.TSet, 0, 2, buffer); return buffer;
    case OT_CoolingControl:                    dtostrf(OTcurrentSystemState.CoolingControl, 0, 2, buffer); return buffer;
    case OT_TsetCH2:                           dtostrf(OTcurrentSystemState.TsetCH2, 0, 2, buffer); return buffer;
    case OT_TrOverride:                        dtostrf(OTcurrentSystemState.TrOverride, 0, 2, buffer); return buffer;
    case OT_TrOverride2:                       dtostrf(OTcurrentSystemState.TrOverride2, 0, 2, buffer); return buffer;
    case OT_MaxRelModLevelSetting:             dtostrf(OTcurrentSystemState.MaxRelModLevelSetting, 0, 2, buffer); return buffer;
    case OT_TrSet:                             dtostrf(OTcurrentSystemState.TrSet, 0, 2, buffer); return buffer;
    case OT_TrSetCH2:                          dtostrf(OTcurrentSystemState.TrSetCH2, 0, 2, buffer); return buffer;
    case OT_RelModLevel:                       dtostrf(OTcurrentSystemState.RelModLevel, 0, 2, buffer); return buffer;
    case OT_CHPressure:                        dtostrf(OTcurrentSystemState.CHPressure, 0, 2, buffer); return buffer;
    case OT_DHWFlowRate:                       dtostrf(OTcurrentSystemState.DHWFlowRate, 0, 2, buffer); return buffer;
    case OT_Tr:                                dtostrf(OTcurrentSystemState.Tr, 0, 2, buffer); return buffer;
    case OT_Tboiler:                           dtostrf(OTcurrentSystemState.Tboiler, 0, 2, buffer); return buffer;
    case OT_Tdhw:                              dtostrf(OTcurrentSystemState.Tdhw, 0, 2, buffer); return buffer;
    case OT_Toutside:                          dtostrf(OTcurrentSystemState.Toutside, 0, 2, buffer); return buffer;
    case OT_Tret:                              dtostrf(OTcurrentSystemState.Tret, 0, 2, buffer); return buffer;
    case OT_Tsolarstorage:                     dtostrf(OTcurrentSystemState.Tsolarstorage, 0, 2, buffer); return buffer;
    case OT_Tsolarcollector:                   dtostrf(OTcurrentSystemState.Tsolarcollector, 0, 2, buffer); return buffer;
    case OT_TflowCH2:                          dtostrf(OTcurrentSystemState.TflowCH2, 0, 2, buffer); return buffer;
    case OT_Tdhw2:                             dtostrf(OTcurrentSystemState.Tdhw2, 0, 2, buffer); return buffer;
    case OT_Texhaust:                          dtostrf(OTcurrentSystemState.Texhaust, 0, 2, buffer); return buffer;
    case OT_Theatexchanger:                    dtostrf(OTcurrentSystemState.Theatexchanger, 0, 2, buffer); return buffer;
    case OT_TdhwSet:                           dtostrf(OTcurrentSystemState.TdhwSet, 0, 2, buffer); return buffer;
    case OT_MaxTSet:                           dtostrf(OTcurrentSystemState.MaxTSet, 0, 2, buffer); return buffer;
    case OT_Hcratio:                           dtostrf(OTcurrentSystemState.Hcratio, 0, 2, buffer); return buffer;
    case OT_Remoteparameter4:                  dtostrf(OTcurrentSystemState.Remoteparameter4, 0, 2, buffer); return buffer;
    case OT_Remoteparameter5:                  dtostrf(OTcurrentSystemState.Remoteparameter5, 0, 2, buffer); return buffer;
    case OT_Remoteparameter6:                  dtostrf(OTcurrentSystemState.Remoteparameter6, 0, 2, buffer); return buffer;
    case OT_Remoteparameter7:                  dtostrf(OTcurrentSystemState.Remoteparameter7, 0, 2, buffer); return buffer;
    case OT_Remoteparameter8:                  dtostrf(OTcurrentSystemState.Remoteparameter8, 0, 2, buffer); return buffer;
    case OT_OpenThermVersionMaster:            dtostrf(OTcurrentSystemState.OpenThermVersionMaster, 0, 2, buffer); return buffer;
    case OT_OpenThermVersionSlave:             dtostrf(OTcurrentSystemState.OpenThermVersionSlave, 0, 2, buffer); return buffer;
    case OT_Statusflags:                       dtostrf(OTcurrentSystemState.Statusflags, 0, 2, buffer); return buffer;
    case OT_ASFflags:                          dtostrf(OTcurrentSystemState.ASFflags, 0, 2, buffer); return buffer;
    case OT_MasterConfigMemberIDcode:          dtostrf(OTcurrentSystemState.MasterConfigMemberIDcode, 0, 2, buffer); return buffer;
    case OT_SlaveConfigMemberIDcode:           dtostrf(OTcurrentSystemState.SlaveConfigMemberIDcode, 0, 2, buffer); return buffer;
    case OT_Command:                           dtostrf(OTcurrentSystemState.Command, 0, 2, buffer); return buffer;
    case OT_RBPflags:                          dtostrf(OTcurrentSystemState.RBPflags, 0, 2, buffer); return buffer;
    case OT_TSP:                               dtostrf(OTcurrentSystemState.TSP, 0, 2, buffer); return buffer;
    case OT_TSPindexTSPvalue:                  dtostrf(OTcurrentSystemState.TSPindexTSPvalue, 0, 2, buffer); return buffer;
    case OT_FHBsize:                           dtostrf(OTcurrentSystemState.FHBsize, 0, 2, buffer); return buffer;
    case OT_FHBindexFHBvalue:                  dtostrf(OTcurrentSystemState.FHBindexFHBvalue, 0, 2, buffer); return buffer;
    case OT_MaxCapacityMinModLevel:            dtostrf(OTcurrentSystemState.MaxCapacityMinModLevel, 0, 2, buffer); return buffer;
    case OT_DayTime:                           dtostrf(OTcurrentSystemState.DayTime, 0, 2, buffer); return buffer;
    case OT_Date:                              dtostrf(OTcurrentSystemState.Date, 0, 2, buffer); return buffer;
    case OT_Year:                              dtostrf(OTcurrentSystemState.Year, 0, 2, buffer); return buffer;
    case OT_TdhwSetUBTdhwSetLB:                dtostrf(OTcurrentSystemState.TdhwSetUBTdhwSetLB, 0, 2, buffer); return buffer;
    case OT_MaxTSetUBMaxTSetLB:                dtostrf(OTcurrentSystemState.MaxTSetUBMaxTSetLB, 0, 2, buffer); return buffer;
    case OT_HcratioUBHcratioLB:                dtostrf(OTcurrentSystemState.HcratioUBHcratioLB, 0, 2, buffer); return buffer;
    case OT_Remoteparameter4boundaries:        dtostrf(OTcurrentSystemState.Remoteparameter4boundaries, 0, 2, buffer); return buffer;
    case OT_Remoteparameter5boundaries:        dtostrf(OTcurrentSystemState.Remoteparameter5boundaries, 0, 2, buffer); return buffer;
    case OT_Remoteparameter6boundaries:        dtostrf(OTcurrentSystemState.Remoteparameter6boundaries, 0, 2, buffer); return buffer;
    case OT_Remoteparameter7boundaries:        dtostrf(OTcurrentSystemState.Remoteparameter7boundaries, 0, 2, buffer); return buffer;
    case OT_Remoteparameter8boundaries:        dtostrf(OTcurrentSystemState.Remoteparameter8boundaries, 0, 2, buffer); return buffer;
    case OT_RemoteOverrideFunction:            dtostrf(OTcurrentSystemState.RemoteOverrideFunction, 0, 2, buffer); return buffer;
    case OT_OEMDiagnosticCode:                 dtostrf(OTcurrentSystemState.OEMDiagnosticCode, 0, 2, buffer); return buffer;
    case OT_BurnerStarts:                      dtostrf(OTcurrentSystemState.BurnerStarts, 0, 2, buffer); return buffer;
    case OT_CHPumpStarts:                      dtostrf(OTcurrentSystemState.CHPumpStarts, 0, 2, buffer); return buffer;
    case OT_DHWPumpValveStarts:                dtostrf(OTcurrentSystemState.DHWPumpValveStarts, 0, 2, buffer); return buffer;
    case OT_DHWBurnerStarts:                   dtostrf(OTcurrentSystemState.DHWBurnerStarts, 0, 2, buffer); return buffer;
    case OT_BurnerOperationHours:              dtostrf(OTcurrentSystemState.BurnerOperationHours, 0, 2, buffer); return buffer;
    case OT_CHPumpOperationHours:              dtostrf(OTcurrentSystemState.CHPumpOperationHours, 0, 2, buffer); return buffer;
    case OT_DHWPumpValveOperationHours:        dtostrf(OTcurrentSystemState.DHWPumpValveOperationHours, 0, 2, buffer); return buffer;
    case OT_DHWBurnerOperationHours:           dtostrf(OTcurrentSystemState.DHWBurnerOperationHours, 0, 2, buffer); return buffer;
    case OT_Brand:                             dtostrf(OTcurrentSystemState.Brand, 0, 2, buffer); return buffer;
    case OT_BrandVersion:                      dtostrf(OTcurrentSystemState.BrandVersion, 0, 2, buffer); return buffer;
    case OT_BrandSerialNumber:                 dtostrf(OTcurrentSystemState.BrandSerialNumber, 0, 2, buffer); return buffer;
    case OT_CoolingOperationHours:             dtostrf(OTcurrentSystemState.CoolingOperationHours, 0, 2, buffer); return buffer;
    case OT_PowerCycles:                       dtostrf(OTcurrentSystemState.PowerCycles, 0, 2, buffer); return buffer;
    case OT_MasterVersion:                     dtostrf(OTcurrentSystemState.MasterVersion, 0, 2, buffer); return buffer;
    case OT_SlaveVersion:                      dtostrf(OTcurrentSystemState.SlaveVersion, 0, 2, buffer); return buffer;
    case OT_StatusVH:                          dtostrf(OTcurrentSystemState.StatusVH, 0, 2, buffer); return buffer;
    case OT_ControlSetpointVH:                 dtostrf(OTcurrentSystemState.ControlSetpointVH, 0, 2, buffer); return buffer;
    case OT_ASFFaultCodeVH:                    dtostrf(OTcurrentSystemState.ASFFaultCodeVH, 0, 2, buffer); return buffer;
    case OT_DiagnosticCodeVH:                  dtostrf(OTcurrentSystemState.DiagnosticCodeVH, 0, 2, buffer); return buffer;
    case OT_ConfigMemberIDVH:                  dtostrf(OTcurrentSystemState.ConfigMemberIDVH, 0, 2, buffer); return buffer;
    case OT_OpenthermVersionVH:                dtostrf(OTcurrentSystemState.OpenthermVersionVH, 0, 2, buffer); return buffer;
    case OT_VersionTypeVH:                     dtostrf(OTcurrentSystemState.VersionTypeVH, 0, 2, buffer); return buffer;
    case OT_RelativeVentilation:               dtostrf(OTcurrentSystemState.RelativeVentilation, 0, 2, buffer); return buffer;
    case OT_RelativeHumidityExhaustAir:        dtostrf(OTcurrentSystemState.RelativeHumidityExhaustAir, 0, 2, buffer); return buffer;
    case OT_CO2LevelExhaustAir:                dtostrf(OTcurrentSystemState.CO2LevelExhaustAir, 0, 2, buffer); return buffer;
    case OT_SupplyInletTemperature:            dtostrf(OTcurrentSystemState.SupplyInletTemperature, 0, 2, buffer); return buffer;
    case OT_SupplyOutletTemperature:           dtostrf(OTcurrentSystemState.SupplyOutletTemperature, 0, 2, buffer); return buffer;
    case OT_ExhaustInletTemperature:           dtostrf(OTcurrentSystemState.ExhaustInletTemperature, 0, 2, buffer); return buffer;
    case OT_ExhaustOutletTemperature:          dtostrf(OTcurrentSystemState.ExhaustOutletTemperature, 0, 2, buffer); return buffer;
    case OT_ActualExhaustFanSpeed:             dtostrf(OTcurrentSystemState.ActualExhaustFanSpeed, 0, 2, buffer); return buffer;
    case OT_ActualSupplyFanSpeed:              dtostrf(OTcurrentSystemState.ActualSupplyFanSpeed, 0, 2, buffer); return buffer;
    case OT_RemoteParameterSettingVH:          dtostrf(OTcurrentSystemState.RemoteParameterSettingVH, 0, 2, buffer); return buffer;
    case OT_NominalVentilationValue:           dtostrf(OTcurrentSystemState.NominalVentilationValue, 0, 2, buffer); return buffer;
    case OT_TSPNumberVH:                       dtostrf(OTcurrentSystemState.TSPNumberVH, 0, 2, buffer); return buffer;
    case OT_TSPEntryVH:                        dtostrf(OTcurrentSystemState.TSPEntryVH, 0, 2, buffer); return buffer;
    case OT_FaultBufferSizeVH:                 dtostrf(OTcurrentSystemState.FaultBufferSizeVH, 0, 2, buffer); return buffer;
    case OT_FaultBufferEntryVH:                dtostrf(OTcurrentSystemState.FaultBufferEntryVH, 0, 2, buffer); return buffer;
    case OT_FanSpeed:                          dtostrf(OTcurrentSystemState.FanSpeed, 0, 2, buffer); return buffer;
    case OT_ElectricalCurrentBurnerFlame:      dtostrf(OTcurrentSystemState.ElectricalCurrentBurnerFlame, 0, 2, buffer); return buffer;
    case OT_TRoomCH2:                          dtostrf(OTcurrentSystemState.TRoomCH2, 0, 2, buffer); return buffer;
    case OT_RelativeHumidity:                  dtostrf(OTcurrentSystemState.RelativeHumidity, 0, 2, buffer); return buffer;
    case OT_RFstrengthbatterylevel:            dtostrf(OTcurrentSystemState.RFstrengthbatterylevel, 0, 2, buffer); return buffer;
    case OT_OperatingMode_HC1_HC2_DHW:         dtostrf(OTcurrentSystemState.OperatingMode_HC1_HC2_DHW, 0, 2, buffer); return buffer;
    case OT_ElectricityProducerStarts:         dtostrf(OTcurrentSystemState.ElectricityProducerStarts, 0, 2, buffer); return buffer;
    case OT_ElectricityProducerHours:          dtostrf(OTcurrentSystemState.ElectricityProducerHours, 0, 2, buffer); return buffer;
    case OT_ElectricityProduction:             dtostrf(OTcurrentSystemState.ElectricityProduction, 0, 2, buffer); return buffer;
    case OT_CumulativeElectricityProduction:   dtostrf(OTcurrentSystemState.CumulativeElectricityProduction, 0, 2, buffer); return buffer;
    case OT_BurnerUnsuccessfulStarts:          dtostrf(OTcurrentSystemState.BurnerUnsuccessfulStarts, 0, 2, buffer); return buffer;
    case OT_FlameSignalTooLow:                 dtostrf(OTcurrentSystemState.FlameSignalTooLow, 0, 2, buffer); return buffer;
    case OT_RemehadFdUcodes:                   dtostrf(OTcurrentSystemState.RemehadFdUcodes, 0, 2, buffer); return buffer;
    case OT_RemehaServicemessage:              dtostrf(OTcurrentSystemState.RemehaServicemessage, 0, 2, buffer); return buffer;
    case OT_RemehaDetectionConnectedSCU:       dtostrf(OTcurrentSystemState.RemehaDetectionConnectedSCU, 0, 2, buffer); return buffer;
    case OT_SolarStorageMaster:                dtostrf(OTcurrentSystemState.SolarStorageStatus, 0, 2, buffer); return buffer;
    case OT_SolarStorageASFflags:              dtostrf(OTcurrentSystemState.SolarStorageASFflags, 0, 2, buffer); return buffer;
    case OT_SolarStorageSlaveConfigMemberIDcode:  dtostrf(OTcurrentSystemState.SolarStorageSlaveConfigMemberIDcode, 0, 2, buffer); return buffer;
    case OT_SolarStorageVersionType:           dtostrf(OTcurrentSystemState.SolarStorageVersionType, 0, 2, buffer); return buffer;
    case OT_SolarStorageTSP:                   dtostrf(OTcurrentSystemState.SolarStorageTSP, 0, 2, buffer); return buffer;
    case OT_SolarStorageTSPindexTSPvalue:      dtostrf(OTcurrentSystemState.SolarStorageTSPindexTSPvalue, 0, 2, buffer); return buffer;
    case OT_SolarStorageFHBsize:               dtostrf(OTcurrentSystemState.SolarStorageFHBsize, 0, 2, buffer); return buffer;
    case OT_SolarStorageFHBindexFHBvalue:      dtostrf(OTcurrentSystemState.SolarStorageFHBindexFHBvalue, 0, 2, buffer); return buffer;
    default: 
      strncpy_P(buffer, PSTR("Error: not implemented yet!\r\n"), sizeof(buffer) - 1);
      buffer[sizeof(buffer) - 1] = '\0';
      return buffer;
  } // switch
} // getOTGWValue

void startPICStream()
{
  if (!settings.mqtt.bLegacyPort25238Enabled) {
    DebugTln(F("[OTGW] Legacy TCP port 25238 disabled"));
    OTGWstream.stop();
    return;
  }

  DebugTln(F("[OTGW] Starting legacy TCP port 25238"));
  // Raw serial bridge for OTmonitor: disable telnet IAC handling so binary 0xFF
  // bytes pass through untouched (lib default NEG_REFUSE would strip them). TASK-866/879.
  OTGWstream.setTelnetNegotiation(AsyncSimpleTelnet<2>::NEG_OFF);
  OTGWstream.begin(false);    // false = skip WiFi check; bind unconditionally
}

void stopPICStream()
{
  DebugTln(F("[OTGW] Stopping legacy TCP port 25238"));
  OTGWstream.stop();
}

void applyLegacyPort25238Setting()
{
  if (settings.mqtt.bLegacyPort25238Enabled) startPICStream();
  else stopPICStream();
}

//---------[ Upgrade PIC stuff taken from Schelte Bron's NodeMCU Firmware ]---------
#if HAS_PIC
// TASK-865.14: bounds on the deferred outbound HTTPClient calls. These now run from
// loop() context under the 30s ESP32 TWDT (ADR-135). The connect + HEAD read use a
// 5s cap so the unreachable-host worst case stays well under the window; the refresh
// GET body uses a larger 15s read cap (a full-hex download needs room on a slow link)
// while still leaving generous TWDT headroom.
static const uint16_t PIC_HTTP_TIMEOUT_MS          = 5000;
static const uint16_t PIC_HTTP_DOWNLOAD_TIMEOUT_MS = 15000;

void upgradepicnow(const char *filename) {
  if (!isPICEnabled()) {
    DebugTln(F("PIC upgrade rejected: no PIC detected"));
    return;
  }
  if (OTGWSerial.busy()) {
    DebugTln(F("ERROR: PIC upgrade already in progress, ignoring request"));
    return; // if already in programming mode, never call it twice
  }
  DebugTln(F(""));
  DebugTln(F(">>> Initiating PIC Upgrade <<<"));
  DebugTf(PSTR("Hex file: %s\r\n"), filename);
  fwupgradestart(filename);
  // Upgrade runs in background via OTGWSerial callbacks and upgradeTick called from available()
  DebugTln(F("PIC upgrade object created and started"));
  DebugTln(F("Upgrade runs in background via:"));
  DebugTln(F("  - handlePICSerial() processes serial data"));
  DebugTln(F("  - OTGWSerial.available() calls upgradeTick()"));
  DebugTln(F("  - Progress callbacks update WebUI"));
  DebugTln(F(">>> Background upgrade active <<<"));
  DebugTln(F(""));
}
#endif // HAS_PIC — upgradepicnow

// Helper function to escape JSON strings for WebSocket messages
// Escapes quotes, backslashes, and control characters
// Note: Input is assumed to be null-terminated; embedded nulls are replaced with spaces
static void jsonEscape(const char *in, char *out, size_t outSize) {
  size_t j = 0;
  for (size_t i = 0; in[i] != '\0' && j + 1 < outSize; ++i) {
    char c = in[i];
    if (c == '"' || c == '\\') {
      if (j + 2 >= outSize) break;
      out[j++] = '\\';
      out[j++] = c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      if (j + 1 >= outSize) break;
      out[j++] = ' '; // Replace control characters with space
    } else {
      out[j++] = c;
    }
  }
  out[j] = '\0';
}

#if HAS_PIC
void fwupgradedone(OTGWError result, short errors = 0, short retries = 0) {
  DebugTln(F(""));
  DebugTln(F("=== PIC Upgrade Complete ==="));
  DebugTf(PSTR("Result code: %d\r\n"), (int)result);
  DebugTf(PSTR("Errors: %d, Retries: %d\r\n"), errors, retries);
  switch (result) {
    case OTGWError::OTGW_ERROR_NONE:          snprintf_P(state.flash.sError, sizeof(state.flash.sError), PSTR("PIC upgrade was successful")); break;
    case OTGWError::OTGW_ERROR_MEMORY:        snprintf_P(state.flash.sError, sizeof(state.flash.sError), PSTR("Not enough memory available")); break;
    case OTGWError::OTGW_ERROR_INPROG:        snprintf_P(state.flash.sError, sizeof(state.flash.sError), PSTR("Firmware upgrade in progress")); break;
    case OTGWError::OTGW_ERROR_HEX_ACCESS:    snprintf_P(state.flash.sError, sizeof(state.flash.sError), PSTR("Could not open hex file")); break;
    case OTGWError::OTGW_ERROR_HEX_FORMAT:    snprintf_P(state.flash.sError, sizeof(state.flash.sError), PSTR("Invalid format of hex file")); break;
    case OTGWError::OTGW_ERROR_HEX_DATASIZE:  snprintf_P(state.flash.sError, sizeof(state.flash.sError), PSTR("Wrong data size in hex file")); break;
    case OTGWError::OTGW_ERROR_HEX_CHECKSUM:  snprintf_P(state.flash.sError, sizeof(state.flash.sError), PSTR("Bad checksum in hex file")); break;
    case OTGWError::OTGW_ERROR_MAGIC:         snprintf_P(state.flash.sError, sizeof(state.flash.sError), PSTR("Hex file does not contain expected data")); break;
    case OTGWError::OTGW_ERROR_RESET:         snprintf_P(state.flash.sError, sizeof(state.flash.sError), PSTR("PIC reset failed")); break;
    case OTGWError::OTGW_ERROR_RETRIES:       snprintf_P(state.flash.sError, sizeof(state.flash.sError), PSTR("Too many retries")); break;
    case OTGWError::OTGW_ERROR_MISMATCHES:    snprintf_P(state.flash.sError, sizeof(state.flash.sError), PSTR("Too many mismatches")); break;
    case OTGWError::OTGW_ERROR_DEVICE:        snprintf_P(state.flash.sError, sizeof(state.flash.sError), PSTR("Wrong PIC (16F88 <=> 16F1847)")); break;
    default:                                  snprintf_P(state.flash.sError, sizeof(state.flash.sError), PSTR("Unknown state")); break;
  }
  DebugTf(PSTR("Message: %s\r\n"), CSTR(state.flash.sError));
  DebugTf(PSTR("File: %s\r\n"), state.flash.sPICfile);
  OTDebugTf(PSTR("Upgrade finished: Errorcode = %d - %s - %d retries, %d errors\r\n"), result, CSTR(state.flash.sError), retries, errors);
  
  // Mark flash as complete
  state.flash.bPICactive = false;
  state.flash.iPICprogress = (result == OTGWError::OTGW_ERROR_NONE) ? 100 : -1; // -1 indicates error
  if (result == OTGWError::OTGW_ERROR_NONE) {
    DebugTln(F("*** UPGRADE SUCCESSFUL ***"));
  } else {
    DebugTln(F("*** UPGRADE FAILED ***"));
  }

#ifndef DISABLE_WEBSOCKET
  // Send completion message in format frontend expects
  // Escape strings to prevent JSON injection
  char buf[320]; // Sized for escaped filename (129) + error (96) + JSON overhead (~70) = ~295 bytes
  char filenameEsc[129]; // state.flash.sPICfile is 65 chars, doubled for worst-case escaping
  char errorEsc[96]; // error messages are short literals (<50 chars); matches _setStatus() pattern
  jsonEscape(state.flash.sPICfile, filenameEsc, sizeof(filenameEsc));
  jsonEscape(state.flash.sError, errorEsc, sizeof(errorEsc));
  
  const char *state = (result == OTGWError::OTGW_ERROR_NONE) ? "end" : "error";
  int written = snprintf_P(buf, sizeof(buf), 
    PSTR("{\"state\":\"%s\",\"flash_written\":100,\"flash_total\":100,\"filename\":\"%s\",\"error\":\"%s\"}"),
    state, filenameEsc, errorEsc);
  
  if (written > 0 && written < (int)sizeof(buf)) {
    DebugTln(F("Sending WebSocket completion message..."));
    sendWebSocketJSON(buf);
  } else {
    DebugTln(F("ERROR: WebSocket message too large, not sent"));
  }
#endif

  DebugTln(F("============================"));
  DebugTln(F(""));

  // Note: Keep filename and progress for polling API until next flash starts
}

void fwupgradestep(int pct) {
  // Only log every 10% to avoid spam, plus always log 0% and 100%
  static int lastReportedPct = -1;
  bool shouldLog = (pct == 0) || (pct == 100) || (pct % 10 == 0 && pct != lastReportedPct);
  if (shouldLog) {
    DebugTf(PSTR("Upgrade progress: %d%%\r\n"), pct);
    lastReportedPct = pct;
  }

  // Update progress for polling API
  state.flash.iPICprogress = pct;

#ifndef DISABLE_WEBSOCKET
  // TASK-865.15: dedupe the WebSocket progress broadcast on integer-pct change.
  // OTGWSerial fires this callback many times per percent during a flash; each
  // sendWebSocketJSON()->otLogWs.textAll() is a per-client heap alloc on AsyncTCP
  // during the most heap-sensitive op of the firmware. Sending once per distinct
  // pct cuts the firehose to <=101 broadcasts over the whole flash (~2/s).
  //
  // Heap-gate trade-off (ADR-133): the progress path deliberately does NOT route
  // through canSendWebSocket() (the ADR-121 gate that sendLogToWebSocket honors).
  // Post-dedupe it is no longer a firehose; flash progress is user-visible
  // operation state, not a droppable log line; and dropping intermediates under
  // heap pressure would freeze the progress bar (e.g. stuck at 99%). The terminal
  // "end"/"error" message is sent independently by fwupgradedone(), so completion
  // is reliable regardless of any dropped intermediate frame.
  // pct==0 is the per-flash "start" frame and always passes: lastSentPct is a
  // function-local static that survives across flashes, so a retried flash whose
  // predecessor also ended on 0 (e.g. failed before any pct>0) would otherwise
  // have its "start" suppressed. Bypassing the dedupe for 0 keeps "start" airtight
  // while every >0 frame is deduped on integer-pct change.
  static int lastSentPct = -1;
  if (pct != 0 && pct == lastSentPct) return;
  lastSentPct = pct;

  // Send progress message in format frontend expects
  // Use percentage as flash_written for progress display
  char buf[256]; // Sized for escaped filename (129) + JSON overhead (~90)
  char filenameEsc[129]; // state.flash.sPICfile is 65 chars, doubled for worst-case escaping
  jsonEscape(state.flash.sPICfile, filenameEsc, sizeof(filenameEsc));

  const char *state = (pct == 0) ? "start" : "write";
  int written = snprintf_P(buf, sizeof(buf),
    PSTR("{\"state\":\"%s\",\"flash_written\":%d,\"flash_total\":100,\"filename\":\"%s\"}"),
    state, pct, filenameEsc);

  if (written > 0 && written < (int)sizeof(buf)) {
    sendWebSocketJSON(buf);
  }
#endif
}

void fwreportinfo(OTGWFirmware fw, const char *version) {
    DebugTln(F("Callback: fwreportinfo"));
    strlcpy(state.pic.sFwversion, version, sizeof(state.pic.sFwversion));
    //state.pic.sFwversion = String(OTGWSerial.firmwareVersion());
    DebugTf(PSTR("Current firmware version: %s\r\n"), state.pic.sFwversion);
    strlcpy(state.pic.sDeviceid, OTGWSerial.processorToString().c_str(), sizeof(state.pic.sDeviceid));
    DebugTf(PSTR("Current device id: %s\r\n"), state.pic.sDeviceid);
    //instead of using the firmware string
    strlcpy(state.pic.sType, OTGWSerial.firmwareToString(fw).c_str(), sizeof(state.pic.sType));
    OTDebugTf(PSTR("Current firmware type: %s\r\n"), state.pic.sType);
    sendMQTTversioninfo();
}

void fwupgradestart(const char *hexfile) {
  DebugTln(F("--- fwupgradestart() ---"));
  DebugTf(PSTR("Hex file path: %s\r\n"), hexfile);
  OTGWError result;
  
  // Store filename for WebSocket progress messages and polling API
  // Extract just the filename from the path
  const char *filename = strrchr(hexfile, '/');
  if (filename) {
    filename++; // Skip the '/'
  } else {
    filename = hexfile; // No path, use as-is
  }
  strlcpy(state.flash.sPICfile, filename, sizeof(state.flash.sPICfile));
  DebugTf(PSTR("Extracted filename: %s\r\n"), state.flash.sPICfile);
  
  // Mark flash as started
  state.flash.bPICactive = true;
  state.flash.iPICprogress = 0;
  state.flash.sError[0] = '\0'; // Clear previous error
  DebugTln(F("Flash state set: state.flash.bPICactive=true, progress=0"));

  // TASK-865.6: bPICactive now makes picSerialTaskShouldPark() true. Wait for
  // the dedicated PIC task to acknowledge it has released the UART before the
  // flash FSM drives it, so the task never read()s mid-upgrade. The loop-side
  // flash path (handlePicFlashBackgroundTasks -> picSerialPumpUpgrade) keeps the
  // upgrade state machine ticking from here on.
  waitForPICTaskParked();

  // Turn on LED to indicate flashing
  digitalWrite(LED1, LOW);
  DebugTln(F("LED1 activated (LOW=ON)"));
  DebugTln(F("Calling OTGWSerial.startUpgrade()..."));
  result = OTGWSerial.startUpgrade(hexfile);
  if (result!= OTGWError::OTGW_ERROR_NONE) {
    DebugTf(PSTR("ERROR: startUpgrade() failed immediately with error code %d\r\n"), (int)result);
    fwupgradedone(result);
  } else {
    DebugTln(F("SUCCESS: startUpgrade() returned OTGW_ERROR_NONE"));
    DebugTln(F("Registering callbacks..."));
    OTGWSerial.registerFinishedCallback(fwupgradedone);
    OTGWSerial.registerProgressCallback(fwupgradestep);
    DebugTln(F("Callbacks registered: fwupgradedone, fwupgradestep"));
    DebugTln(F("Upgrade is now running in background"));
  }
  DebugTln(F("--- fwupgradestart() complete ---"));
}

// Validate that a file stored in LittleFS is a valid Intel HEX file.
// Checks record structure and checksums; requires an EOF record.
// Returns true only if the file passes all checks.
// Security note: This guards against corrupted or non-HEX downloads over the
// unauthenticated HTTP channel; it does NOT authenticate the origin.
bool validateIntelHex(const char *filepath) {
  File f = LittleFS.open(filepath, "r");
  if (!f) return false;

  char line[128];  // 128 bytes handles records up to 59 data bytes, sufficient for PIC hex files
  bool hasEof = false;
  bool valid = true;

  // Helper: parse two hex chars at position pos in line[], returns -1 on invalid input or out of bounds
  auto hexByte = [&](int pos) -> int {
    if (pos + 1 >= (int)sizeof(line)) return -1; // bounds guard
    char h[3] = {line[pos], line[pos + 1], '\0'};
    char *end;
    long v = strtol(h, &end, 16);
    if (end != h + 2) return -1;
    return (int)v;
  };

  while (f.available() && valid) {
    int len = f.readBytesUntil('\n', line, sizeof(line) - 1);
    if (len <= 0) break;
    // Strip trailing CR/LF
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) len--;
    line[len] = '\0';
    if (len == 0) continue; // Skip blank lines

    // Each record must start with ':'
    if (line[0] != ':') { valid = false; break; }

    // Minimum record: ':LLAAAATTCC' = 11 chars (0 data bytes)
    if (len < 11) { valid = false; break; }

    int byteCount = hexByte(1);
    if (byteCount < 0) { valid = false; break; }

    // Expected record length: ':' + (LL + AAAA + TT + data + CC) * 2 hex chars
    // = 1 + (byteCount + 5) * 2  (5 = LL(1) + AAAA(2) + TT(1) + CC(1))
    int expectedLen = 1 + (byteCount + 5) * 2;
    if (len < expectedLen) { valid = false; break; }

    // Verify checksum: sum of all bytes (LL + addr_hi + addr_lo + type + data + CC) must equal 0 mod 256
    byte sum = 0;
    for (int i = 1; i < expectedLen; i += 2) {
      int b = hexByte(i);
      if (b < 0) { valid = false; break; }
      sum += (byte)b;
    }
    if (!valid) break;
    if (sum != 0) { valid = false; break; }

    // Check record type (positions 7-8)
    int recType = hexByte(7);
    if (recType < 0) { valid = false; break; }
    if (recType == 1) { // EOF record
      hasEof = true;
      break;
    }
  }

  f.close();
  return valid && hasEof;
}

String checkforupdatepic(String filename){
  WiFiClient client;
  HTTPClient http;
  String latest = "";
  int code;

  // Security note: download is over unencrypted HTTP; ensure device is on a
  // trusted local network and is not reachable from untrusted networks.
  http.begin(client, "http://otgw.tclcode.com/download/" + String(state.pic.sDeviceid) + "/" + filename);
  // TASK-865.14: this runs in loop() context (deferred off the AsyncTCP task) and
  // loop() is subscribed to the 30s ESP32 TWDT (ADR-135). Bound the connect+read
  // timeouts so an unreachable host (the AC#5 worst case) costs ~5s, not a full
  // default stack timeout, and stays comfortably under the watchdog window.
  http.setConnectTimeout(PIC_HTTP_TIMEOUT_MS);
  http.setTimeout(PIC_HTTP_TIMEOUT_MS);
  char useragent[40] = "esp8266-otgw-firmware/";
  strlcat(useragent, _SEMVER_CORE, sizeof(useragent));
  http.setUserAgent(useragent);
  http.collectHeaders(hexheaders, 2);
  feedWatchDog();              // before the blocking call
  code = http.sendRequest("HEAD");
  feedWatchDog();              // and after, before header iteration
  if (code == HTTP_CODE_OK) {
    for (int i = 0; i< http.headers(); i++) {
      DebugTf(PSTR("%s: %s\r\n"), hexheaders[i], http.header(i).c_str());
    }
    latest = http.header(1);
    DebugTf(PSTR("Update %s -> [%s]\r\n"), filename.c_str(), latest.c_str());
  } else OTDebugln(F("Failed to fetch version from Schelte Bron website"));
  http.end(); // Always close connection, even on failure (Finding #24)

  return latest; 
}

void refreshpic(String filename, String version) {
  if (strcmp_P(state.pic.sDeviceid, PSTR("unknown")) == 0) return; // no pic version found, don't upgrade

  WiFiClient client;
  HTTPClient http;
  String latest;
  int code;

  latest = checkforupdatepic(filename);

  if (latest != version) {
    OTDebugTf(PSTR("Update (%s)%s: %s -> %s\r\n"), state.pic.sDeviceid, filename.c_str(), version.c_str(), latest.c_str());
    OTDebugTln(F("NOTE: PIC firmware is downloaded over plain HTTP (no TLS); ensure device is on a trusted local network."));
    http.begin(client, "http://otgw.tclcode.com/download/" + String(state.pic.sDeviceid) + "/" + filename);
    // TASK-865.14: bounded timeouts — deferred to loop() context, under the 30s TWDT.
    // The connect bound stays at the 5s HEAD value, but the read bound is larger
    // here: this GET streams a full Intel-HEX (~30-60 KB) to LittleFS, so a tight
    // 5s read could truncate it on a slow link (validateIntelHex would then discard
    // it). PIC_HTTP_DOWNLOAD_TIMEOUT_MS (15s) gives the body room while staying well
    // under the watchdog window.
    http.setConnectTimeout(PIC_HTTP_TIMEOUT_MS);
    http.setTimeout(PIC_HTTP_DOWNLOAD_TIMEOUT_MS);
    char useragent[40] = "esp8266-otgw-firmware/";
    strlcat(useragent, _SEMVER_CORE, sizeof(useragent));
    http.setUserAgent(useragent);
    feedWatchDog();            // before the blocking GET
    code = http.GET();
    feedWatchDog();            // and after, before the stream write
    if (code == HTTP_CODE_OK) {
      String hexpath = "/" + String(state.pic.sDeviceid) + "/" + filename;
      File f = LittleFS.open(hexpath, "w");
      if (f) {
        http.writeToStream(&f);
        f.close();
        feedWatchDog();        // after writing the full hex to LittleFS
        // Validate the downloaded file is a well-formed Intel HEX before accepting it.
        // This rejects truncated or non-HEX responses that could corrupt the PIC.
        if (!validateIntelHex(hexpath.c_str())) {
          OTDebugTln(F("ERROR: Downloaded file failed Intel HEX validation - discarding"));
          LittleFS.remove(hexpath);
        } else {
          String verfile = hexpath;
          verfile.replace(".hex", ".ver");
          f = LittleFS.open(verfile, "w");
          if (f) {
            f.print(latest + "\n");
            f.close();
            OTDebugTln(F("Update successful"));
          }
        }
      }
    }
    http.end();
  }
}

// --- Pending Upgrade Logic ---
static char pendingUpgradePath[80] = {0};

void handlePendingUpgrade() {
  if (pendingUpgradePath[0] == '\0') return;
  DebugTln(F(""));
  DebugTln(F("=== Starting Deferred PIC Upgrade ==="));
  DebugTf(PSTR("Hex file path: %s\r\n"), pendingUpgradePath);
  DebugTf(PSTR("Flash state: state.flash.bESPactive=%d, state.flash.bPICactive=%d\r\n"), state.flash.bESPactive, state.flash.bPICactive);
  DebugTf(PSTR("Free heap: %d bytes\r\n"), platformFreeHeap());
  upgradepicnow(pendingUpgradePath);
  pendingUpgradePath[0] = '\0';
  DebugTln(F("Deferred upgrade initiated, upgrade now runs in background"));
  DebugTln(F("Monitor progress via telnet or WebUI"));
  DebugTln(F("======================================="));
}

// --- Deferred PIC outbound-HTTP logic (TASK-865.14, ADR-132 amendment) ---
//
// checkforupdatepic()/refreshpic() make BLOCKING outbound HTTPClient calls to
// http://otgw.tclcode.com. Under ADR-132 the web stack runs every handler on the
// single AsyncTCP service task, so calling them from an async handler froze ALL
// HTTP + the ADR-133 WebSocket for the request's duration (worst case: the full
// DNS/connect timeout when the host is unreachable). This mirrors the existing
// handlePendingUpgrade() bridge: the async handler validates + QUEUES the job and
// returns immediately; this loop()-context worker performs the HTTPClient work.
//
// One single-flight slot. A second request while a job is queued/in-flight is
// rejected at the queue functions below (they return false), so the slot is never
// silently overwritten and the two outbound calls never overlap.
enum class PicHttpJob : uint8_t { None, UpdateCheck, Refresh };
static PicHttpJob pendingPicHttpJob = PicHttpJob::None;
static char       pendingPicFile[64] = {0};   // refresh: hex filename
static char       pendingPicVer[32]  = {0};   // refresh: current on-disk version

// queuePicUpdateCheck — request an outbound HEAD update-check from loop() context.
// Returns false (and changes nothing) if a PIC HTTP job is already pending.
bool queuePicUpdateCheck() {
  if (pendingPicHttpJob != PicHttpJob::None) return false;
  pendingPicHttpJob = PicHttpJob::UpdateCheck;
  state.pic.iUpdateCheck = PIC_UPDATE_CHECKING;
  return true;
}

// queuePicRefresh — request an outbound GET+download of `filename` from loop()
// context. Returns false if a PIC HTTP job is already pending.
bool queuePicRefresh(const char *filename, const char *version) {
  if (pendingPicHttpJob != PicHttpJob::None) return false;
  if (filename == nullptr || filename[0] == '\0') return false;
  strlcpy(pendingPicFile, filename, sizeof(pendingPicFile));
  strlcpy(pendingPicVer,  version ? version : "", sizeof(pendingPicVer));
  pendingPicHttpJob = PicHttpJob::Refresh;
  return true;
}

void handlePendingPicHttp() {
  if (pendingPicHttpJob == PicHttpJob::None) return;
  // Snapshot + clear the slot first so a re-entrant doBackgroundTasks() (via
  // doAutoConfigure's file-reading loop) cannot re-dispatch the same job.
  const PicHttpJob job = pendingPicHttpJob;
  pendingPicHttpJob = PicHttpJob::None;

  if (job == PicHttpJob::UpdateCheck) {
    DebugTln(F("=== Deferred PIC update-check (outbound HEAD) ==="));
    String picFile;
    if (strcmp_P(state.pic.sType, PSTR("diagnose")) == 0) {
      picFile = F("diagnose.hex");
    } else if (strcmp_P(state.pic.sType, PSTR("interface")) == 0) {
      picFile = F("interface.hex");
    } else {
      picFile = F("gateway.hex");
    }
    String latest = checkforupdatepic(picFile);
    if (latest.length() > 0) {
      strlcpy(state.pic.sLatestFw, latest.c_str(), sizeof(state.pic.sLatestFw));
      state.pic.iUpdateCheck = PIC_UPDATE_READY;
      DebugTf(PSTR("Update-check result: %s\r\n"), state.pic.sLatestFw);
    } else {
      state.pic.iUpdateCheck = PIC_UPDATE_ERROR;
      DebugTln(F("Update-check failed (host unreachable or non-200)"));
    }
  } else if (job == PicHttpJob::Refresh) {
    DebugTf(PSTR("=== Deferred PIC refresh (outbound GET): %s ===\r\n"), pendingPicFile);
    // refreshpic still takes String args; the implicit String(const char*) ctor at
    // the call site keeps the change scoped to this worker.
    refreshpic(pendingPicFile, pendingPicVer);
    pendingPicFile[0] = '\0';
    pendingPicVer[0]  = '\0';
    DebugTln(F("Deferred refresh complete"));
  }
}

void upgradepic(AsyncWebServerRequest *request) {
  webBeginRequest(request);
  if (!isPICEnabled()) {
    webSendP(400, PSTR("text/plain"), PSTR("No PIC detected - PIC functions disabled"));
    return;
  }

  // ADR-004: stack char[] buffers replace the previous 3 'const String' locals.
  // 80 bytes covers the longest realistic action/filename/version (typical
  // filenames are ~30 chars; "upgrade"/"refresh"/"delete" are <=7).
  char action[80];
  char filename[80];
  char version[80];
  strlcpy(action,   argCompat("action"),  sizeof(action));
  strlcpy(filename, argCompat("name"),     sizeof(filename));
  strlcpy(version,  argCompat("version"),  sizeof(version));

  DebugTln(F("=== PIC Flash HTTP Request Received ==="));
  DebugTf(PSTR("Action: %s, File: %s, Version: %s\r\n"), action, filename, version);
  DebugTf(PSTR("PIC Device ID: %s\r\n"), state.pic.sDeviceid);
  DebugTf(PSTR("Current state: state.flash.bPICactive=%d, state.flash.bESPactive=%d\r\n"), state.flash.bPICactive, state.flash.bESPactive);

  if (action[0] == '\0' || filename[0] == '\0') {
    DebugTln(F("ERROR: Missing action or filename parameter"));
    webSendP(400, PSTR("text/plain"), PSTR("Missing action or name"));
    return;
  }

  if (strcmp_P(state.pic.sDeviceid, PSTR("unknown")) == 0) {
    DebugTln(F("ERROR: PIC device id is unknown, cannot upgrade"));
    webSendP(400, PSTR("text/plain"), PSTR("PIC device not detected"));
    return; // no pic version found, don't upgrade
  }

  if (strcmp_P(action, PSTR("upgrade")) == 0) {
    DebugTf(PSTR("Upgrade requested for /%s/%s\r\n"), state.pic.sDeviceid, filename);
    // The async response is queued here; the actual upgrade is deferred to the
    // main loop so lwIP can drain the response before the PIC flash begins.
    webSendP(200, PSTR("application/json"), PSTR("{\"status\":\"started\"}"));
    DebugTln(F("HTTP response queued"));

    snprintf_P(pendingUpgradePath, sizeof(pendingUpgradePath), PSTR("/%s/%s"), state.pic.sDeviceid, filename);
    DebugTf(PSTR("Pending upgrade queued: [%s]\r\n"), pendingUpgradePath);
    DebugTln(F("=== HTTP handler complete, upgrade will start in main loop ==="));
    return;
  } else if (strcmp_P(action, PSTR("refresh")) == 0) {
    DebugTf(PSTR("Refresh %s/%s\r\n"), state.pic.sDeviceid, filename);
    // TASK-865.14: refreshpic() does a BLOCKING outbound GET + writeToStream of a
    // full hex to LittleFS. This handler runs on the AsyncTCP task (ADR-132), so
    // do NOT run it here. Queue the download for the loop() worker
    // (handlePendingPicHttp) and return immediately, mirroring the action=upgrade
    // deferral above. The frontend polls firmware/files until the version changes.
    if (queuePicRefresh(filename, version)) {
      DebugTln(F("Refresh queued for loop() context"));
      webSendP(200, PSTR("application/json"), PSTR("{\"status\":\"started\"}"));
    } else {
      DebugTln(F("Refresh rejected: a PIC HTTP job is already pending"));
      webSendP(409, PSTR("application/json"), PSTR("{\"status\":\"busy\"}"));
    }
    return;
  } else if (strcmp_P(action, PSTR("delete")) == 0) {
    DebugTf(PSTR("Delete %s/%s\r\n"), state.pic.sDeviceid, filename);
    char path[64];
    snprintf_P(path, sizeof(path), PSTR("/%s/%s"), state.pic.sDeviceid, filename);
    LittleFS.remove(path);
    char *ext = strstr(path, ".hex");
    if (ext) {
      strlcpy(ext, ".ver", sizeof(path) - (ext - path));
      LittleFS.remove(path);
    }
  }
  webPushHeader(F("Location"), F("index.html#tabPICflash"));
  webSendP(303, PSTR("text/html; charset=UTF-8"), PSTR("<a href='index.html#tabPICflash'>Return</a>"));
}
#endif // HAS_PIC — end of PIC firmware upgrade functions

// =========================================================================
// TASK-693 port (dev TASK-688): persist the OT-bus support map across reboots.
//
// Files (LittleFS):
//   /ot-thermo.json — {"v":1,"device":"thermostat","sent_read":[...],"sent_write":[...]}
//   /ot-boiler.json — {"v":1,"device":"boiler","acked_read":[...],"acked_write":[...],
//                     "unsupported_read":[...],"unsupported_write":[...]}
//
// Read on boot (loadOtSupportFiles, called from setup() after LittleFS.begin).
// Write every 15 min from do15minevent when the per-file dirty flag is
// set. Atomic: write to <path>.tmp, remove canonical, rename. A power loss
// mid-write leaves a *.tmp dangling; the next boot ignores it (only the
// canonical name is loaded) and the next dirty write replaces it.
//
// Stream-based reader (Stream::find + parseInt) and writer (file.print per id)
// — no large RAM buffer at any point.
// =========================================================================

// TASK-695: scan `f` forward until `target` is fully matched at the current
// position. Returns true and leaves the cursor immediately after the match;
// returns false on EOF. Replaces Stream::find() — which depended on Stream's
// timeout-driven read() and is not portably available on LittleFS::File
// across ESP8266 vs ESP32 (the original cause of the PR #641 ESP32 build).
// Naive state machine: fine because our search keys (e.g. "acked_read":[) have
// no repeated prefix character. If a future key has overlapping prefixes,
// switch to KMP.
static bool fileFindToken(File &f, const char *target) {
  const size_t tlen = strlen(target);
  if (tlen == 0) return true;
  size_t mi = 0;
  while (f.available()) {
    int c = f.read();
    if (c < 0) return false;
    if ((char)c == target[mi]) {
      if (++mi == tlen) return true;
    } else {
      mi = ((char)c == target[0]) ? 1 : 0;
    }
  }
  return false;
}

// TASK-695: read a JSON integer array body from `f` (the cursor is positioned
// just after '['). Each unsigned decimal integer found sets the matching bit
// in `bitmap`. Stops on ']' or EOF. Replaces Stream::parseInt(), which had
// the same portability problem as Stream::find(). Tolerates whitespace,
// commas, and trailing values without a final separator.
static void parseIntArrayInto(File &f, uint8_t bitmap[32]) {
  long val = -1;  // -1 = "not accumulating", >=0 = current number in progress
  while (f.available()) {
    int c = f.read();
    if (c < 0 || c == ']') break;
    if (c >= '0' && c <= '9') {
      if (val < 0) val = 0;
      val = val * 10 + (c - '0');
    } else if (val >= 0) {
      if (val <= 255) bitmap[val >> 3] |= (uint8_t)(1u << (val & 7));
      val = -1;
    }
  }
  // Finalize any trailing number not followed by a delimiter (defensive — our
  // writer always emits a ']', but the helper stays robust to malformed files).
  if (val >= 0 && val <= 255) bitmap[val >> 3] |= (uint8_t)(1u << (val & 7));
}

// Read one named integer array from an open File, into a 32-byte bitmap.
// Uses only File::read() / available() — no Stream::find() / parseInt(), which
// are not portably available on LittleFS::File between ESP8266 and ESP32.
// Returns true if the key was found (regardless of how many ids it contained).
static bool readBitmapArrayFromJson(File &f, const __FlashStringHelper *keyPattern, uint8_t bitmap[32]) {
  // Cursor reset so multiple calls on the same file find each key independently.
  f.seek(0);
  char patBuf[40];
  strncpy_P(patBuf, reinterpret_cast<PGM_P>(keyPattern), sizeof(patBuf) - 1);
  patBuf[sizeof(patBuf) - 1] = '\0';
  if (!fileFindToken(f, patBuf)) return false;
  parseIntArrayInto(f, bitmap);
  return true;
}

// Stream one bitmap as a JSON integer array body (no brackets) to an open File.
static void writeBitmapArrayToJson(File &f, const uint8_t bitmap[32]) {
  bool first = true;
  for (int i = 0; i <= 255; i++) {
    if ((bitmap[i >> 3] & (uint8_t)(1u << (i & 7))) == 0) continue;
    if (!first) f.print(',');
    f.print(i);
    first = false;
  }
}

void loadOtSupportFiles() {
  // Thermostat side.
  File f = LittleFS.open("/ot-thermo.json", "r");
  if (f) {
    // Magic gate: read the start of the file and verify "v":1 is present.
    char header[16] = {0};
    int n = f.read((uint8_t*)header, sizeof(header) - 1);
    header[n > 0 ? n : 0] = '\0';
    if (strstr_P(header, PSTR("\"v\":1")) != nullptr) {
      // TASK-696: short keys (sr/sw/ar/aw/ur/uw) to trim flash on ESP32.
      // The file is firmware-internal; the WebUI / REST surfaces use the long
      // names. Magic "v":1 unchanged so a future schema bump can still gate.
      readBitmapArrayFromJson(f, F("\"sr\":["), thermostatSentRead);
      readBitmapArrayFromJson(f, F("\"sw\":["), thermostatSentWrite);
      DebugTln(F("ot-support: thermostat profile loaded from /ot-thermo.json"));
    } else {
      DebugTln(F("ot-support: /ot-thermo.json header missing/invalid — starting fresh"));
    }
    f.close();
  }
  // Boiler side.
  f = LittleFS.open("/ot-boiler.json", "r");
  if (f) {
    char header[16] = {0};
    int n = f.read((uint8_t*)header, sizeof(header) - 1);
    header[n > 0 ? n : 0] = '\0';
    if (strstr_P(header, PSTR("\"v\":1")) != nullptr) {
      readBitmapArrayFromJson(f, F("\"ar\":["), boilerAckedRead);
      readBitmapArrayFromJson(f, F("\"aw\":["), boilerAckedWrite);
      readBitmapArrayFromJson(f, F("\"ur\":["), boilerUnsupportedRead);
      readBitmapArrayFromJson(f, F("\"uw\":["), boilerUnsupportedWrite);
      DebugTln(F("ot-support: boiler profile loaded from /ot-boiler.json"));
    } else {
      DebugTln(F("ot-support: /ot-boiler.json header missing/invalid — starting fresh"));
    }
    f.close();
  }
  // We loaded what's on disk, so the on-disk copy matches RAM: not dirty.
  thermostatFileDirty = false;
  boilerFileDirty     = false;
}

// Atomic write: <path>.tmp -> remove canonical -> rename. A power loss between
// the two final steps leaves only one valid file (either the old canonical or
// the new *.tmp ignored by loadOtSupportFiles); next dirty write recovers.
static bool writeOtThermoFile(const char* canonicalPath, const char* tmpPath) {
  File f = LittleFS.open(tmpPath, "w");
  if (!f) return false;
  // TASK-696: short keys (sr/sw) match the reader and save ~30 B per write.
  f.print(F("{\"v\":1,\"device\":\"thermostat\",\"sr\":["));
  writeBitmapArrayToJson(f, thermostatSentRead);
  f.print(F("],\"sw\":["));
  writeBitmapArrayToJson(f, thermostatSentWrite);
  f.print(F("]}\n"));
  f.close();
  LittleFS.remove(canonicalPath);  // no-op if absent
  return LittleFS.rename(tmpPath, canonicalPath);
}

static bool writeOtBoilerFile(const char* canonicalPath, const char* tmpPath) {
  File f = LittleFS.open(tmpPath, "w");
  if (!f) return false;
  // TASK-696: short keys (ar/aw/ur/uw) match the reader; saves ~100 B per write.
  f.print(F("{\"v\":1,\"device\":\"boiler\",\"ar\":["));
  writeBitmapArrayToJson(f, boilerAckedRead);
  f.print(F("],\"aw\":["));
  writeBitmapArrayToJson(f, boilerAckedWrite);
  f.print(F("],\"ur\":["));
  writeBitmapArrayToJson(f, boilerUnsupportedRead);
  f.print(F("],\"uw\":["));
  writeBitmapArrayToJson(f, boilerUnsupportedWrite);
  f.print(F("]}\n"));
  f.close();
  LittleFS.remove(canonicalPath);
  return LittleFS.rename(tmpPath, canonicalPath);
}

void saveOtSupportFilesIfDirty() {
  if (thermostatFileDirty) {
    if (writeOtThermoFile("/ot-thermo.json", "/ot-thermo.json.tmp")) {
      thermostatFileDirty = false;
      DebugTln(F("ot-support: /ot-thermo.json written"));
    } else {
      DebugTln(F("ot-support: /ot-thermo.json write FAILED — will retry"));
    }
  }
  if (boilerFileDirty) {
    if (writeOtBoilerFile("/ot-boiler.json", "/ot-boiler.json.tmp")) {
      boilerFileDirty = false;
      DebugTln(F("ot-support: /ot-boiler.json written"));
    } else {
      DebugTln(F("ot-support: /ot-boiler.json write FAILED — will retry"));
    }
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
