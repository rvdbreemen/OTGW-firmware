/*
***************************************************************************
**  Program  : OTDirect.ino
**  Version  : v2.0.0-alpha.63
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Direct GPIO OpenTherm driver for OTGW32 (ESP32-S3, no PIC).
**  Uses the project-pinned OpenTherm.h library to drive the OT bus via
**  GPIO interrupts and hardware-timer Manchester encoding. This module
**  requires support for async/non-blocking requests and slave mode.
**
**  Frame bridge pattern: 32-bit OT frames are formatted as 9-char text
**  strings (e.g. "B%08lX") and fed into processOT(), reusing the entire
**  existing parser/MQTT/REST/WebSocket stack unchanged.
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#if HAS_DIRECT_OT

#include <OpenTherm.h>

// ---------------------------------------------------------------------------
// Per-module conditional debug — toggle with key '6' in telnet debug menu
// ---------------------------------------------------------------------------
#define OTDDebugTf(fmt, ...)  do { if (state.debug.bOTDirect) DebugTf(fmt,  ##__VA_ARGS__); } while(0)
#define OTDDebugTln(s)        do { if (state.debug.bOTDirect) DebugTln(s);                  } while(0)
#define OTDDebugf(fmt, ...)   do { if (state.debug.bOTDirect) Debugf(fmt,   ##__VA_ARGS__); } while(0)

// ---------------------------------------------------------------------------
// OpenTherm interface instances — master talks to boiler, slave listens to thermostat
// ---------------------------------------------------------------------------
static OpenTherm otMaster(PIN_OT_MASTER_IN, PIN_OT_MASTER_OUT);
static OpenTherm otSlave(PIN_OT_SLAVE_IN, PIN_OT_SLAVE_OUT, true);  // true = slave mode

// ISR handlers — must be free functions for attachInterrupt
static void IRAM_ATTR masterISR() { otMaster.handleInterrupt(); }
static void IRAM_ATTR slaveISR()  { otSlave.handleInterrupt(); }

// Forward declarations for static helpers defined later in this file.
// ctags does not evaluate #if guards, so it may not generate these
// declarations when scanning a single-TU build (PlatformIO Arduino builder).
static unsigned long buildOTResponse(uint8_t type, uint8_t msgId, uint16_t data);
static unsigned long applyResponseModifiers(unsigned long response);
// TASK-466: TT=/TC= remote-override state machine helpers
static void setRemoteOverride(OTRemoteOverrideMode mode, float celsius);
static void clearRemoteOverride();
static void onThermostatMsgID16(uint8_t msgType, uint16_t f88);

// ---------------------------------------------------------------------------
// Master request scheduler — periodic polls and writes to the boiler
// ---------------------------------------------------------------------------
static constexpr uint32_t OT_STATUS_INTERVAL_MS   = 800;    // MsgID 0 (status) every 800ms (OT-Thing parity)
static constexpr uint32_t OT_OFFLINE_RETRY_MS     = 5000;   // When bus is offline: retry MsgID 0 every 5s, skip rest
static constexpr uint32_t OT_TEMP_INTERVAL_MS     = 10000;  // Temperature reads every 10s
static constexpr uint32_t OT_SLOW_INTERVAL_MS     = 60000;  // Slow-poll items every 60s
static constexpr uint32_t OT_WRITE_INTERVAL_MS    = 15000;  // Periodic writes every 15s (keep boiler values alive)

// Status flags to send in MsgID 0 (master status byte)
static uint8_t otMasterStatusFlags = 0x01;  // bit0=CH enable (default on)

// Current operating mode (gateway/monitor/bypass/master)
static OTDirectMode otCurrentMode = OTD_MODE_GATEWAY;

// Inline mode checks — single source of truth is otCurrentMode
#define IS_BYPASS_MODE()   (otCurrentMode == OTD_MODE_BYPASS)
#define IS_MONITOR_MODE()  (otCurrentMode == OTD_MODE_MONITOR)
#define IS_MASTER_MODE()   (otCurrentMode == OTD_MODE_MASTER)
#define IS_LOOPBACK_MODE() (otCurrentMode == OTD_MODE_LOOPBACK)

// ---------------------------------------------------------------------------
// Parity helper — sets bit 31 to make total number of 1-bits even
// ---------------------------------------------------------------------------
static inline void setOTParityBit(unsigned long &frame) {
  frame &= 0x7FFFFFFFUL;
  uint32_t p = frame;
  p ^= (p >> 16); p ^= (p >> 8); p ^= (p >> 4); p ^= (p >> 2); p ^= (p >> 1);
  if (p & 1) frame |= 0x80000000UL;
}

// Thermostat connectivity tracking — detect disconnect and apply setback
static bool     otThermostatSeen    = false;   // at least one frame received since boot
static uint32_t otLastThermostatMs  = 0;       // millis() of last thermostat frame
static bool     otSetbackEngaged    = false;    // setback override currently active

// Phase 1: Additional master status flags (moved here: used before original definition)
static bool otSummerMode       = false;     // SM= summer mode (bit5 of MsgID 0 master status)

// Phase 4: Message interval — MI= global minimum inter-message gap
static uint32_t otMinIntervalMs  = 100;     // default 100ms
static uint32_t otLastAnySendMs  = 0;       // timestamp of last frame sent

// Phase 5: Fail safety — FS= controls setback on thermostat disconnect
static bool otFailSafeEnabled    = true;    // default: enabled

// Phase 9: DHW push state machine (moved here: used before original definition)
enum DHWPushState : uint8_t { PUSH_IDLE = 0, PUSH_PENDING, PUSH_STARTED };
static DHWPushState otDHWPushState = PUSH_IDLE;
static uint32_t otDHWPushStartMs = 0;
static constexpr uint32_t OT_DHW_PUSH_TIMEOUT_MS = 30000;  // 30s, matches PIC

// Phase 10: PS= summary mode (moved here: used before original definition)
static bool otSummaryMode     = false;      // PS=1 active
static bool otHideReports     = false;      // suppress T/B/R/A frame output
static bool otSummaryPending  = false;      // summary line ready to emit

// Schedule table: supports both reads and periodic writes.
// Write entries periodically re-send a cached value to keep the boiler in sync.
// Entries with `disabled = true` are skipped (auto-set on UNKNOWN_DATA_ID response).
struct OTScheduleEntry {
  uint8_t  msgId;
  uint32_t intervalMs;
  uint32_t lastSentMs;
  bool     disabled;     // self-disabling: set true when boiler returns UNKNOWN_DATA_ID
  bool     isWrite;      // true = send WRITE_DATA with cachedValue instead of READ_DATA
  bool     valueSet;     // true = cachedValue has been set at least once
  uint16_t cachedValue;  // f8.8 or raw value for writes
};

// R = read-only entry, W = periodic write entry (inactive until value set by command)
#define R_ENTRY(id, interval) { id, interval, 0, false, false, false, 0 }
#define W_ENTRY(id, interval) { id, interval, 0, false, true,  false, 0 }

static OTScheduleEntry otSchedule[] = {
  // === Fast poll — essential for control loop and dashboard ===
  R_ENTRY( 0, OT_STATUS_INTERVAL_MS),   // Status (mandatory, ≤1s)

  // === Periodic writes — keep boiler setpoints alive (15s refresh) ===
  // Write entries are inactive until a value is set by a command.
  W_ENTRY( 1, OT_WRITE_INTERVAL_MS),    // Control setpoint (TSet) — CS=/TC=
  W_ENTRY( 7, OT_WRITE_INTERVAL_MS),    // Cooling control signal — CC=
  W_ENTRY( 8, OT_WRITE_INTERVAL_MS),    // Control setpoint CH2 (TsetCH2) — C2=
  W_ENTRY(14, OT_WRITE_INTERVAL_MS),    // Max relative modulation — MM=
  W_ENTRY(16, OT_WRITE_INTERVAL_MS),    // Room setpoint (TrSet) — TT=
  W_ENTRY(24, OT_WRITE_INTERVAL_MS),    // Room temperature (Tr)
  W_ENTRY(27, OT_WRITE_INTERVAL_MS),    // Outside temperature (Toutside) — OT=
  W_ENTRY(56, OT_WRITE_INTERVAL_MS),    // DHW setpoint (TdhwSet) — SW=
  W_ENTRY(57, OT_WRITE_INTERVAL_MS),    // Max CH water setpoint (MaxTSet) — SH=

  // === Temperature & modulation reads — 10s interval ===
  R_ENTRY(25, OT_TEMP_INTERVAL_MS),     // Boiler flow temp (Tboiler)
  R_ENTRY(28, OT_TEMP_INTERVAL_MS),     // Return water temp (Tret)
  R_ENTRY(26, OT_TEMP_INTERVAL_MS),     // DHW temp (Tdhw)
  R_ENTRY(17, OT_TEMP_INTERVAL_MS),     // Relative modulation level
  R_ENTRY(18, OT_TEMP_INTERVAL_MS),     // CH water pressure
  R_ENTRY(19, OT_TEMP_INTERVAL_MS),     // DHW flow rate
  R_ENTRY(29, OT_TEMP_INTERVAL_MS),     // Solar storage temperature
  R_ENTRY(30, OT_TEMP_INTERVAL_MS),     // Solar collector temperature (s16)
  R_ENTRY(31, OT_TEMP_INTERVAL_MS),     // Flow temp CH2
  R_ENTRY(32, OT_TEMP_INTERVAL_MS),     // DHW2 temperature
  R_ENTRY(35, OT_TEMP_INTERVAL_MS),     // Fan speed setpoint/actual
  R_ENTRY(36, OT_TEMP_INTERVAL_MS),     // Flame current (µA)
  R_ENTRY(38, OT_TEMP_INTERVAL_MS),     // Relative humidity

  // === Slow poll — config/diagnostics, 60s interval ===
  // All entries auto-disable if boiler responds UNKNOWN_DATA_ID.
  R_ENTRY( 3, OT_SLOW_INTERVAL_MS),     // Slave configuration / member ID
  R_ENTRY( 5, OT_SLOW_INTERVAL_MS),     // ASF flags / OEM fault code
  R_ENTRY( 6, OT_SLOW_INTERVAL_MS),     // Remote boiler parameter flags
  R_ENTRY( 9, OT_SLOW_INTERVAL_MS),     // Remote override room setpoint
  R_ENTRY(15, OT_SLOW_INTERVAL_MS),     // Max boiler capacity & min modulation
  // Date/time (read from boiler, slow poll)
  R_ENTRY(20, OT_SLOW_INTERVAL_MS),     // Day of week / time of day
  R_ENTRY(21, OT_SLOW_INTERVAL_MS),     // Date (month + day)
  R_ENTRY(22, OT_SLOW_INTERVAL_MS),     // Year
  R_ENTRY(33, OT_SLOW_INTERVAL_MS),     // Exhaust temperature (s16)
  R_ENTRY(34, OT_SLOW_INTERVAL_MS),     // Boiler heat exchanger temp
  R_ENTRY(37, OT_SLOW_INTERVAL_MS),     // Room temperature CH2
  R_ENTRY(39, OT_SLOW_INTERVAL_MS),     // Remote override room setpoint 2
  // Bounds (s8+s8)
  R_ENTRY(48, OT_SLOW_INTERVAL_MS),     // TdhwSet upper/lower bounds
  R_ENTRY(49, OT_SLOW_INTERVAL_MS),     // MaxTSet upper/lower bounds
  R_ENTRY(50, OT_SLOW_INTERVAL_MS),     // OTC heat curve ratio bounds
  R_ENTRY(51, OT_SLOW_INTERVAL_MS),     // Remote param 4 bounds
  R_ENTRY(52, OT_SLOW_INTERVAL_MS),     // Remote param 5 bounds
  R_ENTRY(53, OT_SLOW_INTERVAL_MS),     // Remote param 6 bounds
  R_ENTRY(54, OT_SLOW_INTERVAL_MS),     // Remote param 7 bounds
  R_ENTRY(55, OT_SLOW_INTERVAL_MS),     // Remote param 8 bounds
  // Remote parameters (f8.8, R/W)
  R_ENTRY(58, OT_SLOW_INTERVAL_MS),     // OTC heat curve ratio (Hcratio)
  R_ENTRY(59, OT_SLOW_INTERVAL_MS),     // Remote parameter 4
  R_ENTRY(60, OT_SLOW_INTERVAL_MS),     // Remote parameter 5
  R_ENTRY(61, OT_SLOW_INTERVAL_MS),     // Remote parameter 6
  R_ENTRY(62, OT_SLOW_INTERVAL_MS),     // Remote parameter 7
  R_ENTRY(63, OT_SLOW_INTERVAL_MS),     // Remote parameter 8
  // Ventilation / heat-recovery
  R_ENTRY(70, OT_SLOW_INTERVAL_MS),     // V/H status (flag8+flag8)
  R_ENTRY(71, OT_SLOW_INTERVAL_MS),     // V/H control setpoint
  R_ENTRY(72, OT_SLOW_INTERVAL_MS),     // V/H ASF flags / fault code
  R_ENTRY(73, OT_SLOW_INTERVAL_MS),     // V/H OEM diagnostic code
  R_ENTRY(74, OT_SLOW_INTERVAL_MS),     // V/H config / member ID
  R_ENTRY(75, OT_SLOW_INTERVAL_MS),     // V/H OpenTherm version
  R_ENTRY(76, OT_SLOW_INTERVAL_MS),     // V/H product version
  R_ENTRY(77, OT_SLOW_INTERVAL_MS),     // Relative ventilation level
  R_ENTRY(78, OT_SLOW_INTERVAL_MS),     // Relative humidity exhaust air
  R_ENTRY(79, OT_SLOW_INTERVAL_MS),     // CO2 level exhaust air (ppm)
  R_ENTRY(80, OT_SLOW_INTERVAL_MS),     // Supply inlet temperature
  R_ENTRY(81, OT_SLOW_INTERVAL_MS),     // Supply outlet temperature
  R_ENTRY(82, OT_SLOW_INTERVAL_MS),     // Exhaust inlet temperature
  R_ENTRY(83, OT_SLOW_INTERVAL_MS),     // Exhaust outlet temperature
  R_ENTRY(84, OT_SLOW_INTERVAL_MS),     // Exhaust fan speed (rpm)
  R_ENTRY(85, OT_SLOW_INTERVAL_MS),     // Supply fan speed (rpm)
  R_ENTRY(86, OT_SLOW_INTERVAL_MS),     // V/H remote parameter flags
  R_ENTRY(87, OT_SLOW_INTERVAL_MS),     // Nominal ventilation value
  R_ENTRY(88, OT_SLOW_INTERVAL_MS),     // V/H TSP count
  R_ENTRY(89, OT_SLOW_INTERVAL_MS),     // V/H TSP entry
  R_ENTRY(90, OT_SLOW_INTERVAL_MS),     // V/H fault buffer size
  R_ENTRY(91, OT_SLOW_INTERVAL_MS),     // V/H fault buffer entry
  // Brand info (u8+u8, index-based — will get index 0 only)
  R_ENTRY(93, OT_SLOW_INTERVAL_MS),     // Brand name (index 0)
  R_ENTRY(94, OT_SLOW_INTERVAL_MS),     // Brand version (index 0)
  R_ENTRY(95, OT_SLOW_INTERVAL_MS),     // Brand serial number (index 0)
  // Counters and special
  R_ENTRY(96, OT_SLOW_INTERVAL_MS),     // Cooling operation hours
  R_ENTRY(97, OT_SLOW_INTERVAL_MS),     // Power cycles
  R_ENTRY(98, OT_SLOW_INTERVAL_MS),     // Remote parameter 4 bounds (s8+s8)
  R_ENTRY(99, OT_SLOW_INTERVAL_MS),     // Remote parameter 5 bounds (s8+s8)
  R_ENTRY(100, OT_SLOW_INTERVAL_MS),    // Remote override function
  // Solar storage
  R_ENTRY(101, OT_SLOW_INTERVAL_MS),    // Solar storage status
  R_ENTRY(102, OT_SLOW_INTERVAL_MS),    // Solar storage ASF flags
  R_ENTRY(103, OT_SLOW_INTERVAL_MS),    // Solar storage config / member ID
  R_ENTRY(104, OT_SLOW_INTERVAL_MS),    // Solar storage product version
  R_ENTRY(105, OT_SLOW_INTERVAL_MS),    // Solar storage TSP count
  R_ENTRY(106, OT_SLOW_INTERVAL_MS),    // Solar storage TSP entry
  R_ENTRY(107, OT_SLOW_INTERVAL_MS),    // Solar storage fault buffer size
  R_ENTRY(108, OT_SLOW_INTERVAL_MS),    // Solar storage fault buffer entry
  // Electricity producer
  R_ENTRY(109, OT_SLOW_INTERVAL_MS),    // Electricity producer starts
  R_ENTRY(110, OT_SLOW_INTERVAL_MS),    // Electricity producer hours
  R_ENTRY(111, OT_SLOW_INTERVAL_MS),    // Electricity production (W)
  R_ENTRY(112, OT_SLOW_INTERVAL_MS),    // Cumulative electricity production (kWh)
  // Burner diagnostics
  R_ENTRY(113, OT_SLOW_INTERVAL_MS),    // Unsuccessful burner starts
  R_ENTRY(114, OT_SLOW_INTERVAL_MS),    // Flame signal too low count
  R_ENTRY(115, OT_SLOW_INTERVAL_MS),    // OEM diagnostic code
  // Starts & operation hours counters
  R_ENTRY(116, OT_SLOW_INTERVAL_MS),    // Burner starts
  R_ENTRY(117, OT_SLOW_INTERVAL_MS),    // CH pump starts
  R_ENTRY(118, OT_SLOW_INTERVAL_MS),    // DHW pump/valve starts
  R_ENTRY(119, OT_SLOW_INTERVAL_MS),    // DHW burner starts
  R_ENTRY(120, OT_SLOW_INTERVAL_MS),    // Burner operation hours
  R_ENTRY(121, OT_SLOW_INTERVAL_MS),    // CH pump operation hours
  R_ENTRY(122, OT_SLOW_INTERVAL_MS),    // DHW pump/valve operation hours
  R_ENTRY(123, OT_SLOW_INTERVAL_MS),    // DHW burner operation hours
  // OT version & product info
  R_ENTRY(125, OT_SLOW_INTERVAL_MS),    // OpenTherm version slave
  R_ENTRY(127, OT_SLOW_INTERVAL_MS),    // Slave product type/version
};

#undef R_ENTRY
#undef W_ENTRY

static constexpr uint8_t OT_SCHEDULE_SIZE = sizeof(otSchedule) / sizeof(otSchedule[0]);
static uint8_t otScheduleIdx = 0;

// TASK-583: fast ventilation poll interval — 10s when slave app is vent/HRV.
// Only MsgIDs 70 (V/H status) and 71 (V/H setpoint) move to this tier;
// diagnostic MsgIDs 72-76 remain in the 60s slow-poll tier.
// See OT spec: slave config HB bits 6-7 = 0b10 indicates ventilation/HRV.
// We also accept the cached MsgID 3 HB as a fallback if explicit bits differ.
static constexpr uint32_t OT_VENT_FAST_INTERVAL_MS = 10000;  // 10s fast vent poll

// Raw boiler response cache — forward-declared here so otIsVentSlave() can use
// them; defined with initializers further below.
static uint16_t otBoilerCache[128];
static bool     otBoilerCacheValid[128];

// otIsVentSlave() — true when the boiler slave reports a ventilation/HRV
// application type in its slave configuration (MsgID 3).
//
// OT spec: slave config byte (MsgID 3 high byte) bits 6-7:
//   00 = boiler/CH  01 = solar collector  10 = heat pump  11 = ventilation/HRV
// Interpretation varies by implementation; bit 1 of the low byte (member ID)
// being non-zero is treated as an additional heuristic.
// Primary check: bits 6-7 = 0b11 (0xC0 mask) in the slave config HB.
static inline bool otIsVentSlave() {
  if (!otBoilerCacheValid[3]) return false;
  uint8_t slaveCfgHB = (otBoilerCache[3] >> 8) & 0xFF;
  // bits 6-7 == 0b11 → ventilation/HRV application per OT spec
  return ((slaveCfgHB & 0xC0) == 0xC0);
}

// Command ring buffer — queues frames from handleOTDirectCommand() and
// the various per-channel input paths (MQTT, REST, WebUI, telnet serial).
// This is the FAN-IN convergence point per the OTDirect architecture: the
// channel of origin does not matter, every command lands here.
//
// Sizing rationale (intentional, do not bump): in normal operation the
// queue should never approach capacity. If drops occur, the producer rate
// is the problem, not the queue size. See TASK-494 / TASK-466 / ADR-064
// for the architectural model.
static constexpr uint8_t OT_CMD_QUEUE_SIZE = 12;
static uint32_t otCmdQueue[OT_CMD_QUEUE_SIZE];
static uint8_t  otCmdHead = 0;   // next write position
static uint8_t  otCmdTail = 0;   // next read position
static uint8_t  otCmdQueueHighWater = 0;  // TASK-494: peak depth observed

static bool otCmdQueueEmpty() { return otCmdHead == otCmdTail; }
static bool otCmdQueueFull()  { return ((otCmdHead + 1) % OT_CMD_QUEUE_SIZE) == otCmdTail; }

static uint8_t otCmdQueueDepth() {
  return (otCmdHead + OT_CMD_QUEUE_SIZE - otCmdTail) % OT_CMD_QUEUE_SIZE;
}

// TASK-494: enqueue with coalesce-by-MsgID.
//
// If a frame for the SAME OT MsgID is already pending in the queue, replace
// its data16 field in place and return success without growing the queue
// depth. Otherwise add to the head as a normal ring-buffer push.
//
// Coalescing key: MsgID only (not full frame). Multiple producers issuing
// the SAME MsgID with different data values within one OT cycle (~115 ms)
// would otherwise consume one slot per call — wasteful, since the boiler
// only acts on the most recent WRITE_DATA value per MsgID anyway.
//
// Position-preserving: the existing entry stays in its slot; only the data
// is replaced. Order across DIFFERENT MsgIDs is preserved.
//
// Sequences like `set MsgID 100=X` then `set MsgID 100=Y` within one OT
// cycle collapse to "Y" reaching the bus — intentional PIC-parity
// semantics, self-consistent with the override-table state which is also
// updated to the latest value on each call.
//
// Safe for the entire current producer set (MsgIDs 1, 4, 7, 8, 14, 16,
// 56, 57, 99, 100, 116-123). If a future producer adds an order-sensitive
// MsgID (each value must reach the bus distinctly), it must filter
// upstream; the helper does not whitelist.
static bool otCmdEnqueue(uint32_t frame) {
  uint8_t newMsgId = (frame >> 16) & 0xFF;

  // Walk pending entries tail → head; replace in place on MsgID match.
  uint8_t i = otCmdTail;
  while (i != otCmdHead) {
    uint8_t existingMsgId = (otCmdQueue[i] >> 16) & 0xFF;
    if (existingMsgId == newMsgId) {
      otCmdQueue[i] = frame;
      OTDDebugTf(PSTR("OTD: queue coalesced MsgID %u -> 0x%08lX\r\n"),
                 (unsigned)newMsgId, (unsigned long)frame);
      return true;
    }
    i = (i + 1) % OT_CMD_QUEUE_SIZE;
  }

  if (otCmdQueueFull()) return false;
  otCmdQueue[otCmdHead] = frame;
  otCmdHead = (otCmdHead + 1) % OT_CMD_QUEUE_SIZE;

  // Track peak depth for diagnostic visibility. Normal operation should
  // keep this well below OT_CMD_QUEUE_SIZE; if it climbs, the producer
  // rate is the issue (not size).
  uint8_t depth = otCmdQueueDepth();
  if (depth > otCmdQueueHighWater) {
    otCmdQueueHighWater = depth;
    OTDDebugTf(PSTR("OTD: queue high-water=%u (capacity=%u)\r\n"),
               (unsigned)depth, (unsigned)OT_CMD_QUEUE_SIZE);
  }
  return true;
}

static bool otCmdDequeue(uint32_t &frame) {
  if (otCmdQueueEmpty()) return false;
  frame = otCmdQueue[otCmdTail];
  otCmdTail = (otCmdTail + 1) % OT_CMD_QUEUE_SIZE;
  return true;
}

static bool     otSlaveFramePending = false;
static unsigned long otSlaveFrame = 0;

// ---------------------------------------------------------------------------
// Frame override table — repeater mode (selective thermostat frame modification)
// When thermostat sends a frame for a MsgID with an active override,
// the data value is replaced before forwarding to the boiler.
// ---------------------------------------------------------------------------
struct OTFrameOverride {
  uint8_t  msgId;
  bool     active;
  uint16_t overrideValue;  // f8.8 or raw data to inject
};

static OTFrameOverride otOverrides[] = {
  {  1, false, 0 },   // TSet — CS= override
  {  7, false, 0 },   // CoolingControl — CC= override
  {  8, false, 0 },   // TsetCH2 — C2= override
  { 14, false, 0 },   // MaxRelModLevelSetting — MM= override
  { 16, false, 0 },   // TrSet — TT=/TC= override
  { 56, false, 0 },   // TdhwSet — SW= override
  { 57, false, 0 },   // MaxTSet — SH= override
  // TASK-466: MsgID 100 RemoteOverrideFunction — flag synthesis for TT (0x02) / TC (0x01)
  { 100, false, 0 },  // RemoteOverrideFunction — TT/TC flag synthesis
};
static constexpr uint8_t OT_OVERRIDE_COUNT = sizeof(otOverrides) / sizeof(otOverrides[0]);

// TASK-466: PIC-style TT=/TC= remote-override state machine (see OTDirecttypes.h
// for the enum + struct). Single file-static instance: only one TT/TC override
// is active at a time, second command replaces the first. Not persisted: PIC
// resets these on power cycle / heartbeat reset, and so do we.
static OTRemoteOverrideState otRemoteOverride = {
  OT_OVERRIDE_NONE,  // mode
  0,                 // f88Value
  0,                 // setAtMs
  0,                 // lastThermostatMs
  0,                 // lastThermostatVal (only valid when bHaveLastThermostat=true)
  false,             // bHaveLastThermostat — TASK-498 (1A-M2)
  0                  // honoredCount
};

// TASK-466: tunables for the auto-clear honour state machine. Kept as named
// constants so future tuning is grep-able. Values mirror gateway.asm intent:
// "thermostat is echoing us within ~quarter-degree" -> honoured;
// "thermostat moved more than half a degree after honouring us" -> released.
// NB: the (uint16_t) cast TRUNCATES the float; only fractions of 1/256 land
// exactly on an integer (0.25, 0.50, 0.125, ...). A future tuner picking 0.3
// would silently get 76 instead of 76.8 — keep the fraction grid-aligned or
// switch to integer literals (e.g. 0x0040, 0x0080) with the °C in a comment.
static constexpr uint16_t OT_OVERRIDE_HONOR_DELTA_F88   = (uint16_t)(0.25f * 256.0f); // 0x0040
static constexpr uint16_t OT_OVERRIDE_RELEASE_DELTA_F88 = (uint16_t)(0.50f * 256.0f); // 0x0080
static constexpr uint8_t  OT_OVERRIDE_HONOR_THRESHOLD   = 3;   // cycles before auto-clear is armed

// TASK-497 (4A-M1): single canonical float-to-f8.8 cast with the same
// `-40..127` clamp that TASK-495 added to setRemoteOverride(). f8.8 is a
// 16-bit signed two's-complement fixed-point format; converting from
// float to int16_t outside `[-128, 127]` is C/C++ undefined behaviour.
// Every OTDirect call site that emits an f8.8 value to the bus must
// route through this helper so the contract is checked exactly once.
//
// -40..127 covers all realistic OT v4.2 setpoints (room, flow, DHW)
// including the negative outliers a frost-protect or freezer-room
// deployment would emit.
static inline uint16_t floatToF88(float celsius) {
  if (celsius < -40.0f) celsius = -40.0f;
  if (celsius > 127.0f) celsius = 127.0f;
  return (uint16_t)((int16_t)(celsius * 256.0f));
}

// TASK-442: PIC parity for CS/C2 heartbeat-driven expiry.
// gateway.asm CommandExpiry invalidates OverrideCH/OverrideCH2 when the
// CS/C2 heartbeat flags have not been refreshed within ~60s. The
// underlying intent is safety: a stale flow setpoint should not stay
// active forever. Track last-command timestamp per command; the periodic
// task in loopOTDirect() clears the override when stale.
// 0 means "no active override" (skip expiry check).
static uint32_t otCSLastCommandMs = 0;
static uint32_t otC2LastCommandMs = 0;
static constexpr uint32_t OT_CSC2_EXPIRY_MS = 60000;  // ~1 minute, matches PIC heartbeat window

// Set an override value for a MsgID (activates repeater modification for that ID)
static void setOverride(uint8_t msgId, uint16_t value) {
  for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) {
    if (otOverrides[i].msgId == msgId) {
      otOverrides[i].active = true;
      otOverrides[i].overrideValue = value;
      return;
    }
  }
}

// Clear an override for a MsgID (stop modifying thermostat frames for this ID)
static void clearOverride(uint8_t msgId) {
  for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) {
    if (otOverrides[i].msgId == msgId) {
      otOverrides[i].active = false;
      return;
    }
  }
}

// ---------------------------------------------------------------------------
// PIC-compat state variables — declared early because ESP32's Arduino core
// processes auto-prototypes before variable definitions, unlike ESP8266.
// ---------------------------------------------------------------------------
static float   otSetbackTemp     = 16.0f;   // SB= setback temperature
static uint8_t otIgnoreTransitions = 1;      // IT= (1=ignore, PIC default)
static uint8_t otOverrideHB      = 0;        // OH= override high byte flag
static char    otGpioFunctions[3] = "00";    // GA=/GB= function codes
static char    otLedFunctions[7]  = "XXXXXX";// LA=-LF= config chars
static uint8_t otVoltageRef      = 5;        // VR= voltage reference digit
static char    otTempSensor      = 'O';      // TS= temp sensor function
static char    otForceThermostat = 'A';      // FT= thermostat detection
static uint8_t otDHWOverride     = 0xFF;     // HW state: 0='0', 1='1', 0xFF='A' (auto)

// BS= fake room setpoint — intercepts thermostat MsgID 16 READ_DATA frames
// and replaces the data bytes with the fake setpoint before forwarding to boiler.
// 0.0 = disabled (BS=0 clears the fake). Mirrors PIC gateway.asm:3019-3024.
static float otFakeRoomSetpoint = 0.0f;

// SR/CR response override table
static constexpr uint8_t OT_RESPONSE_OVERRIDE_MAX = 16;
struct OTResponseOverride {
  uint8_t  msgId;
  bool     active;
  uint16_t value;
};
static OTResponseOverride otResponseOverrides[OT_RESPONSE_OVERRIDE_MAX];

// UI/KI unknown-ID table
static constexpr uint8_t OT_UNKNOWN_ID_MAX = 16;
static uint8_t otUnknownIds[OT_UNKNOWN_ID_MAX];
static uint8_t otUnknownIdCount = 0;

static bool isUnknownId(uint8_t msgId) {
  for (uint8_t i = 0; i < otUnknownIdCount; i++) {
    if (otUnknownIds[i] == msgId) return true;
  }
  return false;
}

// RM/CM response-path modifier table
struct OTResponseModify {
  uint8_t  msgId;
  bool     active;
  uint16_t value;
};
static constexpr uint8_t OT_RESPONSE_MODIFY_MAX = 8;
static OTResponseModify otResponseModifiers[OT_RESPONSE_MODIFY_MAX];

// Phase 8: Unknown ID 3-strike auto-blacklist counters (2 bits per MsgID)
static uint8_t otUnknownCounters[32] = {0}; // 128 MsgIDs × 2 bits = 32 bytes

static uint8_t getUnknownCount(uint8_t msgId) {
  uint8_t byteIdx = msgId >> 2;
  uint8_t bitShift = (msgId & 0x03) * 2;
  return (otUnknownCounters[byteIdx] >> bitShift) & 0x03;
}
static void incUnknownCount(uint8_t msgId) {
  uint8_t byteIdx = msgId >> 2;
  uint8_t bitShift = (msgId & 0x03) * 2;
  uint8_t count = (otUnknownCounters[byteIdx] >> bitShift) & 0x03;
  if (count < 3) {
    otUnknownCounters[byteIdx] &= ~(0x03 << bitShift);
    otUnknownCounters[byteIdx] |= ((count + 1) << bitShift);
  }
}
static void clearUnknownCount(uint8_t msgId) {
  uint8_t byteIdx = msgId >> 2;
  uint8_t bitShift = (msgId & 0x03) * 2;
  otUnknownCounters[byteIdx] &= ~(0x03 << bitShift);
}

// Apply overrides to a thermostat frame before forwarding to boiler.
// Returns the (potentially modified) frame. If modified, also bridges the
// original thermostat frame as 'T' and the modified frame as 'R'.
static unsigned long applyOverrides(unsigned long frame, bool &modified) {
  modified = false;
  uint8_t msgType = (frame >> 28) & 0x07;
  uint8_t msgId   = (frame >> 16) & 0xFF;
  uint16_t origData = frame & 0xFFFF;

  // TASK-466: observe thermostat-originated MsgID 16 frames for the TT=/TC=
  // auto-clear state machine. Done BEFORE override replacement so the
  // honour/release detector sees the thermostat's intended value, not ours.
  // Bounded work: a couple of compares + one 8-bit increment, no I/O.
  if (msgId == 16) {
    onThermostatMsgID16(msgType, origData);
  }

  for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) {
    if (otOverrides[i].active && otOverrides[i].msgId == msgId) {
      // Replace data value (lower 16 bits) while keeping msg type + data-id
      frame = (frame & 0xFFFF0000UL) | otOverrides[i].overrideValue;
      setOTParityBit(frame);
      modified = true;
      OTDDebugTf(PSTR("OTD: override MsgID=%u orig=0x%04X new=0x%04X\r\n"),
                 msgId, origData, otOverrides[i].overrideValue);
      break;
    }
  }
  return frame;
}

// Update the write cache in the schedule table for a given MsgID
static void updateWriteCache(uint8_t msgId, uint16_t value) {
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
    if (otSchedule[i].isWrite && otSchedule[i].msgId == msgId) {
      otSchedule[i].cachedValue = value;
      otSchedule[i].valueSet = true;
      return;
    }
  }
}

// Async state tracking for non-blocking OT requests
static bool     otMasterRequestActive = false;   // true while waiting for boiler response
static unsigned long otLastSentRequest = 0;      // frame we sent (for bridge logging)
static OTDirectRequestOrigin otLastRequestOrigin = OT_DIRECT_ORIGIN_GATEWAY;

static void handleSlaveRequest(unsigned long request, OpenThermResponseStatus status) {
  if (status == OpenThermResponseStatus::SUCCESS) {
    otSlaveFrame = request;
    otSlaveFramePending = true;
  }
}

// ---------------------------------------------------------------------------
// OTDirect bridge log helper — mirrors the PIC ser2net command log shape.
// ---------------------------------------------------------------------------
static void otDirectBridgeEvent(char prefix, const char* msg, size_t len) {
  ClrLog();
  AddLog(getOTLogTimestamp());
  AddLogf_P(PSTR(" %c "), prefix);
  if (msg && len > 0) {
    AddLogf_P(PSTR("%.*s"), static_cast<int>(len), msg);
  }
  AddLogln();
  Debug(ot_log_buffer);
  sendLogToWebSocket(ot_log_buffer);
  ClrLog();
}

// ---------------------------------------------------------------------------
// otDirectBridgeWriteLine — fan OT-direct bridge output to TCP port 25238.
// Mirrors dispatchOTGWInputLine(): write payload, then CRLF, no heap.
// ---------------------------------------------------------------------------
static void otDirectBridgeWriteLine(const char* line, size_t len) {
  if (!line || len == 0) return;
  if (!settings.mqtt.bLegacyPort25238Enabled) return;
  OTGWstream.write(reinterpret_cast<const uint8_t*>(line), len);
  OTGWstream.write('\r');
  OTGWstream.write('\n');
}

static void otDirectBridgeProcessStatus(const char* status) {
  if (!status) return;
  otDirectBridgeWriteLine(status, 2);
  processOT(status, 2);
}

// ---------------------------------------------------------------------------
// handleOTDirectBridgeStream — line-buffered TCP bridge input for OTGW32
// Recreates the command-line behavior of the PIC-backed 25238 bridge, but
// routes complete lines to the OTDirect command path instead of OTGWSerial.
// ---------------------------------------------------------------------------
void handleOTDirectBridgeStream() {
  if (!isOTDirectEnabled()) return;
  if (!settings.mqtt.bLegacyPort25238Enabled) return;

  static constexpr size_t kMaxBridgeWrite = 128;
  // Sync webserver: bound dispatched commands per call so a flooded TCP client
  // on port 25238 cannot starve httpServer.handleClient(). Pending bytes drain
  // on the next call.
  static constexpr size_t kMaxLinesPerDrain = 4;
  static char sWrite[kMaxBridgeWrite];
  static size_t bytes_write = 0;
  static bool discardCurrentWriteLine = false;
  static uint32_t droppedWriteLines = 0;
  size_t cmdsProcessed = 0;

  while (OTGWstream.available() && cmdsProcessed < kMaxLinesPerDrain) {
    int inByte = OTGWstream.read();
    if (inByte < 0) break;

    char outByte = static_cast<char>(inByte);
    if (outByte == '\r') {
      if ((bytes_write == 0) && !discardCurrentWriteLine) continue;

      sWrite[bytes_write] = '\0';
      if (discardCurrentWriteLine) {
        droppedWriteLines++;
        OTDDebugTf(PSTR("OTDirect bridge line dropped after overflow. Dropped lines total: %lu\r\n"),
                   static_cast<unsigned long>(droppedWriteLines));
      } else {
        OTDDebugTf(PSTR("OTDirect bridge: Sending [%s] (%d)\r\n"), sWrite, static_cast<int>(bytes_write));
        if (bytes_write > 0) {
          otDirectBridgeEvent('>', sWrite, bytes_write);
          sendPICSerial(sWrite, static_cast<int>(bytes_write));
        }
      }

      bytes_write = 0;
      discardCurrentWriteLine = false;
      cmdsProcessed++;
    } else if (outByte == '\n') {
      continue;
    } else if (bytes_write < (kMaxBridgeWrite - 1)) {
      if (!discardCurrentWriteLine) {
        sWrite[bytes_write++] = outByte;
      }
    } else {
      OTDDebugTf(PSTR("OTDirect bridge buffer overflow! Discarding %d bytes. Total overflows: %lu\r\n"),
                 static_cast<int>(bytes_write), static_cast<unsigned long>(droppedWriteLines + 1));
      discardCurrentWriteLine = true;
      bytes_write = 0;
    }
  }
}

// ---------------------------------------------------------------------------
// bridgeFrameToParser — format a 32-bit OT frame and feed to processOT()
// ---------------------------------------------------------------------------
static void bridgeFrameToParser(char prefix, unsigned long frame) {
  // TASK-293: always feed the frame into processOT() so state, decoded
  // values, and connected-state flags stay fresh. In PS=1 mode, pass
  // suppressOutput=true so the per-frame raw "otmessage" publish and the
  // auto-leave-PS heuristic are skipped. Previously we early-returned here,
  // which froze the whole ESP32 OT-direct pipeline while PS=1 was active.
  char buf[10];
  snprintf_P(buf, sizeof(buf), PSTR("%c%08lX"), prefix, frame);
  otDirectBridgeWriteLine(buf, 9);
  processOT(buf, 9, otHideReports);
}

// ---------------------------------------------------------------------------
// buildStatusRequest — construct MsgID 0 master request with current flags
// ---------------------------------------------------------------------------
static unsigned long buildStatusRequest() {
  // Build a complete READ_DATA request so message type, data-id and parity are correct.
  // MsgID 0: Master status flags in data-value HB (bits 15-8), LB = 0 (slave fills it)
  return OpenTherm::buildRequest(
    OpenThermMessageType::READ_DATA,
    OpenThermMessageID::Status,
    ((unsigned int)otMasterStatusFlags << 8)
  );
}

// ---------------------------------------------------------------------------
// enableStepUp — enable 24V step-up converter for OT slave bus
// ---------------------------------------------------------------------------
static void enableStepUp() {
  pinMode(PIN_STEPUP_ENABLE, OUTPUT);
  digitalWrite(PIN_STEPUP_ENABLE, HIGH);
  delay(50);  // Allow voltage to stabilize
  OTDDebugTln(F("OT-direct: 24V step-up enabled"));
}

// ---------------------------------------------------------------------------
// probeOTBus — check if OT master input shows idle (HIGH) state
// ---------------------------------------------------------------------------
static bool probeOTBus() {
  pinMode(PIN_OT_MASTER_IN, INPUT);
  // Manchester idle = line HIGH. Sample a few times to confirm stable.
  int highCount = 0;
  for (int i = 0; i < 10; i++) {
    if (digitalRead(PIN_OT_MASTER_IN) == HIGH) highCount++;
    delayMicroseconds(100);
  }
  return (highCount >= 7);  // ≥70% HIGH = likely idle OT bus
}

// ---------------------------------------------------------------------------
// initOTDirect — called from setup() on OTGW32 builds
// ---------------------------------------------------------------------------
void initOTDirect() {
  DebugTln(F("OT-direct: Initializing direct GPIO OpenTherm..."));

  // 1. Restore operating mode from persistent settings
  otCurrentMode = static_cast<OTDirectMode>(settings.otd.iMode);
#if !HAS_BYPASS_RELAY
  // Boards without relay can't do bypass mode — force gateway if saved as bypass
  if (otCurrentMode == OTD_MODE_BYPASS) otCurrentMode = OTD_MODE_GATEWAY;
#else
  // Runtime check: even if board has relay, it must be enabled in settings
  if (otCurrentMode == OTD_MODE_BYPASS && !settings.otd.bHasBypassRelay) {
    OTDDebugTln(F("OT-direct: Bypass relay not enabled in settings — forcing gateway mode"));
    otCurrentMode = OTD_MODE_GATEWAY;
  }
#endif
  // Initialize bypass relay hardware
#if HAS_BYPASS_RELAY
  pinMode(PIN_BYPASS_RELAY, OUTPUT);
  digitalWrite(PIN_BYPASS_RELAY, IS_BYPASS_MODE() ? HIGH : LOW);
  OTDDebugTf(PSTR("OT-direct: Bypass relay initialized (%s)\r\n"),
          IS_BYPASS_MODE() ? "BYPASS mode" : "off");
#else
  OTDDebugTln(F("OT-direct: No bypass relay on this board"));
#endif

  // TASK-184: initialize flame ratio ring buffers
  initFlameRatio();

  // Restore persisted settings to local variables
  otSetbackTemp     = settings.otd.fSetbackTemp;
  otSummerMode      = settings.otd.bSummerMode;
  otFailSafeEnabled = settings.otd.bFailSafe;
  otMinIntervalMs   = settings.otd.iMsgInterval;
  if (otSummerMode) otMasterStatusFlags |= 0x20;

  // TASK-584: restore persisted ventilation setpoint to write cache so the
  // scheduler will re-apply it to the boiler after boot without a new command.
  if (settings.otd.iVentSetpoint > 0) {
    uint16_t ventData = ((uint16_t)settings.otd.iVentSetpoint) << 8;
    updateWriteCache(71, ventData);
    setOverride(71, ventData);
    OTDDebugTf(PSTR("OT-direct: restored vent setpoint %u%% from settings\r\n"),
               settings.otd.iVentSetpoint);
  }

  {
    const char* modeNames[] = { "bypass", "gateway", "monitor", "master" };
    uint8_t modeIdx = (uint8_t)otCurrentMode;
    OTDDebugTf(PSTR("OT-direct: Mode=%d (%s)\r\n"), modeIdx,
            modeIdx <= 3 ? modeNames[modeIdx] : "unknown");
  }

  // 2. Enable step-up converter
  enableStepUp();

  // 3. Probe OT master input for idle bus
  bool busPresent = probeOTBus();
  if (busPresent) {
    OTDDebugTln(F("OT-direct: OT bus idle detected on master input"));
  } else {
    DebugTln(F("OT-direct: WARNING — OT master input not idle, boiler may be disconnected"));
  }

  // 4. Start OpenTherm master (talks to boiler)
  otMaster.begin(masterISR);
  OTDDebugTln(F("OT-direct: Master interface started"));

  // 5. Start OpenTherm slave (listens to thermostat)
  //    In master mode with slave disabled, skip starting the slave interface.
  bool startSlave = true;
  if (IS_MASTER_MODE() && !settings.otd.bEnableSlave) {
    startSlave = false;
    OTDDebugTln(F("OT-direct: Slave interface DISABLED (master mode, bEnableSlave=false)"));
  }
  if (startSlave) {
    otSlave.begin(slaveISR, handleSlaveRequest);
    OTDDebugTln(F("OT-direct: Slave interface started"));
  }

  // 6. Always set OT_DIRECT mode — this is OTGW32 hardware.
  //    Bus liveness is tracked via state.otBus.bOnline, not eMode.
  //    The OT-direct loop must keep running so it can retry/recover.
  state.hw.eMode = HW_MODE_OT_DIRECT;
  OTDDebugTln(F("OT-direct: Hardware mode set to OT_DIRECT"));

  if (!busPresent) {
    DebugTln(F("OT-direct: WARNING — OT bus not idle, boiler may be disconnected"));
  }

  // 7. Send initial status request to boiler to check connectivity
  // Blocking calls are acceptable during setup() — not yet in cooperative loop
  unsigned long request = buildStatusRequest();
  unsigned long response = otMaster.sendRequest(request);
  if (otMaster.isValidResponse(response)) {
    state.otBus.bOnline = true;
    OTDDebugTln(F("OT-direct: Boiler responded — OT bus online"));
    sendWebSocketJSON("{\"type\":\"status\",\"msg\":\"OTDirect: connected\"}");
    bridgeFrameToParser('R', request);
    bridgeFrameToParser('B', response);
  } else {
    state.otBus.bOnline = false;
    DebugTln(F("OT-direct: No valid boiler response — OT bus offline (will retry in loop)"));
  }

  // 8. OT protocol handshake — identify ourselves to the boiler.
  //    MsgID 2: Master config flags (HB) + MemberID (LB). We set Smart Power bit if available.
  //    MsgID 124: OpenTherm version we speak (f8.8, 2.2 = 0x0233).
  //    MsgID 126: Master product type (HB) + version (LB).
  //    These are one-time WRITE_DATA, not polled.
  if (state.otBus.bOnline) {
    unsigned long handshake;

    // MsgID 2: Master config (HB=flags bit0=SmartPower, LB=MemberID 0)
    handshake = OpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::MConfigMMemberIDcode,
      0x0100  // HB=0x01 (Smart Power supported), LB=0x00 (MemberID)
    );
    response = otMaster.sendRequest(handshake);
    bridgeFrameToParser('R', handshake);
    if (otMaster.isValidResponse(response)) bridgeFrameToParser('B', response);

    // MsgID 3: Read slave config to learn what the boiler supports
    handshake = OpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA,
      OpenThermMessageID::SConfigSMemberIDcode,
      0
    );
    response = otMaster.sendRequest(handshake);
    bridgeFrameToParser('R', handshake);
    if (otMaster.isValidResponse(response)) {
      bridgeFrameToParser('B', response);
      uint8_t slaveConfig = (response >> 8) & 0xFF;
      OTDDebugTf(PSTR("OT-direct: Slave config=0x%02X (DHW=%d ModCtrl=%d Cool=%d CH2=%d)\r\n"),
        slaveConfig,
        (slaveConfig & 0x01) ? 1 : 0,  // DHW present
        (slaveConfig & 0x02) ? 0 : 1,  // 0=modulating, 1=on-off
        (slaveConfig & 0x04) ? 1 : 0,  // Cooling supported
        (slaveConfig & 0x10) ? 1 : 0); // CH2 present
    }

    // MsgID 124: OT protocol version (f8.8: 2.2 = 0x0233 ≈ 2.20)
    handshake = OpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::OpenThermVersionMaster,
      0x0233  // 2.2 in f8.8 (2 + 51/256 ≈ 2.20)
    );
    response = otMaster.sendRequest(handshake);
    bridgeFrameToParser('R', handshake);
    if (otMaster.isValidResponse(response)) bridgeFrameToParser('B', response);

    // MsgID 126: Master product type=0 (gateway), version=1
    handshake = OpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      OpenThermMessageID::MasterVersion,
      0x0001  // HB=type 0, LB=version 1
    );
    response = otMaster.sendRequest(handshake);
    bridgeFrameToParser('R', handshake);
    if (otMaster.isValidResponse(response)) bridgeFrameToParser('B', response);

    OTDDebugTln(F("OT-direct: Protocol handshake complete"));
  }

  // 9. Auto-detect thermostat presence (if enabled and mode is gateway)
  //    Wait a few seconds to see if any thermostat frames arrive on the slave bus.
  //    OpenTherm thermostats send MsgID 0 every ~1 second, so 5 seconds gives
  //    high confidence. If none seen, switch to master (standalone) mode.
  if (settings.otd.bAutoDetect && otCurrentMode == OTD_MODE_GATEWAY) {
    OTDDebugTln(F("OT-direct: Auto-detecting thermostat (5 second window)..."));
    uint32_t detectStart = millis();
    bool thermostatFound = false;
    while ((millis() - detectStart) < 5000UL) {
      otSlave.process();
      if (otSlaveFramePending) {
        thermostatFound = true;
        OTDDebugTln(F("OT-direct: Thermostat detected!"));
        otLastThermostatMs = millis();
        otThermostatSeen = true;
        state.otd.bThermostatConnected = true;
        break;
      }
      feedWatchDog();
      delay(50);
    }
    if (!thermostatFound) {
      OTDDebugTln(F("OT-direct: No thermostat detected — switching to master (standalone) mode"));
      setOTDirectMode(OTD_MODE_MASTER);
    } else {
      OTDDebugTln(F("OT-direct: Thermostat present — staying in gateway mode"));
    }
  }
}

// ---------------------------------------------------------------------------
// updateOTDirectStatus — refresh state.otd with current schedule/override stats.
// Called from loopOTDirect() once per second so REST/MQTT always have fresh data.
// ---------------------------------------------------------------------------
void updateOTDirectStatus() {
  uint8_t total = OT_SCHEDULE_SIZE;
  uint8_t disabled = 0;
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
    if (otSchedule[i].disabled) disabled++;
  }
  uint8_t overrides = 0;
  for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) {
    if (otOverrides[i].active) overrides++;
  }
  state.otd.iScheduleTotal    = total;
  state.otd.iScheduleActive   = total - disabled;
  state.otd.iScheduleDisabled = disabled;
  state.otd.iOverrideCount    = overrides;
  state.otd.bBypassActive     = IS_BYPASS_MODE();
  state.otd.bStepUpEnabled    = (digitalRead(PIN_STEPUP_ENABLE) == HIGH);
  state.otd.bMonitorMode      = IS_MONITOR_MODE();
  state.otd.bMasterMode       = IS_MASTER_MODE();
  state.otd.eMode             = otCurrentMode;
  state.otd.iLastThermostatMs = otLastThermostatMs;
}

// ---------------------------------------------------------------------------
// sendOTDirectOverridesJSON — stream all active overrides as chunked JSON response.
// Uses HTTP chunked transfer encoding: no large buffer needed, only a 48-byte
// formatting scratch on the stack per entry.  Called from restAPI.ino.
// Caller must set CORS headers before calling this function.
// ---------------------------------------------------------------------------
void sendOTDirectOverridesJSON() {
  char chunk[48];

  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send_P(200, PSTR("application/json"), PSTR(""));
  httpServer.sendContent_P(PSTR("{\"overrides\":{\"write\":["));

  bool first = true;
  for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) {
    if (!otOverrides[i].active) continue;
    snprintf_P(chunk, sizeof(chunk), PSTR("%s{\"msgid\":%u,\"value\":%u}"),
               first ? "" : ",", otOverrides[i].msgId, otOverrides[i].overrideValue);
    httpServer.sendContent(chunk);
    first = false;
  }

  httpServer.sendContent_P(PSTR("],\"response\":["));
  first = true;
  for (uint8_t i = 0; i < OT_RESPONSE_OVERRIDE_MAX; i++) {
    if (!otResponseOverrides[i].active) continue;
    snprintf_P(chunk, sizeof(chunk), PSTR("%s{\"msgid\":%u,\"value\":%u}"),
               first ? "" : ",", otResponseOverrides[i].msgId, otResponseOverrides[i].value);
    httpServer.sendContent(chunk);
    first = false;
  }

  httpServer.sendContent_P(PSTR("],\"modify\":["));
  first = true;
  for (uint8_t i = 0; i < OT_RESPONSE_MODIFY_MAX; i++) {
    if (!otResponseModifiers[i].active) continue;
    snprintf_P(chunk, sizeof(chunk), PSTR("%s{\"msgid\":%u,\"value\":%u}"),
               first ? "" : ",", otResponseModifiers[i].msgId, otResponseModifiers[i].value);
    httpServer.sendContent(chunk);
    first = false;
  }

  httpServer.sendContent_P(PSTR("],\"unknown\":["));
  first = true;
  for (uint8_t i = 0; i < otUnknownIdCount; i++) {
    snprintf_P(chunk, sizeof(chunk), PSTR("%s%u"), first ? "" : ",", otUnknownIds[i]);
    httpServer.sendContent(chunk);
    first = false;
  }

  // TASK-498 (4B-M2): expose queue depth + high-water-mark so non-telnet
  // clients (web UI, MQTT-driven dashboards) can monitor the OT command
  // queue load. "queue" is a sibling of "overrides", not nested under it.
  // Normal operation should keep highWater well below capacity; climbing
  // values indicate a producer-rate problem worth investigation.
  snprintf_P(chunk, sizeof(chunk),
             PSTR("]},\"queue\":{\"depth\":%u,\"highWater\":%u,\"capacity\":%u}}"),
             (unsigned)otCmdQueueDepth(),
             (unsigned)otCmdQueueHighWater,
             (unsigned)OT_CMD_QUEUE_SIZE);
  httpServer.sendContent(chunk);
  httpServer.sendContent(F(""));  // end chunked stream
}

// ---------------------------------------------------------------------------
// Loopback test mode — simulated boiler data table
// Provides realistic OT values so the full stack (parser, MQTT, WebSocket,
// REST, HA discovery) can be exercised without any boiler hardware.
// Table indexed by MsgID; 0xFFFF = not supported (reply UNKNOWN_DATA_ID).
// Values are f8.8 or raw uint16 depending on the MsgID data type.
// ---------------------------------------------------------------------------
static const uint16_t PROGMEM otLoopbackData[128] = {
  // MsgID  0: Status — slave flags: flame=1, CH=1, DHW=0 → 0x0A (bits: flame+CH mode)
  0x000A,
  // MsgID  1: TSet (control setpoint) — 45.0°C → 0x2D00
  0x2D00,
  // MsgID  2: Master config / member ID
  0x0000,
  // MsgID  3: Slave config / member ID — DHW present, modulating
  0x000B,
  // MsgID  4: Remote command — not supported
  0xFFFF,
  // MsgID  5: Application-specific flags
  0x0000,
  // MsgID  6: Remote param flags
  0x0000,
  // MsgID  7: Cooling control signal — not supported
  0xFFFF,
  // MsgID  8: TsetCH2 — not supported
  0xFFFF,
  // MsgID  9: Remote override room setpoint — 0
  0x0000,
  // MsgID 10: TSP count
  0x0000,
  // MsgID 11: TSP entry — not supported
  0xFFFF,
  // MsgID 12: FHB size — 0
  0x0000,
  // MsgID 13: FHB entry — not supported
  0xFFFF,
  // MsgID 14: Max relative modulation — 80%
  0x5000,
  // MsgID 15: Max boiler capacity / min modulation
  0x1E14,   // 30kW / 20%
  // MsgID 16: TrSet (room setpoint) — 20.5°C
  0x1480,
  // MsgID 17: Relative modulation level — 55.0%
  0x3700,
  // MsgID 18: CH water pressure — 1.5 bar
  0x0180,
  // MsgID 19: DHW flow rate — 0.0 (no DHW active)
  0x0000,
  // MsgID 20: Day of week / time
  0x0000,
  // MsgID 21: Date
  0x0000,
  // MsgID 22: Year
  0x07E8,   // 2024
  // MsgID 23: Second setpoint room — not supported
  0xFFFF,
  // MsgID 24: Room temperature — 20.2°C → 0x1433
  0x1433,
  // MsgID 25: Boiler flow temperature — 42.5°C
  0x2A80,
  // MsgID 26: DHW temperature — 48.0°C
  0x3000,
  // MsgID 27: Outside temperature — 8.5°C
  0x0880,
  // MsgID 28: Return water temperature — 35.0°C
  0x2300,
  // MsgID 29: Solar storage temperature — not supported
  0xFFFF,
  // MsgID 30: Solar collector temperature — not supported
  0xFFFF,
  // MsgID 31: Flow temperature CH2 — not supported
  0xFFFF,
  // MsgID 32: DHW2 temperature — not supported
  0xFFFF,
  // MsgID 33: Exhaust temperature (s16) — 120°C → 0x0078
  0x0078,
  // MsgID 34-47: reserved / not supported
  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
  // MsgID 48: DHW setpoint upper/lower bounds
  0x3C1E,   // 60°C / 30°C
  // MsgID 49: Max CH water setpoint upper/lower bounds
  0x5A14,   // 90°C / 20°C
  // MsgID 50-55: not supported
  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
  // MsgID 56: DHW setpoint — 50.0°C
  0x3200,
  // MsgID 57: Max CH water setpoint — 65.0°C
  0x4100,
  // MsgID 58-115: not supported
  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,  // 58-65
  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,  // 66-73
  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,  // 74-81
  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,  // 82-89
  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,  // 90-97
  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,  // 98-105
  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,  // 106-113
  0xFFFF, 0xFFFF,                                                    // 114-115
  // MsgID 116: Burner starts — 1234
  0x04D2,
  // MsgID 117: CH pump starts — 567
  0x0237,
  // MsgID 118: DHW pump/valve starts — 89
  0x0059,
  // MsgID 119: DHW burner starts — 45
  0x002D,
  // MsgID 120: Burner hours — 5678
  0x162E,
  // MsgID 121: CH pump hours — 4321
  0x10E1,
  // MsgID 122: DHW pump/valve hours — 876
  0x036C,
  // MsgID 123: DHW burner hours — 234
  0x00EA,
  // MsgID 124: OT version master — 2.2
  0x0202,
  // MsgID 125: OT version slave — 2.2
  0x0202,
  // MsgID 126: Master product — type 1, version 1
  0x0101,
  // MsgID 127: Slave product — type 4, version 5
  0x0405,
};

// ---------------------------------------------------------------------------
// simulateLoopbackResponse — generate a fake boiler response for a request.
// Looks up the MsgID in otLoopbackData[]; 0xFFFF → UNKNOWN_DATA_ID.
// WRITE_DATA requests are accepted (WRITE_ACK) and update the table cache.
// ---------------------------------------------------------------------------
static unsigned long simulateLoopbackResponse(unsigned long request) {
  uint8_t msgId = (request >> 16) & 0xFF;
  uint8_t reqType = (request >> 28) & 0x07;

  uint16_t data = pgm_read_word(&otLoopbackData[msgId]);

  // WRITE_DATA (type 1) — accept the write, echo data back as WRITE_ACK
  if (reqType == 1) {
    uint16_t writeData = request & 0xFFFF;
    return buildOTResponse(5, msgId, writeData);  // type 5 = WRITE_ACK
  }

  // READ_DATA (type 0) — look up simulated value
  if (data == 0xFFFF) {
    return buildOTResponse(7, msgId, 0);  // type 7 = UNKNOWN_DATA_ID
  }
  return buildOTResponse(4, msgId, data);   // type 4 = READ_ACK
}

// ---------------------------------------------------------------------------
// sendMasterRequestAsync — initiate an async OT request (non-blocking)
// ---------------------------------------------------------------------------
static bool sendMasterRequestAsync(unsigned long request, OTDirectRequestOrigin origin) {
  // Loopback mode: simulate response immediately, no bus activity
  if (IS_LOOPBACK_MODE()) {
    bridgeFrameToParser((origin == OT_DIRECT_ORIGIN_THERMOSTAT) ? 'T' : 'R', request);
    unsigned long response = simulateLoopbackResponse(request);
    bridgeFrameToParser('B', response);
    state.otBus.bOnline = true;

    // Cache response for master mode slave handler (reuses same cache)
    uint8_t cacheId = (response >> 16) & 0x7F;
    otBoilerCache[cacheId] = response & 0xFFFF;
    otBoilerCacheValid[cacheId] = true;

    // If forwarded thermostat frame, send simulated response back
    if (origin == OT_DIRECT_ORIGIN_THERMOSTAT) {
      otSlave.sendResponse(response);
    }
    return true;  // "completed" instantly
  }

  if (!otMaster.isReady()) return false;  // bus busy — try again later
  otLastSentRequest = request;
  otLastRequestOrigin = origin;
  otMasterRequestActive = otMaster.sendRequestAsync(request);
  if (otMasterRequestActive) {
    bridgeFrameToParser((origin == OT_DIRECT_ORIGIN_THERMOSTAT) ? 'T' : 'R', request);
    otLastAnySendMs = millis();  // MI= gap tracking: covers thermostat-forward and gateway paths
  }
  return otMasterRequestActive;
}

// ---------------------------------------------------------------------------
// handleMasterResponse — process a completed async master request
// ---------------------------------------------------------------------------
static void handleMasterResponse() {
  unsigned long response = otMaster.getLastResponse();
  OpenThermResponseStatus status = otMaster.getLastResponseStatus();

  if (status == OpenThermResponseStatus::SUCCESS) {
    bridgeFrameToParser('B', response);
    if (!state.otBus.bOnline) {
      // Transition: was offline, now online
      OTDDebugTln(F("OTD: boiler responded — OT bus back online"));
      sendWebSocketJSON("{\"type\":\"status\",\"msg\":\"OTDirect: connected\"}");
    }
    state.otBus.bOnline = true;

    // Cache boiler response data for master mode slave responses
    {
      uint8_t cacheId = (response >> 16) & 0x7F;
      otBoilerCache[cacheId] = response & 0xFFFF;
      otBoilerCacheValid[cacheId] = true;
    }

    // TASK-184: update flame ratio state from MsgID 0 slave status byte
    // Flame bit = bit 3 of slave status LB (bit 3 of response byte 0)
    {
      uint8_t cacheId0 = (response >> 16) & 0x7F;
      if (cacheId0 == 0) {
        bool flameOn = (otBoilerCache[0] & 0x08) != 0;  // bit 3 of LB = flame active
        flameRatioSet(flameOn);
      }
    }

    // 3-strike auto-blacklist: if boiler responds with UNKNOWN_DATA_ID,
    // increment 2-bit counter. Disable schedule entry after 3 consecutive unknowns.
    // MsgID 0 (status) is never disabled — it's mandatory.
    OpenThermMessageType respType = OpenTherm::getMessageType(response);
    uint8_t respMsgId = (response >> 16) & 0xFF;
    if (respType == OpenThermMessageType::UNKNOWN_DATA_ID) {
      if (respMsgId != 0) {
        incUnknownCount(respMsgId);
        if (getUnknownCount(respMsgId) >= 3) {
          for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
            if (otSchedule[i].msgId == respMsgId && !otSchedule[i].disabled) {
              otSchedule[i].disabled = true;
              OTDDebugTf(PSTR("OTD: MsgID %u disabled (3x UNKNOWN_DATA_ID)\r\n"), respMsgId);
              break;
            }
          }
        }
      }
    } else if (respMsgId != 0) {
      // Successful response — reset unknown counter for this MsgID
      clearUnknownCount(respMsgId);
    }

    // DHW push state machine — monitor MsgID 0 boiler responses
    if (respMsgId == 0 && otDHWPushState != PUSH_IDLE) {
      // Timeout check (~30s, matching PIC firmware)
      if ((millis() - otDHWPushStartMs) >= OT_DHW_PUSH_TIMEOUT_MS) {
        otDHWPushState = PUSH_IDLE;
        otMasterStatusFlags &= ~0x02;
        otDHWOverride = 0xFF;
        OTDDebugTln(F("OTD: DHW push timed out"));
      } else {
        uint8_t slaveByte4 = response & 0xFF;
        if (otDHWPushState == PUSH_PENDING) {
          if ((slaveByte4 & 0x08) && (slaveByte4 & 0x04)) {  // flame on + DHW mode
            otDHWPushState = PUSH_STARTED;
          }
        } else if (otDHWPushState == PUSH_STARTED) {
          if (!(slaveByte4 & 0x04)) {  // DHW mode off — push complete
            otDHWPushState = PUSH_IDLE;
            otMasterStatusFlags &= ~0x02;  // clear DHW enable
            otDHWOverride = 0xFF;          // back to auto
            OTDDebugTln(F("OTD: DHW push complete"));
          }
        }
      }
    }

    // PS=1 summary: trigger pending summary after MsgID 0 response
    if (respMsgId == 0 && otSummaryMode) {
      otSummaryPending = true;
    }

    // Log per-frame: MsgID, data value, override applied
    {
      uint8_t logMsgId = (response >> 16) & 0xFF;
      uint16_t logData = response & 0xFFFF;
      OTDDebugTf(PSTR("OTD: resp MsgID=%u data=0x%04X origin=%u\r\n"),
                 logMsgId, logData, (uint8_t)otLastRequestOrigin);
    }

    // If this was a forwarded thermostat frame, send response back
    if (otLastRequestOrigin == OT_DIRECT_ORIGIN_THERMOSTAT) {
      // In gateway mode, apply response-path modifications before forwarding
      if (otCurrentMode == OTD_MODE_GATEWAY) {
        unsigned long origResp = response;
        response = applyResponseModifiers(response);
        if (response != origResp) {
          OTDDebugTf(PSTR("OTD: resp-modify MsgID=%u orig=0x%04X new=0x%04X\r\n"),
                     (uint8_t)((origResp >> 16) & 0xFF),
                     (uint16_t)(origResp & 0xFFFF),
                     (uint16_t)(response & 0xFFFF));
        }
      }
      otSlave.sendResponse(response);
    }
  } else {
    // Only mark offline on status request (MsgID 0) failures
    uint8_t msgId = (otLastSentRequest >> 16) & 0xFF;
    if (msgId == 0) {
      if (state.otBus.bOnline) {
        // Transition: was online, now offline
        sendWebSocketJSON("{\"type\":\"status\",\"msg\":\"OTDirect: disconnected\"}");
        OTDDebugTln(F("OTD: boiler no response — OT bus offline"));
      }
      state.otBus.bOnline = false;
    }
  }

  otMasterRequestActive = false;
  otLastRequestOrigin = OT_DIRECT_ORIGIN_GATEWAY;
}

// ---------------------------------------------------------------------------
// scheduleMasterRequest — pick next scheduled message and send async
// ---------------------------------------------------------------------------
static void scheduleMasterRequest() {
  // Don't schedule if master is already busy (collision avoidance)
  if (otMasterRequestActive) return;

  uint32_t now = millis();

  // MI= global minimum inter-message gap
  if ((now - otLastAnySendMs) < otMinIntervalMs) return;

  // Pending commands from the ring buffer take priority.
  // Peek first — only dequeue after successful async send (Codex P1 fix).
  if (!otCmdQueueEmpty()) {
    uint32_t cmdFrame = otCmdQueue[otCmdTail];
    if (sendMasterRequestAsync(cmdFrame, OT_DIRECT_ORIGIN_GATEWAY)) {
      otCmdTail = (otCmdTail + 1) % OT_CMD_QUEUE_SIZE;  // consume on success
    }
    return;
  }

  // When the bus is offline: only probe MsgID 0 at a slow rate — no point
  // cycling through the full schedule if nobody is home.
  bool busOffline = !state.otBus.bOnline;

  // Round-robin through the schedule table, skipping disabled entries
  uint8_t startIdx = otScheduleIdx;
  do {
    OTScheduleEntry &entry = otSchedule[otScheduleIdx];
    otScheduleIdx = (otScheduleIdx + 1) % OT_SCHEDULE_SIZE;

    if (entry.disabled) continue;  // boiler doesn't support this MsgID

    // Offline: skip everything except MsgID 0 (the online/offline probe)
    if (busOffline && entry.msgId != 0) continue;

    // Write entries only fire when a value has been set by a command
    if (entry.isWrite && !entry.valueSet) continue;

    // Offline: use slow retry interval for MsgID 0 to avoid hammering a dead bus
    uint32_t interval = (busOffline && entry.msgId == 0) ? OT_OFFLINE_RETRY_MS : entry.intervalMs;
    // TASK-583: promote MsgID 70 (V/H status) and 71 (V/H setpoint) to fast-poll
    // when the slave configuration (MsgID 3 HB bits 6-7 == 0b11) indicates a
    // ventilation/HRV application.  MsgIDs 72-76 remain in the slow-poll tier.
    if ((entry.msgId == 70 || entry.msgId == 71) && otIsVentSlave()) {
      interval = OT_VENT_FAST_INTERVAL_MS;
    }

    if ((now - entry.lastSentMs) >= interval) {
      unsigned long request;
      if (entry.msgId == 0) {
        request = buildStatusRequest();
      } else if (entry.isWrite) {
        // Periodic write: send WRITE_DATA with cached value
        request = OpenTherm::buildRequest(
          OpenThermMessageType::WRITE_DATA,
          static_cast<OpenThermMessageID>(entry.msgId),
          entry.cachedValue
        );
      } else {
        request = OpenTherm::buildRequest(
          OpenThermMessageType::READ_DATA,
          static_cast<OpenThermMessageID>(entry.msgId),
          0
        );
      }

      if (sendMasterRequestAsync(request, OT_DIRECT_ORIGIN_GATEWAY)) {
        entry.lastSentMs = now;  // Only advance timer on successful send
      }
      return;  // One request per call
    }
  } while (otScheduleIdx != startIdx);
}

// ---------------------------------------------------------------------------
// emitSummaryLine — PS=1 summary: 34 comma-separated values matching PIC format
// MsgIDs: 0,1,6,7,8,14,15,16,17,18,19,23,24,25,26,27,28,31,33,48,49,56,57,70,71,77,116-123
// ---------------------------------------------------------------------------
static void emitSummaryLine() {
  // Build summary into a buffer. Max ~34 fields × ~12 chars = ~408 bytes.
  static char line[420];
  int pos = 0;
  OTdataStruct &s = OTcurrentSystemState;

  // Helper: append binary status (BBBBBBBB/BBBBBBBB)
  auto appendStatus = [&](uint16_t val) {
    for (int b = 7; b >= 0; b--) pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%c"), (val >> (8 + b)) & 1 ? '1' : '0');
    line[pos++] = '/';
    for (int b = 7; b >= 0; b--) pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%c"), (val >> b) & 1 ? '1' : '0');
  };
  // Helper: append float (f8.8)
  auto appendFloat = [&](float val) {
    char tmp[12]; dtostrf(val, 1, 2, tmp);
    pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%s"), tmp);
  };
  // Helper: append signed float
  auto appendSigned = [&](float val) {
    char tmp[12]; dtostrf(val, 1, 2, tmp);
    if (val >= 0) { line[pos++] = '+'; }
    pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%s"), tmp);
  };

  #define COMMA() do { line[pos++] = ','; } while(0)

  // MsgID 0: Status (binary HB/LB)
  appendStatus(s.Statusflags); COMMA();
  // MsgID 1: TSet (f8.8)
  appendFloat(s.TSet); COMMA();
  // MsgID 6: RBPflags (status)
  appendStatus(s.RBPflags); COMMA();
  // MsgID 7: CoolingControl (f8.8)
  appendFloat(s.CoolingControl); COMMA();
  // MsgID 8: TsetCH2 (f8.8)
  appendFloat(s.TsetCH2); COMMA();
  // MsgID 14: MaxRelModLevelSetting (f8.8)
  appendFloat(s.MaxRelModLevelSetting); COMMA();
  // MsgID 15: MaxCapacityMinModLevel (bytes HB,LB)
  pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%u,%u"),
    (s.MaxCapacityMinModLevel >> 8) & 0xFF, s.MaxCapacityMinModLevel & 0xFF); COMMA();
  // MsgID 16: TrSet (f8.8)
  appendFloat(s.TrSet); COMMA();
  // MsgID 17: RelModLevel (f8.8)
  appendFloat(s.RelModLevel); COMMA();
  // MsgID 18: CHPressure (f8.8)
  appendFloat(s.CHPressure); COMMA();
  // MsgID 19: DHWFlowRate (f8.8)
  appendFloat(s.DHWFlowRate); COMMA();
  // MsgID 23: TrSetCH2 (f8.8)
  appendFloat(s.TrSetCH2); COMMA();
  // MsgID 24: Tr (f8.8)
  appendFloat(s.Tr); COMMA();
  // MsgID 25: Tboiler (f8.8)
  appendFloat(s.Tboiler); COMMA();
  // MsgID 26: Tdhw (f8.8)
  appendFloat(s.Tdhw); COMMA();
  // MsgID 27: Toutside (signed f8.8)
  appendSigned(s.Toutside); COMMA();
  // MsgID 28: Tret (f8.8)
  appendFloat(s.Tret); COMMA();
  // MsgID 31: TflowCH2 (f8.8)
  appendFloat(s.TflowCH2); COMMA();
  // MsgID 33: Texhaust (signed short)
  pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%d"), s.Texhaust); COMMA();
  // MsgID 48: TdhwSetUBTdhwSetLB (bytes)
  pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%u,%u"),
    (s.TdhwSetUBTdhwSetLB >> 8) & 0xFF, s.TdhwSetUBTdhwSetLB & 0xFF); COMMA();
  // MsgID 49: MaxTSetUBMaxTSetLB (bytes)
  pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%u,%u"),
    (s.MaxTSetUBMaxTSetLB >> 8) & 0xFF, s.MaxTSetUBMaxTSetLB & 0xFF); COMMA();
  // MsgID 56: TdhwSet (f8.8)
  appendFloat(s.TdhwSet); COMMA();
  // MsgID 57: MaxTSet (f8.8)
  appendFloat(s.MaxTSet); COMMA();
  // MsgID 70: StatusVH (status)
  appendStatus(s.StatusVH); COMMA();
  // MsgID 71: RelativeVentilation (low byte)
  pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%u"), s.RelativeVentilation & 0xFF); COMMA();
  // MsgID 77: OEMDiagnosticCode → use FanSpeed as proxy for ID77 (exhaust fan speed)
  // PIC stores raw ID77 value; we use the closest available field
  pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%u"), s.FanSpeed & 0xFF); COMMA();
  // MsgID 116-123: counters (unsigned 16-bit)
  pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%u"), s.BurnerStarts); COMMA();
  pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%u"), s.CHPumpStarts); COMMA();
  pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%u"), s.DHWPumpValveStarts); COMMA();
  pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%u"), s.DHWBurnerStarts); COMMA();
  pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%u"), s.BurnerOperationHours); COMMA();
  pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%u"), s.CHPumpOperationHours); COMMA();
  pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%u"), s.DHWPumpValveOperationHours); COMMA();
  pos += snprintf_P(line + pos, sizeof(line) - pos, PSTR("%u"), s.DHWBurnerOperationHours);

  #undef COMMA

  line[pos] = '\0';
  processOT(line, pos);
}

// ===========================================================================
// TASK-183: PI room compensation + weather-compensated heating curve
// ===========================================================================

// PI controller state (static locals — one heating circuit)
static float  otPiIntegState   = 0.0f;   // integrator state (K)
static float  otPiRoomFilt     = 0.0f;   // low-pass filtered room temp
static float  otPiRspPrev      = 0.0f;   // previous room setpoint
static bool   otPiInit         = false;  // first-run flag
static float  otPiDeltaT       = 0.0f;  // current PI correction (K)
static uint32_t otNextPiCtrl   = 0;      // next PI update (millis)

static constexpr uint32_t OT_PI_INTERVAL_MS = 60000UL;  // 60s PI period (matches OT-Thing)

// getFlowTemp — compute target flow temperature from settings + PI correction.
// Returns 0 if CH is off; clipped to [0, fFlowMax].
float getFlowTemp() {
  float flow = settings.otd.fFlowTemp;  // fixed-flow default

  if (settings.otd.iCHMode == 0) {
    return 0.0f;  // off
  }

  if (settings.otd.iCHMode == 2) {
    // AUTO mode: weather-compensated heating curve
    // Need outside temperature from OT cache (MsgID 27 = Toutside, f8.8)
    if (otBoilerCacheValid[27]) {
      int16_t rawOT = (int16_t)otBoilerCache[27];
      float outTmp = rawOT / 256.0f;

      float rsp = settings.otd.fRoomSetpoint;
      float flowMax = settings.otd.fFlowMax;
      float gradient = settings.otd.fGradient;
      float exponent = settings.otd.fExponent;
      float offset = settings.otd.fOffset;

      // Formula from OT-Thing getFlow() (CTRLMODE_AUTO):
      // minOutside = rsp - (flowMax - rsp) / gradient
      // c1 = (flowMax - rsp) / pow(rsp - minOutside, 1/exponent)
      // flow = rsp + c1 * pow(rsp - outTmp, 1/exponent) + offset
      float minOutside = rsp - (flowMax - rsp) / gradient;
      float span = rsp - minOutside;
      if (span > 0.0f) {
        float c1 = (flowMax - rsp) / powf(span, 1.0f / exponent);
        float diff = rsp - outTmp;
        if (diff > 0.0f) {
          flow = rsp + c1 * powf(diff, 1.0f / exponent) + offset;
        } else {
          flow = rsp + offset;  // outside >= room setpoint: minimal flow
        }
      }
    }
    // else: outside temp unknown, fall back to fixed flow
  }

  // Apply PI room compensation correction
  if (settings.otd.bRoomCompEnabled) {
    flow += otPiDeltaT;
  }

  // Clip to [0, flowMax]
  if (flow < 0.0f) flow = 0.0f;
  if (flow > settings.otd.fFlowMax) flow = settings.otd.fFlowMax;

  return flow;
}

// loopPiCtrl — run PI room temperature compensation (called every 60s).
// Reads room temperature from MsgID 24 cache and room setpoint from MsgID 16 cache.
static void loopPiCtrl() {
  // Force the boiler write for MsgID 1 on next scheduler cycle
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
    if (otSchedule[i].msgId == 1 && otSchedule[i].isWrite) {
      otSchedule[i].lastSentMs = 0;  // force immediate send
      break;
    }
  }

  if (!settings.otd.bRoomCompEnabled) {
    otPiDeltaT = 0.0f;
    return;
  }

  // Need room temp (MsgID 24 = Tr) and room setpoint (MsgID 16 = TrSet)
  if (!otBoilerCacheValid[24] || !otBoilerCacheValid[16]) {
    // Not enough data — decay integrator gently
    otPiIntegState *= 0.95f;
    otPiDeltaT = 0.0f;
    return;
  }

  float rt  = (int16_t)otBoilerCache[24] / 256.0f;  // room temp (f8.8)
  float rsp = (int16_t)otBoilerCache[16] / 256.0f;  // room setpoint (f8.8)

  if (!otPiInit) {
    otPiRoomFilt = rt;
    otPiRspPrev  = rsp;
    otPiInit     = true;
  } else {
    otPiRoomFilt = 0.1f * rt + 0.9f * otPiRoomFilt;
  }

  float e = rsp - rt;       // error
  if (e > -0.2f && e < 0.2f) e = 0.0f;  // 0.2°C deadband

  // Proportional part
  float p = settings.otd.fKp * e;

  // Integral part — track setpoint changes
  otPiIntegState += rsp - otPiRspPrev;
  otPiRspPrev = rsp;

  // Integrate only when CH is active (infer from flame on, bit 3 of MsgID 0 slave byte)
  bool chActive = false;
  if (otBoilerCacheValid[0]) {
    chActive = (otBoilerCache[0] & 0x02) != 0;  // bit 1 of slave status LB = CH active
  }
  if (chActive) {
    if (e > 0.0f)
      otPiIntegState += settings.otd.fKi * e * (OT_PI_INTERVAL_MS / 1000.0f) / 3600.0f;
    else
      otPiIntegState += settings.otd.fKi * e * 0.3f * (OT_PI_INTERVAL_MS / 1000.0f) / 3600.0f;
  } else {
    otPiIntegState *= 0.95f;  // decay when not heating
  }

  // Anti-windup: clip integral to [-5, +5] K
  if (otPiIntegState < -5.0f) otPiIntegState = -5.0f;
  if (otPiIntegState >  5.0f) otPiIntegState =  5.0f;

  // Boost term: extra push when significantly cold
  float boost = 0.0f;
  if (e > 1.0f) boost = e * settings.otd.fKboost;

  otPiDeltaT = p + otPiIntegState + boost;

  // Clip total correction to [-5, +12] K
  if (otPiDeltaT < -5.0f)  otPiDeltaT = -5.0f;
  if (otPiDeltaT > 12.0f)  otPiDeltaT = 12.0f;

  OTDDebugTf(PSTR("OTD: PI rt=%.1f rsp=%.1f e=%.2f p=%.2f I=%.2f boost=%.2f dT=%.2f clamp=%s\r\n"),
             rt, rsp, e, p, otPiIntegState, boost, otPiDeltaT,
             (otPiDeltaT <= -5.0f || otPiDeltaT >= 12.0f) ? "yes" : "no");
}

// ===========================================================================
// TASK-582: CH hysteresis suspension logic
// ===========================================================================
// loopCHHysteresis — suspend/resume CH heating based on configurable deadband.
//
// When room temperature exceeds (setpoint + hysteresis/2), MsgID 0 CH-enable
// bit (bit0) is cleared, suspending central heating.  When room temperature
// falls back to (setpoint - hysteresis/2) or below, CH-enable is restored.
//
// Guards:
//   - bHysteresisEnable must be true in settings.otd (false = backward compat)
//   - Room temperature must be available in otBoilerCache[24] (MsgID 24 = Tr)
//   - CH setpoint must be available in otBoilerCache[16] (MsgID 16 = TrSet)
//   - SAT active: bypass entirely; SAT manages its own control (state.sat.bActive)
//   - Publishes transition events to MQTT topic otgw32/ch_suspended (retained)
//
// Called every 10s from doOTDirectLoop() — matches the temperature read interval.
static void loopCHHysteresis() {
  // SAT active: SAT owns the control loop, skip hysteresis
#if defined(HAS_SAT) && HAS_SAT
  if (state.sat.bActive) {
    // If we had suspended CH, restore it before handing back to SAT
    if (state.otd.bCHSuspended) {
      state.otd.bCHSuspended = false;
      otMasterStatusFlags |= 0x01;  // re-enable CH
      sendMQTTData(F("otgw32/ch_suspended"), "false", true);
      OTDDebugTln(F("OTD: hysteresis: SAT active, CH-suspend released"));
    }
    return;
  }
#endif

  if (!settings.otd.bHysteresisEnable) {
    // Feature disabled — ensure we're not holding a stale suspension
    if (state.otd.bCHSuspended) {
      state.otd.bCHSuspended = false;
      otMasterStatusFlags |= 0x01;
      sendMQTTData(F("otgw32/ch_suspended"), "false", true);
      OTDDebugTln(F("OTD: hysteresis: disabled, CH-suspend released"));
    }
    return;
  }

  // Room temperature and setpoint must both be in cache
  if (!otBoilerCacheValid[24] || !otBoilerCacheValid[16]) {
    OTDDebugTln(F("OTD: hysteresis: room temp or setpoint not available, skipping"));
    return;
  }

  float rt  = (int16_t)otBoilerCache[24] / 256.0f;  // room temp (f8.8)
  float rsp = (int16_t)otBoilerCache[16] / 256.0f;  // room setpoint (f8.8)
  float half = settings.otd.fHysteresis / 2.0f;

  bool wasSuspended = state.otd.bCHSuspended;

  if (!wasSuspended && rt >= (rsp + half)) {
    // Room temp exceeded upper threshold — suspend CH
    state.otd.bCHSuspended = true;
    otMasterStatusFlags &= ~0x01;  // clear CH enable bit
    sendMQTTData(F("otgw32/ch_suspended"), "true", true);
    OTDDebugTf(PSTR("OTD: hysteresis: CH suspended rt=%.1f rsp=%.1f (+%.2f)\r\n"),
               rt, rsp, half);
  } else if (wasSuspended && rt <= (rsp - half)) {
    // Room temp fell back below lower threshold — resume CH
    state.otd.bCHSuspended = false;
    otMasterStatusFlags |= 0x01;   // set CH enable bit
    sendMQTTData(F("otgw32/ch_suspended"), "false", true);
    OTDDebugTf(PSTR("OTD: hysteresis: CH resumed rt=%.1f rsp=%.1f (-%.2f)\r\n"),
               rt, rsp, half);
  }
}

// ===========================================================================
// TASK-184: Flame ratio tracking
// ===========================================================================

// Ring buffer: 180 slots x 1 slot/minute = 180 minutes (3h rolling window)
static constexpr uint8_t FLAME_BUF_SIZE = 180;

struct FlameRatioBuf {
  uint8_t buf[FLAME_BUF_SIZE];
  uint32_t sum;
  uint8_t  current;   // accumulator for current minute
};

// Forward declaration — prevents ESP32 auto-prototype conflict (type not yet visible at sketch top)
static void flameRatioBufCommit(FlameRatioBuf &b);

static struct {
  FlameRatioBuf on;      // seconds of flame-on per minute
  FlameRatioBuf cycles;  // flame-on transitions per minute
  uint8_t  idx;          // current ring buffer index
  bool     currentFlame; // last observed flame state
  bool     init;         // true once first minute completes
  uint32_t lastEdge;     // millis() of last flame transition
  uint32_t lastInc;      // millis() of last 1-minute increment
} otFlame;

// Call once from init — zero the buffers
static void initFlameRatio() {
  memset(&otFlame, 0, sizeof(otFlame));
}

// flameRatioAccum — add elapsed on-time since lastEdge to current accumulator.
// Called on flame transitions and at the end of each minute slot.
static void flameRatioAccum() {
  if (otFlame.currentFlame) {
    uint32_t diff = (millis() - otFlame.lastEdge) / 1000;
    if (diff > 60) diff = 60;  // clamp to one minute slot
    otFlame.on.current += (uint8_t)diff;
    OTDDebugTf(PSTR("OTD: flame-accum on+%lus cur=%us cycles=%u\r\n"),
               diff, (unsigned)otFlame.on.current, (unsigned)otFlame.cycles.current);
  }
  otFlame.lastEdge = millis();
}

// Commit current minute into ring buffer, advance index
static void flameRatioBufCommit(FlameRatioBuf &b) {
  uint8_t prev = b.buf[otFlame.idx];
  b.sum -= prev;
  b.buf[otFlame.idx] = b.current;
  b.sum += b.current;
  OTDDebugTf(PSTR("OTD: flame-commit idx=%u prev=%u cur=%u sum=%lu\r\n"),
             (unsigned)otFlame.idx, (unsigned)prev, (unsigned)b.current, b.sum);
  b.current = 0;
}

// Called when flame state changes (from MsgID 0 response)
static void flameRatioSet(bool flame) {
  if (otFlame.currentFlame != flame) {
    if (!otFlame.init) {
      // First transition: pre-fill on-time buffer with 60 so duty starts sensibly
      // (matches OT-Thing behaviour for the first incomplete minute)
      memset(otFlame.on.buf, 60, sizeof(otFlame.on.buf));
      otFlame.on.sum = (uint32_t)FLAME_BUF_SIZE * 60UL;
      otFlame.init = true;
    }
    if (flame) otFlame.cycles.current++;  // rising edge = one cycle
    flameRatioAccum();                    // account for time in previous state
    otFlame.currentFlame = flame;
  }
}

// Return flame duty cycle 0-100%
uint8_t getFlameRatioDuty() {
  if (!otFlame.init) return 0;
  return (uint8_t)(100UL * otFlame.on.sum / FLAME_BUF_SIZE / 60);
}

// Return flame cycle frequency (cycles/h, one decimal)
float getFlameRatioFreq() {
  if (!otFlame.init) return 0.0f;
  // sum of cycles over FLAME_BUF_SIZE minutes = FLAME_BUF_SIZE/60 hours
  return otFlame.cycles.sum / (FLAME_BUF_SIZE / 60.0f);
}

// Called every second — advance ring buffer every 60s, publish MQTT
static void loopFlameRatio() {
  if ((millis() - otFlame.lastInc) >= 60000UL) {
    flameRatioAccum();  // finalize on-time for this minute slot
    flameRatioBufCommit(otFlame.on);
    flameRatioBufCommit(otFlame.cycles);
    otFlame.idx = (otFlame.idx + 1) % FLAME_BUF_SIZE;
    otFlame.lastInc = millis();
    otFlame.init = true;

    // Publish to MQTT using sendMQTTData (prepends MQTTPubNamespace/)
    if (state.mqtt.bConnected) {
      char val[12];
      snprintf_P(val, sizeof(val), PSTR("%u"), getFlameRatioDuty());
      sendMQTTData(F("otgw32/flame_duty_pct"), val);
      dtostrf(getFlameRatioFreq(), 1, 1, val);
      sendMQTTData(F("otgw32/flame_cycles_per_hour"), val);
    }
  }
}

// ===========================================================================
// TASK-185: MQTT-sourced room temp/setpoint routing to OT bus
// Public API callable from MQTTstuff.ino
// ===========================================================================

// Route room temperature from MQTT into MsgID 24 write cache
void otdMqttSetRoomTemp(float tempC) {
  if (!isOTDirectEnabled()) return;
  if (tempC < -40.0f || tempC > 127.0f) return;  // sanity range — drop, don't clamp
  enqueueWriteCommand(24, floatToF88(tempC), "MQTT-room_temp");
}

// Route room setpoint from MQTT into MsgID 16 write cache
void otdMqttSetRoomSetpoint(float tempC) {
  if (!isOTDirectEnabled()) return;
  if (tempC < -40.0f || tempC > 127.0f) return;  // sanity range — drop, don't clamp
  enqueueWriteCommand(16, floatToF88(tempC), "MQTT-room_setpoint");
}

// ---------------------------------------------------------------------------
// loopOTDirect — called from doBackgroundTasks() on OTGW32 builds
// Non-blocking: uses async requests, never blocks for OT response.
// ---------------------------------------------------------------------------
void loopOTDirect() {
  // Process OpenTherm library state machines (drives ISR-based TX/RX)
  otMaster.process();
  otSlave.process();

  // Check if an async master request has completed
  if (otMasterRequestActive && otMaster.isReady()) {
    handleMasterResponse();
  }

  // Track thermostat connectivity — any pending frame means thermostat is alive
  if (otSlaveFramePending) {
    otLastThermostatMs = millis();
    otThermostatSeen = true;
  }

  // Forward pending thermostat frame when master bus is idle.
  // Master mode: respond directly with cached boiler values (we ARE the boiler).
  // Monitor mode: forward all frames unmodified — skip UI/SR/override tables.
  // Gateway mode: check UI (unknown-ID) and SR (response override) tables first,
  // then apply repeater-mode value overrides before forwarding to boiler.
  if (otSlaveFramePending && IS_MASTER_MODE()) {
    // Master mode: we are the sole OT master — respond to thermostat ourselves.
    // If a thermostat reconnects while in master mode, we serve it cached data.
    handleMasterModeSlaveFrame(otSlaveFrame);
    otSlaveFramePending = false;
    // If auto-detect is enabled and thermostat appears, notify but stay in master mode.
    // User can manually switch to gateway mode via GW=1 if they reconnect a thermostat.
  }
  else if (!otMasterRequestActive && otSlaveFramePending) {

    if (IS_MONITOR_MODE()) {
      // Monitor mode: transparent pass-through — no overrides, no interception
      if (sendMasterRequestAsync(otSlaveFrame, OT_DIRECT_ORIGIN_THERMOSTAT)) {
        otSlaveFramePending = false;
      }
    }
    else {
      // Gateway mode: full override processing
      uint8_t slaveMsgId = (otSlaveFrame >> 16) & 0xFF;

      // UI table: respond UNKNOWN_DATAID directly, don't forward to boiler
      if (isUnknownId(slaveMsgId)) {
        unsigned long unknownResp = (otSlaveFrame & 0x00FF0000UL) | (7UL << 28);  // type=UNKNOWN_DATAID
        setOTParityBit(unknownResp);
        bridgeFrameToParser('T', otSlaveFrame);
        bridgeFrameToParser('A', unknownResp);
        otSlave.sendResponse(unknownResp);
        otSlaveFramePending = false;
      }
      // SR table: respond with stored value directly, don't forward to boiler
      else {
        bool srHandled = false;
        for (uint8_t i = 0; i < OT_RESPONSE_OVERRIDE_MAX; i++) {
          if (otResponseOverrides[i].active && otResponseOverrides[i].msgId == slaveMsgId) {
            // Build READ_ACK with the stored data value
            unsigned long srResp = (4UL << 28) | ((unsigned long)slaveMsgId << 16) | otResponseOverrides[i].value;
            setOTParityBit(srResp);
            bridgeFrameToParser('T', otSlaveFrame);
            bridgeFrameToParser('A', srResp);
            otSlave.sendResponse(srResp);
            otSlaveFramePending = false;
            srHandled = true;
            break;
          }
        }
        // Normal path: apply value overrides and forward to boiler
        if (!srHandled) {
          bool modified = false;
          unsigned long frameToSend = applyOverrides(otSlaveFrame, modified);
          if (modified) {
            bridgeFrameToParser('T', otSlaveFrame);
          }
          if (sendMasterRequestAsync(frameToSend,
                modified ? OT_DIRECT_ORIGIN_GATEWAY : OT_DIRECT_ORIGIN_THERMOSTAT)) {
            otSlaveFramePending = false;
          }
        }
      }
    }
  }

  // Schedule periodic master requests (non-blocking, skips if bus busy)
  // In monitor mode the gateway must not inject its own frames — transparent pass-through only.
  DECLARE_TIMER_MS(timerOTSchedule, 100, SKIP_MISSED_TICKS);
  if (DUE(timerOTSchedule) && !IS_MONITOR_MODE()) {
    scheduleMasterRequest();
  }

  // PS=1 summary emission
  if (otSummaryPending) {
    otSummaryPending = false;
    emitSummaryLine();
  }

  // Refresh state.otd stats once per second for REST/MQTT
  DECLARE_TIMER_SEC(timerOTDStatus, 1, SKIP_MISSED_TICKS);
  if (DUE(timerOTDStatus)) {
    updateOTDirectStatus();
    checkThermostatTimeout();
    // TASK-184: flame ratio — update every second, publish every 60s
    loopFlameRatio();
    // TASK-442: expire CS/C2 overrides if not refreshed within heartbeat window.
    uint32_t nowExp = millis();
    if (otCSLastCommandMs != 0 && (uint32_t)(nowExp - otCSLastCommandMs) > OT_CSC2_EXPIRY_MS) {
      OTDDebugTln(F("OTD: CS heartbeat expired, clearing MsgID 1 override"));
      clearWriteOverride(1);
      otCSLastCommandMs = 0;
    }
    if (otC2LastCommandMs != 0 && (uint32_t)(nowExp - otC2LastCommandMs) > OT_CSC2_EXPIRY_MS) {
      OTDDebugTln(F("OTD: C2 heartbeat expired, clearing MsgID 8 override"));
      clearWriteOverride(8);
      otC2LastCommandMs = 0;
    }
  }

  // Periodic state log — every 30s for diagnostics
  DECLARE_TIMER_SEC(timerOTDStateLog, 30, SKIP_MISSED_TICKS);
  if (DUE(timerOTDStateLog)) {
    bool flameOn = otBoilerCacheValid[0] && ((otBoilerCache[0] & 0x08) != 0);
    float flowTemp = otBoilerCacheValid[25] ? (int16_t)otBoilerCache[25] / 256.0f : 0.0f;
    const char* modeStr = IS_BYPASS_MODE() ? "bypass" :
                          IS_MONITOR_MODE() ? "monitor" :
                          IS_MASTER_MODE()  ? "master" : "gateway";
    OTDDebugTf(PSTR("OTD: state online=%d mode=%s flow=%.1f flame=%s thermostat=%d\r\n"),
               (int)state.otBus.bOnline, modeStr, flowTemp,
               flameOn ? "on" : "off",
               (int)state.otd.bThermostatConnected);
  }

  // TASK-183: PI room compensation — run every 60s
  if ((millis() - otNextPiCtrl) >= OT_PI_INTERVAL_MS || otNextPiCtrl == 0) {
    loopPiCtrl();
    // Apply heating curve flow in master mode when CH mode is not fixed
    if (IS_MASTER_MODE() && settings.otd.iCHMode != 0) {
      float flow = getFlowTemp();
      if (flow > 0.0f) {
        enqueueWriteCommand(1, floatToF88(flow), "heating-curve");
      }
    }
    otNextPiCtrl = millis();
  }

  // TASK-582: CH hysteresis suspension — run every 10s (matches temperature read interval)
  DECLARE_TIMER_SEC(timerCHHysteresis, 10, SKIP_MISSED_TICKS);
  if (DUE(timerCHHysteresis)) {
    loopCHHysteresis();
  }
}

// ---------------------------------------------------------------------------
// enqueueWriteCommand — build frame, enqueue, update write cache + override
// Helper to reduce repetition in handleOTDirectCommand.
//
// Returns true iff the frame was successfully enqueued (cache + override table
// updated). Callers that own extra-state outside the override table (e.g.
// setRemoteOverride writing otRemoteOverride.* and state.otd.*) MUST sequence
// their state-write AFTER a successful return so the queue is the single
// source of truth — TASK-500 (1A-M1).
// ---------------------------------------------------------------------------
static bool enqueueWriteCommand(uint8_t msgId, uint16_t dataValue, const char* label) {
  unsigned long frame = OpenTherm::buildRequest(
    OpenThermMessageType::WRITE_DATA,
    static_cast<OpenThermMessageID>(msgId),
    dataValue
  );
  if (!otCmdEnqueue(frame)) {
    DebugTf(PSTR("OT-direct: %s command dropped (queue full)\r\n"), label);
    return false;
  }
  // Update periodic write cache so the scheduler keeps refreshing this value
  updateWriteCache(msgId, dataValue);
  // Activate repeater override so thermostat frames for this MsgID get modified
  setOverride(msgId, dataValue);
  OTDDebugTf(PSTR("OTD: %s -> MsgID %u frame 0x%08lX\r\n"), label, msgId, frame);
  return true;
}

// ---------------------------------------------------------------------------
// synthesizeResponse — format a PIC-style "XX: value" response and feed it
// into processOT(). This makes the command queue clearing, MQTT confirmations,
// WebSocket events, and PR= settings updates work identically whether a real
// PIC or OT-direct is handling commands.
// ---------------------------------------------------------------------------
static void synthesizeResponse(char c0, char c1, const char* value) {
  char buf[48];
  snprintf_P(buf, sizeof(buf), PSTR("%c%c: %s"), c0, c1, value);
  size_t respLen = strlen(buf);
  otDirectBridgeWriteLine(buf, respLen);
  processOT(buf, respLen);
}

// Convenience: synthesize from the original command buffer (first 2 chars)
static void synthesizeResponse(const char* cmd, const char* value) {
  synthesizeResponse(cmd[0], cmd[1], value);
}

static void otDirectBridgeProcessPRResponse(const char* prLine) {
  if (!prLine) return;
  size_t prLen = strlen(prLine);
  otDirectBridgeWriteLine(prLine, prLen);
  processOT(prLine, prLen);
}

// checkBoolean — PIC-compatible strict 0/1 validation. Returns true if valid.
static inline bool checkBoolean(const char* value) {
  return (value[0] == '0' || value[0] == '1') && value[1] == '\0';
}

// PIC-compat + Phase 1-10 state variables and tables: moved to top of file.
// Remaining state variables that are only used after this point:
static bool otDHWBlocking      = false;     // BW= DHW blocking (bit6, not persisted)
static bool otCoolingEnable    = false;     // CE= cooling enable (bit2, not persisted)
static uint8_t otOperModeDHW   = 0;         // MW= lower nibble of byte3
static uint8_t otOperModeCH1   = 0;         // MH= lower nibble of byte4
static uint8_t otOperModeCH2   = 0;         // M2= upper nibble of byte4

// ---------------------------------------------------------------------------
// clearWriteOverride — helper to clear both write cache and repeater override
// for a MsgID (used by TT=0, CS=0, etc.)
// ---------------------------------------------------------------------------
static void clearWriteOverride(uint8_t msgId) {
  clearOverride(msgId);
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
    if (otSchedule[i].isWrite && otSchedule[i].msgId == msgId) {
      otSchedule[i].valueSet = false;
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// TASK-466: TT=/TC= remote-override state machine
// ---------------------------------------------------------------------------
// PIC distinguishes TT (temporary) from TC (constant) via MsgID 100
// (RemoteOverrideFunction) low-byte flag bits (gateway.asm SetSetPoint):
//   bit0 = ManualChangePriority  -> TC sets, TT clears
//   bit1 = ProgramChangePriority -> TT sets, TC clears
// MsgID 16 (TrSet) carries the actual setpoint value in both cases. The
// thermostat reads MsgID 100 from us (the gateway-as-slave) to decide whether
// to honour the override or its own program. The boiler ignores MsgID 100 and
// only sees MsgID 16, so MsgID 100 is purely a UX hint to the thermostat.
//
// Note: setRemoteOverride / clearRemoteOverride deliberately do NOT touch the
// TASK-442 CS/C2 expiry timestamps (otCSLastCommandMs / otC2LastCommandMs).
// TT/TC and CS/C2 are independent state machines.
// ---------------------------------------------------------------------------
static void setRemoteOverride(OTRemoteOverrideMode mode, float celsius) {
  // TASK-497 (4A-M1): cast through the canonical floatToF88 helper which
  // includes the -40..127 clamp formerly inlined here (TASK-495).
  uint16_t f88 = floatToF88(celsius);

  // TASK-500 (1A-M1): "queue is the channel, no side-channels". Enqueue the
  // primary frame (MsgID 16, TrSet) FIRST. If the queue is full, leave
  // otRemoteOverride.* and state.otd.* untouched — the ghost-override bug
  // happens when state claims an active override with no frame in flight.
  // MsgID 16: replace thermostat's TrSet on outbound to boiler.
  if (!enqueueWriteCommand(16, f88, mode == OT_OVERRIDE_TEMPORARY ? "TT" : "TC")) {
    OTDDebugTln(F("OTD: OVR-set MsgID 16 dropped (queue full); state unchanged"));
    return;
  }

  // MsgID 100: low byte = priority bits (high byte unused per OT v4.2). This
  // is a UX hint to the thermostat — boiler ignores it. If MsgID 16 made it
  // through but MsgID 100 didn't, we accept the partial state: boiler-side
  // is correct, only the thermostat UI may briefly show "manual" until the
  // periodic refresh re-emits it.
  uint16_t flags = (mode == OT_OVERRIDE_TEMPORARY) ? 0x0002 : 0x0001;
  if (!enqueueWriteCommand(100, flags, "OVR-flags")) {
    OTDDebugTln(F("OTD: OVR-flags MsgID 100 dropped (queue full); MsgID 16 in flight"));
  }

  // Both queued (or 16-only with logged 100-drop) — now safe to commit state.
  otRemoteOverride.mode               = mode;
  otRemoteOverride.f88Value           = f88;
  otRemoteOverride.setAtMs            = millis();
  otRemoteOverride.honoredCount       = 0;
  otRemoteOverride.lastThermostatVal  = 0;       // value undefined until bHaveLastThermostat flips
  otRemoteOverride.bHaveLastThermostat = false;  // TASK-498 (1A-M2): reset honour history
  state.otd.eOverrideMode             = mode;
  state.otd.iOverrideF88              = f88;
}

static void clearRemoteOverride() {
  otRemoteOverride.mode                = OT_OVERRIDE_NONE;
  otRemoteOverride.f88Value            = 0;
  otRemoteOverride.honoredCount        = 0;
  otRemoteOverride.lastThermostatVal   = 0;
  otRemoteOverride.bHaveLastThermostat = false;  // TASK-498 (1A-M2)
  state.otd.eOverrideMode              = OT_OVERRIDE_NONE;
  state.otd.iOverrideF88               = 0;

  clearWriteOverride(16);
  clearWriteOverride(100);
  // Push a clearing MsgID 100 = 0 frame so the thermostat's UI flips back to
  // its own program. This is a one-shot WRITE_DATA, not a sticky override.
  unsigned long clearFrame = OpenTherm::buildRequest(
    OpenThermMessageType::WRITE_DATA,
    static_cast<OpenThermMessageID>(100),
    0
  );
  if (!otCmdEnqueue(clearFrame)) {
    OTDDebugTln(F("OTD: OVR-clear dropped (queue full)"));
  }
}

// Hook into the thermostat-side MsgID 16 inbound path (called from
// applyOverrides). Implements the PIC TT= auto-clear trigger:
//
//   1. Honour-detection: while a TT/TC is active, count cycles where the
//      thermostat reports a MsgID 16 value within ~0.25 C of our override.
//   2. Release-detection (TT only): once we have been honoured for at least
//      OT_OVERRIDE_HONOR_THRESHOLD cycles, the next thermostat MsgID 16
//      whose value diverges by > 0.5 C means the thermostat program has
//      taken over; release the override.
//
// TC overrides intentionally never auto-clear here — only TT does. TC is
// "vacation mode": it stays until the user explicitly sends TC=0 (or a
// replacement TT=). This matches gateway.asm SetContSetPoint semantics.
static void onThermostatMsgID16(uint8_t msgType, uint16_t f88) {
  // OT v4.2: thermostat-originated MsgID 16 frames are WRITE_DATA (type 1)
  // carrying TrSet. Other types (READ_DATA queries) carry no setpoint and
  // would skew honour detection. Filter strictly.
  if (msgType != 1 /* WRITE_DATA */) return;

  uint32_t now = millis();
  otRemoteOverride.lastThermostatMs = now;

  if (otRemoteOverride.mode == OT_OVERRIDE_NONE) {
    otRemoteOverride.lastThermostatVal   = f88;
    otRemoteOverride.bHaveLastThermostat = true;
    return;
  }

  // Compute |delta| in f8.8 units. f8.8 is two's-complement signed, so we
  // sign-extend through int16_t before widening to int32_t. Without the
  // (int16_t) hop a uint16_t -> int32_t cast keeps the high bit unset
  // (e.g. -5 °C raw 0xFB00 would read as +64256 instead of -1280) and the
  // honour/release deltas would mis-fire for negative TrSet values.
  int16_t curSigned = (int16_t)f88;
  int16_t ovrSigned = (int16_t)otRemoteOverride.f88Value;
  int32_t signedDelta = (int32_t)curSigned - (int32_t)ovrSigned;
  uint32_t delta = (signedDelta < 0) ? (uint32_t)(-signedDelta) : (uint32_t)signedDelta;

  if (delta < OT_OVERRIDE_HONOR_DELTA_F88) {
    // Thermostat is echoing our value — count it as an honoured cycle.
    if (otRemoteOverride.honoredCount < 0xFF) otRemoteOverride.honoredCount++;
  } else if (otRemoteOverride.mode == OT_OVERRIDE_TEMPORARY
             && otRemoteOverride.honoredCount >= OT_OVERRIDE_HONOR_THRESHOLD
             && delta > OT_OVERRIDE_RELEASE_DELTA_F88) {
    // TT only: thermostat program has resumed after honouring us. Release.
    OTDDebugTln(F("OTD: TT auto-clear (thermostat program resumed)"));
    clearRemoteOverride();
  }

  otRemoteOverride.lastThermostatVal   = f88;
  otRemoteOverride.bHaveLastThermostat = true;
}

