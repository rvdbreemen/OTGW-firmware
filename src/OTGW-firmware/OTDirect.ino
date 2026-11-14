/*
***************************************************************************
**  Program  : OTDirect.ino
**  Version  : v2.0.0-beta.234
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
**  TERMS OF USE: GNU GPLv3. See bottom of file.
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
static unsigned long otdRelayedStatusFrame(unsigned long frame);
static uint16_t otdStatusReplyData(uint16_t thermostatData, uint16_t boilerData);
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
static uint8_t otMasterStatusFlags = 0x01;  // bit0=CH enable: board default on; in master mode buildStatusRequest() decides what the boiler sees
// CH= state, mirrors the PIC's CHModeOff: set by CH=0, cleared by CH=1, persists until the next CH=.
// It only reaches bit0 of MsgID 0 while a CS override is active (otdEffectiveMasterFlags).
static bool otChModeOff = false;

// Current operating mode (gateway/monitor/bypass/master)
static OTDirectMode otCurrentMode = OTD_MODE_GATEWAY;

// Inline mode checks — single source of truth is otCurrentMode
#define IS_BYPASS_MODE()   (otCurrentMode == OTD_MODE_BYPASS)
#define IS_MONITOR_MODE()  (otCurrentMode == OTD_MODE_MONITOR)
#define IS_MASTER_MODE()   (otCurrentMode == OTD_MODE_MASTER)
#define IS_LOOPBACK_MODE() (otCurrentMode == OTD_MODE_LOOPBACK)

// Set by the settings path when OTDmode is written (TASK-1702); loopOTDirect() applies it on
// the loop task. The settings POST runs on the async_tcp task, so it only raises this flag.
// The release store publishes settings.otd.iMode, written just before, to the acquiring loop.
static bool otdModeSettingPending = false;

void otdModeSettingChanged() { __atomic_store_n(&otdModeSettingPending, true, __ATOMIC_RELEASE); }
void otdModeSettingCancel()  { __atomic_store_n(&otdModeSettingPending, false, __ATOMIC_RELEASE); }

// otdModeFromSetting: the mode the bus runs for a stored OTDmode value. Bypass needs the
// relay on the board and enabled in the settings; without it the gateway runs Gateway.
static OTDirectMode otdModeFromSetting(int iMode) {
  OTDirectMode mode = static_cast<OTDirectMode>(constrain(iMode, 0, 4));
#if !HAS_BYPASS_RELAY
  if (mode == OTD_MODE_BYPASS) mode = OTD_MODE_GATEWAY;
#else
  if (mode == OTD_MODE_BYPASS && !settings.otd.bHasBypassRelay) {
    OTDDebugTln(F("OT-direct: Bypass relay not enabled in settings, using gateway mode"));
    mode = OTD_MODE_GATEWAY;
  }
#endif
  return mode;
}

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

// TASK-1845: manual DHW push (HW=P) as the PIC 6.8 (gateway.asm: StartDHWPush/PushOperModes 3270-3288, MessageID99
// 3218-3262, readstatus 2720-2747, statusreturn/dhwpushcheck 2753-2797). The flags carry the PIC names. Declared here
// because otdRelayedStatusFrame() and buildStatusRequest() read otPushSpoof before the push functions.
static bool    otPushManual  = false;   // manualdhwpush (operatingmode1 bit 4): a push is requested or running
static bool    otPushPending = false;   // dhwpushpending: WRITE 99 with the push bit sent, no boiler answer yet
static bool    otPushSpoof   = false;   // dhwpushspoof: the boiler refused or lacks the push, the gateway forces DHW enable
static bool    otPushStarted = false;   // dhwpushstarted (spoof path): flame on with DHW mode seen
static uint8_t otPushFails   = 0;       // dhwpushflags bits 0..2: boiler status answers with the flame off before the start

// Phase 10: PS= summary mode (moved here: used before original definition)
static bool otHideReports     = false;      // PS=1: T/B/R/A reports hidden until PS=0 (PIC HideReports)
static bool otSummaryPending  = false;      // PS=1 asked for one summary line (PIC SummaryRequest)

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
  W_ENTRY(71, OT_WRITE_INTERVAL_MS),    // Ventilation setpoint (Vset) — VS= (TASK-1831)

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

// TASK-795 §4.2: real boiler-slave presence on the OT-direct bus. A boiler is
// "present" once it has answered MsgID 3 (Slave Config/MemberID) — otBoilerCache[3]
// valid. Loopback mode synthesises that cache entry, so it is explicitly excluded:
// the availability gate must read a signal that synthetic-online does NOT set,
// otherwise enabling simulation would immediately disable itself. Called cross-file
// from satBoilerHardwarePresent() in SATcontrol.ino.
// TASK-1138: this exclusion is local to the SAT gate. The generic presence flag
// state.otBus.bBoilerState (stamped in processOT, OTGW-Core.ino) deliberately
// DOES count loopback B-frames, so a bench rig in loopback reports
// boiler_connected. The asymmetry is by decision, not drift.
// TASK-1856: the MsgID 3 entry of the cache is filled by real boiler answers and, in loopback,
// by the synthetic ones, and the cache survives a mode switch. A presence that read the entry
// alone took the loopback boiler for a real one right after loopback -> master/gateway, and the
// SATcontrol.ino backstop then switched SAT simulation off for good (re-enable refused until a
// reboot emptied the cache). The marker says who stored MsgID 3 last.
// --- otdBoilerPresence begin
static bool otdMsgId3FromBoiler = false;   // MsgID 3 in the cache came from a real boiler answer
static void otdNoteMsgId3Source(unsigned long response, bool realBoiler) {
  if (((response >> 16) & 0xFF) == 3) otdMsgId3FromBoiler = realBoiler;
}
bool otDirectBoilerPresent() {
  if (IS_LOOPBACK_MODE()) return false;   // synthetic responses are not a real boiler
  return otBoilerCacheValid[3] && otdMsgId3FromBoiler;
}
// --- otdBoilerPresence end

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
// Flush request from resetTransientState(), which can run on the async_tcp task. The consumer on the
// loop task applies it (otCmdApplyPendingFlush), so otCmdTail keeps a single writer.
static uint8_t  otCmdFlushHead = 0;      // otCmdHead when the flush was requested
static bool     otCmdFlushPending = false;

static bool otCmdQueueEmpty() { return otCmdHead == otCmdTail; }
static bool otCmdQueueFull()  { return ((otCmdHead + 1) % OT_CMD_QUEUE_SIZE) == otCmdTail; }

static uint8_t otCmdQueueDepth() {
  return (otCmdHead + OT_CMD_QUEUE_SIZE - otCmdTail) % OT_CMD_QUEUE_SIZE;
}

// The ring is flushed by its consumer, so otCmdTail keeps a single writer; frames queued before a
// mode switch or GW=R never go out under the new mode (ADR-223). The tail moves forward to the
// head snapshot taken at the reset, never past the current head and never backwards: frames
// enqueued after the reset stay, and a consumer that already drained past the snapshot is left alone.
static void otCmdApplyPendingFlush() {
  if (!__atomic_exchange_n(&otCmdFlushPending, false, __ATOMIC_ACQ_REL)) return;
  const uint8_t snap = __atomic_load_n(&otCmdFlushHead, __ATOMIC_RELAXED);
  const uint8_t dSnap = (uint8_t)((snap + OT_CMD_QUEUE_SIZE - otCmdTail) % OT_CMD_QUEUE_SIZE);
  if (dSnap <= otCmdQueueDepth()) otCmdTail = snap;
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
  { 16, false, 0 },   // TrSet — BS= / MQTT room_setpoint override
  { 24, false, 0 },   // Tr — RT= / MQTT room_temp override (TASK-1842)
  { 56, false, 0 },   // TdhwSet — SW= override
  { 57, false, 0 },   // MaxTSet — SH= override
  { 71, false, 0 },   // Vset — VS= override (TASK-1831)
};
static constexpr uint8_t OT_OVERRIDE_COUNT = sizeof(otOverrides) / sizeof(otOverrides[0]);

// TASK-466: PIC-style TT=/TC= remote-override state machine (see OTDirecttypes.h
// for the enum + struct). Single file-static instance: only one TT/TC override
// is active at a time, second command replaces the first. Not persisted: PIC
// resets these on power cycle / heartbeat reset, and so do we.
// TASK-1718: set by clearRemoteOverride(); the next thermostat MsgID 9 read answers READ_ACK 0 once.
static bool otRemoteOverrideClearPending = false;
static OTRemoteOverrideState otRemoteOverride = {
  OT_OVERRIDE_NONE,  // mode
  0,                 // f88Value
  0,                 // setAtMs
  0,                 // lastThermostatMs
  0,                 // lastThermostatVal (only valid when bHaveLastThermostat=true)
  false,             // bHaveLastThermostat — TASK-498 (1A-M2)
  0,                 // mismatchCount
  false,             // bAccepted
  false              // bMsgId9Answered
};

// MsgID 9 / 100 are never an alternative slot in gateway mode: the thermostat always gets READ_ACK
// (0 or the override) from replyToThermostat() via otdRemoteOverrideReply, as on the PIC
// (gateway.asm SendAltResponse :2463-2472, MessageID9/MessageID100 :2932-2938, :3271-3283).
static bool otdRemoteOverrideHoldsSlot(uint8_t msgId) {
  return msgId == 9 || msgId == 100;
}

// ADR-223: tunables for the TT=/TC= release rule (gateway.asm roomsetptdiff). A thermostat
// MsgID 16 within 0.25 C of the override is a match. The value is a multiple of 1/256, so the
// (uint16_t) cast of the float is exact.
static constexpr uint16_t OT_OVERRIDE_HONOR_DELTA_F88   = (uint16_t)(0.25f * 256.0f); // 0x0040
static constexpr uint8_t  OT_OVERRIDE_RELEASE_MISMATCHES = 4;  // mismatching frames that release a not yet accepted override

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
// TASK-1834: the PIC counts 121 Timer-1 overflows of 524.288 ms (gateway.asm:157-158, StartTimer :4233-4239,
// CommandExpiry :1623-1646); a single CS=/C2= lives 120..121 ticks, 62.9..63.4 s by tick phase, a refresh inside a running
// window up to ~126.9 s. OT-Direct keeps 63.5 s from the last command (the SC= window, OT_SC_EXPIRY_MS), never shorter
// than the PIC, so a client refreshing every 60 s never lapses. Checked once a second below.
static constexpr uint32_t OT_CSC2_EXPIRY_MS = 63500;
// TASK-733, TASK-1840: SC= stores the time for the thermostat's MsgID 20 only; nothing goes to
// the boiler (PIC SetClock, gateway.asm:4331-4388). The value is valid until CommandExpiry sets
// InvalidTime (gateway.asm:1623-1646): at least 121 Timer-1 overflows of 524.288 ms = 63.4 s
// after the last SC= (gateway.asm:157-158; tests/thermostat-2.stc: SC= at 2 s still answered at 66 s).
// otSCLastCommandMs == 0 means no valid time.
static uint32_t otSCLastCommandMs = 0;
static constexpr uint32_t OT_SC_EXPIRY_MS = 63500;
static uint16_t otSCClockData = 0;   // MsgID 20 data: HB = day of week << 5 | hours, LB = minutes

// TASK-1840: the gateway's answer to a thermostat MsgID 20 frame, as the PIC's MessageID20 and
// missingclock (gateway.asm:3048-3056, :2694-2706). With a valid SC= time the request becomes the
// matching acknowledgement (messageack, :2676-2679) with that time, whatever the boiler answered.
// Without one, a boiler Data-Invalid or Unknown-DataID becomes Data-Invalid for a READ (messageinv
// changes only the type, :2674-2675) and WRITE_ACK with the thermostat's own data for a WRITE
// (datarestore, :2699-2701); any other boiler answer passes. Other frames are returned unchanged.
// --- otdClockReply begin
static unsigned long otdClockReply(unsigned long thermostatReq, unsigned long reply,
                                   bool clockValid, uint16_t clockData) {
  if (((thermostatReq >> 16) & 0xFF) != 20) return reply;
  const uint8_t reqType = (uint8_t)((thermostatReq >> 28) & 0x07);
  if (reqType != 0 && reqType != 1) return reply;           // READ_DATA, WRITE_DATA
  const uint8_t ackType = (uint8_t)(reqType | 0x04);         // READ_ACK, WRITE_ACK
  if (clockValid) return buildOTResponse(ackType, 20, clockData);
  const uint8_t replyType = (uint8_t)((reply >> 28) & 0x07);
  if (replyType != 6 && replyType != 7) return reply;        // the boiler's own answer
  if (reqType == 0) return buildOTResponse(6, 20, (uint16_t)(reply & 0xFFFF));
  return buildOTResponse(5, 20, (uint16_t)(thermostatReq & 0xFFFF));
}
// --- otdClockReply end

// TASK-1844: answers the PIC always gives the thermostat for MsgID 6, 125 and an unsupported 56/57, whatever the
// boiler answered (gateway.asm 6.8, MsgUnknown = byte1 bit 5 = Data-Invalid or Unknown-DataID, :298):
// - MsgID 6 (remote-parameter flags): READ_ACK, the transfer-enable bits 0 and 1 of the HB set, "the gateway will
//   simulate read access" (MessageID6 :2900-2918).
// - MsgID 125 (OpenTherm version of the slave): READ_ACK 3.0 when the boiler gave none or one below 3.0, 4.51 when it
//   gave 5.0 or higher, else the boiler's version (MessageID125 :3327-3344; tests/otversion-1/2/4.stc AC07D0300,
//   A407D0433).
// - MsgID 56/57 (DHW / max CH setpoint) answered Unknown-DataID: acknowledged with the upper bound (hotwaterreturn
//   :3149-3161, maxchsetptret :3185-3197). A held SW=/SH= is answered by buildOverriddenReply() instead.
// MsgID 2 (master configuration, Smart Power) is not rewritten here: see TASK-1844 notes.
// --- otdPicAnswer begin
static unsigned long otdPicAnswer(unsigned long thermostatReq, unsigned long reply,
                                  uint16_t dhwUpperF88, uint16_t chUpperF88) {
  const uint8_t id = (uint8_t)((thermostatReq >> 16) & 0xFF);
  const uint8_t reqType = (uint8_t)((thermostatReq >> 28) & 0x07);
  if (reqType != 0 && reqType != 1) return reply;          // READ_DATA, WRITE_DATA
  const uint8_t replyType = (uint8_t)((reply >> 28) & 0x07);
  const bool unknown = (replyType & 0x02) != 0;             // MsgUnknown: Data-Invalid or Unknown-DataID
  const uint16_t data = (uint16_t)(reply & 0xFFFF);
  if (id == 6 && reqType == 0) {
    return buildOTResponse(4, 6, (uint16_t)(data | 0x0300));
  }
  if (id == 125 && reqType == 0) {
    const uint8_t major = (uint8_t)(data >> 8);
    uint16_t v = data;
    if (unknown || major < 3) v = 0x0300;
    else if (major >= 5) v = 0x0433;
    return buildOTResponse(4, 125, v);
  }
  if ((id == 56 || id == 57) && replyType == 7) {
    return buildOTResponse((uint8_t)(reqType | 0x04), id, id == 56 ? dhwUpperF88 : chUpperF88);
  }
  return reply;
}
// --- otdPicAnswer end
// The upper bounds otdPicAnswer() uses: the boiler's MsgID 48/49 upper bound (HB) when it gave one, else the PIC
// defaults 80 and 90 (MAX_DHWSETPOINT, MAX_CHSETPOINT, gateway.asm:262-265).
static uint16_t otdUpperBoundF88(uint8_t boundId, uint8_t fallback) {
  const uint8_t hb = otBoilerCacheValid[boundId] ? (uint8_t)(otBoilerCache[boundId] >> 8) : 0;
  return (uint16_t)((hb ? hb : fallback) << 8);
}

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
#define OTD_PIC_COMPAT_VERSION "6.8"   // PIC firmware whose command set OT-Direct implements (PR=A); 6.8 since RT= (TASK-1842) and the MsgID 125 clamp (TASK-1844)
// TASK-1860: the same version for the REST API (device/info "otdpiccompat") without depending on the .ino order.
const char* otdPicCompatVersion() { return OTD_PIC_COMPAT_VERSION; }
static uint8_t otIgnoreTransitions = 1;      // IT= (1=ignore, PIC default)
static uint8_t otOverrideHB      = 0;        // OH= override high byte flag
static char    otGpioFunctions[3] = "00";    // GA=/GB= function codes
static char    otLedFunctions[7]  = "XXXXXX";// LA=-LF= config chars
static uint8_t otVoltageRef      = 5;        // VR= voltage reference digit
static char    otTempSensor      = 'O';      // TS= temp sensor function
static char    otForceThermostat = 'A';      // FT= thermostat detection
static uint8_t otDHWOverride     = 0xFF;     // HW state: 0='0', 1='1', 0xFF='A' (auto)
static bool    otDHWBlocking     = false;    // BW= DHW blocking (bit6, not persisted); read by otdRelayedStatusFrame (ADR-229)
// TASK-1833: CE= and H2= as the PIC's CoolModeOff/CH2ModeOff, clear at reset (gateway.asm 6.8:1178-1188): while a CL= or
// C2= value is held, the relayed MsgID 0 has bit 2 / bit 4 set until CE=0 / H2=0 (readstatus, gateway.asm:2720-2747).
static bool    otCoolingEnable   = true;     // CE= cooling enable (bit2, not persisted); read by otdRelayedStatusFrame (ADR-229)
static bool    otCH2Enable       = true;     // H2= CH2 enable for the relayed status (bit4, not persisted)

// BS= fake room setpoint: intercepts thermostat MsgID 16 WRITE_DATA frames
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

// Phase 8: Unknown ID 3-strike auto-blacklist counters (2 bits per MsgID).
// They cover MsgIDs 0-127, the range otSchedule polls. The helpers ignore
// 128-255, the OT Test & Diagnostic area (spec v4.2 section 5.1), such as
// Remeha 131-133. A reply can still carry one: the thermostat pass-through
// relays them, and nothing checks a reply's MsgID against the request.
static uint8_t otUnknownCounters[32] = {0}; // 128 MsgIDs × 2 bits = 32 bytes

static uint8_t getUnknownCount(uint8_t msgId) {
  uint8_t byteIdx = msgId >> 2;
  if (byteIdx >= sizeof(otUnknownCounters)) return 0;
  uint8_t bitShift = (msgId & 0x03) * 2;
  return (otUnknownCounters[byteIdx] >> bitShift) & 0x03;
}
static void incUnknownCount(uint8_t msgId) {
  uint8_t byteIdx = msgId >> 2;
  if (byteIdx >= sizeof(otUnknownCounters)) return;
  uint8_t bitShift = (msgId & 0x03) * 2;
  uint8_t count = (otUnknownCounters[byteIdx] >> bitShift) & 0x03;
  if (count < 3) {
    otUnknownCounters[byteIdx] &= ~(0x03 << bitShift);
    otUnknownCounters[byteIdx] |= ((count + 1) << bitShift);
  }
}
static void clearUnknownCount(uint8_t msgId) {
  uint8_t byteIdx = msgId >> 2;
  if (byteIdx >= sizeof(otUnknownCounters)) return;
  uint8_t bitShift = (msgId & 0x03) * 2;
  otUnknownCounters[byteIdx] &= ~(0x03 << bitShift);
}

// Apply an active override to a thermostat frame before it goes to the boiler.
// Only a WRITE-DATA is substituted, and only when the override value differs
// from the thermostat's own (the PIC's setbyte3/setbyte4 rule, gateway.asm:2680-2697).
// A READ-DATA passes unchanged, so the thermostat gets the boiler's own answer
// to its read. The one exception is the thermostat's MsgID 0 in gateway mode
// (ADR-229, superseding ADR-208): bits 0, 1, 2, 4, 5 and 6 of its master status
// follow CH=, HW=, CE=, H2=, SM= and BW= by the PIC readstatus rule
// (gateway.asm:2712-2742, otdRelayedStatusFrame). Returns the frame to send;
// modified is true when the data was replaced.
static unsigned long applyOverrides(unsigned long frame, bool &modified) {
  modified = false;
  uint8_t msgType = (frame >> 28) & 0x07;
  uint8_t msgId   = (frame >> 16) & 0xFF;
  uint16_t origData = frame & 0xFFFF;

  if (msgType == 0 /* READ_DATA */ && msgId == 0 && otCurrentMode == OTD_MODE_GATEWAY) {
    unsigned long rewritten = otdRelayedStatusFrame(frame);
    if (rewritten != frame) {
      modified = true;
      OTDDebugTf(PSTR("OTD: status rewrite MsgID=0 orig=0x%04X new=0x%04X\r\n"),
                 origData, (unsigned)(rewritten & 0xFFFF));
    }
    return rewritten;
  }

  // TASK-1831: with VS= held, a thermostat READ of MsgID 71 goes to the boiler as a WRITE of the VS value, as
  // the PIC does (MessageID71 sets MsgWriteOp, gateway.asm 6.8:3210-3217).
  if (msgType == 0 /* READ_DATA */ && msgId == 71) {
    for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) {
      if (otOverrides[i].active && otOverrides[i].msgId == 71) {
        frame = (1UL << 28) | (frame & 0x00FF0000UL) | otOverrides[i].overrideValue;   // WRITE_DATA, MsgID 71
        setOTParityBit(frame);
        modified = true;
        return frame;
      }
    }
  }
  if (msgType != 1 /* WRITE_DATA */) return frame;

  for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) {
    if (otOverrides[i].active && otOverrides[i].msgId == msgId) {
      if (otOverrides[i].overrideValue == origData) break;  // already the override value
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

// TASK-1855: a one-shot WRITE from the command ring that carries the value of a write schedule entry counts as that
// entry's send, so the schedule's refresh follows after its interval instead of repeating the same WRITE at once
// (a new entry has lastSentMs 0 and is due immediately). The PIC sends one frame per command.
static void otdNoteCmdSent(uint32_t frame, uint32_t now) {
  if (((frame >> 28) & 0x07) != 1) return;   // WRITE_DATA only
  const uint8_t id = (uint8_t)((frame >> 16) & 0xFF);
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
    if (otSchedule[i].isWrite && otSchedule[i].msgId == id && otSchedule[i].cachedValue == (uint16_t)(frame & 0xFFFF)) {
      otSchedule[i].lastSentMs = now;
      return;
    }
  }
}

// Async state tracking for non-blocking OT requests
static bool     otMasterRequestActive = false;   // true while waiting for boiler response
static unsigned long otLastSentRequest = 0;      // frame we sent (for bridge logging)
static OTDirectRequestOrigin otLastRequestOrigin = OT_DIRECT_ORIGIN_GATEWAY;
// The thermostat's own frame behind the request in flight, as it arrived before
// applyOverrides(). otSlaveFrame can take the next thermostat frame before the
// boiler answers, so the reply to an overridden frame is built from this copy.
static unsigned long otLastThermostatRequest = 0;
// ADR-223: the gateway loop hands each received thermostat frame to the TT=/TC= release rule once,
// before the alternative-slot, UI= and SR= branches (gateway.asm TreatMessage runs before them),
// although it may take several loop passes before the frame is forwarded.
static bool otSlaveFrameNoted = false;

// TASK-1798: an answer to the thermostat goes out once the slave side is ready. The library refuses a
// slave send for RESPONSE_TIME (50 ms) after a received frame (OpenTherm.cpp:443, sendResponse() returns
// false while isReady() is false), and the gateway makes some answers itself in the same loop pass
// (master mode, UI=, SR=, the alternative and MsgID 28 slots). A refused answer is kept here and sent
// from loopOTDirect() as soon as the gate opens; it is dropped, and logged, when the next thermostat
// frame arrives or after OTD_REPLY_DEADLINE_MS (the slave must answer within 800 ms, OT spec).
static constexpr uint32_t OTD_REPLY_DEADLINE_MS = 750;
static unsigned long otdReplyFrame = 0;
static uint32_t otdReplySinceMs = 0;
static bool otdReplyPending = false;
static OTDirectMode otdReplyMode = OTD_MODE_GATEWAY;   // a mode switch drops a held answer
static bool otdReplyLogA = false;   // log the held answer as A once it leaves (relayed path only)
static void bridgeFrameToParser(char, unsigned long);   // defined below; host tests cut the definition by its signature

static void otdReplyDrop(const char* why) {
  if (!otdReplyPending) return;
  otdReplyPending = false;
  OTDDebugTf(PSTR("OTD: reply to thermostat dropped (%s), MsgID=%u\r\n"), why,
             (unsigned)((otdReplyFrame >> 16) & 0xFF));
}

// Send now, or keep it for otdReplyService(). Returns true when it went out now.
static bool otdSendReply(unsigned long reply) {
  otdReplyDrop("replaced");
  if (otSlave.sendResponse(reply)) return true;
  otdReplyFrame = reply;
  otdReplySinceMs = millis();
  otdReplyMode = otCurrentMode;
  otdReplyLogA = false;
  otdReplyPending = true;
  return false;
}

static void otdReplyService() {
  if (!otdReplyPending) return;
  if (otCurrentMode != otdReplyMode) { otdReplyDrop("mode switch"); return; }
  // the deadline first: after a stalled loop the thermostat no longer waits for this answer
  if ((uint32_t)(millis() - otdReplySinceMs) >= OTD_REPLY_DEADLINE_MS) { otdReplyDrop("too late"); return; }
  if (otSlave.sendResponse(otdReplyFrame)) {
    otdReplyPending = false;
    if (otdReplyLogA) bridgeFrameToParser('A', otdReplyFrame);
  }
}

static void handleSlaveRequest(unsigned long request, OpenThermResponseStatus status) {
  otdReplyDrop("next thermostat frame");   // any received frame, also an invalid one, ends the old request
  if (status == OpenThermResponseStatus::SUCCESS) {
    otSlaveFrame = request;
    otSlaveFramePending = true;
    otSlaveFrameNoted = false;
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
  // Bound dispatched commands per call so a flooded TCP client on port 25238
  // cannot starve the rest of doBackgroundTasks(). Pending bytes drain on the
  // next call. (HTTP now serves on the AsyncTCP task; this bound protects the
  // cooperative loop, not handleClient().)
  static constexpr size_t kMaxLinesPerDrain = 4;
  static char sWrite[kMaxBridgeWrite];
  static size_t bytes_write = 0;
  static bool discardCurrentWriteLine = false;
  static uint32_t droppedWriteLines = 0;
  size_t cmdsProcessed = 0;

  while (OTGWstream.available() && cmdsProcessed < kMaxLinesPerDrain) {
    int inByte = OTGWstream.read();
    if (inByte < 0) break;
    otgwStreamRxNote();   // TASK-1450: LAN witness for the WiFi dead-link check

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
          diagRec.bridgeLine = true;    // TASK-1665: ADR-213 does not record port 25238 bridge traffic
          sendPICSerial(sWrite, static_cast<int>(bytes_write));
          diagRec.bridgeLine = false;
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
static void otdPsNoteFrame(char prefix, unsigned long frame);   // TASK-1800, PS=1 store below
static void bridgeFrameToParser(char prefix, unsigned long frame) {
  otdPsNoteFrame(prefix, frame);
  // TASK-293: always feed the frame into processOT() so state, decoded
  // values, and connected-state flags stay fresh. In PS=1 mode, pass
  // suppressOutput=true so the per-frame raw "otmessage" publish and the
  // auto-leave-PS heuristic are skipped. Previously we early-returned here,
  // which froze the whole ESP32 OT-direct pipeline while PS=1 was active.
  char buf[10];
  snprintf_P(buf, sizeof(buf), PSTR("%c%08lX"), prefix, frame);
  // TASK-1800: while PS=1 is on the PIC prints no T/B/R/A reports (PrintMessage, gateway.asm:1921-1922)
  if (!otHideReports) otDirectBridgeWriteLine(buf, 9);
  // TASK-865.5: OTDirect producer. Enqueue the 9-char frame instead of calling
  // processOT() inline; the consumer (drainOTFrameQueue) parses it in loop()
  // context. otDirectBridgeWriteLine above already does the OTDirect ser2net
  // mirror producer-side, so tag source=OTDIRECT: the consumer must NOT mirror
  // it again to 25238 (TASK-865.6). suppressOutput rides the queue item as
  // otHideReports (PS=1 raw-frame suppression).
  //
  // TASK-1086: a frame this gateway made itself goes out as
  // OTFRAME_SRC_OTDIRECT_LOCAL, and processOT() does not count it as boiler
  // evidence in the unsupported bitmaps. Every 'A' is one: master mode, the
  // UI= and SR= tables and replyToThermostat() answer the thermostat
  // themselves. In loopback mode every 'B' is taken as one
  // (simulateLoopbackResponse). That also tags the few real boiler replies
  // loopback mode sees (the boot probe and handshake in initOTDirect(), a
  // reply still in flight at GW=L): an observation lost, never a fake one
  // counted. The tag is fixed when the frame is made, so a frame still queued
  // across a GW= mode switch keeps it.
  const bool localAnswer = (prefix == 'A') || (prefix == 'B' && IS_LOOPBACK_MODE());
  enqueueOTFrame(buf, 9, otHideReports, localAnswer ? OTFRAME_SRC_OTDIRECT_LOCAL : OTFRAME_SRC_OTDIRECT);
}

// ---------------------------------------------------------------------------
// buildStatusRequest — construct MsgID 0 master request with current flags
// ---------------------------------------------------------------------------
// TASK-1346: true while the CS= override for MsgID 1 is in force. CS=0, the TASK-442
// heartbeat expiry and satDisable() all clear it through clearWriteOverride(1).
static bool otdCsOverrideActive() {
  for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) {
    if (otOverrides[i].msgId == 1) return otOverrides[i].active;
  }
  return false;
}

// TASK-1346: bit0 of the MsgID 0 master status the boiler sees. PIC semantics
// (gateway.asm:53 "CH=1 Override the CH enable bit when CS != 0"; pyotgw/pyotgw.py:602-603):
// CH= takes effect only while a CS override is active; otherwise the board's own bit0 stands.
// --- otdEffectiveMasterFlags begin
static uint8_t otdEffectiveMasterFlags(uint8_t boardFlags, bool chModeOff, bool csOverrideActive) {
  if (!csOverrideActive) return boardFlags;
  return chModeOff ? (uint8_t)(boardFlags & ~0x01) : (uint8_t)(boardFlags | 0x01);
}
// --- otdEffectiveMasterFlags end

// ADR-229 (supersedes ADR-208): the thermostat's relayed MsgID 0 READ_DATA gets bits 0, 1, 2, 4,
// 5 and 6 of its master status (high byte) by the PIC readstatus rule (gateway.asm:2712-2742):
// per bit a mask (the command is in force) and a value. Bit 0 keeps the ADR-208 rule (CH= while a
// CS override is active, otdEffectiveMasterFlags). Bits 3 and 7 and the low byte pass unchanged;
// parity is recomputed only when the frame changed. No command in force: unchanged.
// --- otdRelayedStatus begin
static uint8_t otdRelayedStatusHB(uint8_t hb, const OtdStatusCmd& c) {
  uint8_t mask = 0, val = 0;
  if (c.dhwSwitch)   { mask |= 0x02; if (c.dhwEnable)  val |= 0x02; }
  if (c.coolHeld)    { mask |= 0x04; if (c.coolEnable) val |= 0x04; }
  if (c.ch2Held)     { mask |= 0x10; if (c.ch2Enable)  val |= 0x10; }
  if (c.summer)      { mask |= 0x20; val |= 0x20; }
  if (c.dhwBlocking) { mask |= 0x40; val |= 0x40; }
  hb = (uint8_t)((hb & (uint8_t)~mask) | val);
  return otdEffectiveMasterFlags(hb, c.chModeOff, c.csOverride);
}

static bool otdWriteOverrideActive(uint8_t msgId) {
  for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) {
    if (otOverrides[i].msgId == msgId) return otOverrides[i].active;
  }
  return false;
}

static unsigned long otdRelayedStatusFrame(unsigned long frame) {
  const uint8_t hb = (uint8_t)((frame >> 8) & 0xFF);
  const bool push = otPushSpoof;   // TASK-1845: DHW enable is forced only while the gateway spoofs the push
  OtdStatusCmd c;
  c.csOverride  = otdCsOverrideActive();
  c.chModeOff   = otChModeOff;
  c.dhwSwitch   = (otDHWOverride != 0xFF) || push;
  c.dhwEnable   = (otDHWOverride == 1) || push;
  c.coolHeld    = otdWriteOverrideActive(7);    // CL= (and CC=) write MsgID 7
  c.coolEnable  = otCoolingEnable;
  c.ch2Held     = otdWriteOverrideActive(8);    // C2= writes MsgID 8
  c.ch2Enable   = otCH2Enable;                 // TASK-1833: on until H2=0
  c.summer      = otSummerMode;
  c.dhwBlocking = otDHWBlocking;
  const uint8_t eff = otdRelayedStatusHB(hb, c);
  if (eff == hb) return frame;
  frame = (frame & 0xFFFF00FFUL) | ((unsigned long)eff << 8);
  setOTParityBit(frame);
  return frame;
}

// Reply data for a rewritten MsgID 0: the boiler's slave status (low byte) with the
// thermostat's original master status (high byte), as the PIC statusreturn (gateway.asm:2745-2750).
static uint16_t otdStatusReplyData(uint16_t thermostatData, uint16_t boilerData) {
  return (uint16_t)((thermostatData & 0xFF00) | (boilerData & 0x00FF));
}
// --- otdRelayedStatus end

// TASK-1701, TASK-1707: whether the gateway may send frames of its own between the thermostat's.
// In gateway mode with a thermostat on the bus the boiler gets one frame per thermostat frame, as
// on the PIC (gateway.asm:2384-2414, HandleRequest/CreateRequest): the thermostat's own frame, with
// the ADR-208 bit 0 rule applied to its MsgID 0 in applyOverrides, or a request of the gateway's own
// in an alternative slot (otdAltSlotTaken). The PIC generates frames on its own timer only
// without a thermostat (gateway.asm:1770-1816, StandAloneMsg). A second MsgID 0 with the
// gateway's own CH enable made the boiler follow whichever status came last (tjfs, 2026-11-07).
// --- otdOwnStatusAllowed begin
static bool otdOwnFramesAllowed(bool gatewayMode, bool thermostatConnected) {
  return !(gatewayMode && thermostatConnected);
}
// --- otdOwnStatusAllowed end

static unsigned long buildStatusRequest() {
  // Build a complete READ_DATA request so message type, data-id and parity are correct.
  // MsgID 0: Master status flags in data-value HB (bits 15-8), LB = 0 (slave fills it)
  uint8_t flags = otdEffectiveMasterFlags(otMasterStatusFlags, otChModeOff, otdCsOverrideActive());
  if (otPushSpoof) flags |= 0x02;   // TASK-1845: a spoofed DHW push enables DHW in the gateway's own status
  // Master mode: CH enable (bit 0) stays on while SAT is enabled, also during its hand-back or before
  // its first CS. Without SAT and without a CS override (an external CS= or a thermostat write on the
  // slave interface) nothing sets a flow setpoint, so bit 0 is off. DHW and the other bits are unchanged.
  if (IS_MASTER_MODE() && !otdCsOverrideActive() && !satOwnsControlSetpoint()) flags &= (uint8_t)~0x01;
  return OpenTherm::buildRequest(
    OpenThermMessageType::READ_DATA,
    OpenThermMessageID::Status,
    ((unsigned int)flags << 8)
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
// otdBootHandshake — boot status request and protocol handshake to the boiler (master mode only, TASK-1835)
// Blocking calls are acceptable: it runs from initOTDirect() in setup(), before the cooperative loop.
// ---------------------------------------------------------------------------
static void otdBootHandshake() {
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
}

// ---------------------------------------------------------------------------
// initOTDirect — called from setup() on OTGW32 builds
// ---------------------------------------------------------------------------
static void otdRestorePersistedCommands();   // TASK-1836, defined with the command handlers
void initOTDirect() {
  DebugTln(F("OT-direct: Initializing direct GPIO OpenTherm..."));

  // 1. Restore operating mode from persistent settings
  otCurrentMode = otdModeFromSetting(settings.otd.iMode);
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
    uint16_t ventData = (uint16_t)settings.otd.iVentSetpoint;   // TASK-1831: MsgID 71 LB (spec v4.2 MsgID 71)
    updateWriteCache(71, ventData);
    setOverride(71, ventData);
    OTDDebugTf(PSTR("OT-direct: restored vent setpoint %u%% from settings\r\n"),
               settings.otd.iVentSetpoint);
  }
  otdRestorePersistedCommands();   // TASK-1836: HW, SW, SH, IT, OH and the UI= list

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

  // 6. Set OT_DIRECT mode — unless a PIC was already detected on a combo board
  //    (ADR-127): do NOT clobber HW_MODE_PIC in that case. On the fixed OTGW32
  //    isPICEnabled() is always false, so this is unconditional there.
  //    Bus liveness is tracked via state.otBus.bOnline, not eMode; the
  //    OT-direct loop must keep running so it can retry/recover.
  if (!isPICEnabled()) {
    state.hw.eMode = HW_MODE_OT_DIRECT;
    OTDDebugTln(F("OT-direct: Hardware mode set to OT_DIRECT"));
  } else {
    OTDDebugTln(F("OT-direct: PIC already active — leaving HW_MODE_PIC (combo board)"));
  }

  if (!busPresent) {
    DebugTln(F("OT-direct: WARNING — OT bus not idle, boiler may be disconnected"));
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
      OTDDebugTln(F("OT-direct: No thermostat detected — switching to master (standalone) mode for this boot"));
      applyOTDirectMode(OTD_MODE_MASTER);   // not persisted: the next boot detects again
    } else {
      OTDDebugTln(F("OT-direct: Thermostat present — staying in gateway mode"));
    }
  }

  // 10. TASK-1835 (PIC 6.8 parity audit N5): the boot status request and the protocol handshake go out only when the
  //     gateway is the master, i.e. in master mode, also when the auto-detect above just chose it for this boot. In
  //     gateway, monitor and bypass mode the gateway sends nothing of its own at boot, as the PIC (own frames only
  //     without a thermostat: gateway.asm 6.8 StandAloneMsg :1797-1820, HandleRequest :2388-2407). Gateway mode
  //     without a thermostat gets its own frames from the scheduler (otdOwnFramesAllowed, TASK-1701).
  if (IS_MASTER_MODE()) otdBootHandshake();
}

// ---------------------------------------------------------------------------
// otdActiveOverrideCount — write-override slots in use, plus 1 while a TT=/TC=
// remote override is active (TASK-1749: in gateway mode ADR-223 keeps TT=/TC= out
// of the otOverrides write table, so the count adds it for overrides_active and
// the web UI badge). In master mode TT=/TC= still write MsgID 16, so the write slot
// already shows it and the remote entry is not listed or counted twice.
// ---------------------------------------------------------------------------
static bool otdRemoteOverrideListed() {
  return otRemoteOverride.mode != OT_OVERRIDE_NONE && otCurrentMode != OTD_MODE_MASTER;
}
static uint8_t otdActiveOverrideCount() {
  uint8_t overrides = 0;
  for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) {
    if (otOverrides[i].active) overrides++;
  }
  if (otdRemoteOverrideListed()) overrides++;
  return overrides;
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
  uint8_t overrides = otdActiveOverrideCount();
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
// sendOTDirectOverridesJSON — emit all active overrides as a JSON response.
// ADR-141 revert / TASK-885: streaming JsonEmit replaces the JsonDocument path
// (emits field-by-field directly into the AsyncResponseStream). Called from
// restAPI.ino. Caller must set CORS headers before calling this function.
// ---------------------------------------------------------------------------
void sendOTDirectOverridesJSON() {
  // Bounded JSON into the per-request response stream (TASK-865.9). The caller
  // (restAPI handleOTDirect) has already queued the CORS header via
  // sendCorsOriginHeader() before calling this.
  AsyncResponseStream* strm = restBeginStream("application/json");
  if (strm) {
    JsonEmit je(*strm);
    je.beginObject();                  // root {
    je.beginObject(F("overrides"));    // "overrides":{

    // All four arrays are emitted unconditionally so empty tables still
    // serialize as [] — preserving the exact structure the JsonDocument path
    // (and the original hand-rolled stream before it) emitted.
    je.beginArray(F("write"));
    for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) {
      if (!otOverrides[i].active) continue;
      je.beginObject();
      je.field(F("msgid"), (uint32_t)otOverrides[i].msgId);
      je.field(F("value"), (uint32_t)otOverrides[i].overrideValue);
      je.endObject();
    }
    je.endArray();

    je.beginArray(F("response"));
    for (uint8_t i = 0; i < OT_RESPONSE_OVERRIDE_MAX; i++) {
      if (!otResponseOverrides[i].active) continue;
      je.beginObject();
      je.field(F("msgid"), (uint32_t)otResponseOverrides[i].msgId);
      je.field(F("value"), (uint32_t)otResponseOverrides[i].value);
      je.endObject();
    }
    je.endArray();

    je.beginArray(F("modify"));
    for (uint8_t i = 0; i < OT_RESPONSE_MODIFY_MAX; i++) {
      if (!otResponseModifiers[i].active) continue;
      je.beginObject();
      je.field(F("msgid"), (uint32_t)otResponseModifiers[i].msgId);
      je.field(F("value"), (uint32_t)otResponseModifiers[i].value);
      je.endObject();
    }
    je.endArray();

    je.beginArray(F("unknown"));
    for (uint8_t i = 0; i < otUnknownIdCount; i++) {
      je.value((uint32_t)otUnknownIds[i]);
    }
    je.endArray();

    // TASK-1681: while SAT is enabled the firmware refuses CS=, CH=, MM=, C2=, H2=, TT= and TC= from
    // every other channel (TASK-1695), so the web UI shows why such a write override has no Clear.
    je.field(F("sat_owns"), satOwnsControlSetpoint());

    // TASK-1749: the TT=/TC= remote override (ADR-223 answer state, not a write override) as its
    // own entry; null when none is active. msgid9_answered: the thermostat read MsgID 9 since it
    // was set. accepted: a thermostat MsgID 16 frame matched it. mismatches: frames that did not.
    if (otdRemoteOverrideListed()) {
      je.beginObject(F("remote"));
      je.field(F("mode"), otRemoteOverride.mode == OT_OVERRIDE_TEMPORARY ? F("temporary") : F("constant"));
      je.field(F("value"), (float)((int16_t)otRemoteOverride.f88Value / 256.0f), (uint8_t)2);
      je.field(F("msgid9_answered"), otRemoteOverride.bMsgId9Answered);
      je.field(F("accepted"), otRemoteOverride.bAccepted);
      je.field(F("mismatches"), (uint32_t)otRemoteOverride.mismatchCount);
      je.endObject();
    } else {
      je.fieldRaw(F("remote"), "null");
    }

    je.endObject();                    // close "overrides"

    // TASK-498 (4B-M2): expose queue depth + high-water-mark so non-telnet
    // clients (web UI, MQTT-driven dashboards) can monitor the OT command
    // queue load. "queue" is a sibling of "overrides", not nested under it.
    // Normal operation should keep highWater well below capacity; climbing
    // values indicate a producer-rate problem worth investigation.
    je.beginObject(F("queue"));        // "queue":{
    je.field(F("depth"),     (uint32_t)otCmdQueueDepth());
    je.field(F("highWater"), (uint32_t)otCmdQueueHighWater);
    je.field(F("capacity"),  (uint32_t)OT_CMD_QUEUE_SIZE);
    je.endObject();                    // close "queue"

    je.endObject();                    // close root
  }
  restFinalize();
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

  // WRITE_DATA (type 1) — accept the write, echo data back as WRITE_ACK.
  // Answered before the table lookup: a write never reads otLoopbackData, so it
  // stays valid across the whole 0-255 id range.
  if (reqType == 1) {
    uint16_t writeData = request & 0xFFFF;
    return buildOTResponse(5, msgId, writeData);  // type 5 = WRITE_ACK
  }

  // TASK-1072: msgId spans 0-255 but the table holds 128 entries. Reading past
  // the end is a valid flash access on ESP32 so it does not fault; it silently
  // fabricates a value from whatever PROGMEM follows and returns it as a
  // READ_ACK. That range is the OEM/vendor area, which includes the Remeha ids
  // 131-133 that TASK-1068 made decodable, so an unguarded lookup can feed the
  // decoder invented Remeha data. An id the table does not cover is simply one
  // this simulated boiler does not implement, which is what UNKNOWN_DATA_ID says.
  constexpr uint8_t kLoopbackTableEntries =
      (uint8_t)(sizeof(otLoopbackData) / sizeof(otLoopbackData[0]));
  if (msgId >= kLoopbackTableEntries) {
    return buildOTResponse(7, msgId, 0);  // type 7 = UNKNOWN_DATA_ID
  }

  // READ_DATA (type 0) — look up simulated value
  uint16_t data = pgm_read_word(&otLoopbackData[msgId]);
  if (data == 0xFFFF) {
    return buildOTResponse(7, msgId, 0);  // type 7 = UNKNOWN_DATA_ID
  }
  return buildOTResponse(4, msgId, data);   // type 4 = READ_ACK
}

// ---------------------------------------------------------------------------
// buildOverriddenReply: the reply to a thermostat WRITE-DATA whose data an
// override replaced. As the PIC builds it (gateway.asm:2456-2472, CreateMessage
// :3464-3471): the acknowledgement that matches the request type, with the
// thermostat's own Data-ID and data, whatever the boiler answered. MsgID 56/57
// (SW=/SH=) have one rule of their own, whatever the frame's parity bit: a
// boiler WRITE-ACK returns the value the boiler echoed, a boiler UNKNOWN-DATAID
// the override value, and a DATA-INVALID the thermostat's own value. The PIC
// gives these answers for a frame with parity bit 0 (gateway.asm:3128-3147,
// :3164-3182). For parity bit 1 its setbyte1 (:2672-2677) marks the frame
// rewritten, so a boiler WRITE-ACK gets WRITE-ACK(SW/SH) and a DATA-INVALID
// gets WRITE-ACK with the MsgID 48/49 upper bound (:2416-2418, :2463-2473).
// ---------------------------------------------------------------------------
static unsigned long buildOverriddenReply(unsigned long thermostatReq, unsigned long sentReq,
                                          unsigned long boilerResp) {
  const uint8_t msgId = (thermostatReq >> 16) & 0xFF;
  const uint8_t type  = ((thermostatReq >> 28) & 0x07) | 0x04;   // request type + response bit: WRITE_DATA -> WRITE_ACK
  uint16_t data = thermostatReq & 0xFFFF;
  if (msgId == 0) {
    // ADR-208: a rewritten MsgID 0 READ_DATA. A boiler READ_ACK gives the slave status;
    // any other boiler answer goes back to the thermostat as it came.
    if (((boilerResp >> 28) & 0x07) != 4) return boilerResp;
    return buildOTResponse(type, msgId, otdStatusReplyData(data, (uint16_t)(boilerResp & 0xFFFF)));
  }
  if (msgId == 56 || msgId == 57) {
    const uint8_t boilerType = (boilerResp >> 28) & 0x07;
    if (boilerType == 5) data = boilerResp & 0xFFFF;         // WRITE_ACK: the value the boiler took
    else if (boilerType == 7) data = sentReq & 0xFFFF;      // UNKNOWN_DATA_ID: the override value
  }
  return buildOTResponse(type, msgId, data);
}

// TASK-1718: the thermostat READs MsgID 9 (TrOverride, remote override room setpoint) and
// MsgID 100 (remote override function flags) to learn about a TT=/TC= override (OT spec v4.2
// 5.3.9/5.3.10: MsgID 9 value != 0 = override active, 0 = none). Parity with the PIC
// (gateway.asm MessageID9 / MessageID100): an active override answers READ_ACK with the override
// setpoint / the flags in the low byte; the first MsgID 9 read after a clear answers READ_ACK 0
// once; a boiler Data-Invalid / Unknown-DataID on either id becomes READ_ACK 0; a boiler
// READ_ACK passes through. Any other frame is returned unchanged.
// --- otdRemoteOverrideReply begin
static unsigned long otdRemoteOverrideReply(unsigned long boilerResp, bool overrideActive,
                                            uint16_t f88Value, uint16_t flags, bool &clearPending) {
  const uint8_t msgId = (uint8_t)((boilerResp >> 16) & 0xFF);
  if (msgId != 9 && msgId != 100) return boilerResp;
  const uint8_t type = (uint8_t)((boilerResp >> 28) & 0x07);
  if (type != 4 && type != 6 && type != 7) return boilerResp;   // READ_ACK, DATA_INVALID, UNKNOWN_DATA_ID
  uint16_t data = 0;
  bool own = false;
  if (overrideActive) {
    data = (msgId == 9) ? f88Value : (uint16_t)(flags & 0x00FF);
    own = true;
  } else if (msgId == 9 && clearPending) {
    clearPending = false;
    own = true;
  } else if (type != 4) {
    own = true;
  }
  if (!own) return boilerResp;
  unsigned long reply = (4UL << 28) | ((unsigned long)msgId << 16) | data;
  setOTParityBit(reply);
  return reply;
}
// --- otdRemoteOverrideReply end

// ---------------------------------------------------------------------------
// replyToThermostat: answer a forwarded thermostat frame once the boiler has
// replied (OT spec v4.2 section 4.3.2: the gateway forwards the next slave's
// answer to the master, or creates its own). A gateway request gets no reply.
// An overridden frame gets buildOverriddenReply(); an unchanged frame gets the
// boiler's reply, with the RM= response modifiers applied in gateway mode.
// ---------------------------------------------------------------------------
static void replyToThermostat(OTDirectRequestOrigin origin, unsigned long sentReq,
                              unsigned long boilerResp) {
  if (origin == OT_DIRECT_ORIGIN_GATEWAY) return;
  unsigned long reply = boilerResp;
  bool answersOverrideRead = false;
  if (origin == OT_DIRECT_ORIGIN_THERMOSTAT_OVERRIDDEN) {
    reply = buildOverriddenReply(otLastThermostatRequest, sentReq, boilerResp);
  } else if (otCurrentMode == OTD_MODE_GATEWAY) {
    reply = applyResponseModifiers(boilerResp);
    reply = otdRemoteOverrideReply(reply, otRemoteOverride.mode != OT_OVERRIDE_NONE,
                                   otRemoteOverride.f88Value,
                                   otRemoteOverride.mode == OT_OVERRIDE_TEMPORARY ? 0x0002 : 0x0001,
                                   otRemoteOverrideClearPending);
    // TASK-1840: MsgID 20 is answered from the SC= time (sentReq is the thermostat's own frame here)
    reply = otdClockReply(sentReq, reply, otSCLastCommandMs != 0, otSCClockData);
    // TASK-1844: MsgID 6, 125 and an unsupported 56/57 get the PIC's own answer
    reply = otdPicAnswer(sentReq, reply, otdUpperBoundF88(48, 80), otdUpperBoundF88(49, 90));
    // ADR-223: the release rule counts thermostat MsgID 16 frames only after this answer.
    if (otRemoteOverride.mode != OT_OVERRIDE_NONE && ((reply >> 16) & 0xFF) == 9
        && ((reply >> 28) & 0x07) == 4) {
      otRemoteOverride.bMsgId9Answered = true;
    }
  }
  const bool sentNow = otdSendReply(reply);
  if (!sentNow) {
    OTDDebugTf(PSTR("OTD: reply to thermostat held, slave not ready yet, MsgID=%u\r\n"),
               (unsigned)((reply >> 16) & 0xFF));
  }
  // A reply that differs from the boiler's frame is the gateway's own answer,
  // logged as A so the parser keeps the boiler's B as the canonical value (ADR-096/103).
  // A held reply is logged when it leaves (otdReplyService); a dropped one never is.
  if (reply != boilerResp) {
    OTDDebugTf(PSTR("OTD: reply MsgID=%u boiler=0x%04X thermostat=0x%04X\r\n"),
               (unsigned)((reply >> 16) & 0xFF), (unsigned)(boilerResp & 0xFFFF),
               (unsigned)(reply & 0xFFFF));
    if (sentNow) bridgeFrameToParser('A', reply);
    else otdReplyLogA = true;
  }
}

// ---------------------------------------------------------------------------
// bridgeSentRequest: log a request as it goes to the boiler. An overridden
// thermostat frame logs as T (what the thermostat sent) followed by R (what
// the boiler gets), the pair the parser reads as a gateway substitution (ADR-096).
// ---------------------------------------------------------------------------
static void bridgeSentRequest(unsigned long request, OTDirectRequestOrigin origin) {
  if (origin == OT_DIRECT_ORIGIN_THERMOSTAT_OVERRIDDEN) bridgeFrameToParser('T', otLastThermostatRequest);
  bridgeFrameToParser((origin == OT_DIRECT_ORIGIN_THERMOSTAT) ? 'T' : 'R', request);
}

// ---------------------------------------------------------------------------
// otBoilerCacheStore: keep a boiler reply for the master-mode slave handler and
// the other cache readers. The cache holds the OT spec's data-ids 0-127. A reply
// for an OEM data-id 128-255 is not stored: masking it with 0x7F would put it
// in another id's slot (MsgID 131 would land on MsgID 3). An UNKNOWN-DATA-ID
// reply carries no value, only "not supported", so it clears the id's slot
// instead of filling it with the reply's data bytes (a reader would take those
// as an outside temperature of 0.0 C).
// ---------------------------------------------------------------------------
static void otBoilerCacheStore(unsigned long response) {
  const uint8_t id = (response >> 16) & 0xFF;
  if (id >= sizeof(otBoilerCacheValid)) return;
  if (OpenTherm::getMessageType(response) == OpenThermMessageType::UNKNOWN_DATA_ID) {
    otBoilerCacheValid[id] = false;
    return;
  }
  otBoilerCache[id] = response & 0xFFFF;
  otBoilerCacheValid[id] = true;
}

// ---------------------------------------------------------------------------
// sendMasterRequestAsync — initiate an async OT request (non-blocking)
// ---------------------------------------------------------------------------
static bool sendMasterRequestAsync(unsigned long request, OTDirectRequestOrigin origin) {
  // Loopback mode: simulate response immediately, no bus activity
  if (IS_LOOPBACK_MODE()) {
    bridgeSentRequest(request, origin);
    unsigned long response = simulateLoopbackResponse(request);
    bridgeFrameToParser('B', response);
    state.otBus.bOnline = true;

    // Cache response for master mode slave handler (reuses same cache)
    otBoilerCacheStore(response);
    otdNoteMsgId3Source(response, false);   // TASK-1856: synthetic, never boiler presence

    // A forwarded thermostat frame is answered by the same rule as a real boiler reply
    replyToThermostat(origin, request, response);
    return true;  // "completed" instantly
  }

  // TASK-795 §4.1 paths (b)+(c): block the real boiler-side master TX when SAT
  // boiler sim (or legacy OTGW sim) is active. This single emitter is shared by
  // the gateway-origin path (sendMasterRequestAsync callers ~1357/1409) and the
  // thermostat pass-through/translation path (~1897/1937), so one gate covers
  // both — no separate path-(c) emitter exists. Loopback mode already returned
  // above (its synthetic responses never reach the bus). The shared helper also
  // captures the would-be frame for the §4.3 trace.
  {
    char otCmd[24];
    const uint8_t  mid = (uint8_t)((request >> 16) & 0xFF);
    const uint16_t val = (uint16_t)(request & 0xFFFF);
    snprintf_P(otCmd, sizeof(otCmd), PSTR("MID=%u VAL=%u"), mid, val);
    if (satSimulationBlocksBusTx(otCmd, F("otdirect-tx"))) return false;
  }

  if (!otMaster.isReady()) return false;  // bus busy — try again later
  otLastSentRequest = request;
  otLastRequestOrigin = origin;
  otMasterRequestActive = otMaster.sendRequestAsync(request);
  if (otMasterRequestActive) {
    bridgeSentRequest(request, origin);
    otLastAnySendMs = millis();  // MI= gap tracking: covers thermostat-forward and gateway paths
  }
  return otMasterRequestActive;
}

// ---------------------------------------------------------------------------
// handleMasterResponse — process a completed async master request
// ---------------------------------------------------------------------------
static void otdRetNoteReply(OTDirectRequestOrigin origin, unsigned long request, unsigned long response,
                            uint32_t now);   // TASK-1759, otdRetSlot block below
static void handleMasterResponse() {
  unsigned long response = otMaster.getLastResponse();
  OpenThermResponseStatus status = otMaster.getLastResponseStatus();

  // The library reports a boiler DATA-INVALID as INVALID, the status it also
  // gives a corrupt or partial frame (OpenTherm.cpp:444, :450-453). Even parity,
  // the DATA-INVALID type and the Data-ID of our request mark the genuine reply,
  // which is a legal slave answer (OT spec v4.2 sections 4.4.1 and 4.4.2).
  const bool boilerDataInvalid =
      status == OpenThermResponseStatus::INVALID &&
      !OpenTherm::parity(response) &&
      OpenTherm::getMessageType(response) == OpenThermMessageType::DATA_INVALID &&
      ((response >> 16) & 0xFF) == ((otLastSentRequest >> 16) & 0xFF);

  if (status == OpenThermResponseStatus::SUCCESS) {
    bridgeFrameToParser('B', response);
    if (!state.otBus.bOnline) {
      // Transition: was offline, now online
      OTDDebugTln(F("OTD: boiler responded — OT bus back online"));
      sendWebSocketJSON("{\"type\":\"status\",\"msg\":\"OTDirect: connected\"}");
    }
    state.otBus.bOnline = true;

    // Cache boiler response data for master mode slave responses
    otBoilerCacheStore(response);
    otdNoteMsgId3Source(response, true);    // TASK-1856: a real boiler answered
    otdRetNoteReply(otLastRequestOrigin, otLastSentRequest, response, millis());   // TASK-1759

    // TASK-795 §4.2: a real boiler answered on the OT-direct bus. If SAT
    // simulation is active, trip the edge hook (deferred auto-disable). Not in
    // loopback — loopback responses are synthesised in sendMasterRequestAsync
    // and never reach handleMasterResponse.
    satNotifyBoilerFrameSeen();

    // TASK-184: update flame ratio state from MsgID 0 slave status byte
    // Flame bit = bit 3 of slave status LB (bit 3 of response byte 0)
    if (((response >> 16) & 0xFF) == 0 && otBoilerCacheValid[0]) {
      bool flameOn = (otBoilerCache[0] & 0x08) != 0;  // bit 3 of LB = flame active
      flameRatioSet(flameOn);
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

    // TASK-1845: the DHW push follows the boiler's answers, as the PIC (MessageID99, statusreturn).
    if (respMsgId == 99) otdPushOnBoiler99((uint8_t)respType, (uint16_t)(response & 0xFFFF));
    if (respMsgId == 0 && respType == OpenThermMessageType::READ_ACK) otdPushOnBoilerStatus((uint8_t)(response & 0xFF));

    // Log per-frame: MsgID, data value, override applied
    {
      uint8_t logMsgId = (response >> 16) & 0xFF;
      uint16_t logData = response & 0xFFFF;
      OTDDebugTf(PSTR("OTD: resp MsgID=%u data=0x%04X origin=%u\r\n"),
                 logMsgId, logData, (uint8_t)otLastRequestOrigin);
    }
  } else if (boilerDataInvalid) {
    // Logged like any boiler reply. Not cached: its data is not a boiler value.
    bridgeFrameToParser('B', response);
    // TASK-1907: the PIC handles a Data-Invalid answer to MsgID 99 as Unknown-DataID (MessageID99: the Data-Invalid
    // entry is a nop that falls into ID99Unknown, gateway.asm:3227-3230), so a pending HW=P push is spoofed.
    if (((response >> 16) & 0xFF) == 99) otdPushOnBoiler99(6 /* DATA_INVALID */, (uint16_t)(response & 0xFFFF));
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

  // A forwarded thermostat frame gets its one reply now. A timeout or a corrupt
  // reply gets none: the thermostat notes the incomplete conversation and
  // retries (OT spec v4.2 section 4.5), as it does behind a PIC gateway.
  if (status == OpenThermResponseStatus::SUCCESS || boilerDataInvalid) {
    replyToThermostat(otLastRequestOrigin, otLastSentRequest, response);
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

  // TASK-1707: with a thermostat in gateway mode the queue and the schedule go out only in
  // alternative slots (otdAltSlotTaken), never between the thermostat's frames.
  if (!otdOwnFramesAllowed(otCurrentMode == OTD_MODE_GATEWAY, state.otd.bThermostatConnected)) return;

  uint32_t now = millis();

  // MI= global minimum inter-message gap
  if ((now - otLastAnySendMs) < otMinIntervalMs) return;

  // Pending commands from the ring buffer take priority.
  // Peek first — only dequeue after successful async send (Codex P1 fix).
  otCmdApplyPendingFlush();
  if (!otCmdQueueEmpty()) {
    uint32_t cmdFrame = otCmdQueue[otCmdTail];
    if (sendMasterRequestAsync(cmdFrame, OT_DIRECT_ORIGIN_GATEWAY)) {
      otdNoteCmdSent(cmdFrame, now);   // TASK-1855: the schedule refresh waits its interval
      otCmdTail = (otCmdTail + 1) % OT_CMD_QUEUE_SIZE;  // consume on success
      otCmdApplyPendingFlush();  // a reset during the send flushes now; a stale snapshot is rejected
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
// TASK-1707: alternative slots in gateway mode, as on the PIC. With a thermostat
// on the bus the PIC sends the boiler one frame per thermostat frame. When the
// thermostat asks a Data-ID the boiler does not support (three consecutive
// Unknown-DataID answers, HandleResponse gateway.asm:2437-2449; UI= marks the same
// map) the PIC sends a request of its own in that slot (CreateRequest/SendAltRequest
// :2396-2414, Alternative :3390-3461) and answers the thermostat Unknown-DataID
// (SendAltResponse :2464-2473, CreateMessage :3464-3471). Here the thermostat gets
// that answer at once and the gateway's request goes to the boiler in the same slot. Data-IDs 128-255 are never marked
// unsupported (UnknownMask :3491-3498; getUnknownCount() covers 0-127).
// ---------------------------------------------------------------------------
// --- otdAltSlot begin
static bool otdResponseOverrideActive(uint8_t msgId) {
  for (uint8_t i = 0; i < OT_RESPONSE_OVERRIDE_MAX; i++) {
    if (otResponseOverrides[i].active && otResponseOverrides[i].msgId == msgId) return true;
  }
  return false;
}

// A thermostat Data-ID whose slot the gateway may use: one the UI= table marks
// unknown, or one the boiler answered Unknown-DataID three times in a row and the
// SR= table does not answer. MsgID 0 is never counted (handleMasterResponse).
// MsgID 9 and 100 are no slot (otdRemoteOverrideHoldsSlot): the thermostat gets READ_ACK
// from replyToThermostat() (otdRemoteOverrideReply, TASK-1718), not Unknown-DataID.
static bool otdRemoteOverrideHoldsSlot(uint8_t msgId);
static bool otdIsAltSlot(uint8_t msgId) {
  if (otdRemoteOverrideHoldsSlot(msgId)) return false;
  if (isUnknownId(msgId)) return true;
  return getUnknownCount(msgId) >= 3 && !otdResponseOverrideActive(msgId);
}

// The schedule entry for an alternative slot: of the entries that may go out, the
// one furthest past its interval (largest age / interval), so a slot is used while
// any entry qualifies. MsgID 0 stays out: the boiler's status request is the
// thermostat's. Returns its index, OT_SCHEDULE_SIZE when no entry qualifies.
// Schedule write entries whose Data-ID the thermostat writes itself (TSet 1, 16, 24 ...).
// Such an entry never goes out in a slot: the thermostat's own write reaches the boiler,
// with an override applied in applyOverrides(), so a slot write would be a second master
// writing the same value (the TASK-1701 defect for MsgID 1). Cleared when the thermostat drops.
static bool otdThermostatWritesEntry[OT_SCHEDULE_SIZE];

static uint8_t otdAltScheduleIndex(uint32_t now) {
  uint8_t best = OT_SCHEDULE_SIZE;
  uint64_t bestAge = 0, bestInterval = 1;
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
    const OTScheduleEntry &e = otSchedule[i];
    if (e.msgId == 0 || e.disabled || (e.isWrite && (!e.valueSet || otdThermostatWritesEntry[i]))) continue;
    uint32_t interval = e.intervalMs;
    if ((e.msgId == 70 || e.msgId == 71) && otIsVentSlave()) interval = OT_VENT_FAST_INTERVAL_MS;
    const uint64_t age = (uint32_t)(now - e.lastSentMs);
    if (best == OT_SCHEDULE_SIZE || age * bestInterval > bestAge * interval) {
      best = i; bestAge = age; bestInterval = interval;
    }
  }
  return best;
}

// Uses the slot of a pending thermostat frame for the gateway's own request: the
// oldest queued command first, as scheduleMasterRequest() sends them, then the
// schedule. The thermostat gets Unknown-DataID; a sent command leaves the queue and
// a sent schedule entry restarts its timer, a refused send leaves both for the next
// slot. Returns false, with nothing answered or sent, when the frame is no slot,
// nothing waits or the master bus is not ready: the caller then handles the frame
// as before.
static void otdNoteThermostatFrame(unsigned long frame, uint32_t now);
static bool otdAltSlotTaken(unsigned long thermostatFrame) {
  if (otCurrentMode != OTD_MODE_GATEWAY) return false;
  if (!otdIsAltSlot((uint8_t)((thermostatFrame >> 16) & 0xFF))) return false;
  if (!otMaster.isReady()) return false;
  const uint32_t now = millis();
  otdNoteThermostatFrame(thermostatFrame, now);
  uint8_t idx = OT_SCHEDULE_SIZE;   // OT_SCHEDULE_SIZE: the queued command
  unsigned long request;
  otCmdApplyPendingFlush();
  if (!otCmdQueueEmpty()) {
    request = otCmdQueue[otCmdTail];
  } else {
    idx = otdAltScheduleIndex(now);
    if (idx == OT_SCHEDULE_SIZE) return false;
    const OTScheduleEntry &e = otSchedule[idx];
    request = OpenTherm::buildRequest(
      e.isWrite ? OpenThermMessageType::WRITE_DATA : OpenThermMessageType::READ_DATA,
      static_cast<OpenThermMessageID>(e.msgId),
      e.isWrite ? e.cachedValue : 0);
  }
  // Unknown-DataID with the thermostat's own Data-ID and data bytes, as CreateMessage builds it
  unsigned long unknownResp = buildOTResponse(7, (uint8_t)((thermostatFrame >> 16) & 0xFF),
                                              (uint16_t)(thermostatFrame & 0xFFFF));
  // TASK-1840: a blacklisted MsgID 20 still gets the PIC's clock answer (TreatMessage runs MessageID20
  // on the fake Unknown-DataID, gateway.asm:2467-2477, :3048-3056)
  unknownResp = otdClockReply(thermostatFrame, unknownResp, otSCLastCommandMs != 0, otSCClockData);
  unknownResp = otdPicAnswer(thermostatFrame, unknownResp, otdUpperBoundF88(48, 80), otdUpperBoundF88(49, 90));   // TASK-1844
  bridgeFrameToParser('T', thermostatFrame);
  bridgeFrameToParser('A', unknownResp);
  otdSendReply(unknownResp);
  if (sendMasterRequestAsync(request, OT_DIRECT_ORIGIN_GATEWAY)) {
    if (idx < OT_SCHEDULE_SIZE) otSchedule[idx].lastSentMs = now;
    else {
      otdNoteCmdSent((uint32_t)request, now);   // TASK-1855: the schedule refresh waits its interval
      otCmdTail = (otCmdTail + 1) % OT_CMD_QUEUE_SIZE;
      otCmdApplyPendingFlush();  // a reset during the send flushes now; a stale snapshot is rejected
    }
  }
  return true;
}

// A thermostat READ-DATA refreshes that value as a gateway read does, so the
// alternative slots go to values the thermostat does not read. A thermostat WRITE-DATA
// marks the write entry of that Data-ID as the thermostat's (otdThermostatWritesEntry).
static void otdNoteThermostatFrame(unsigned long frame, uint32_t now) {
  const uint8_t type  = (frame >> 28) & 0x07;
  const uint8_t msgId = (frame >> 16) & 0xFF;
  if (type != 0 && type != 1) return;   // READ_DATA, WRITE_DATA
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
    if (otSchedule[i].msgId != msgId) continue;
    if (type == 0 && !otSchedule[i].isWrite) otSchedule[i].lastSentMs = now;
    if (type == 1 && otSchedule[i].isWrite) otdThermostatWritesEntry[i] = true;
  }
}
static void otdForgetThermostatWrites() {
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) otdThermostatWritesEntry[i] = false;
}
// ---------------------------------------------------------------------------
// TASK-1759, ADR-224 (Sergeant D entries 287 and 291; Robert entry 290): the return temperature
// (MsgID 28) for SAT when the thermostat never reads it and offers no unsupported slot (tjfs,
// 2026-11-09: IDs 0, 1, 16, 17, 24, 25, all supported). Every 30 s, while SAT is on and no valid
// MsgID 28 arrived for 30 s, the gateway takes the slot of one supported thermostat READ (the
// "candidate"), sends READ_DATA MsgID 28 to the boiler there, and answers the thermostat READ_ACK
// with the boiler's cached value for the ID it asked. The boiler still gets one frame per
// thermostat frame (TASK-1707). A deliberate deviation from the PIC model (ADR-224, ADR-223).
// The candidate is the first eligible thermostat READ answered READ_ACK by the boiler, unmodified;
// its stamp is the cache age (one stamp, ADR-224's 8 B variant). A candidate not answered for
// longer than the interval is replaced by the next eligible read.
// --- otdRetSlot begin
static constexpr uint32_t OTD_RET_SLOT_INTERVAL_MS = 30000UL;   // entry 291: every 30 s, while no valid MsgID 28 for 30 s
static uint8_t  otdRetCandidateId = 0;   // 0 = none yet
static uint32_t otdRetCandidateMs = 0;   // millis() of the candidate's last unmodified boiler READ_ACK
static uint32_t otdRetLastSlotMs  = 0;   // millis() of the last replaced frame, 0 = never

// A thermostat Data-ID whose read may be answered from the cache: never MsgID 0 (status, CH enable),
// never 9/100 (remote override, ADR-223), never an ID the UI=/SR= tables answer, never an
// unsupported (TASK-1707) slot, never MsgID 28 itself, and only the OT spec's 1-127.
static bool otdRetEligibleId(uint8_t id) {
  if (id == 0 || id == 28 || id >= 128) return false;
  if (otdRemoteOverrideHoldsSlot(id)) return false;
  if (isUnknownId(id) || otdResponseOverrideActive(id)) return false;
  return !otdIsAltSlot(id);
}

// handleMasterResponse() only (never the loopback path): an unmodified thermostat READ answered READ_ACK
// stamps the candidate. Any other reply for the candidate ID (Data-Invalid, a WRITE_ACK echo, Unknown-DataID),
// from any origin, clears the stamp: otBoilerCacheStore() keeps that reply's value, so a stamp from an
// earlier READ_ACK would otherwise let otdRetSlotTaken() answer the thermostat with it (audit 1795 area E).
static void otdRetNoteReply(OTDirectRequestOrigin origin, unsigned long request, unsigned long response,
                            uint32_t now) {
  if (otdRetCandidateId != 0 && (uint8_t)((response >> 16) & 0xFF) == otdRetCandidateId &&
      OpenTherm::getMessageType(response) != OpenThermMessageType::READ_ACK) {
    otdRetCandidateMs = 0;
    return;
  }
  if (origin != OT_DIRECT_ORIGIN_THERMOSTAT) return;
  if (OpenTherm::getMessageType(request) != OpenThermMessageType::READ_DATA) return;
  if (OpenTherm::getMessageType(response) != OpenThermMessageType::READ_ACK) return;
  const uint8_t id = (uint8_t)((request >> 16) & 0xFF);
  if ((uint8_t)((response >> 16) & 0xFF) != id || !otdRetEligibleId(id)) return;
  if (otdRetCandidateId == 0 || (uint32_t)(now - otdRetCandidateMs) > OTD_RET_SLOT_INTERVAL_MS) otdRetCandidateId = id;
  if (id == otdRetCandidateId) otdRetCandidateMs = now ? now : 1;
}

// Gateway mode with a thermostat, SAT on (the same "on" as the PM=25 gate, SATcontrol.ino),
// MsgID 28 supported, not fresh, and at least one interval since the last replaced frame.
static bool otdRetWanted(uint32_t now) {
  if (otCurrentMode != OTD_MODE_GATEWAY || !state.otd.bThermostatConnected) return false;
  if (!(satIsEnabled() && state.sat.bActive && !settings.sat.bSimulation)) return false;
  if (isBoilerMsgIdUnsupportedRead(28)) return false;
  if (satFeedFresh(state.otBus.feedTret, now, OTD_RET_SLOT_INTERVAL_MS)) return false;
  return otdRetLastSlotMs == 0 || (uint32_t)(now - otdRetLastSlotMs) >= OTD_RET_SLOT_INTERVAL_MS;
}

// Takes the slot of a pending thermostat frame for MsgID 28 when all of the above hold, the frame is
// a READ of the candidate with a cached answer younger than the interval, and no override applies
// to it. Returns false, with nothing sent or answered, otherwise: the caller relays the frame.
static bool otdRetSlotTaken(unsigned long thermostatFrame) {
  const uint32_t now = millis();
  const uint8_t id = (uint8_t)((thermostatFrame >> 16) & 0xFF);
  if (((thermostatFrame >> 28) & 0x07) != 0) return false;            // READ_DATA only, never a write
  if (id != otdRetCandidateId || !otdRetEligibleId(id)) return false;
  if (!otdRetWanted(now)) return false;
  if (!otBoilerCacheValid[id] || (uint32_t)(now - otdRetCandidateMs) >= OTD_RET_SLOT_INTERVAL_MS) return false;
  bool modified = false;
  (void)applyOverrides(thermostatFrame, modified);
  if (modified) return false;                                          // an override applies: relay it as usual
  if (!otMaster.isReady()) return false;
  const unsigned long reply = applyResponseModifiers(buildOTResponse(4 /* READ_ACK */, id, otBoilerCache[id]));
  bridgeFrameToParser('T', thermostatFrame);
  bridgeFrameToParser('A', reply);
  otdSendReply(reply);
  otdRetLastSlotMs = now ? now : 1;
  const unsigned long request = OpenTherm::buildRequest(OpenThermMessageType::READ_DATA,
                                                        static_cast<OpenThermMessageID>(28), 0);
  if (sendMasterRequestAsync(request, OT_DIRECT_ORIGIN_GATEWAY)) {
    for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
      if (otSchedule[i].msgId == 28 && !otSchedule[i].isWrite) otSchedule[i].lastSentMs = now;
    }
  }
  OTDDebugTf(PSTR("OTD: MsgID 28 read in the slot of thermostat MsgID %u (answered from cache 0x%04X)\r\n"),
             (unsigned)id, (unsigned)otBoilerCache[id]);
  return true;
}
// --- otdRetSlot end
// --- otdAltSlot end

// ---------------------------------------------------------------------------
// PS=1 summary (TASK-1800): the PIC's value store and print routines, so the line is byte for byte the
// PIC's (Robert 2026-11-11: identical to the PIC's answers).
// Store (StoreValue, gateway.asm:3574-3609, table :3530-3572): the raw data word of 34 Data-IDs, taken
// from a thermostat WRITE_DATA request (HandleRequest :2384-2385, before any override), from the
// gateway's own WRITE_DATA without a thermostat (StandAloneMsg :1806-1808) and from a boiler READ_ACK
// (HandleResponse :2434-2436). Data-Invalid, Unknown-DataID and Data-IDs >= 128 are not stored.
// Print (StoreProcTable :1978-2012): one field per Data-ID in this order, separated by ','.
// ---------------------------------------------------------------------------
enum : uint8_t { PS_STATUS, PS_FLOAT, PS_BYTES, PS_SIGNED, PS_SIGSHORT, PS_LOWBYTE, PS_SHORT };
static const uint8_t PROGMEM otdPsIds[] = { 0, 1, 6, 7, 8, 14, 15, 16, 17, 18, 19, 23, 24, 25, 26, 27, 28,
                                            31, 33, 48, 49, 56, 57, 70, 71, 77,
                                            116, 117, 118, 119, 120, 121, 122, 123 };
static const uint8_t PROGMEM otdPsKinds[] = {
  PS_STATUS, PS_FLOAT, PS_STATUS, PS_FLOAT, PS_FLOAT, PS_FLOAT, PS_BYTES, PS_FLOAT, PS_FLOAT, PS_FLOAT,
  PS_FLOAT, PS_FLOAT, PS_FLOAT, PS_FLOAT, PS_FLOAT, PS_SIGNED, PS_FLOAT, PS_FLOAT, PS_SIGSHORT, PS_BYTES,
  PS_BYTES, PS_FLOAT, PS_FLOAT, PS_STATUS, PS_LOWBYTE, PS_LOWBYTE,
  PS_SHORT, PS_SHORT, PS_SHORT, PS_SHORT, PS_SHORT, PS_SHORT, PS_SHORT, PS_SHORT };
static constexpr uint8_t OTD_PS_FIELDS = sizeof(otdPsIds);   // SUMMARYFIELDS = 34 (:3572)
static_assert(sizeof(otdPsKinds) == sizeof(otdPsIds), "one print kind per summary Data-ID");
static uint16_t otdPsValues[OTD_PS_FIELDS];   // zero at boot, as the PIC's cleared RAM

static void otdPsNoteFrame(char prefix, unsigned long frame) {
  const uint8_t type = (uint8_t)((frame >> 28) & 0x07);
  const bool keep = (prefix == 'T' && type == 1) || (prefix == 'B' && type == 4) ||
                    (prefix == 'R' && type == 1 && !state.otd.bThermostatConnected);
  if (!keep) return;
  const uint8_t id = (uint8_t)((frame >> 16) & 0xFF);
  for (uint8_t i = 0; i < OTD_PS_FIELDS; i++) {
    if (pgm_read_byte(&otdPsIds[i]) == id) { otdPsValues[i] = (uint16_t)(frame & 0xFFFF); return; }
  }
}

// TASK-1846: OT= stores the outside temperature in the summary slot of MsgID 27, as the PIC's SetOutsideT ->
// StoreOutTemp (gateway.asm 6.8 :4256-4287, outsideval1 = valuestorage+30 :568-569), with or without a thermostat.
// OT=- only clears the PIC's OutsideTemp flag, so the slot keeps its last value; a boiler READ_ACK still overwrites it.
static void otdPsStoreOutside(uint16_t f88) {
  for (uint8_t i = 0; i < OTD_PS_FIELDS; i++) {
    if (pgm_read_byte(&otdPsIds[i]) == 27) { otdPsValues[i] = f88; return; }
  }
}

// Append one field in the PIC's format. Returns the new length.
static int otdPsAppend(char* line, int pos, int size, uint8_t kind, uint16_t v) {
  char* out = line + pos;
  const int room = size - pos;
  int n = 0;
  switch (kind) {
    case PS_STATUS:                                   // PrintStatus: 8 bits '/' 8 bits
      for (int b = 15; b >= 0; b--) {
        if (b == 7) out[n++] = '/';
        out[n++] = ((v >> b) & 1) ? '1' : '0';
      }
      out[n] = '\0';
      break;
    case PS_BYTES:                                    // PrintBytes: HB '/' LB
      n = snprintf_P(out, room, PSTR("%u/%u"), (unsigned)(v >> 8), (unsigned)(v & 0xFF));
      break;
    case PS_LOWBYTE:                                  // PrintLowByte
      n = snprintf_P(out, room, PSTR("%u"), (unsigned)(v & 0xFF));
      break;
    case PS_SIGSHORT:                                 // PrintSigShort: '-' and the magnitude
    case PS_SHORT:                                    // PrintShort: unsigned 16 bit
      if (kind == PS_SIGSHORT && (v & 0x8000)) { out[n++] = '-'; v = (uint16_t)(0u - v); }
      n += snprintf_P(out + n, room - n, PSTR("%u"), (unsigned)v);
      break;
    default: {                                        // PS_FLOAT, PS_SIGNED: PrintFloat (:2047-2059)
      if (kind == PS_SIGNED && (v & 0x8000)) { out[n++] = '-'; v = (uint16_t)(0u - v); }   // PrintNegative
      uint8_t hi = (uint8_t)(v >> 8), lo = (uint8_t)(v & 0xFF);
      if (lo == 0xFF) { hi = (uint8_t)(hi + 1); lo = 0; }   // .996 rounds up to the next integer
      const uint16_t prod = (uint16_t)lo * 100u;             // Multiply: the high byte is the fraction
      uint8_t frac = (uint8_t)(prod >> 8);
      if (prod & 0x80) frac++;                               // rounding
      n += snprintf_P(out + n, room - n, PSTR("%u.%02u"), (unsigned)hi, (unsigned)frac);
      break;
    }
  }
  return pos + n;
}

static void emitSummaryLine() {
  static char line[420];
  int pos = 0;
  for (uint8_t i = 0; i < OTD_PS_FIELDS; i++) {
    if (i) line[pos++] = ',';
    pos = otdPsAppend(line, pos, (int)sizeof(line), pgm_read_byte(&otdPsKinds[i]), otdPsValues[i]);
  }
  line[pos] = '\0';
  otDirectBridgeWriteLine(line, (size_t)pos);   // port 25238, as the PIC's serial line
  processOT(line, pos);
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
      // (first incomplete minute)
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
  // Apply a pending command-ring flush every pass, also in modes where no consumer runs.
  otCmdApplyPendingFlush();

  // TASK-1702: a saved OTDmode takes effect at once. Compared with the running mode, not
  // the stored one, so choosing Gateway after the boot auto-detect picked Master switches
  // back. Waits while a master request is in flight.
  if (!otMasterRequestActive && __atomic_exchange_n(&otdModeSettingPending, false, __ATOMIC_ACQ_REL)) {
    OTDirectMode wanted = otdModeFromSetting(settings.otd.iMode);
    if (wanted != otCurrentMode) applyOTDirectMode(wanted);
  }

  // Process OpenTherm library state machines (drives ISR-based TX/RX)
  otMaster.process();
  otSlave.process();
  otdReplyService();   // TASK-1798: a held answer leaves once the slave gate opens

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
      if (!otSlaveFrameNoted) {
        otSlaveFrameNoted = true;
        if (slaveMsgId == 16) onThermostatMsgID16((uint8_t)((otSlaveFrame >> 28) & 0x07), (uint16_t)(otSlaveFrame & 0xFFFF));
      }

      // TASK-1707: the slot of a Data-ID the boiler does not support carries the gateway's own request
      if (otdAltSlotTaken(otSlaveFrame)) {
        otSlaveFramePending = false;
      }
      // TASK-1759, ADR-224: MsgID 28 in the slot of a supported read, answered from the cache
      else if (otdRetSlotTaken(otSlaveFrame)) {
        otSlaveFramePending = false;
      }
      // UI table: respond UNKNOWN_DATAID directly, don't forward to boiler
      else if (isUnknownId(slaveMsgId) && !otdRemoteOverrideHoldsSlot(slaveMsgId)) {
        unsigned long unknownResp = (otSlaveFrame & 0x00FF0000UL) | (7UL << 28);  // type=UNKNOWN_DATAID
        setOTParityBit(unknownResp);
        // TASK-1840: UI=20 still gets the PIC's clock answer (MessageID20 on the fake Unknown-DataID)
        unknownResp = otdClockReply(otSlaveFrame, unknownResp, otSCLastCommandMs != 0, otSCClockData);
        unknownResp = otdPicAnswer(otSlaveFrame, unknownResp, otdUpperBoundF88(48, 80), otdUpperBoundF88(49, 90));   // TASK-1844
        bridgeFrameToParser('T', otSlaveFrame);
        bridgeFrameToParser('A', unknownResp);
        otdSendReply(unknownResp);
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
            otdSendReply(srResp);
            otSlaveFramePending = false;
            srHandled = true;
            break;
          }
        }
        // Normal path: apply value overrides and forward to boiler. The
        // thermostat is answered when the boiler replies (replyToThermostat).
        if (!srHandled) {
          bool modified = false;
          unsigned long frameToSend = applyOverrides(otSlaveFrame, modified);
          otLastThermostatRequest = otSlaveFrame;
          if (sendMasterRequestAsync(frameToSend,
                modified ? OT_DIRECT_ORIGIN_THERMOSTAT_OVERRIDDEN : OT_DIRECT_ORIGIN_THERMOSTAT)) {
            otSlaveFramePending = false;
            if (otCurrentMode == OTD_MODE_GATEWAY) otdNoteThermostatFrame(otSlaveFrame, millis());
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
    // TASK-1150: the heartbeat guards against an external actor that set CS= and then
    // vanished. SAT is not that actor: it refreshes on change or every
    // SAT_CMD_REFRESH_MS (300 s), five times this window, so expiring its override
    // handed TSet back to the thermostat for the remainder of every quiet stretch —
    // exactly during the ADR-150 cold cutoff and while DHW is active, when SAT
    // deliberately holds a stable setpoint. While SAT owns the setpoint the override
    // stays; SAT itself clears it on satDisable().
    if (otCSLastCommandMs != 0 && !satOwnsControlSetpoint() &&
        (uint32_t)(nowExp - otCSLastCommandMs) > OT_CSC2_EXPIRY_MS) {
      OTDDebugTln(F("OTD: CS heartbeat expired, clearing MsgID 1 override"));
      clearWriteOverride(1);
      otCSLastCommandMs = 0;
    }
    if (otC2LastCommandMs != 0 && (uint32_t)(nowExp - otC2LastCommandMs) > OT_CSC2_EXPIRY_MS) {
      OTDDebugTln(F("OTD: C2 heartbeat expired, clearing MsgID 8 override"));
      clearWriteOverride(8);
      otC2LastCommandMs = 0;
    }
    // TASK-733, TASK-1840: the SC= time becomes invalid (PIC CommandExpiry sets InvalidTime)
    if (otSCLastCommandMs != 0 && (uint32_t)(nowExp - otSCLastCommandMs) > OT_SC_EXPIRY_MS) {
      OTDDebugTln(F("OTD: SC time expired, MsgID 20 answers follow the boiler again"));
      otSCLastCommandMs = 0;
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
// TASK-1593: set when enqueueWriteCommand() could not enqueue (ring full). handleOTDirectCommand() clears it on
// entry and reports it to its caller, so a refused write is not mistaken for a delivered one.
static bool otdWriteRefused = false;

static bool enqueueWriteCommand(uint8_t msgId, uint16_t dataValue, const char* label) {
  unsigned long frame = OpenTherm::buildRequest(
    OpenThermMessageType::WRITE_DATA,
    static_cast<OpenThermMessageID>(msgId),
    dataValue
  );
  if (!otCmdEnqueue(frame)) {
    DebugTf(PSTR("OT-direct: %s command dropped (queue full)\r\n"), label);
    otdWriteRefused = true;
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
static uint8_t otOperModeDHW   = 0;         // MW= MsgID 99 HB bits 0..3
static uint8_t otOperModeCH1   = 0;         // MH= MsgID 99 LB bits 0..3
static uint8_t otOperModeCH2   = 0;         // M2= MsgID 99 LB bits 4..7
// TASK-1832: MsgID 99 data as the spec (v4.2 MsgID 99) and the PIC (OperModeData, gateway.asm 6.8:3247-3250) lay it out:
// HB = DHW operating mode (bits 0..3) + manual DHW push (bit 4), LB = HC1 (bits 0..3) + HC2 (bits 4..7).
static uint16_t otdOperModeData(bool push) {
  return (uint16_t)(((otOperModeDHW & 0x0F) | (push ? 0x10 : 0)) << 8) | (uint16_t)((otOperModeCH1 & 0x0F) | ((otOperModeCH2 & 0x0F) << 4));
}

// TASK-1845: the manual DHW push, as the PIC 6.8 (see the flags at the top of the file).
// --- otdPush begin
static void otdPushEnd() {   // dhwpushdone / abort: clrf dhwpushflags, bcf manualdhwpush (gateway.asm:2795-2796, 4398-4399)
  otPushManual = otPushPending = otPushSpoof = otPushStarted = false;
  otPushFails = 0;
}
// HW=P: StartDHWPush (gateway.asm:3270-3288). A push in progress is left alone. The HW= state is not touched. When the
// boiler is known not to support MsgID 99 the gateway spoofs the push at once, else it writes MsgID 99 with the push bit.
static void otdPushStart() {
  if (otPushManual) return;
  otPushManual = true;
  if (isUnknownId(99)) { otPushSpoof = true; return; }
  otPushPending = true;
  unsigned long frame99 = OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA,
                                                  static_cast<OpenThermMessageID>(99), otdOperModeData(true));
  if (!otCmdEnqueue(frame99)) { OTDDebugTf(PSTR("OTD: queue full, dropped frame %08lX\r\n"), frame99); }
}
// The boiler's answer to MsgID 99 (MessageID99, gateway.asm:3218-3262): Unknown-DataID or Data-Invalid (TASK-1907: the
// Data-Invalid entry falls through into ID99Unknown, :3227-3230) during a push, or a WRITE_ACK
// without the push bit to the push request, makes the gateway spoof it. Otherwise the push bit follows the boiler, so
// a push the boiler ran ends when it clears the bit.
static void otdPushOnBoiler99(uint8_t type, uint16_t data) {
  const bool boilerPush = (data & 0x1000) != 0;
  if (type == 7 /* UNKNOWN_DATA_ID */ || type == 6 /* DATA_INVALID */) { if (otPushManual) otPushSpoof = true; return; }
  if (type == 5 /* WRITE_ACK */ && otPushPending && !boilerPush) { otPushSpoof = true; otPushPending = false; return; }
  if (type != 4 /* READ_ACK */ && type != 5 /* WRITE_ACK */) return;
  // A READ_ACK while the push WRITE is still on its way answers an earlier READ 99 (the gateway's poll or the
  // thermostat's); the PIC turns such reads into writes while opermodewrite is set (ID99ReadData, gateway.asm:3253-3257).
  if (type == 4 && otPushPending) return;
  otPushPending = false;
  if (otPushSpoof) return;                       // the gateway runs this push (ID99ReadAck keeps it)
  if (otPushManual && !boilerPush) otdPushEnd(); // the boiler finished its push
  else if (boilerPush) otPushManual = true;
}
// A boiler MsgID 0 READ_ACK while the gateway spoofs the push (statusreturn -> dhwpushcheck, gateway.asm:2753-2797):
// before the start, flame on with DHW mode starts it and 8 answers with the flame off end it; once started it ends when
// the boiler leaves DHW mode. No wall-clock timeout.
static void otdPushOnBoilerStatus(uint8_t slaveLB) {
  if (!otPushSpoof) return;
  if (!otPushStarted) {
    if (slaveLB & 0x08) { if (slaveLB & 0x04) otPushStarted = true; return; }
    if (++otPushFails >= 8) otdPushEnd();
    return;
  }
  if (!(slaveLB & 0x04)) otdPushEnd();
}
// --- otdPush end

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
// TT=/TC= remote-override state machine (TASK-466, ADR-223)
// ---------------------------------------------------------------------------
// Gateway mode: TT=/TC= is answer state only, like the PIC (gateway.asm SetSetPoint).
// The thermostat's READ of MsgID 9 is answered with the setpoint and its READ of
// MsgID 100 with the flags (otdRemoteOverrideReply). Nothing is written to the
// boiler: the thermostat's own WRITE 16 passes unchanged.
// Master mode (no thermostat): TT=/TC= writes TrSet (MsgID 16) and the flags
// (MsgID 100) to the boiler through the write queue.
//
// setRemoteOverride / clearRemoteOverride deliberately do NOT touch the
// TASK-442 CS/C2 expiry timestamps (otCSLastCommandMs / otC2LastCommandMs).
// TT/TC and CS/C2 are independent state machines.
// ---------------------------------------------------------------------------
static void setRemoteOverride(OTRemoteOverrideMode mode, float celsius) {
  // TASK-497 (4A-M1): cast through the canonical floatToF88 helper which
  // includes the -40..127 clamp formerly inlined here (TASK-495).
  uint16_t f88 = floatToF88(celsius);

  if (otCurrentMode == OTD_MODE_MASTER) {
    // TASK-500 (1A-M1): "queue is the channel, no side-channels". Enqueue the
    // primary frame (MsgID 16, TrSet) FIRST. If the queue is full, leave
    // otRemoteOverride.* and state.otd.* untouched.
    if (!enqueueWriteCommand(16, f88, mode == OT_OVERRIDE_TEMPORARY ? "TT" : "TC")) {
      OTDDebugTln(F("OTD: OVR-set MsgID 16 dropped (queue full); state unchanged"));
      return;
    }
    // MsgID 100: low byte = priority bits. If MsgID 16 made it through but
    // MsgID 100 did not, the state is still committed.
    uint16_t flags = (mode == OT_OVERRIDE_TEMPORARY) ? 0x0002 : 0x0001;
    if (!enqueueWriteCommand(100, flags, "OVR-flags")) {
      OTDDebugTln(F("OTD: OVR-flags MsgID 100 dropped (queue full); MsgID 16 in flight"));
    }
  }

  otRemoteOverride.mode               = mode;
  otRemoteOverride.f88Value           = f88;
  otRemoteOverride.setAtMs            = millis();
  otRemoteOverride.lastThermostatVal  = 0;       // value undefined until bHaveLastThermostat flips
  otRemoteOverride.bHaveLastThermostat = false;  // TASK-498 (1A-M2): reset history
  otRemoteOverride.mismatchCount      = 0;
  otRemoteOverride.bAccepted          = false;
  otRemoteOverride.bMsgId9Answered    = false;
  otRemoteOverrideClearPending        = false;
  state.otd.eOverrideMode             = mode;
  state.otd.iOverrideF88              = f88;
}

static void clearRemoteOverride() {
  otRemoteOverride.mode                = OT_OVERRIDE_NONE;
  otRemoteOverride.f88Value            = 0;
  otRemoteOverride.lastThermostatVal   = 0;
  otRemoteOverride.bHaveLastThermostat = false;  // TASK-498 (1A-M2)
  otRemoteOverride.mismatchCount       = 0;
  otRemoteOverride.bAccepted           = false;
  otRemoteOverride.bMsgId9Answered     = false;
  state.otd.eOverrideMode              = OT_OVERRIDE_NONE;
  state.otd.iOverrideF88               = 0;
  otRemoteOverrideClearPending         = true;   // TASK-1718

  // Gateway mode: answer state only. A BS= or MQTT room_setpoint override on MsgID 16 stays.
  if (otCurrentMode != OTD_MODE_MASTER) return;

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
// applyOverrides). Release rule of the PIC (gateway.asm roomsetptdiff),
// the same for TT and TC:
//   - frames count only after the gateway answered a MsgID 9 read with the override;
//   - a frame within OT_OVERRIDE_HONOR_DELTA_F88 of the override is a match and
//     marks the override accepted;
//   - before acceptance the OT_OVERRIDE_RELEASE_MISMATCHES-th mismatch releases it,
//     after acceptance the first mismatch does.
// A thermostat that never reads MsgID 9 never releases the override.
static void onThermostatMsgID16(uint8_t msgType, uint16_t f88) {
  // OT v4.2: thermostat-originated MsgID 16 frames are WRITE_DATA (type 1)
  // carrying TrSet. Other types (READ_DATA queries) carry no setpoint. Filter strictly.
  if (msgType != 1 /* WRITE_DATA */) return;

  uint32_t now = millis();
  otRemoteOverride.lastThermostatMs = now;
  otRemoteOverride.lastThermostatVal   = f88;
  otRemoteOverride.bHaveLastThermostat = true;

  if (otRemoteOverride.mode == OT_OVERRIDE_NONE) return;
  if (!otRemoteOverride.bMsgId9Answered) return;

  // f8.8 is two's-complement signed: sign-extend through int16_t before widening.
  int32_t signedDelta = (int32_t)(int16_t)f88 - (int32_t)(int16_t)otRemoteOverride.f88Value;
  uint32_t delta = (signedDelta < 0) ? (uint32_t)(-signedDelta) : (uint32_t)signedDelta;

  if (delta < OT_OVERRIDE_HONOR_DELTA_F88) {
    otRemoteOverride.bAccepted = true;
    return;
  }
  if (otRemoteOverride.bAccepted || ++otRemoteOverride.mismatchCount >= OT_OVERRIDE_RELEASE_MISMATCHES) {
    OTDDebugTln(F("OTD: TT/TC released (thermostat setpoint differs)"));
    clearRemoteOverride();
  }
}

// ---------------------------------------------------------------------------
// resetTransientState — clear all transient state (called on GW=R and mode change)
// Mirrors PIC hardware reset clearing all RAM, except persisted settings.
// ---------------------------------------------------------------------------
static void resetTransientState() {
  // Flush the command ring: a frame queued under the previous mode (a WRITE 16/100 from TT= in
  // master mode) must not reach the boiler after the switch (ADR-223). The consumer applies the
  // flush; frames queued after this point are kept. otCmdQueueHighWater stays: it is a diagnostic
  // peak, not queue content.
  __atomic_store_n(&otCmdFlushHead, otCmdHead, __ATOMIC_RELAXED);
  __atomic_store_n(&otCmdFlushPending, true, __ATOMIC_RELEASE);
  // Operating mode overrides
  otOperModeDHW = 0;
  otOperModeCH1 = 0;
  otOperModeCH2 = 0;
  // DHW push (TASK-1845)
  otdPushEnd();
  // PS=1 is not touched here: a GW= mode switch leaves HideReports alone on the PIC (gateway.asm:4690-4733);
  // only PS=0 and GW=R clear it (TASK-1800).
  // Thermostat connectivity tracking — reset to boot state
  otThermostatSeen = false;
  otLastThermostatMs = 0;
  // Force all schedule entries to poll immediately on next cycle
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
    otSchedule[i].lastSentMs = 0;
  }
  // Status bit overrides (non-persisted) — reset to CH enable only (boot default)
  otDHWBlocking = false;
  otCoolingEnable = true;      // TASK-1833: PIC CoolModeOff/CH2ModeOff clear at reset
  otCH2Enable = true;
  otMasterStatusFlags = 0x01;  // CH enable only; summer mode re-applied below if persisted
  otChModeOff = false;         // PIC boots with CHModeOff clear
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
  otRemoteOverrideClearPending         = false;
  otRemoteOverride.f88Value            = 0;
  otRemoteOverride.mismatchCount       = 0;
  otRemoteOverride.bAccepted           = false;
  otRemoteOverride.bMsgId9Answered     = false;
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
  // BS= fake room setpoint — clear on reset
  otFakeRoomSetpoint = 0.0f;
}

// ---------------------------------------------------------------------------
// applyOTDirectMode — switch operating mode with hardware reconfiguration, in RAM only.
// The boot auto-detect uses it directly, so the mode it picks is decided again on every
// boot (TASK-1694). setOTDirectMode() below also persists the mode for user changes.
// ---------------------------------------------------------------------------
static void applyOTDirectMode(OTDirectMode newMode) {
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

}

// setOTDirectMode — a mode change chosen by the user (command, REST): apply it and persist it
// via the public updateSetting() API, which handles the dirty flag and the debounce timer.
static void setOTDirectMode(OTDirectMode newMode) {
  // TASK-1839 (PIC 6.8 parity audit N7): a GW= for the mode already running changes nothing, as on the PIC
  // (SetGatewayMode only switches the comparators and saves the mode: gateway.asm 6.8 :4729-4747, :4749-4774).
  // Before, every GW= reset the transient state, so a client sending PS=0;GW=1 lost CS, HW, MM, SR= and UI=.
  // The setting is still written: after a boot auto-detect the running mode and the stored one can differ.
  if (newMode != otCurrentMode) applyOTDirectMode(newMode);
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
      otdForgetThermostatWrites();   // TASK-1707: standalone writes its own values again
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
      clearWriteOverride(1);      // Release the TSet override and its cached write (TASK-1707)
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
    // READ_DATA: respond with the cached boiler value. The cache holds data-ids
    // 0-127 only (otBoilerCacheStore), so an OEM id 128-255 is always unknown.
    if (msgId < sizeof(otBoilerCacheValid) && otBoilerCacheValid[msgId]) {
      response = buildOTResponse(4, msgId, otBoilerCache[msgId]);  // 4 = READ_ACK
    } else {
      response = buildOTResponse(7, msgId, 0);  // 7 = UNKNOWN_DATAID (no cached data yet)
    }
  }
  else if (msgType == 1) {
    // WRITE_DATA — accept and echo back as WRITE_ACK
    uint16_t dataVal = frame & 0xFFFF;
    response = buildOTResponse(5, msgId, dataVal);  // 5 = WRITE_ACK
    // Apply thermostat's TSet as our write cache so scheduler sends it to boiler.
    // TASK-1150: not while SAT owns the control setpoint — a thermostat writing MsgID 1
    // in master mode would otherwise silently replace SAT's setpoint. The WRITE_ACK is
    // still echoed above so the thermostat sees a well-formed exchange. A write of MsgID 1 > 0
    // sets the CS override, which enables CH in buildStatusRequest(); a later write of 0 is ignored.
    if (msgId == 1 && dataVal != 0 && !satOwnsControlSetpoint()) {
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
  otdSendReply(response);
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

// TASK-1842: the PIC's GetPercentArg (gateway.asm GetPosFloat :5114-5117 + GetFloat :5118-5185, GetPercent
// :5099-5112) as an f8.8 value. An integer part of 0-255 digits, an optional '.' and digits: the first two
// fraction digits are kept, the third rounds (:5138-5159), further digits are checked but ignored; the hundredths
// become 1/256 steps rounded UP when not exact (:5169-5178, "the honeywell thermostat always rounds down").
// No sign other than '+'; above 100.00 is OR. Returns nullptr on success, else the PIC error code.
// --- otdPicPercentF88 begin
static const char* otdPicPercentF88(const char* s, uint16_t &out) {
  if (*s == '+') s++;
  if (*s < '0' || *s > '9') return "SE";
  unsigned ip = 0;
  while (*s >= '0' && *s <= '9') { ip = ip * 10 + (unsigned)(*s - '0'); if (ip > 255) return "OR"; s++; }
  unsigned h = 0;
  if (*s == '.') {
    s++;
    for (uint8_t n = 0; *s; n++, s++) {
      if (*s < '0' || *s > '9') return "SE";
      if (n < 2) h = h * 10 + (unsigned)(*s - '0');
      else if (n == 2 && *s >= '5') h++;
    }
  } else if (*s) {
    return "SE";
  }
  if (h >= 100) { ip++; h -= 100; }                  // Divide100 carry (:5170-5172)
  if (ip > 100 || (ip == 100 && h > 0)) return "OR";  // GetPercent: above 100.00
  const unsigned frac = (h * 256u + 99u) / 100u;     // round up when not exact
  out = (uint16_t)((ip << 8) | frac);
  return nullptr;
}
// --- otdPicPercentF88 end

// ---------------------------------------------------------------------------
// handleOTDirectCommand — translate PIC-style commands to OT frames
// Called from sendPICSerial() when HAS_DIRECT_OT is enabled.
//
// Full PIC command emulation: all commands that the PIC firmware supports
// are handled here — either translated to OpenTherm frames, applied to
// local state, or acknowledged with a valid response.
// ---------------------------------------------------------------------------
// TASK-1836: the commands whose state the PIC keeps in EEPROM (SaveConfig: HW gateway.asm:4408,
// SW/SH :4187-4188 and :4214-4215, IT/OH :4959-4965, UI/KI :4900-4905, SM, SB, MI, FS) write it to
// settings.otd and mark the settings dirty; the deferred flush writes settings.ini, and
// otdRestorePersistedCommands() applies it again at boot (the PIC restores it at reset, :1205-1240,
// :1298-1324). A value that did not change marks nothing.
// --- otdPersist begin
static void otdPersistBool(bool &field, bool v) {
  if (field != v) { field = v; settingsMarkDirty(); }
}
static void otdPersistFloat(float &field, float v) {
  if (field != v) { field = v; settingsMarkDirty(); }
}
static void otdPersistU16(uint16_t &field, uint16_t v) {
  if (field != v) { field = v; settingsMarkDirty(); }
}
// HW=0, 1 or A (any other character). HW=P is a one-off push and leaves the stored state (gateway.asm:4410-4412).
static void otdPersistHW(char v) {
  const uint8_t s = (v == '0') ? 0 : (v == '1') ? 1 : 0xFF;
  if (settings.otd.iDhwOverride != s) { settings.otd.iDhwOverride = s; settingsMarkDirty(); }
}
// The UI= list as "5 6 70"; 16 Data-IDs of at most 3 digits fit sUnknownIds[64].
static void otdPersistUnknownIds() {
  char list[sizeof(settings.otd.sUnknownIds)];
  size_t n = 0;
  list[0] = '\0';
  for (uint8_t i = 0; i < otUnknownIdCount; i++) {
    const int w = snprintf(list + n, sizeof(list) - n, n ? " %u" : "%u", (unsigned)otUnknownIds[i]);
    if (w < 0 || (size_t)w >= sizeof(list) - n) break;
    n += (size_t)w;
  }
  if (strcmp(list, settings.otd.sUnknownIds) != 0) {
    strlcpy(settings.otd.sUnknownIds, list, sizeof(settings.otd.sUnknownIds));
    settingsMarkDirty();
  }
}
// --- otdPersist end

// TASK-1836: apply the stored command state at boot, as the PIC does after a reset. SW/SH are
// re-sent to the boiler (gateway.asm:1218-1229) through the same write path as the commands;
// HW=A keeps the boot default; the UI= list marks its Data-IDs unknown again and disables their
// schedule entries, as the UI= handler does.
// --- otdRestorePersistedCommands begin
static void otdRestorePersistedCommands() {
  if (settings.otd.iDhwOverride == 1) { otMasterStatusFlags |= 0x02;  otDHWOverride = 1; }
  else if (settings.otd.iDhwOverride == 0) { otMasterStatusFlags &= ~0x02; otDHWOverride = 0; }
  if (settings.otd.fDhwSetpoint > 0.0f)   enqueueWriteCommand(56, floatToF88(settings.otd.fDhwSetpoint), "SW-boot");
  if (settings.otd.fMaxChSetpoint > 0.0f) enqueueWriteCommand(57, floatToF88(settings.otd.fMaxChSetpoint), "SH-boot");
  otIgnoreTransitions = settings.otd.bIgnoreTransitions ? 1 : 0;
  otOverrideHB        = settings.otd.bOverrideHB ? 1 : 0;
  const char* p = settings.otd.sUnknownIds;
  while (*p && otUnknownIdCount < OT_UNKNOWN_ID_MAX) {
    char* end = nullptr;
    const long id = strtol(p, &end, 10);
    if (end == p) { p++; continue; }
    p = end;
    if (id < 0 || id > 127 || isUnknownId((uint8_t)id)) continue;
    otUnknownIds[otUnknownIdCount++] = (uint8_t)id;
    for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
      if (otSchedule[i].msgId == (uint8_t)id) { otSchedule[i].disabled = true; break; }
    }
  }
}
// --- otdRestorePersistedCommands end

bool handleOTDirectCommand(const char* buf, int len, bool satOwn) {
  otdWriteRefused = false;
  if (diagRec.active) diagRec.recOtdCommand(buf, (size_t)len);   // TASK-1665 (ADR-213)
  if (len < 4) return false;  // minimum "XX=Y"

  OTDDebugTf(PSTR("OTD: cmd \"%s\" (len=%d)\r\n"), buf, len);

  char cmd0 = buf[0];
  char cmd1 = buf[1];
  if (buf[2] != '=') return false;
  const char* value = buf + 3;
  char rspBuf[16];  // scratch for formatting response values

  // TASK-1150 / ADR-179 / TASK-1695: while SAT is enabled, an EXTERNAL CV control command (CS= CH= MM= C2= H2= TT= TC=) is refused
  // rather than silently winning by being the last writer. Maintainer decision 2026-09-22: a visible refusal beats a race whose
  // outcome depends on timing. SAT's own commands arrive through the same queue with satOwn = true (TASK-1575); disable SAT to
  // control the boiler from outside.
  if (!satOwn && satOwnsControlSetpoint() && satBlocksExternalCommand(buf, len)) {
    satNoteExternalRefused(buf, len);
    otDirectBridgeProcessStatus("NG");
    return false;
  }

  // =====================================================================
  // Setpoint write commands (enqueue + write cache + override)
  // =====================================================================

  // CS=xx.x — Control setpoint (MsgID 1 = TSet). Value 0 clears override.
  if (cmd0 == 'C' && cmd1 == 'S') {
    float temp = atof(value);
    bool enqueued = true;
    if (temp == 0.0f) {
      // CS=0 clears the flow setpoint override only and does not write bit0 (CH enable)
      // of MsgID 0. CH= overrides bit0 only while a CS override is active (gateway.asm:53;
      // pyotgw/pyotgw.py:602-603), so clearing the override hands bit0 back to the board's
      // own value (otdEffectiveMasterFlags).
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
  // ADR-223: gateway mode answers the thermostat's MsgID 9 / 100 reads with the
  // value and flags 0x01 (Manual override priority) and writes nothing to the
  // boiler; master mode writes MsgID 16 / 100. Ends at TC=0, a replacement TT=/TC=,
  // or the release rule of onThermostatMsgID16.
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
  // ADR-223: gateway mode answers the thermostat's MsgID 9 / 100 reads with the
  // value and flags 0x02 (Program override priority) and writes nothing to the
  // boiler; master mode writes MsgID 16 / 100. Ends at TT=0, a replacement TT=/TC=,
  // or the release rule of onThermostatMsgID16.
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
  // Intercepts the thermostat's own MsgID 16 WRITE_DATA and replaces the data bytes
  // with the fake setpoint before forwarding to the boiler. This is the same
  // mechanism as the PIC firmware (gateway.asm:3019-3024): the modified frame
  // is the thermostat's own request, not a separate WRITE_DATA from the gateway.
  // BS=0 clears the fake and restores the thermostat's original room setpoint.
  else if (cmd0 == 'B' && cmd1 == 'S') {
    float temp = atof(value);
    if (temp == 0.0f) {
      otFakeRoomSetpoint = 0.0f;
      clearWriteOverride(16);   // also an MQTT room_setpoint write
    } else {
      otFakeRoomSetpoint = temp;
      uint16_t f88 = floatToF88(temp);
      setOverride(16, f88);
    }
    dtostrf(temp, 1, 2, rspBuf);
    synthesizeResponse(buf, rspBuf);
  }
  // RT=xx.xx — Room temperature for the boiler (MsgID 24 = Tr), PIC 6.8 (gateway.asm SetRoomTemp
  // :4300-4309, MessageID24 :3061-3068, tests/heatpump-1.stc RT=25.41 -> R10181969). Some heat pumps
  // control on room setpoint and room temperature. Like BS= for MsgID 16, nothing is sent at once:
  // the thermostat's own WRITE of MsgID 24 gets the RT value (applyOverrides; the thermostat gets
  // WRITE_ACK with its own data, buildOverriddenReply), and without a thermostat the schedule's
  // MsgID 24 write entry sends it. RT=0 stops it (NoFakeRoomTemp). Parsed as the PIC's
  // GetPercentArg (otdPicPercentF88); the reply prints the stored value with two decimals.
  else if (cmd0 == 'R' && cmd1 == 'T') {
    uint16_t f88 = 0;
    const char* err = otdPicPercentF88(value, f88);
    if (err) { otDirectBridgeProcessStatus(err); return false; }
    if (f88 == 0) {
      clearWriteOverride(24);
    } else {
      setOverride(24, f88);
      updateWriteCache(24, f88);
    }
    dtostrf(f88 / 256.0f, 1, 2, rspBuf);
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
      otdPersistFloat(settings.otd.fDhwSetpoint, temp);   // TASK-1836: SaveConfig, re-sent at boot
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
      otdPersistFloat(settings.otd.fMaxChSetpoint, temp);   // TASK-1836: SaveConfig, re-sent at boot
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
    // TASK-1846: a value is a digit, or a '-'/'+' sign followed by a digit (PIC GetFloatArg/GetFloat gateway.asm 6.8 :5118-5129); anything else,
    // OT=- included, clears as the PIC's ClrOutsideT. Before, OT=- passed as a number and wrote 0.00.
    const bool signedNum = (value[0] == '-' || value[0] == '+') && isdigit((unsigned char)value[1]);
    if (!isdigit((unsigned char)value[0]) && !signedNum) {
      clearWriteOverride(27);
      synthesizeResponse(buf, value);
    } else {
      uint16_t f88 = floatToF88(atof(value));
      if (enqueueWriteCommand(27, f88, "OT")) {
        otdPsStoreOutside(f88);   // TASK-1846: the PS=1 line shows the OT= value, as on the PIC
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
      synthesizeResponse(buf, "-");   // TASK-1831: the PIC's ValueCleared reply
      // Clear persisted ventilation setpoint on explicit clear command
      if (settings.otd.iVentSetpoint != 0) {
        settings.otd.iVentSetpoint = 0;
        writeSettings(false);
      }
    } else {
      int setpt = constrain(atoi(value), 0, 100);
      uint16_t data = (uint16_t)(setpt & 0xFF);   // TASK-1831: LB, as the spec and the PIC (setbyte4)
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
  // SC=HH:MM/DOW — Time sync (MsgID 20 = day/time, ~61s expiry per PIC CommandExpiry)
  else if (cmd0 == 'S' && cmd1 == 'C') {
    int hh = 0, mm = 0, dow = 0;
    if (siscanf(value, "%d:%d/%d", &hh, &mm, &dow) >= 2) {
      // TASK-1840: stored for the thermostat's MsgID 20 only, never written to the boiler
      otSCClockData = (uint16_t)(((dow & 0x07) << 5 | (hh & 0x1F)) << 8) | (mm & 0x3F);
      otSCLastCommandMs = millis();
      if (otSCLastCommandMs == 0) otSCLastCommandMs = 1;  // 0 sentinel reserved
      snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d:%02d/%d"), hh & 0x1F, mm & 0x3F, dow & 0x07);
      synthesizeResponse(buf, rspBuf);
    }
  }

  // =====================================================================
  // Status flag commands (immediate effect on MsgID 0 status byte)
  // =====================================================================

  // HW=0/1/A/P — DHW enable (bit1 of master status) + DHW push state machine
  else if (cmd0 == 'H' && cmd1 == 'W') {
    char v = value[0];
    if (v != 'P') otdPushEnd();   // TASK-1845: any other HW= aborts a manual push (gateway.asm:4398-4399)
    if (v == 'P') {
      otdPushStart();   // TASK-1845: the push leaves the HW= state alone (PIC SetHotWaterPush)
    } else if (v == '1') {
      otMasterStatusFlags |= 0x02;
      otDHWOverride = 1;
    } else if (v == '0') {
      otMasterStatusFlags &= ~0x02;
      otDHWOverride = 0;
    } else {
      // Any other char = auto (thermostat controls DHW)
      otMasterStatusFlags |= 0x02;  // default on for auto
      otDHWOverride = 0xFF;
      v = 'A';
    }
    if (v != 'P') otdPersistHW(v);   // TASK-1836: SaveConfig; HW=P leaves the stored state
    rspBuf[0] = v; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // CH=0/1 — CH enable override of bit0, applied only while a CS override is active (PIC CHModeOff)
  else if (cmd0 == 'C' && cmd1 == 'H') {
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return false; }
    otChModeOff = (value[0] == '0');
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // H2=0/1 — CH2 enable (bit4 of master status)
  else if (cmd0 == 'H' && cmd1 == '2') {
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return false; }
    if (value[0] == '1') {
      otMasterStatusFlags |= 0x10;
    } else {
      otMasterStatusFlags &= ~0x10;
    }
    otCH2Enable = (value[0] == '1');   // TASK-1833: the relayed status reads this one
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // SM=0/1 — Summer mode (bit5 of master status)
  else if (cmd0 == 'S' && cmd1 == 'M') {
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return false; }
    if (value[0] == '1') {
      otMasterStatusFlags |= 0x20;
      otSummerMode = true;
    } else {
      otMasterStatusFlags &= ~0x20;
      otSummerMode = false;
    }
    otdPersistBool(settings.otd.bSummerMode, otSummerMode);   // TASK-1836: SaveConfig
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // BW=0/1 — DHW blocking (bit6 of master status, not persisted)
  else if (cmd0 == 'B' && cmd1 == 'W') {
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return false; }
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
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return false; }
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
    // TASK-1833: as the PIC SetCoolLevel (gateway.asm 6.8:4454-4464): a number, 0 included, is held as the
    // cooling level; only a value that is not a number clears it, with the ValueCleared reply "CL: -".
    char* end = nullptr;
    float cl = strtof(value, &end);
    bool enqueued = true;
    if (end == value) {
      clearWriteOverride(7);
      synthesizeResponse(buf, "-");
    } else {
      uint16_t f88 = floatToF88(cl);
      enqueued = enqueueWriteCommand(7, f88, "CL");
      if (enqueued) {
        dtostrf(cl, 1, 2, rspBuf);
        synthesizeResponse(buf, rspBuf);
      }
    }
  }

  // =====================================================================
  // Operating mode commands (MH, MW, M2) — MsgID 99
  // =====================================================================

  // MH=x — Remote override operating mode CH1 (LB bits 0..3)
  else if (cmd0 == 'M' && cmd1 == 'H') {
    uint8_t val = atoi(value);
    if (val > 15) { otDirectBridgeProcessStatus("BV"); return false; }
    otOperModeCH1 = val;
    // Send MsgID 99 WRITE_DATA
    uint16_t data99 = otdOperModeData(otPushManual);   // TASK-1845: the push bit stays while a push runs (operatingmode1 bit 4)
    unsigned long frame99 = OpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      static_cast<OpenThermMessageID>(99), data99);
    if (!otCmdEnqueue(frame99)) { OTDDebugTf(PSTR("OTD: queue full, dropped frame %08lX\r\n"), frame99); }
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), val);
    synthesizeResponse(buf, rspBuf);
  }
  // MW=x — Remote override operating mode DHW (HB bits 0..3)
  else if (cmd0 == 'M' && cmd1 == 'W') {
    uint8_t val = atoi(value);
    if (val > 15) { otDirectBridgeProcessStatus("BV"); return false; }
    otOperModeDHW = val;
    uint16_t data99 = otdOperModeData(otPushManual);   // TASK-1845: the push bit stays while a push runs (operatingmode1 bit 4)
    unsigned long frame99 = OpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA,
      static_cast<OpenThermMessageID>(99), data99);
    if (!otCmdEnqueue(frame99)) { OTDDebugTf(PSTR("OTD: queue full, dropped frame %08lX\r\n"), frame99); }
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), val);
    synthesizeResponse(buf, rspBuf);
  }
  // M2=x — Remote override operating mode CH2 (LB bits 4..7)
  else if (cmd0 == 'M' && cmd1 == '2') {
    uint8_t val = atoi(value);
    if (val > 15) { otDirectBridgeProcessStatus("BV"); return false; }
    otOperModeCH2 = val;
    uint16_t data99 = otdOperModeData(otPushManual);   // TASK-1845: the push bit stays while a push runs (operatingmode1 bit 4)
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
        return false;
      }
      setOTDirectMode(OTD_MODE_BYPASS);
#else
      OTDDebugTln(F("OTD: GW=P not supported (no bypass relay on this board)"));
      otDirectBridgeProcessStatus("NG");
      return false;
#endif
    } else if (value[0] == 'L') {
      // GW=L — Loopback test mode: simulated boiler, no hardware needed
      setOTDirectMode(OTD_MODE_LOOPBACK);
    } else if (value[0] == 'R') {
      // Full reset: clear all transient state (mirrors PIC hardware reset)
      OTDDebugTln(F("OTD: full gateway reset"));
      resetTransientState();
      // TASK-1800: a PIC reset clears its RAM: the PS=1 store, the pending summary and HideReports
      memset(otdPsValues, 0, sizeof(otdPsValues));
      otHideReports = false;
      otSummaryPending = false;
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
        // Banner "OpenTherm Gateway <version> OTGW32" (Robert, 2026-11-12): the PIC format (gateway.asm:5434-5435)
        // with the board after the version, a deliberate difference from the PIC. The version is the PIC command
        // set OT-Direct implements (OTD_PIC_COMPAT_VERSION) and must stay right after "OpenTherm Gateway ":
        // pyotgw reads int(ver[18]) (pyotgw.py:182) and OTmonitor takes the third word (otmonitor.tcl:552,
        // gui.tcl:1080 needs 6.2). The board name alone broke pyotgw / HA opentherm_gw (TASK-1797).
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: A=OpenTherm Gateway " OTD_PIC_COMPAT_VERSION " OTGW32"));
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
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: T=%c%c"),   // two booleans, no separator (PrintTweaks, gateway.asm:2278-2285)
                   otIgnoreTransitions ? '1' : '0', otOverrideHB ? '1' : '0');
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
        // PIC-compatible reset-cause code (P/C/W/B/E). The platform shim maps
        // esp_reset_reason() to the PIC vocabulary; see platform_esp32.h.
        char rc = platformGetResetReasonChar();
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
      case 'X':
        // TASK-1860 (Robert, 2026-11-12): an OT-Direct report the PIC does not have. The PIC 6.8 answers BV for
        // PR=H/J/K/U/X/Y/Z (gateway.asm:2163-2189), so a tool must not rely on PR=X on a PIC board; the PR=A
        // banner and /api/v2/device/info carry the same version on every board.
        snprintf_P(prBuf, sizeof(prBuf), PSTR("PR: X=" OTD_PIC_COMPAT_VERSION));
        otDirectBridgeProcessPRResponse(prBuf);
        break;
      default:
        OTDDebugTf(PSTR("OTD: PR=%c unknown register\r\n"), reg);
        break;
    }
  }

  // =====================================================================
  // PS=0/1 — Print Summary, as the PIC (PrintSummary, gateway.asm:2341-2358)
  // PS=1: one summary line (SummaryReport, :1644-1658), T/B/R/A reports hidden until PS=0
  // PS=0: reports back. One character only (else "PS: SE"), 0 or 1 (else "PS: BV"): the PIC prints
  // "PS: " before it checks the value (SerialCommand, gateway.asm:3697-3704).
  // =====================================================================

  else if (cmd0 == 'P' && cmd1 == 'S') {
    if (value[0] == '\0' || value[1] != '\0') { synthesizeResponse(buf, "SE"); return false; }
    if (!checkBoolean(value)) { synthesizeResponse(buf, "BV"); return false; }
    otHideReports = (value[0] == '1');
    otSummaryPending = otHideReports;
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }

  // =====================================================================
  // Schedule management commands (AA, DA, PM)
  // =====================================================================

  // AA=MsgID — Add alternative: re-enable a disabled schedule entry.
  // PIC semantic: adds MsgID to a 32-slot EEPROM ring buffer (persistent, survives reboot).
  // OTDirect semantic: re-enables an existing otSchedule[] entry (RAM only, lost on restart).
  // Deliberate deviation: otSchedule already covers all relevant MsgIDs; EEPROM ring not needed.
  // If MsgID is not in otSchedule, returns "NF" (PIC would add it to the ring).
  else if (cmd0 == 'A' && cmd1 == 'A') {
    uint8_t msgId = atoi(value);
    if (msgId == 0 || msgId > 127) {
      otDirectBridgeProcessStatus("BV"); return false;
    }
    bool found = false;
    for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
      if (otSchedule[i].msgId == msgId) {
        otSchedule[i].disabled = false;
        found = true; break;
      }
    }
    if (!found) { otDirectBridgeProcessStatus("NF"); return false; }
    clearUnknownCount(msgId);  // Reset 3-strike counter so it doesn't re-disable immediately
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), msgId);
    synthesizeResponse(buf, rspBuf);
  }
  // DA=MsgID — Delete alternative: disable a schedule entry.
  // PIC semantic: removes MsgID from the EEPROM ring buffer.
  // OTDirect semantic: sets disabled=true in otSchedule[] (entry stays in table).
  else if (cmd0 == 'D' && cmd1 == 'A') {
    uint8_t msgId = atoi(value);
    if (msgId == 0 || msgId > 127) {
      otDirectBridgeProcessStatus("BV"); return false;
    }
    bool found = false;
    for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
      if (otSchedule[i].msgId == msgId) {
        otSchedule[i].disabled = true;
        found = true; break;
      }
    }
    if (!found) { otDirectBridgeProcessStatus("NF"); return false; }
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), msgId);
    synthesizeResponse(buf, rspBuf);
  }
  // PM=MsgID — Priority message: send immediately on next cycle
  else if (cmd0 == 'P' && cmd1 == 'M') {
    uint8_t msgId = atoi(value);
    if (msgId > 127) {
      otDirectBridgeProcessStatus("BV"); return false;
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
    if (!colon) { otDirectBridgeProcessStatus("BV"); return false; }
    if (comma) {
      // PIC byte-pair: SR=<msgId>:<highByte>,<lowByte> (decimal 0..255 each)
      unsigned int hiByte = 0, loByte = 0;
      if (siscanf(value, "%u:%u,%u", &msgId, &hiByte, &loByte) != 3 ||
          msgId > 127 || hiByte > 255 || loByte > 255) {
        otDirectBridgeProcessStatus("BV"); return false;
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
      if (!hasHexDigit && siscanf(value, "%u:%u", &msgId, &oneVal) == 2 && oneVal <= 255) {
        if (msgId > 127) { otDirectBridgeProcessStatus("BV"); return false; }
        dataVal = oneVal & 0xFF;  // high byte = 0
      } else if (siscanf(value, "%u:%x", &msgId, &dataVal) == 2 && msgId <= 127) {
        // legacy hex16 form
      } else {
        otDirectBridgeProcessStatus("BV"); return false;
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
    if (slot < 0) { otDirectBridgeProcessStatus("NS"); return false; }  // table full
    otResponseOverrides[slot].msgId = (uint8_t)msgId;
    otResponseOverrides[slot].value = (uint16_t)dataVal;
    otResponseOverrides[slot].active = true;
    synthesizeResponse(buf, value);
  }
  // CR=MsgID — Clear response override
  else if (cmd0 == 'C' && cmd1 == 'R') {
    uint8_t msgId = atoi(value);
    if (msgId > 127) { otDirectBridgeProcessStatus("BV"); return false; }
    bool found = false;
    for (uint8_t i = 0; i < OT_RESPONSE_OVERRIDE_MAX; i++) {
      if (otResponseOverrides[i].active && otResponseOverrides[i].msgId == msgId) {
        otResponseOverrides[i].active = false;
        found = true; break;
      }
    }
    if (!found) { otDirectBridgeProcessStatus("NF"); return false; }
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), msgId);
    synthesizeResponse(buf, rspBuf);
  }

  // UI=MsgID — Mark as unknown (gateway responds UNKNOWN_DATAID to thermostat)
  else if (cmd0 == 'U' && cmd1 == 'I') {
    uint8_t msgId = atoi(value);
    if (msgId > 127) { otDirectBridgeProcessStatus("BV"); return false; }
    if (!isUnknownId(msgId)) {
      if (otUnknownIdCount >= OT_UNKNOWN_ID_MAX) { otDirectBridgeProcessStatus("NS"); return false; }
      otUnknownIds[otUnknownIdCount++] = msgId;
      otdPersistUnknownIds();   // TASK-1836
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
    if (msgId > 127) { otDirectBridgeProcessStatus("BV"); return false; }
    for (uint8_t i = 0; i < otUnknownIdCount; i++) {
      if (otUnknownIds[i] == msgId) {
        otUnknownIds[i] = otUnknownIds[--otUnknownIdCount];
        otdPersistUnknownIds();   // TASK-1836
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
    if (siscanf(value, "%u:%x", &msgId, &dataVal) != 2 || msgId > 127) {
      otDirectBridgeProcessStatus("BV"); return false;
    }
    setResponseModifier((uint8_t)msgId, (uint16_t)dataVal);
    synthesizeResponse(buf, value);
  }
  // CM=MsgID — Clear response modifier
  else if (cmd0 == 'C' && cmd1 == 'M') {
    uint8_t msgId = atoi(value);
    if (msgId > 127) { otDirectBridgeProcessStatus("BV"); return false; }
    clearResponseModifier(msgId);
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%d"), msgId);
    synthesizeResponse(buf, rspBuf);
  }

  // SB=xx.x — Setback temperature (used when thermostat disconnects)
  else if (cmd0 == 'S' && cmd1 == 'B') {
    otSetbackTemp = constrain(atof(value), 1.0f, 30.0f);
    otdPersistFloat(settings.otd.fSetbackTemp, otSetbackTemp);   // TASK-1836: persist (marks dirty)
    dtostrf(otSetbackTemp, 1, 2, rspBuf);
    synthesizeResponse(buf, rspBuf);
  }
  // IT=0/1 — Ignore transitions
  else if (cmd0 == 'I' && cmd1 == 'T') {
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return false; }
    otIgnoreTransitions = (value[0] == '1') ? 1 : 0;
    otdPersistBool(settings.otd.bIgnoreTransitions, otIgnoreTransitions != 0);   // TASK-1836
    rspBuf[0] = '0' + otIgnoreTransitions; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // OH=0/1 — Override high byte
  else if (cmd0 == 'O' && cmd1 == 'H') {
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return false; }
    otOverrideHB = (value[0] == '1') ? 1 : 0;
    otdPersistBool(settings.otd.bOverrideHB, otOverrideHB != 0);   // TASK-1836
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
    if (targetMsgId == 0) { otDirectBridgeProcessStatus("BV"); return false; }
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
    if (val < 100 || val > 1275) { otDirectBridgeProcessStatus("OR"); return false; }
    otMinIntervalMs = val;
    otdPersistU16(settings.otd.iMsgInterval, val);   // TASK-1836: SaveConfig (gateway.asm:4610-4611)
    // TASK-440: PIC SetInterval/PrintInterval reports milliseconds, not
    // centiseconds. Internally PIC stores 5ms ticks but echoes ms.
    snprintf_P(rspBuf, sizeof(rspBuf), PSTR("%u"), val);
    synthesizeResponse(buf, rspBuf);
  }
  // FS=0/1 — Fail safety (controls setback on thermostat disconnect)
  else if (cmd0 == 'F' && cmd1 == 'S') {
    if (!checkBoolean(value)) { otDirectBridgeProcessStatus("BV"); return false; }
    otFailSafeEnabled = (value[0] == '1');
    otdPersistBool(settings.otd.bFailSafe, otFailSafeEnabled);   // TASK-1836: SaveConfig (gateway.asm:5300)
    rspBuf[0] = value[0]; rspBuf[1] = '\0';
    synthesizeResponse(buf, rspBuf);
  }
  // TP=MsgID:Index[=Value] — Thermostat slave parameters (TSP/FHB)
  else if (cmd0 == 'T' && cmd1 == 'P') {
    unsigned int tpMsgId = 0, tpIndex = 0, tpValue = 0;
    bool isWrite = false;
    // Parse "MsgID:Index" or "MsgID:Index=Value"
    char* colonPos = strchr(value, ':');
    if (!colonPos) { otDirectBridgeProcessStatus("SE"); return false; }
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
        tpMsgId != 106 && tpMsgId != 108) { otDirectBridgeProcessStatus("BV"); return false; }
    if (tpIndex > 255 || tpValue > 255) { otDirectBridgeProcessStatus("OR"); return false; }
    // FHB entries (13, 91, 108) are read-only
    if (isWrite && (tpMsgId == 13 || tpMsgId == 91 || tpMsgId == 108)) { otDirectBridgeProcessStatus("SE"); return false; }
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
    return false;
  }
  return !otdWriteRefused;
}

#endif // HAS_DIRECT_OT

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
