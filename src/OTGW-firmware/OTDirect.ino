/*
***************************************************************************
**  Program  : OTDirect.ino
**  Version  : v1.4.0-beta
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
// OpenTherm interface instances — master talks to boiler, slave listens to thermostat
// ---------------------------------------------------------------------------
static OpenTherm otMaster(PIN_OT_MASTER_IN, PIN_OT_MASTER_OUT);
static OpenTherm otSlave(PIN_OT_SLAVE_IN, PIN_OT_SLAVE_OUT, true);  // true = slave mode

// ISR handlers — must be free functions for attachInterrupt
static void IRAM_ATTR masterISR() { otMaster.handleInterrupt(); }
static void IRAM_ATTR slaveISR()  { otSlave.handleInterrupt(); }

// ---------------------------------------------------------------------------
// Master request scheduler — periodic polls and writes to the boiler
// ---------------------------------------------------------------------------
static constexpr uint32_t OT_STATUS_INTERVAL_MS   = 800;    // MsgID 0 (status) every 800ms (OT-Thing parity)
static constexpr uint32_t OT_TEMP_INTERVAL_MS     = 10000;  // Temperature reads every 10s
static constexpr uint32_t OT_SLOW_INTERVAL_MS     = 60000;  // Slow-poll items every 60s
static constexpr uint32_t OT_WRITE_INTERVAL_MS    = 15000;  // Periodic writes every 15s (keep boiler values alive)

// Status flags to send in MsgID 0 (master status byte)
static uint8_t otMasterStatusFlags = 0x01;  // bit0=CH enable (default on)

// Bypass relay state (GW=0 bypass on, GW=1 gateway mode)
static bool otBypassActive = false;

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

// Command ring buffer — queues frames from handleOTDirectCommand()
static constexpr uint8_t OT_CMD_QUEUE_SIZE = 8;
static uint32_t otCmdQueue[OT_CMD_QUEUE_SIZE];
static uint8_t  otCmdHead = 0;   // next write position
static uint8_t  otCmdTail = 0;   // next read position

static bool otCmdQueueEmpty() { return otCmdHead == otCmdTail; }
static bool otCmdQueueFull()  { return ((otCmdHead + 1) % OT_CMD_QUEUE_SIZE) == otCmdTail; }

static bool otCmdEnqueue(uint32_t frame) {
  if (otCmdQueueFull()) return false;
  otCmdQueue[otCmdHead] = frame;
  otCmdHead = (otCmdHead + 1) % OT_CMD_QUEUE_SIZE;
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
  {  1, false, 0 },   // TSet — CS=/TC= override
  {  7, false, 0 },   // CoolingControl — CC= override
  {  8, false, 0 },   // TsetCH2 — C2= override
  { 14, false, 0 },   // MaxRelModLevelSetting — MM= override
  { 16, false, 0 },   // TrSet — TT= override
  { 56, false, 0 },   // TdhwSet — SW= override
  { 57, false, 0 },   // MaxTSet — SH= override
};
static constexpr uint8_t OT_OVERRIDE_COUNT = sizeof(otOverrides) / sizeof(otOverrides[0]);

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

// Apply overrides to a thermostat frame before forwarding to boiler.
// Returns the (potentially modified) frame. If modified, also bridges the
// original thermostat frame as 'T' and the modified frame as 'R'.
static unsigned long applyOverrides(unsigned long frame, bool &modified) {
  modified = false;
  uint8_t msgId = (frame >> 16) & 0xFF;
  for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) {
    if (otOverrides[i].active && otOverrides[i].msgId == msgId) {
      // Replace data value (lower 16 bits) while keeping msg type + data-id
      frame = (frame & 0xFFFF0000UL) | otOverrides[i].overrideValue;
      // Recalculate parity (bit 31)
      frame &= 0x7FFFFFFFUL;  // clear parity
      uint32_t p = frame;
      p ^= (p >> 16); p ^= (p >> 8); p ^= (p >> 4); p ^= (p >> 2); p ^= (p >> 1);
      if (p & 1) frame |= 0x80000000UL;
      modified = true;
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
// bridgeFrameToParser — format a 32-bit OT frame and feed to processOT()
// ---------------------------------------------------------------------------
static void bridgeFrameToParser(char prefix, unsigned long frame) {
  char buf[10];
  snprintf_P(buf, sizeof(buf), PSTR("%c%08lX"), prefix, frame);
  processOT(buf, 9);
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
  DebugTln(F("OT-direct: 24V step-up enabled"));
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

  // 1. Initialize bypass relay (off = gateway mode)
  pinMode(PIN_BYPASS_RELAY, OUTPUT);
  digitalWrite(PIN_BYPASS_RELAY, LOW);
  otBypassActive = false;
  DebugTln(F("OT-direct: Bypass relay initialized (gateway mode)"));

  // 2. Enable step-up converter
  enableStepUp();

  // 3. Probe OT master input for idle bus
  bool busPresent = probeOTBus();
  if (busPresent) {
    DebugTln(F("OT-direct: OT bus idle detected on master input"));
  } else {
    DebugTln(F("OT-direct: WARNING — OT master input not idle, boiler may be disconnected"));
  }

  // 4. Start OpenTherm master (talks to boiler)
  otMaster.begin(masterISR);
  DebugTln(F("OT-direct: Master interface started"));

  // 5. Start OpenTherm slave (listens to thermostat)
  otSlave.begin(slaveISR, handleSlaveRequest);
  DebugTln(F("OT-direct: Slave interface started"));

  // 6. Always set OT_DIRECT mode — this is OTGW32 hardware.
  //    Bus liveness is tracked via state.otgw.bOnline, not eMode.
  //    The OT-direct loop must keep running so it can retry/recover.
  state.hw.eMode = HW_MODE_OT_DIRECT;
  DebugTln(F("OT-direct: Hardware mode set to OT_DIRECT"));

  if (!busPresent) {
    DebugTln(F("OT-direct: WARNING — OT bus not idle, boiler may be disconnected"));
  }

  // 7. Send initial status request to boiler to check connectivity
  // Blocking calls are acceptable during setup() — not yet in cooperative loop
  unsigned long request = buildStatusRequest();
  unsigned long response = otMaster.sendRequest(request);
  if (otMaster.isValidResponse(response)) {
    state.otgw.bOnline = true;
    DebugTln(F("OT-direct: Boiler responded — OT bus online"));
    bridgeFrameToParser('R', request);
    bridgeFrameToParser('B', response);
  } else {
    state.otgw.bOnline = false;
    DebugTln(F("OT-direct: No valid boiler response — OT bus offline (will retry in loop)"));
  }

  // 8. OT protocol handshake — identify ourselves to the boiler.
  //    MsgID 2: Master config flags (HB) + MemberID (LB). We set Smart Power bit if available.
  //    MsgID 124: OpenTherm version we speak (f8.8, 2.2 = 0x0233).
  //    MsgID 126: Master product type (HB) + version (LB).
  //    These are one-time WRITE_DATA, not polled.
  if (state.otgw.bOnline) {
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
      DebugTf(PSTR("OT-direct: Slave config=0x%02X (DHW=%d ModCtrl=%d Cool=%d CH2=%d)\r\n"),
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

    DebugTln(F("OT-direct: Protocol handshake complete"));
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
  state.otd.bBypassActive     = otBypassActive;
  state.otd.bStepUpEnabled    = (digitalRead(PIN_STEPUP_ENABLE) == HIGH);
}

// ---------------------------------------------------------------------------
// sendMasterRequestAsync — initiate an async OT request (non-blocking)
// ---------------------------------------------------------------------------
static bool sendMasterRequestAsync(unsigned long request, OTDirectRequestOrigin origin) {
  if (!otMaster.isReady()) return false;  // bus busy — try again later
  otLastSentRequest = request;
  otLastRequestOrigin = origin;
  otMasterRequestActive = otMaster.sendRequestAsync(request);
  if (otMasterRequestActive) {
    bridgeFrameToParser((origin == OT_DIRECT_ORIGIN_THERMOSTAT) ? 'T' : 'R', request);
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
    state.otgw.bOnline = true;

    // Self-disabling poll: if boiler responds with UNKNOWN_DATA_ID,
    // disable that MsgID from future scheduled polling (OT-Thing pattern).
    // MsgID 0 (status) is never disabled — it's mandatory.
    OpenThermMessageType respType = OpenTherm::getMessageType(response);
    if (respType == OpenThermMessageType::UNKNOWN_DATA_ID) {
      uint8_t msgId = (response >> 16) & 0xFF;
      if (msgId != 0) {
        for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
          if (otSchedule[i].msgId == msgId && !otSchedule[i].disabled) {
            otSchedule[i].disabled = true;
            DebugTf(PSTR("OT-direct: MsgID %u disabled (UNKNOWN_DATA_ID)\r\n"), msgId);
            break;
          }
        }
      }
    }

    // If this was a forwarded thermostat frame, send response back
    if (otLastRequestOrigin == OT_DIRECT_ORIGIN_THERMOSTAT) {
      otSlave.sendResponse(response);
    }
  } else {
    // Only mark offline on status request (MsgID 0) failures
    uint8_t msgId = (otLastSentRequest >> 16) & 0xFF;
    if (msgId == 0) {
      state.otgw.bOnline = false;
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

  // Pending commands from the ring buffer take priority.
  // Peek first — only dequeue after successful async send (Codex P1 fix).
  if (!otCmdQueueEmpty()) {
    uint32_t cmdFrame = otCmdQueue[otCmdTail];
    if (sendMasterRequestAsync(cmdFrame, OT_DIRECT_ORIGIN_GATEWAY)) {
      otCmdTail = (otCmdTail + 1) % OT_CMD_QUEUE_SIZE;  // consume on success
    }
    return;
  }

  // Round-robin through the schedule table, skipping disabled entries
  uint8_t startIdx = otScheduleIdx;
  do {
    OTScheduleEntry &entry = otSchedule[otScheduleIdx];
    otScheduleIdx = (otScheduleIdx + 1) % OT_SCHEDULE_SIZE;

    if (entry.disabled) continue;  // boiler doesn't support this MsgID

    // Write entries only fire when a value has been set by a command
    if (entry.isWrite && !entry.valueSet) continue;

    if ((now - entry.lastSentMs) >= entry.intervalMs) {
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

  // Forward pending thermostat frame when master bus is idle.
  // Apply repeater-mode overrides before forwarding.
  // Keep frame pending until send succeeds (retry on next loop).
  if (!otMasterRequestActive && otSlaveFramePending) {
    bool modified = false;
    unsigned long frameToSend = applyOverrides(otSlaveFrame, modified);
    if (modified) {
      // Bridge original thermostat frame as 'T', modified frame sent as 'R'
      bridgeFrameToParser('T', otSlaveFrame);
    }
    if (sendMasterRequestAsync(frameToSend,
          modified ? OT_DIRECT_ORIGIN_GATEWAY : OT_DIRECT_ORIGIN_THERMOSTAT)) {
      otSlaveFramePending = false;
    }
  }

  // Schedule periodic master requests (non-blocking, skips if bus busy)
  DECLARE_TIMER_MS(timerOTSchedule, 100, SKIP_MISSED_TICKS);
  if (DUE(timerOTSchedule)) {
    scheduleMasterRequest();
  }

  // Refresh state.otd stats once per second for REST/MQTT
  DECLARE_TIMER_SEC(timerOTDStatus, 1, SKIP_MISSED_TICKS);
  if (DUE(timerOTDStatus)) {
    updateOTDirectStatus();
  }
}

// ---------------------------------------------------------------------------
// enqueueWriteCommand — build frame, enqueue, update write cache + override
// Helper to reduce repetition in handleOTDirectCommand.
// ---------------------------------------------------------------------------
static void enqueueWriteCommand(uint8_t msgId, uint16_t dataValue, const char* label) {
  unsigned long frame = OpenTherm::buildRequest(
    OpenThermMessageType::WRITE_DATA,
    static_cast<OpenThermMessageID>(msgId),
    dataValue
  );
  if (!otCmdEnqueue(frame)) {
    DebugTf(PSTR("OT-direct: %s command dropped (queue full)\r\n"), label);
    return;
  }
  // Update periodic write cache so the scheduler keeps refreshing this value
  updateWriteCache(msgId, dataValue);
  // Activate repeater override so thermostat frames for this MsgID get modified
  setOverride(msgId, dataValue);
  if (state.debug.bOTmsg) DebugTf(PSTR("OT-direct: %s -> MsgID %u frame 0x%08lX\r\n"), label, msgId, frame);
}

// ---------------------------------------------------------------------------
// handleOTDirectCommand — translate PIC-style commands to OT frames
// Called from sendOTGW() when HAS_DIRECT_OT is enabled.
//
// Supports all commands that map to OpenTherm MsgIDs, status flags, or
// hardware control. PIC-only commands are explicitly rejected.
// ---------------------------------------------------------------------------
void handleOTDirectCommand(const char* buf, int len) {
  if (len < 4) return;  // minimum "XX=Y"

  char cmd0 = buf[0];
  char cmd1 = buf[1];
  if (buf[2] != '=') return;
  const char* value = buf + 3;

  // --- Setpoint write commands (enqueue + write cache + override) ----------

  // CS=xx.x — Control setpoint (MsgID 1 = TSet)
  if (cmd0 == 'C' && cmd1 == 'S') {
    uint16_t f88 = (uint16_t)((int16_t)(atof(value) * 256.0f));
    enqueueWriteCommand(1, f88, "CS");
  }
  // TC=xx.x — Constant temperature override (same as CS= for OT-direct)
  else if (cmd0 == 'T' && cmd1 == 'C') {
    uint16_t f88 = (uint16_t)((int16_t)(atof(value) * 256.0f));
    enqueueWriteCommand(1, f88, "TC");
  }
  // TT=xx.x — Room setpoint / thermostat override (MsgID 16 = TrSet)
  else if (cmd0 == 'T' && cmd1 == 'T') {
    float setpoint = atof(value);
    if (setpoint == 0.0f) {
      // TT=0 clears the override (PIC behavior)
      clearOverride(16);
      // Clear write cache so scheduler stops refreshing
      for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) {
        if (otSchedule[i].isWrite && otSchedule[i].msgId == 16) {
          otSchedule[i].valueSet = false;
          break;
        }
      }
      if (state.debug.bOTmsg) DebugTln(F("OT-direct: TT=0 -> override cleared"));
    } else {
      uint16_t f88 = (uint16_t)((int16_t)(setpoint * 256.0f));
      enqueueWriteCommand(16, f88, "TT");
    }
  }
  // C2=xx.x — Control setpoint CH2 (MsgID 8 = TsetCH2)
  else if (cmd0 == 'C' && cmd1 == '2') {
    uint16_t f88 = (uint16_t)((int16_t)(atof(value) * 256.0f));
    enqueueWriteCommand(8, f88, "C2");
  }
  // CC=xx.x — Cooling control signal (MsgID 7, 0-100%)
  else if (cmd0 == 'C' && cmd1 == 'C') {
    uint16_t f88 = (uint16_t)((int16_t)(atof(value) * 256.0f));
    enqueueWriteCommand(7, f88, "CC");
  }
  // SW=xx.x — DHW setpoint (MsgID 56 = TdhwSet)
  else if (cmd0 == 'S' && cmd1 == 'W') {
    uint16_t f88 = (uint16_t)((int16_t)(atof(value) * 256.0f));
    enqueueWriteCommand(56, f88, "SW");
  }
  // SH=xx.x — Max CH water setpoint (MsgID 57 = MaxTSet)
  else if (cmd0 == 'S' && cmd1 == 'H') {
    uint16_t f88 = (uint16_t)((int16_t)(atof(value) * 256.0f));
    enqueueWriteCommand(57, f88, "SH");
  }
  // MM=xx — Max relative modulation level (MsgID 14)
  else if (cmd0 == 'M' && cmd1 == 'M') {
    uint16_t f88 = (uint16_t)((int16_t)(atof(value) * 256.0f));
    enqueueWriteCommand(14, f88, "MM");
  }
  // OT=xx.x — Outside temperature (MsgID 27 = Toutside)
  else if (cmd0 == 'O' && cmd1 == 'T') {
    uint16_t f88 = (uint16_t)((int16_t)(atof(value) * 256.0f));
    enqueueWriteCommand(27, f88, "OT");
  }
  // VS=xx — Ventilation setpoint (MsgID 71)
  else if (cmd0 == 'V' && cmd1 == 'S') {
    uint16_t data = ((uint16_t)(atoi(value) & 0xFF)) << 8;  // value in HB
    enqueueWriteCommand(71, data, "VS");
  }
  // SC=HH:MM/DOW — Time sync (MsgID 20 = day/time, MsgID 21 = date)
  // Format: SC=HH:MM/DOW (e.g. SC=14:25/3)
  // OT MsgID 20: HB = DOW(3bits)<<5 | hours(5bits), LB = minutes
  else if (cmd0 == 'S' && cmd1 == 'C') {
    int hh = 0, mm = 0, dow = 0;
    if (sscanf(value, "%d:%d/%d", &hh, &mm, &dow) >= 2) {
      uint16_t data20 = (uint16_t)(((dow & 0x07) << 5 | (hh & 0x1F)) << 8) | (mm & 0x3F);
      enqueueWriteCommand(20, data20, "SC-time");
    }
  }

  // --- Status flag commands (immediate effect on MsgID 0 status byte) ------

  // HW=0/1/A — DHW enable (bit1 of master status)
  else if (cmd0 == 'H' && cmd1 == 'W') {
    if (value[0] == '1' || value[0] == 'A' || value[0] == 'P') {
      otMasterStatusFlags |= 0x02;
      if (state.debug.bOTmsg) DebugTln(F("OT-direct: DHW enabled"));
    } else {
      otMasterStatusFlags &= ~0x02;
      if (state.debug.bOTmsg) DebugTln(F("OT-direct: DHW disabled"));
    }
  }
  // CH=0/1 — CH enable (bit0 of master status)
  else if (cmd0 == 'C' && cmd1 == 'H') {
    if (value[0] == '1') {
      otMasterStatusFlags |= 0x01;
      if (state.debug.bOTmsg) DebugTln(F("OT-direct: CH enabled"));
    } else {
      otMasterStatusFlags &= ~0x01;
      if (state.debug.bOTmsg) DebugTln(F("OT-direct: CH disabled"));
    }
  }
  // H2=0/1 — CH2 enable (bit4 of master status)
  else if (cmd0 == 'H' && cmd1 == '2') {
    if (value[0] == '1') {
      otMasterStatusFlags |= 0x10;
      if (state.debug.bOTmsg) DebugTln(F("OT-direct: CH2 enabled"));
    } else {
      otMasterStatusFlags &= ~0x10;
      if (state.debug.bOTmsg) DebugTln(F("OT-direct: CH2 disabled"));
    }
  }

  // --- Gateway control commands --------------------------------------------

  // GW=0 — Bypass mode (relay connects thermostat directly to boiler)
  // GW=1 — Gateway mode (OT-direct intercepts traffic)
  // GW=R — Reset OT interfaces
  else if (cmd0 == 'G' && cmd1 == 'W') {
    if (value[0] == '0') {
      // Bypass: activate relay, stop intercepting
      digitalWrite(PIN_BYPASS_RELAY, HIGH);
      otBypassActive = true;
      DebugTln(F("OT-direct: Bypass mode ON (thermostat direct to boiler)"));
    } else if (value[0] == '1') {
      // Gateway: deactivate relay, resume intercepting
      digitalWrite(PIN_BYPASS_RELAY, LOW);
      otBypassActive = false;
      DebugTln(F("OT-direct: Gateway mode ON (OT-direct active)"));
    } else if (value[0] == 'R') {
      DebugTln(F("OT-direct: Resetting OT interfaces"));
      otMaster.end();
      otSlave.end();
      delay(100);
      otMaster.begin(masterISR);
      otSlave.begin(slaveISR, handleSlaveRequest);
      DebugTln(F("OT-direct: OT interfaces restarted"));
    }
  }

  // --- PIC-only informational (ignored on OT-direct) -----------------------

  // PS=0/1 — Print Summary mode (PIC serial only)
  else if (cmd0 == 'P' && cmd1 == 'S') {
    // No-op on OT-direct
  }
  // PR=x — PIC query commands (version, mode, GPIO, LEDs, etc.)
  else if (cmd0 == 'P' && cmd1 == 'R') {
    if (state.debug.bOTmsg) DebugTf(PSTR("OT-direct: PR=%s ignored (PIC-only query)\r\n"), value);
  }

  // --- PIC-only configuration commands — explicitly reject -----------------
  // SB (setback), AA/DA (alt MsgID), UI/KI (unknown/known ID),
  // PM (priority msg), SR/CR (set/clear response), RS (reset counter),
  // IT/OH (ignore transitions/override high byte), FT (force thermostat),
  // VR (voltage ref), DP (debug pointer), LA-LF (LED functions)
  else if ((cmd0 == 'S' && cmd1 == 'B') || (cmd0 == 'A' && cmd1 == 'A') ||
           (cmd0 == 'D' && cmd1 == 'A') || (cmd0 == 'U' && cmd1 == 'I') ||
           (cmd0 == 'K' && cmd1 == 'I') || (cmd0 == 'P' && cmd1 == 'M') ||
           (cmd0 == 'S' && cmd1 == 'R') || (cmd0 == 'C' && cmd1 == 'R') ||
           (cmd0 == 'R' && cmd1 == 'S') || (cmd0 == 'I' && cmd1 == 'T') ||
           (cmd0 == 'O' && cmd1 == 'H') || (cmd0 == 'F' && cmd1 == 'T') ||
           (cmd0 == 'V' && cmd1 == 'R') || (cmd0 == 'D' && cmd1 == 'P') ||
           (cmd0 == 'L' && cmd1 >= 'A' && cmd1 <= 'F') ||
           (cmd0 == 'T' && cmd1 == 'S')) {
    if (state.debug.bOTmsg) DebugTf(PSTR("OT-direct: %c%c not supported (PIC-only)\r\n"), cmd0, cmd1);
  }
  else {
    if (state.debug.bOTmsg) DebugTf(PSTR("OT-direct: Unknown command [%c%c=%s] ignored\r\n"), cmd0, cmd1, value);
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