// ---------------------------------------------------------------------------
// resetTransientState — clear all transient state (called on GW=R and mode change)
// Mirrors PIC hardware reset clearing all RAM, except persisted settings.
// ---------------------------------------------------------------------------
static void resetTransientState() {
  // Operating mode overrides
  otOperModeDHW = 0;
  otOperModeCH1 = 0;
  otOperModeCH2 = 0;
  // DHW push state machine
  otDHWPushState = PUSH_IDLE;
  otDHWPushStartMs = 0;
  // PS=1 summary mode
  otSummaryMode = false;
  otHideReports = false;
  otSummaryPending = false;
  // Thermostat connectivity tracking — reset to boot state
  otThermostatSeen = false;
  otLastThermostatMs = 0;
  // Force all schedule entries to poll immediately on next cycle
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
    otSchedule[i].lastSentMs = 0;
  }
  // Status bit overrides (non-persisted) — reset to CH enable only (boot default)
  otDHWBlocking = false;
  otCoolingEnable = false;
  otMasterStatusFlags = 0x01;  // CH enable only; summer mode re-applied below if persisted
  if (settings.otd.bSummerMode) otMasterStatusFlags |= 0x20;
  // DHW override
  otDHWOverride = 0xFF;  // auto
  // 3-strike unknown counters
  memset(otUnknownCounters, 0, sizeof(otUnknownCounters));
  // Clear all schedule disable flags (re-enable all MsgIDs)
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
    otSchedule[i].disabled = false;
  }
  // Clear unknown ID list
  otUnknownIdCount = 0;
  // Clear response overrides
  for (uint8_t i = 0; i < OT_RESPONSE_OVERRIDE_MAX; i++) {
    otResponseOverrides[i].active = false;
  }
  // Clear write caches (stop periodic writes)
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
    otSchedule[i].valueSet = false;
  }
  // Clear repeater overrides
  for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) {
    otOverrides[i].active = false;
  }
  // TASK-466: clear TT/TC remote-override state machine. Mirrors PIC
  // behaviour where a hardware reset wipes setpoint1/2 + OverrideReq.
  otRemoteOverride.mode                = OT_OVERRIDE_NONE;
  otRemoteOverride.f88Value            = 0;
  otRemoteOverride.honoredCount        = 0;
  otRemoteOverride.lastThermostatVal   = 0;
  otRemoteOverride.bHaveLastThermostat = false;  // TASK-498 (1A-M2)
  state.otd.eOverrideMode              = OT_OVERRIDE_NONE;
  state.otd.iOverrideF88               = 0;
  // Clear response modifiers
  clearAllResponseModifiers();
  // Clear setback state
  if (otSetbackEngaged) {
    otSetbackEngaged = false;
    state.otd.bSetbackActive = false;
  }
  // TASK-183: reset PI controller state
  otPiIntegState = 0.0f;
  otPiRoomFilt   = 0.0f;
  otPiRspPrev    = 0.0f;
  otPiInit       = false;
  otPiDeltaT     = 0.0f;
  otNextPiCtrl   = 0;
  // BS= fake room setpoint — clear on reset
  otFakeRoomSetpoint = 0.0f;
}

// ---------------------------------------------------------------------------
// setOTDirectMode — switch operating mode with hardware reconfiguration
// ---------------------------------------------------------------------------
static void setOTDirectMode(OTDirectMode newMode) {
  resetTransientState();  // Clear all transient state on mode change
  otCurrentMode  = newMode;

  switch (newMode) {
    case OTD_MODE_BYPASS:
#if HAS_BYPASS_RELAY
      digitalWrite(PIN_BYPASS_RELAY, HIGH);
      OTDDebugTln(F("OTD: mode -> bypass (thermostat direct to boiler)"));
      sendWebSocketJSON("{\"type\":\"status\",\"msg\":\"OTDirect: mode bypass\"}");
#endif
      break;

    case OTD_MODE_GATEWAY:
#if HAS_BYPASS_RELAY
      digitalWrite(PIN_BYPASS_RELAY, LOW);
#endif
      // Ensure slave interface is running (may have been stopped in master mode)
      otSlave.begin(slaveISR, handleSlaveRequest);
      OTDDebugTln(F("OTD: mode -> gateway (OT-direct active, overrides enabled)"));
      sendWebSocketJSON("{\"type\":\"status\",\"msg\":\"OTDirect: mode gateway\"}");
      break;

    case OTD_MODE_MONITOR:
#if HAS_BYPASS_RELAY
      digitalWrite(PIN_BYPASS_RELAY, LOW);
#endif
      // Ensure slave interface is running (may have been stopped in master mode)
      otSlave.begin(slaveISR, handleSlaveRequest);
      OTDDebugTln(F("OTD: mode -> monitor (transparent pass-through)"));
      sendWebSocketJSON("{\"type\":\"status\",\"msg\":\"OTDirect: mode monitor\"}");
      break;

    case OTD_MODE_MASTER:
#if HAS_BYPASS_RELAY
      digitalWrite(PIN_BYPASS_RELAY, LOW);
#endif
      // Start or stop slave interface based on bEnableSlave setting
      if (settings.otd.bEnableSlave) {
        otSlave.begin(slaveISR, handleSlaveRequest);
        OTDDebugTln(F("OTD: mode -> master (slave enabled for thermostat)"));
      } else {
        otSlave.end();
        OTDDebugTln(F("OTD: mode -> master (pure standalone, no slave)"));
      }
      sendWebSocketJSON("{\"type\":\"status\",\"msg\":\"OTDirect: mode master\"}");
      break;

    case OTD_MODE_LOOPBACK:
#if HAS_BYPASS_RELAY
      digitalWrite(PIN_BYPASS_RELAY, LOW);
#endif
      OTDDebugTln(F("OTD: mode -> loopback (simulated boiler responses)"));
      sendWebSocketJSON("{\"type\":\"status\",\"msg\":\"OTDirect: mode loopback\"}");
      break;
  }

  // Persist mode to settings via the public updateSetting() API.
  // This keeps all settings mutation through a single path and gets
  // side-effect handling (dirty flag, debounce timer) for free.
  char modeBuf[4];
  snprintf_P(modeBuf, sizeof(modeBuf), PSTR("%d"), (int)newMode);
  updateSetting("OTDmode", modeBuf);
}

// ---------------------------------------------------------------------------
// checkThermostatTimeout — detect thermostat disconnect and apply setback
// Called once per second from loopOTDirect().
// ---------------------------------------------------------------------------
static void checkThermostatTimeout() {
  if (!otThermostatSeen) return;  // no frame ever received — nothing to time out

  uint32_t elapsed = millis() - otLastThermostatMs;
  uint32_t timeoutMs = (uint32_t)settings.otd.iSetbackTimeout * 1000UL;

  if (elapsed > timeoutMs) {
    // Thermostat disconnected
    if (state.otd.bThermostatConnected) {
      state.otd.bThermostatConnected = false;
      OTDDebugTf(PSTR("OTD: thermostat disconnected (no frame for %lus)\r\n"),
              elapsed / 1000UL);
      sendWebSocketJSON("{\"type\":\"status\",\"msg\":\"OTDirect: disconnected\"}");
    }
    // Engage setback if fail-safe enabled and in gateway mode
    if (otFailSafeEnabled && otCurrentMode == OTD_MODE_GATEWAY && !otSetbackEngaged) {
      otSetbackEngaged = true;
      state.otd.bSetbackActive = true;
      uint16_t f88 = floatToF88(settings.otd.fSetbackTemp);
      setOverride(1, f88);        // Override TSet (MsgID 1) to setback temp
      updateWriteCache(1, f88);   // Keep writing it to boiler
      char tb[8]; dtostrf(settings.otd.fSetbackTemp, 1, 1, tb);
      OTDDebugTf(PSTR("OTD: setback engaged — TSet overridden to %s C\r\n"), tb);
    }
  } else {
    state.otd.bThermostatConnected = true;
    // Release setback when thermostat reconnects
    if (otSetbackEngaged) {
      otSetbackEngaged = false;
      state.otd.bSetbackActive = false;
      clearOverride(1);           // Release TSet override
      OTDDebugTln(F("OTD: thermostat reconnected, setback released"));
      sendWebSocketJSON("{\"type\":\"status\",\"msg\":\"OTDirect: connected\"}");
    }
  }
}

// ---------------------------------------------------------------------------
// handleMasterModeSlaveFrame — respond to thermostat as if we ARE the boiler.
// In master mode (standalone), the gateway is the sole OT master. If a
// thermostat is connected anyway (or reconnects), we respond with cached
// boiler values from OTcurrentSystemState so the thermostat sees valid data.
// ---------------------------------------------------------------------------
// Helper: build an OT response frame with parity (no library buildResponse needed)
// type: 4=READ_ACK, 5=WRITE_ACK, 7=UNKNOWN_DATAID
static unsigned long buildOTResponse(uint8_t type, uint8_t msgId, uint16_t data) {
  unsigned long resp = ((unsigned long)type << 28) | ((unsigned long)msgId << 16) | data;
  setOTParityBit(resp);
  return resp;
}

static void handleMasterModeSlaveFrame(unsigned long frame) {
  uint8_t msgId = (frame >> 16) & 0xFF;
  uint8_t msgType = (frame >> 28) & 0x07;

  unsigned long response;

  if (msgType == 0) {
    // READ_DATA — respond with cached boiler value
    if (otBoilerCacheValid[msgId & 0x7F]) {
      response = buildOTResponse(4, msgId, otBoilerCache[msgId & 0x7F]);  // 4 = READ_ACK
    } else {
      response = buildOTResponse(7, msgId, 0);  // 7 = UNKNOWN_DATAID (no cached data yet)
    }
  }
  else if (msgType == 1) {
    // WRITE_DATA — accept and echo back as WRITE_ACK
    uint16_t dataVal = frame & 0xFFFF;
    response = buildOTResponse(5, msgId, dataVal);  // 5 = WRITE_ACK
    // Apply thermostat's TSet as our write cache so scheduler sends it to boiler
    if (msgId == 1 && dataVal != 0) {
      setOverride(1, dataVal);
      updateWriteCache(1, dataVal);
    }
    // Apply thermostat's room setpoint
    if (msgId == 16) {
      updateWriteCache(16, dataVal);
    }
    // Apply thermostat's room temperature
    if (msgId == 24) {
      updateWriteCache(24, dataVal);
    }
  }
  else {
    // Anything else — respond UNKNOWN_DATAID
    response = buildOTResponse(7, msgId, 0);  // 7 = UNKNOWN_DATAID
  }

  // Bridge the thermostat frame + our response for logging
  bridgeFrameToParser('T', frame);
  bridgeFrameToParser('A', response);
  otSlave.sendResponse(response);
}

// ---------------------------------------------------------------------------
// Response-path modifier functions (struct/array declared at top of file)
// ---------------------------------------------------------------------------
static void setResponseModifier(uint8_t msgId, uint16_t value) {
  // Find existing or free slot
  int8_t slot = -1;
  for (uint8_t i = 0; i < OT_RESPONSE_MODIFY_MAX; i++) {
    if (otResponseModifiers[i].active && otResponseModifiers[i].msgId == msgId) {
      slot = i; break;
    }
  }
  if (slot < 0) {
    for (uint8_t i = 0; i < OT_RESPONSE_MODIFY_MAX; i++) {
      if (!otResponseModifiers[i].active) { slot = i; break; }
    }
  }
  if (slot < 0) return;  // table full
  otResponseModifiers[slot].msgId = msgId;
  otResponseModifiers[slot].value = value;
  otResponseModifiers[slot].active = true;
}

static void clearResponseModifier(uint8_t msgId) {
  for (uint8_t i = 0; i < OT_RESPONSE_MODIFY_MAX; i++) {
    if (otResponseModifiers[i].active && otResponseModifiers[i].msgId == msgId) {
      otResponseModifiers[i].active = false;
      return;
    }
  }
}
static void clearAllResponseModifiers() {
  for (uint8_t i = 0; i < OT_RESPONSE_MODIFY_MAX; i++) otResponseModifiers[i].active = false;
}

// Apply response-path modifications to a boiler response frame before
// forwarding to thermostat. Returns the (potentially modified) frame.
static unsigned long applyResponseModifiers(unsigned long response) {
  uint8_t msgId = (response >> 16) & 0xFF;
  for (uint8_t i = 0; i < OT_RESPONSE_MODIFY_MAX; i++) {
    if (otResponseModifiers[i].active && otResponseModifiers[i].msgId == msgId) {
      response = (response & 0xFFFF0000UL) | otResponseModifiers[i].value;
      setOTParityBit(response);
      break;
    }
  }
  return response;
}

// ---------------------------------------------------------------------------
// handleOTDirectCommand — translate PIC-style commands to OT frames
// Called from sendPICSerial() when HAS_DIRECT_OT is enabled.
//
// Full PIC command emulation: all commands that the PIC firmware supports
// are handled here — either translated to OpenTherm frames, applied to
// local state, or acknowledged with a valid response.
// ---------------------------------------------------------------------------
void handleOTDirectCommand(const char* buf, int len) {
  if (len < 4) return;  // minimum "XX=Y"

  OTDDebugTf(PSTR("OTD: cmd \"%s\" (len=%d)\r\n"), buf, len);

  char cmd0 = buf[0];
  char cmd1 = buf[1];
  if (buf[2] != '=') return;
  const char* value = buf + 3;
  char rspBuf[16];  // scratch for formatting response values

  // =====================================================================
  // Setpoint write commands (enqueue + write cache + override)
  // =====================================================================

  // CS=xx.x — Control setpoint (MsgID 1 = TSet). Value 0 clears override.
  if (cmd0 == 'C' && cmd1 == 'S') {
    float temp = atof(value);
    bool enqueued = true;
    if (temp == 0.0f) {
      clearWriteOverride(1);
      otCSLastCommandMs = 0;  // TASK-442: explicit clear, no expiry
    } else {
      uint16_t f88 = floatToF88(temp);
      enqueued = enqueueWriteCommand(1, f88, "CS");
      if (enqueued) {
        otCSLastCommandMs = millis();  // TASK-442: refresh expiry timer
        if (otCSLastCommandMs == 0) otCSLastCommandMs = 1;  // 0 sentinel reserved
      }
    }
    if (enqueued) {
      dtostrf(temp, 1, 2, rspBuf);
      synthesizeResponse(buf, rspBuf);
    }
  }
  // TC=xx.x — Constant ("vacation") room temperature override.
  // TASK-466: PIC parity. Sets MsgID 16 (TrSet) to the requested value AND
  // MsgID 100 (RemoteOverrideFunction) low byte = 0x01 (Manual override
  // priority). Persists until TC=0 or TT= replaces it; the TT auto-clear
  // state machine does NOT release a TC override.
  //
  // TASK-500 (1A-M3): "clear" trigger is exact-zero (gateway.asm SetSetPoint
  // parity). Negative values are NOT treated as clear; they set a TEMPORARY/
  // CONSTANT override at the negative °C, clamped to -40 °C by the floatToF88
  // helper (f8.8 lower bound). This is by design, not a bug — matches PIC
  // behaviour. To clear, send TC=0 (or TT=0).
  else if (cmd0 == 'T' && cmd1 == 'C') {
    float temp = atof(value);
    if (temp == 0.0f) {
      clearRemoteOverride();
    } else {
      setRemoteOverride(OT_OVERRIDE_CONSTANT, temp);
    }
    dtostrf(temp, 1, 2, rspBuf);
    synthesizeResponse(buf, rspBuf);
  }
  // TT=xx.x — Temporary room setpoint override.
  // TASK-466: PIC parity. Sets MsgID 16 (TrSet) AND MsgID 100 low byte = 0x02
  // (Program override priority). Auto-clears after the thermostat has echoed
  // our value for OT_OVERRIDE_HONOR_THRESHOLD cycles and then moved on its
  // own (program took over).
  else if (cmd0 == 'T' && cmd1 == 'T') {
    float temp = atof(value);
    if (temp == 0.0f) {
      clearRemoteOverride();
    } else {
      setRemoteOverride(OT_OVERRIDE_TEMPORARY, temp);
    }
    dtostrf(temp, 1, 2, rspBuf);
    synthesizeResponse(buf, rspBuf);
  }
  // BS=xx.x — Fake boiler room setpoint (MsgID 16 frame interception).
  // Intercepts the thermostat's own MsgID 16 READ_DATA and replaces the data bytes
  // with the fake setpoint before forwarding to the boiler. This is the same
  // mechanism as the PIC firmware (gateway.asm:3019-3024): the modified frame
  // is the thermostat's own request, not a separate WRITE_DATA from the gateway.
  // BS=0 clears the fake and restores the thermostat's original room setpoint.
  else if (cmd0 == 'B' && cmd1 == 'S') {
    float temp = atof(value);
    if (temp == 0.0f) {
      otFakeRoomSetpoint = 0.0f;
      clearOverride(16);
    } else {
      otFakeRoomSetpoint = temp;
      uint16_t f88 = floatToF88(temp);
      setOverride(16, f88);
    }
    dtostrf(temp, 1, 2, rspBuf);
    synthesizeResponse(buf, rspBuf);
  }
  // C2=xx.x — Control setpoint CH2 (MsgID 8 = TsetCH2). Value 0 clears.
  else if (cmd0 == 'C' && cmd1 == '2') {
    float temp = atof(value);
    bool enqueued = true;
    if (temp == 0.0f) {
      clearWriteOverride(8);
      otC2LastCommandMs = 0;  // TASK-442: explicit clear, no expiry
    } else {
      uint16_t f88 = floatToF88(temp);
      enqueued = enqueueWriteCommand(8, f88, "C2");
      if (enqueued) {
        otC2LastCommandMs = millis();  // TASK-442: refresh expiry timer
        if (otC2LastCommandMs == 0) otC2LastCommandMs = 1;
      }
    }
    if (enqueued) {
      dtostrf(temp, 1, 2, rspBuf);
      synthesizeResponse(buf, rspBuf);
    }
  }
  // CC=xx.x — Cooling control signal (MsgID 7, 0-100%)
  else if (cmd0 == 'C' && cmd1 == 'C') {
    uint16_t f88 = floatToF88(atof(value));
    if (enqueueWriteCommand(7, f88, "CC")) {
      dtostrf(atof(value), 1, 2, rspBuf);
      synthesizeResponse(buf, rspBuf);
    }
  }
  // SW=xx.x — DHW setpoint (MsgID 56 = TdhwSet). Value 0 clears.
  else if (cmd0 == 'S' && cmd1 == 'W') {
    float temp = atof(value);
    bool enqueued = true;
    if (temp == 0.0f) {
      clearWriteOverride(56);
    } else {
      uint16_t f88 = floatToF88(temp);
      enqueued = enqueueWriteCommand(56, f88, "SW");
    }
    if (enqueued) {
      dtostrf(temp, 1, 2, rspBuf);
      synthesizeResponse(buf, rspBuf);
    }
  }
  // SH=xx.x — Max CH water setpoint (MsgID 57 = MaxTSet). Value 0 clears.
  else if (cmd0 == 'S' && cmd1 == 'H') {
    float temp = atof(value);
    bool enqueued = true;
    if (temp == 0.0f) {
      clearWriteOverride(57);
    } else {
      uint16_t f88 = floatToF88(temp);
      enqueued = enqueueWriteCommand(57, f88, "SH");
    }
    if (enqueued) {
      dtostrf(temp, 1, 2, rspBuf);
      synthesizeResponse(buf, rspBuf);
    }
  }
  // MM=xx — Max relative modulation level (MsgID 14). Non-numeric clears.
  else if (cmd0 == 'M' && cmd1 == 'M') {
    if (!isdigit((unsigned char)value[0]) && value[0] != '-') {
      clearWriteOverride(14);
      synthesizeResponse(buf, value);
    } else {
      uint16_t f88 = floatToF88(atof(value));
      if (enqueueWriteCommand(14, f88, "MM")) {
        snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), atoi(value));
        synthesizeResponse(buf, rspBuf);
      }
    }
  }
  // OT=xx.x — Outside temperature (MsgID 27 = Toutside). Non-numeric clears.
  else if (cmd0 == 'O' && cmd1 == 'T') {
    if (!isdigit((unsigned char)value[0]) && value[0] != '-') {
      clearWriteOverride(27);
      synthesizeResponse(buf, value);
    } else {
      uint16_t f88 = floatToF88(atof(value));
      if (enqueueWriteCommand(27, f88, "OT")) {
        OTcurrentSystemState.Toutside = (int16_t)f88 / 256.0f;
        dtostrf(OTcurrentSystemState.Toutside, 1, 2, rspBuf);
        synthesizeResponse(buf, rspBuf);
      }
    }
  }
  // VS=xx — Ventilation setpoint (MsgID 71). Non-numeric clears.
  // TASK-584: accepted value is also written through to settings.otd.iVentSetpoint
  // so it survives a reboot without user re-applying the command.
  else if (cmd0 == 'V' && cmd1 == 'S') {
    if (!isdigit((unsigned char)value[0])) {
      clearWriteOverride(71);
      synthesizeResponse(buf, value);
      // Clear persisted ventilation setpoint on explicit clear command
      if (settings.otd.iVentSetpoint != 0) {
        settings.otd.iVentSetpoint = 0;
        writeSettings(false);
      }
    } else {
      int setpt = constrain(atoi(value), 0, 100);
      uint16_t data = ((uint16_t)(setpt & 0xFF)) << 8;
      if (enqueueWriteCommand(71, data, "VS")) {
        snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), setpt);
        synthesizeResponse(buf, rspBuf);
        // TASK-584: persist across reboots
        if (settings.otd.iVentSetpoint != (uint8_t)setpt) {
          settings.otd.iVentSetpoint = (uint8_t)setpt;
          writeSettings(false);
        }
      }
    }
  }
  // SC=HH:MM/DOW — Time sync (MsgID 20 = day/time)
  else if (cmd0 == 'S' && cmd1 == 'C') {
    int hh = 0, mm = 0, dow = 0;
    if (sscanf(value, "%d:%d/%d", &hh, &mm, &dow) >= 2) {
      uint16_t data20 = (uint16_t)(((dow & 0x07) << 5 | (hh & 0x1F)) << 8) | (mm & 0x3F);
      enqueueWriteCommand(20, data20, "SC-time");
    }
    // SC doesn't produce a PIC-style response
  }

  // =====================================================================
  // Status flag commands (immediate effect on MsgID 0 status byte)
  // =====================================================================

  // HW=0/1/A/P — DHW enable (bit1 of master status) + DHW push state machine
  else if (cmd0 == 'H' && cmd1 == 'W') {
    char v = value[0];
    if (v == 'P') {
      // DHW push: enable DHW + start push state machine
      otMasterStatusFlags |= 0x02;
      otDHWOverride = 1;
      if (otDHWPushState == PUSH_IDLE) {
        otDHWPushState = PUSH_PENDING;
        otDHWPushStartMs = millis();
        // Send MsgID 99 with DHW push bit (bit4 of byte3)
        uint16_t data99 = ((uint16_t)(otOperModeDHW | 0x10)) |
                          ((uint16_t)(otOperModeCH1 | (otOperModeCH2 << 4)) << 8);
        unsigned long frame99 = OpenTherm::buildRequest(
          OpenThermMessageType::WRITE_DATA,
          static_cast<OpenThermMessageID>(99), data99);
        if (!otCmdEnqueue(frame99)) { OTDDebugTf(PSTR("OTD: queue full, dropped frame %08lX\r\n"), frame99); }
      }
    } else if (v == '1') {
      otMasterStatusFlags |= 0x02;
      otDHWOverride = 1;
    } else if (v == '0') {
      otMasterStatusFlags &= ~0x02;
      otDHWOverride = 0;
      // Abort DHW push if in progress
      if (otDHWPushState != PUSH_IDLE) {
        otDHWPushState = PUSH_IDLE;
        OTDDebugTln(F("OTD: DHW push aborted"));
      }
    } else {
      // Any other char = auto (thermostat controls DHW)
      otMasterStatusFlags |= 0x02;  // default on for auto
      otDHWOverride = 0xFF;
      v = 'A';
    }
    rspBuf[0] = v; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // CH=0/1 — CH enable (bit0 of master status)
  else if (cmd0 == 'C' && cmd1 == 'H') {
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return; }
    if (value[0] == '1') {
      otMasterStatusFlags |= 0x01;
    } else {
      otMasterStatusFlags &= ~0x01;
    }
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // H2=0/1 — CH2 enable (bit4 of master status)
  else if (cmd0 == 'H' && cmd1 == '2') {
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return; }
    if (value[0] == '1') {
      otMasterStatusFlags |= 0x10;
    } else {
      otMasterStatusFlags &= ~0x10;
    }
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // SM=0/1 — Summer mode (bit5 of master status)
  else if (cmd0 == 'S' && cmd1 == 'M') {
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return; }
    if (value[0] == '1') {
      otMasterStatusFlags |= 0x20;
      otSummerMode = true;
    } else {
      otMasterStatusFlags &= ~0x20;
      otSummerMode = false;
    }
    settings.otd.bSummerMode = otSummerMode;
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // BW=0/1 — DHW blocking (bit6 of master status, not persisted)
  else if (cmd0 == 'B' && cmd1 == 'W') {
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return; }
    if (value[0] == '1') {
      otMasterStatusFlags |= 0x40;
      otDHWBlocking = true;
    } else {
      otMasterStatusFlags &= ~0x40;
      otDHWBlocking = false;
    }
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // CE=0/1 — Cooling enable (bit2 of master status, not persisted)
  else if (cmd0 == 'C' && cmd1 == 'E') {
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return; }
    if (value[0] == '1') {
      otMasterStatusFlags |= 0x04;
      otCoolingEnable = true;
    } else {
      otMasterStatusFlags &= ~0x04;
      otCoolingEnable = false;
    }
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // CL=xx.x — Cooling level (PIC-compatible alias for CC=, MsgID 7)
  else if (cmd0 == 'C' && cmd1 == 'L') {
    float cl = atof(value);
    bool enqueued = true;
    if (cl == 0.0f) {
      clearWriteOverride(7);
    } else {
      uint16_t f88 = floatToF88(cl);
      enqueued = enqueueWriteCommand(7, f88, "CL");
    }
    if (enqueued) {
      dtostrf(cl, 1, 2, rspBuf);
      synthesizeResponse(buf, rspBuf);
    }
  }

  // =====================================================================
  // Operating mode commands (MH, MW, M2) — MsgID 99
  // =====================================================================

  // MH=x — Remote override operating mode CH1 (lower nibble of byte4)
  else if (cmd0 == 'M' && cmd1 == 'H') {
    uint8_t val = atoi(value);
    if (val > 15) { otDirectBridgeProcessStatus("BV"); return; }
    otOperModeCH1 = val;
    // Send MsgID 99 WRITE_DATA
    uint16_t data99 = ((uint16_t)otOperModeDHW) | ((uint16_t)(otOperModeCH1 | (otOperModeCH2 << 4)) << 8);
    unsigned long frame99 = OpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      static_cast<OpenThermMessageID>(99), data99);
    if (!otCmdEnqueue(frame99)) { OTDDebugTf(PSTR("OTD: queue full, dropped frame %08lX\r\n"), frame99); }
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), val);
    synthesizeResponse(buf, rspBuf);
  }
  // MW=x — Remote override operating mode DHW (lower nibble of byte3)
  else if (cmd0 == 'M' && cmd1 == 'W') {
    uint8_t val = atoi(value);
    if (val > 15) { otDirectBridgeProcessStatus("BV"); return; }
    otOperModeDHW = val;
    uint16_t data99 = ((uint16_t)otOperModeDHW) | ((uint16_t)(otOperModeCH1 | (otOperModeCH2 << 4)) << 8);
    unsigned long frame99 = OpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      static_cast<OpenThermMessageID>(99), data99);
    if (!otCmdEnqueue(frame99)) { OTDDebugTf(PSTR("OTD: queue full, dropped frame %08lX\r\n"), frame99); }
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), val);
    synthesizeResponse(buf, rspBuf);
  }
  // M2=x — Remote override operating mode CH2 (upper nibble of byte4)
  else if (cmd0 == 'M' && cmd1 == '2') {
    uint8_t val = atoi(value);
    if (val > 15) { otDirectBridgeProcessStatus("BV"); return; }
    otOperModeCH2 = val;
    uint16_t data99 = ((uint16_t)otOperModeDHW) | ((uint16_t)(otOperModeCH1 | (otOperModeCH2 << 4)) << 8);
    unsigned long frame99 = OpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      static_cast<OpenThermMessageID>(99), data99);
    if (!otCmdEnqueue(frame99)) { OTDDebugTf(PSTR("OTD: queue full, dropped frame %08lX\r\n"), frame99); }
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), val);
    synthesizeResponse(buf, rspBuf);
  }
  // RR=x — Remote request (MsgID 4, one-shot WRITE_DATA)
  else if (cmd0 == 'R' && cmd1 == 'R') {
    uint8_t code = atoi(value);
    unsigned long frame4 = OpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      static_cast<OpenThermMessageID>(4), (uint16_t)code << 8);
    if (!otCmdEnqueue(frame4)) { OTDDebugTf(PSTR("OTD: queue full, dropped frame %08lX\r\n"), frame4); }
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), code);
    synthesizeResponse(buf, rspBuf);
  }

  // =====================================================================
  // Gateway control commands
  // =====================================================================

  else if (cmd0 == 'G' && cmd1 == 'W') {
    // TASK-438: PIC parity. gateway.asm: GW=0 = monitor, GW=1 = gateway,
    // GW=R = reset. Bypass moved to GW=P (OTDirect-only alias) so PIC
    // tooling sending GW=0 cannot accidentally engage bypass relay.
    if (value[0] == '0' || value[0] == 'M') {
      // GW=0 / GW=M — Monitor mode: transparent pass-through.
      setOTDirectMode(OTD_MODE_MONITOR);
    } else if (value[0] == '1') {
      // GW=1 — Gateway mode: full override processing
      setOTDirectMode(OTD_MODE_GATEWAY);
    } else if (value[0] == '2' || value[0] == 'S') {
      // GW=2 or GW=S — Master/Standalone mode: sole OT master, no thermostat
      setOTDirectMode(OTD_MODE_MASTER);
    } else if (value[0] == 'P') {
      // TASK-438: GW=P — Bypass mode (OTDirect extension; thermostat -> boiler via relay).
#if HAS_BYPASS_RELAY
      if (!settings.otd.bHasBypassRelay) {
        OTDDebugTln(F("OTD: GW=P rejected (bypass relay not enabled in settings)"));
        otDirectBridgeProcessStatus("NG");
        return;
      }
      setOTDirectMode(OTD_MODE_BYPASS);
#else
      OTDDebugTln(F("OTD: GW=P not supported (no bypass relay on this board)"));
      otDirectBridgeProcessStatus("NG");
      return;
#endif
    } else if (value[0] == 'L') {
      // GW=L — Loopback test mode: simulated boiler, no hardware needed
      setOTDirectMode(OTD_MODE_LOOPBACK);
    } else if (value[0] == 'R') {
      // Full reset: clear all transient state (mirrors PIC hardware reset)
      OTDDebugTln(F("OTD: full gateway reset"));
      resetTransientState();
      // Restart OT interfaces
      otMaster.end();
      otSlave.end();
      delay(100);
      otMaster.begin(masterISR);
      otSlave.begin(slaveISR, handleSlaveRequest);
      OTDDebugTln(F("OTD: OT interfaces restarted"));
    }
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }

  // =====================================================================
  // PR= queries — synthesize responses from OT-direct internal state
  // =====================================================================

  else if (cmd0 == 'P' && cmd1 == 'R') {
    char reg = value[0];
    char prBuf[48];
    switch (reg) {
      case 'A':
        // Banner — tools pattern-match on "OpenTherm Gateway" to detect device
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: A=OpenTherm Gateway OTGW32"));
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      case 'M':
        // Gateway mode: G=gateway, M=monitor, P=passthru (bypass), S=standalone, L=loopback
        {
          char modeChar = 'G';
          if (IS_BYPASS_MODE()) modeChar = 'P';
          else if (IS_MONITOR_MODE()) modeChar = 'M';
          else if (IS_MASTER_MODE()) modeChar = 'S';
          else if (IS_LOOPBACK_MODE()) modeChar = 'L';
          snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: M=%c"), modeChar);
          otDirectBridgeProcessPRResponse(prBuf);
        }
        break;
      case 'O': {
        // Setpoint override report. TASK-466: TT/TC remote-override state
        // machine takes precedence — it carries the explicit mode (T or C).
        // Falls back to CS (boiler control setpoint) if no TT/TC is active
        // and a CS override is in force.
        if (otRemoteOverride.mode == OT_OVERRIDE_TEMPORARY) {
          dtostrf((int16_t)otRemoteOverride.f88Value / 256.0f, 1, 2, rspBuf);
          snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: O=T%s"), rspBuf);
        } else if (otRemoteOverride.mode == OT_OVERRIDE_CONSTANT) {
          dtostrf((int16_t)otRemoteOverride.f88Value / 256.0f, 1, 2, rspBuf);
          snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: O=C%s"), rspBuf);
        } else {
          // No TT/TC: check for a CS (MsgID 1) flow setpoint override.
          bool csActive = false;
          uint16_t csVal = 0;
          for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) {
            if (otOverrides[i].active && otOverrides[i].msgId == 1) {
              csActive = true; csVal = otOverrides[i].overrideValue; break;
            }
          }
          if (csActive) {
            dtostrf((int16_t)csVal / 256.0f, 1, 2, rspBuf);
            snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: O=C%s"), rspBuf);
          } else {
            snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: O=N"));
          }
        }
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      }
      case 'S':
        dtostrf(otSetbackTemp, 1, 2, rspBuf);
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: S=%s"), rspBuf);
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      case 'W':
        // DHW override state
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: W=%c"),
          (otDHWOverride == 0xFF) ? 'A' : (otDHWOverride ? '1' : '0'));
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      case 'G':
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: G=%s"), otGpioFunctions);
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      case 'I':
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: I=00"));
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      case 'L':
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: L=%s"), otLedFunctions);
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      case 'T':
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: T=%d/%d"), otIgnoreTransitions, otOverrideHB);
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      case 'D':
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: D=%c"), otTempSensor);
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      case 'P':
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: P=M"));  // medium power (default)
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      case 'R':
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: R=I"));  // internal detection
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      case 'B':
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: B=%s"), __DATE__);
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      case 'C':
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: C=240"));  // ESP32-S3 @ 240MHz
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      case 'Q': {
        // Map ESP32 reset reason to PIC-compatible codes:
        // P=power-on, C=cold(SW reset), W=watchdog, B=brownout, E=external
        char rc = 'P';
#if defined(ESP32)
        switch (esp_reset_reason()) {
          case ESP_RST_SW:      rc = 'C'; break;  // Software reset → Cold start
          case ESP_RST_INT_WDT:
          case ESP_RST_TASK_WDT:
          case ESP_RST_WDT:     rc = 'W'; break;  // Watchdog
          case ESP_RST_BROWNOUT: rc = 'B'; break;
          case ESP_RST_EXT:     rc = 'E'; break;  // External reset
          default:              rc = 'P'; break;  // Power-on or unknown
        }
#endif
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: Q=%c"), rc);
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      }
      case 'N':
        // Message interval in centiseconds (10ms units), matching PIC PR=N format
        // TASK-440: report MI in milliseconds (PIC PrintInterval parity).
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: N=%u"), (unsigned)otMinIntervalMs);
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      case 'V':
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: V=%d"), otVoltageRef);
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      default:
        OTDDebugTf(PSTR("OTD: PR=%c unknown register\r\n"), reg);
        break;
    }
  }

  // =====================================================================
  // PS=0/1 — Print Summary mode
  // PS=1: suppress T/B/R/A frames, emit comma-separated summary per cycle
  // PS=0: resume normal frame output
  // =====================================================================

  else if (cmd0 == 'P' && cmd1 == 'S') {
    if (value[0] == '1') {
      otSummaryMode = true;
      otHideReports = true;
      otSummaryPending = true;  // trigger immediate first summary
    } else {
      otSummaryMode = false;
      otHideReports = false;
      otSummaryPending = false;
    }
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }

  // =====================================================================
  // Schedule management commands (AA, DA, PM)
  // =====================================================================

  // AA=MsgID — Add alternative: re-enable a disabled schedule entry
  else if (cmd0 == 'A' && cmd1 == 'A') {
    uint8_t msgId = atoi(value);
    if (msgId == 0 || msgId > 127) {
      otDirectBridgeProcessStatus("BV"); return;
    }
    bool found = false;
    for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
      if (otSchedule[i].msgId == msgId) {
        otSchedule[i].disabled = false;
        found = true; break;
      }
    }
    if (!found) { otDirectBridgeProcessStatus("NF"); return; }
    clearUnknownCount(msgId);  // Reset 3-strike counter so it doesn't re-disable immediately
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), msgId);
    synthesizeResponse(buf, rspBuf);
  }
  // DA=MsgID — Delete alternative: disable a schedule entry
  else if (cmd0 == 'D' && cmd1 == 'A') {
    uint8_t msgId = atoi(value);
    if (msgId == 0 || msgId > 127) {
      otDirectBridgeProcessStatus("BV"); return;
    }
    bool found = false;
    for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
      if (otSchedule[i].msgId == msgId) {
        otSchedule[i].disabled = true;
        found = true; break;
      }
    }
    if (!found) { otDirectBridgeProcessStatus("NF"); return; }
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), msgId);
    synthesizeResponse(buf, rspBuf);
  }
  // PM=MsgID — Priority message: send immediately on next cycle
  else if (cmd0 == 'P' && cmd1 == 'M') {
    uint8_t msgId = atoi(value);
    if (msgId > 127) {
      otDirectBridgeProcessStatus("BV"); return;
    }
    bool found = false;
    for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
      if (otSchedule[i].msgId == msgId) {
        otSchedule[i].lastSentMs = 0;  // force immediate on next cycle
        otSchedule[i].disabled = false; // re-enable if it was disabled
        found = true; break;
      }
    }
    if (!found) {
      // MsgID not in schedule — build and enqueue a one-shot READ request
      unsigned long frame = OpenTherm::buildRequest(
        OpenThermMessageType::READ_DATA,
        static_cast<OpenThermMessageID>(msgId), 0);
      if (!otCmdEnqueue(frame)) { OTDDebugTf(PSTR("OTD: queue full, dropped frame %08lX\r\n"), frame); }
    }
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), msgId);
    synthesizeResponse(buf, rspBuf);
  }

  // =====================================================================
  // Override commands (SR, CR)
  // =====================================================================

  // SR=MsgID:lowByte  or  SR=MsgID:highByte,lowByte (PIC-compatible decimal byte syntax)
  // Legacy SR=MsgID:HHHH hex16 form retained for backward compatibility but
  // PIC-style comma syntax takes precedence.
  // TASK-441: gateway.asm SetResponse parses decimal bytes (0..255). Time/date
  // sync via networkStuff.ino sendtimecommand emits SR=21:month,day and
  // SR=22:yearHi,yearLo using PIC syntax, so OTDirect must accept it.
  else if (cmd0 == 'S' && cmd1 == 'R') {
    unsigned int msgId = 0;
    unsigned int dataVal = 0;
    const char* colon = strchr(value, ':');
    const char* comma = colon ? strchr(colon, ',') : NULL;
    if (!colon) { otDirectBridgeProcessStatus("BV"); return; }
    if (comma) {
      // PIC byte-pair: SR=<msgId>:<highByte>,<lowByte> (decimal 0..255 each)
      unsigned int hiByte = 0, loByte = 0;
      if (sscanf(value, "%u:%u,%u", &msgId, &hiByte, &loByte) != 3 ||
          msgId > 127 || hiByte > 255 || loByte > 255) {
        otDirectBridgeProcessStatus("BV"); return;
      }
      dataVal = (hiByte << 8) | loByte;
    } else {
      // Single argument after colon. PIC SetResponse: decimal byte (0..255)
      // for the low byte, high byte = 0. Try decimal byte first; if the value
      // is > 255 OR the string contains hex digits a-f/A-F, fall back to the
      // legacy hex16 form for backward compatibility with existing clients.
      unsigned int oneVal = 0;
      bool hasHexDigit = false;
      for (const char* p = colon + 1; *p; ++p) {
        if ((*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) { hasHexDigit = true; break; }
      }
      if (!hasHexDigit && sscanf(value, "%u:%u", &msgId, &oneVal) == 2 && oneVal <= 255) {
        if (msgId > 127) { otDirectBridgeProcessStatus("BV"); return; }
        dataVal = oneVal & 0xFF;  // high byte = 0
      } else if (sscanf(value, "%u:%x", &msgId, &dataVal) == 2 && msgId <= 127) {
        // legacy hex16 form
      } else {
        otDirectBridgeProcessStatus("BV"); return;
      }
    }
    // Find existing or free slot
    int8_t slot = -1;
    for (uint8_t i = 0; i < OT_RESPONSE_OVERRIDE_MAX; i++) {
      if (otResponseOverrides[i].active && otResponseOverrides[i].msgId == (uint8_t)msgId) {
        slot = i; break;
      }
    }
    if (slot < 0) {
      for (uint8_t i = 0; i < OT_RESPONSE_OVERRIDE_MAX; i++) {
        if (!otResponseOverrides[i].active) { slot = i; break; }
      }
    }
    if (slot < 0) { otDirectBridgeProcessStatus("NS"); return; }  // table full
    otResponseOverrides[slot].msgId = (uint8_t)msgId;
    otResponseOverrides[slot].value = (uint16_t)dataVal;
    otResponseOverrides[slot].active = true;
    synthesizeResponse(buf, value);
  }
  // CR=MsgID — Clear response override
  else if (cmd0 == 'C' && cmd1 == 'R') {
    uint8_t msgId = atoi(value);
    if (msgId > 127) { otDirectBridgeProcessStatus("BV"); return; }
    bool found = false;
    for (uint8_t i = 0; i < OT_RESPONSE_OVERRIDE_MAX; i++) {
      if (otResponseOverrides[i].active && otResponseOverrides[i].msgId == msgId) {
        otResponseOverrides[i].active = false;
        found = true; break;
      }
    }
    if (!found) { otDirectBridgeProcessStatus("NF"); return; }
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), msgId);
    synthesizeResponse(buf, rspBuf);
  }

  // UI=MsgID — Mark as unknown (gateway responds UNKNOWN_DATAID to thermostat)
  else if (cmd0 == 'U' && cmd1 == 'I') {
    uint8_t msgId = atoi(value);
    if (msgId > 127) { otDirectBridgeProcessStatus("BV"); return; }
    if (!isUnknownId(msgId)) {
      if (otUnknownIdCount >= OT_UNKNOWN_ID_MAX) { otDirectBridgeProcessStatus("NS"); return; }
      otUnknownIds[otUnknownIdCount++] = msgId;
    }
    // Also disable from schedule so we stop polling it
    for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
      if (otSchedule[i].msgId == msgId) { otSchedule[i].disabled = true; break; }
    }
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), msgId);
    synthesizeResponse(buf, rspBuf);
  }
  // KI=MsgID — Mark as known again (restore normal forwarding)
  else if (cmd0 == 'K' && cmd1 == 'I') {
    uint8_t msgId = atoi(value);
    if (msgId > 127) { otDirectBridgeProcessStatus("BV"); return; }
    for (uint8_t i = 0; i < otUnknownIdCount; i++) {
      if (otUnknownIds[i] == msgId) {
        otUnknownIds[i] = otUnknownIds[--otUnknownIdCount];
        break;
      }
    }
    clearUnknownCount(msgId);  // Reset 3-strike counter
    // Re-enable in schedule
    for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
      if (otSchedule[i].msgId == msgId) { otSchedule[i].disabled = false; break; }
    }
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), msgId);
    synthesizeResponse(buf, rspBuf);
  }

  // =====================================================================
  // Local storage commands (SB, IT, OH, RS)
  // =====================================================================

  // RM=MsgID:HHHH — Set response modifier (modify boiler→thermostat response data)
  else if (cmd0 == 'R' && cmd1 == 'M') {
    unsigned int msgId = 0;
    unsigned int dataVal = 0;
    if (sscanf(value, "%u:%x", &msgId, &dataVal) != 2 || msgId > 127) {
      otDirectBridgeProcessStatus("BV"); return;
    }
    setResponseModifier((uint8_t)msgId, (uint16_t)dataVal);
    synthesizeResponse(buf, value);
  }
  // CM=MsgID — Clear response modifier
  else if (cmd0 == 'C' && cmd1 == 'M') {
    uint8_t msgId = atoi(value);
    if (msgId > 127) { otDirectBridgeProcessStatus("BV"); return; }
    clearResponseModifier(msgId);
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), msgId);
    synthesizeResponse(buf, rspBuf);
  }

  // SB=xx.x — Setback temperature (used when thermostat disconnects)
  else if (cmd0 == 'S' && cmd1 == 'B') {
    otSetbackTemp = constrain(atof(value), 1.0f, 30.0f);
    settings.otd.fSetbackTemp = otSetbackTemp;  // persist to settings
    dtostrf(otSetbackTemp, 1, 2, rspBuf);
    synthesizeResponse(buf, rspBuf);
  }
  // IT=0/1 — Ignore transitions
  else if (cmd0 == 'I' && cmd1 == 'T') {
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return; }
    otIgnoreTransitions = (value[0] == '1') ? 1 : 0;
    rspBuf[0] = '0' + otIgnoreTransitions; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // OH=0/1 — Override high byte
  else if (cmd0 == 'O' && cmd1 == 'H') {
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return; }
    otOverrideHB = (value[0] == '1') ? 1 : 0;
    rspBuf[0] = '0' + otOverrideHB; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // RS=HBS/HBH/HPS/HPH/WBS/WBH/WPS/WPH — Reset boiler counter via WRITE_DATA
  else if (cmd0 == 'R' && cmd1 == 'S') {
    // Map 3-char counter code to MsgID
    uint8_t targetMsgId = 0;
    if (strlen(value) >= 3) {
      char c0 = value[0], c1 = value[1], c2 = value[2];
      if (c0 == 'H' && c1 == 'B' && c2 == 'S') targetMsgId = 116;
      else if (c0 == 'H' && c1 == 'P' && c2 == 'S') targetMsgId = 117;
      else if (c0 == 'W' && c1 == 'P' && c2 == 'S') targetMsgId = 118;
      else if (c0 == 'W' && c1 == 'B' && c2 == 'S') targetMsgId = 119;
      else if (c0 == 'H' && c1 == 'B' && c2 == 'H') targetMsgId = 120;
      else if (c0 == 'H' && c1 == 'P' && c2 == 'H') targetMsgId = 121;
      else if (c0 == 'W' && c1 == 'P' && c2 == 'H') targetMsgId = 122;
      else if (c0 == 'W' && c1 == 'B' && c2 == 'H') targetMsgId = 123;
    }
    if (targetMsgId == 0) { otDirectBridgeProcessStatus("BV"); return; }
    // Send WRITE_DATA with value 0 to reset the counter
    unsigned long frame = OpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      static_cast<OpenThermMessageID>(targetMsgId), 0);
    if (!otCmdEnqueue(frame)) { OTDDebugTf(PSTR("OTD: queue full, dropped frame %08lX\r\n"), frame); }
    synthesizeResponse(buf, value);
  }
  // MI=nnn — Message interval (minimum gap between OT messages, 100-1275ms)
  else if (cmd0 == 'M' && cmd1 == 'I') {
    uint16_t val = atoi(value);
    if (val < 100 || val > 1275) { otDirectBridgeProcessStatus("OR"); return; }
    otMinIntervalMs = val;
    settings.otd.iMsgInterval = val;
    // TASK-440: PIC SetInterval/PrintInterval reports milliseconds, not
    // centiseconds. Internally PIC stores 5ms ticks but echoes ms.
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%u"), val);
    synthesizeResponse(buf, rspBuf);
  }
  // FS=0/1 — Fail safety (controls setback on thermostat disconnect)
  else if (cmd0 == 'F' && cmd1 == 'S') {
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return; }
    otFailSafeEnabled = (value[0] == '1');
    settings.otd.bFailSafe = otFailSafeEnabled;
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // TP=MsgID:Index[=Value] — Thermostat slave parameters (TSP/FHB)
  else if (cmd0 == 'T' && cmd1 == 'P') {
    unsigned int tpMsgId = 0, tpIndex = 0, tpValue = 0;
    bool isWrite = false;
    // Parse "MsgID:Index" or "MsgID:Index=Value"
    char* colonPos = strchr(value, ':');
    if (!colonPos) { otDirectBridgeProcessStatus("SE"); return; }
    tpMsgId = atoi(value);
    char* afterColon = colonPos + 1;
    tpIndex = atoi(afterColon);
    char* eqPos = strchr(afterColon, '=');
    if (eqPos) {
      isWrite = true;
      tpValue = atoi(eqPos + 1);
    }
    // Validate MsgID: 11/13 (main HC), 89/91 (DHW), 106/108 (solar)
    // Odd = TSP r/w, Even+2 = FHB read-only
    if (tpMsgId != 11 && tpMsgId != 13 &&
        tpMsgId != 89 && tpMsgId != 91 &&
        tpMsgId != 106 && tpMsgId != 108) { otDirectBridgeProcessStatus("BV"); return; }
    if (tpIndex > 255 || tpValue > 255) { otDirectBridgeProcessStatus("OR"); return; }
    // FHB entries (13, 91, 108) are read-only
    if (isWrite && (tpMsgId == 13 || tpMsgId == 91 || tpMsgId == 108)) { otDirectBridgeProcessStatus("SE"); return; }
    // Build and queue the frame
    uint16_t tpData = ((uint16_t)tpIndex << 8) | (isWrite ? (uint16_t)tpValue : 0);
    OpenThermMessageType tpType = isWrite ?
      OpenThermMessageType::WRITE_DATA : OpenThermMessageType::READ_DATA;
    unsigned long tpFrame = OpenTherm::buildRequest(
      tpType, static_cast<OpenThermMessageID>(tpMsgId), tpData);
    if (!otCmdEnqueue(tpFrame)) { OTDDebugTf(PSTR("OTD: queue full, dropped frame %08lX\r\n"), tpFrame); }
    if (isWrite) {
      snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%u:%u=%u"), tpMsgId, tpIndex, tpValue);
    } else {
      snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%u:%u"), tpMsgId, tpIndex);
    }
    synthesizeResponse(buf, rspBuf);
  }

  // =====================================================================
  // No-op commands with valid responses (no hardware equivalent on OTGW32)
  // Store values locally for PR= queries but don't change hardware.
  // =====================================================================

  // GA=x / GB=x — GPIO function codes
  else if (cmd0 == 'G' && (cmd1 == 'A' || cmd1 == 'B')) {
    uint8_t idx = (cmd1 == 'A') ? 0 : 1;
    otGpioFunctions[idx] = value[0];
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // LA= through LF= — LED function config
  else if (cmd0 == 'L' && cmd1 >= 'A' && cmd1 <= 'F') {
    uint8_t idx = cmd1 - 'A';
    otLedFunctions[idx] = value[0];
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // VR=x — Voltage reference
  else if (cmd0 == 'V' && cmd1 == 'R') {
    otVoltageRef = atoi(value);
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), otVoltageRef);
    synthesizeResponse(buf, rspBuf);
  }
  // TS=O/R — Temperature sensor function
  else if (cmd0 == 'T' && cmd1 == 'S') {
    otTempSensor = value[0];
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // FT=x — Force thermostat detection
  else if (cmd0 == 'F' && cmd1 == 'T') {
    otForceThermostat = value[0];
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // DP=x — Debug pointer (no-op, just acknowledge)
  else if (cmd0 == 'D' && cmd1 == 'P') {
    synthesizeResponse(buf, value);
  }
  // Unknown command
  else {
    OTDDebugTf(PSTR("OTD: unknown cmd %c%c= (rejected)\r\n"), cmd0, cmd1);
    otDirectBridgeProcessStatus("NG");  // No Good — unknown command code
  }
}

#endif // HAS_DIRECT_OT

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
