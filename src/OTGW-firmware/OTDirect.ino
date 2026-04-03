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
// Master request scheduler — periodic polls to the boiler
// ---------------------------------------------------------------------------
static constexpr uint32_t OT_STATUS_INTERVAL_MS   = 1000;   // MsgID 0 (status) every 1s
static constexpr uint32_t OT_TEMP_INTERVAL_MS     = 10000;  // Temperature reads every 10s
static constexpr uint32_t OT_SLOW_INTERVAL_MS     = 60000;  // Slow-poll items every 60s

// Status flags to send in MsgID 0 (master status byte)
static uint8_t otMasterStatusFlags = 0x01;  // bit0=CH enable (default on)

// Schedule table: { msgId, interval_ms, lastSent_ms }
struct OTScheduleEntry {
  uint8_t  msgId;
  uint32_t intervalMs;
  uint32_t lastSentMs;
};

static OTScheduleEntry otSchedule[] = {
  // Fast poll — essential for control loop and dashboard
  {  0, OT_STATUS_INTERVAL_MS,  0 },  // Status (mandatory, ≤1s)
  // Temperature & modulation — 10s interval
  { 25, OT_TEMP_INTERVAL_MS,    0 },  // Boiler flow temp (Tboiler)
  { 28, OT_TEMP_INTERVAL_MS,    0 },  // Return water temp (Tret)
  { 26, OT_TEMP_INTERVAL_MS,    0 },  // DHW temp (Tdhw)
  { 27, OT_TEMP_INTERVAL_MS,    0 },  // Outside temp (Toutside)
  { 17, OT_TEMP_INTERVAL_MS,    0 },  // Relative modulation level
  { 18, OT_TEMP_INTERVAL_MS,    0 },  // CH water pressure
  {  1, OT_TEMP_INTERVAL_MS,    0 },  // Control setpoint (TSet)
  { 16, OT_TEMP_INTERVAL_MS,    0 },  // Room setpoint (TrSet)
  { 24, OT_TEMP_INTERVAL_MS,    0 },  // Room temperature (Tr)
  { 56, OT_TEMP_INTERVAL_MS,    0 },  // DHW setpoint (TdhwSet)
  { 57, OT_TEMP_INTERVAL_MS,    0 },  // Max CH water setpoint (MaxTSet)
  { 19, OT_TEMP_INTERVAL_MS,    0 },  // Boiler capacity & min modulation
  // Slow poll — config/diagnostics, 60s interval
  {  3, OT_SLOW_INTERVAL_MS,    0 },  // Slave configuration
  {  5, OT_SLOW_INTERVAL_MS,    0 },  // ASF flags / fault code
  {115, OT_SLOW_INTERVAL_MS,    0 },  // OEM fault code
  { 14, OT_SLOW_INTERVAL_MS,    0 },  // Max relative modulation setting
  { 33, OT_SLOW_INTERVAL_MS,    0 },  // Exhaust temperature
  { 34, OT_SLOW_INTERVAL_MS,    0 },  // Boiler heat exchanger temp
  {116, OT_SLOW_INTERVAL_MS,    0 },  // Burner starts
  {117, OT_SLOW_INTERVAL_MS,    0 },  // CH pump starts
  {118, OT_SLOW_INTERVAL_MS,    0 },  // DHW pump starts
  {119, OT_SLOW_INTERVAL_MS,    0 },  // DHW burner starts
  {120, OT_SLOW_INTERVAL_MS,    0 },  // Burner operation hours
  {121, OT_SLOW_INTERVAL_MS,    0 },  // CH pump operation hours
  {122, OT_SLOW_INTERVAL_MS,    0 },  // DHW pump operation hours
  {123, OT_SLOW_INTERVAL_MS,    0 },  // DHW burner operation hours
};
static constexpr uint8_t OT_SCHEDULE_SIZE = sizeof(otSchedule) / sizeof(otSchedule[0]);
static uint8_t otScheduleIdx = 0;

// Pending command from addOTWGcmdtoqueue() routed here via sendOTGW()
static bool     otCmdPending = false;
static uint32_t otCmdFrame   = 0;
static bool     otSlaveFramePending = false;
static unsigned long otSlaveFrame = 0;

enum OTDirectRequestOrigin : uint8_t {
  OT_DIRECT_ORIGIN_GATEWAY = 0,
  OT_DIRECT_ORIGIN_THERMOSTAT
};

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
    OpenThermLibMessageType::READ_DATA,
    OpenThermLibMessageID::Status,
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

  // 1. Enable step-up converter
  enableStepUp();

  // 2. Probe OT master input for idle bus
  bool busPresent = probeOTBus();
  if (busPresent) {
    DebugTln(F("OT-direct: OT bus idle detected on master input"));
  } else {
    DebugTln(F("OT-direct: WARNING — OT master input not idle, boiler may be disconnected"));
  }

  // 3. Start OpenTherm master (talks to boiler)
  otMaster.begin(masterISR);
  DebugTln(F("OT-direct: Master interface started"));

  // 4. Start OpenTherm slave (listens to thermostat)
  otSlave.begin(slaveISR, handleSlaveRequest);
  DebugTln(F("OT-direct: Slave interface started"));

  // 5. Always set OT_DIRECT mode — this is OTGW32 hardware.
  //    Bus liveness is tracked via state.otgw.bOnline, not eMode.
  //    The OT-direct loop must keep running so it can retry/recover.
  state.hw.eMode = HW_MODE_OT_DIRECT;
  DebugTln(F("OT-direct: Hardware mode set to OT_DIRECT"));

  if (!busPresent) {
    DebugTln(F("OT-direct: WARNING — OT bus not idle, boiler may be disconnected"));
  }

  // 6. Send initial status request to boiler to check connectivity
  // Blocking call is acceptable during setup() — not yet in cooperative loop
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
}

// ---------------------------------------------------------------------------
// sendMasterRequestAsync — initiate an async OT request (non-blocking)
// ---------------------------------------------------------------------------
static bool sendMasterRequestAsync(unsigned long request, OTDirectRequestOrigin origin) {
  if (!otMaster.isReady()) return false;  // bus busy — try again later
  otLastSentRequest = request;
  otLastRequestOrigin = origin;
  otMasterRequestActive = otMaster.sendRequestAync(request);
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

  // Pending command from the queue takes priority
  if (otCmdPending) {
    if (sendMasterRequestAsync(otCmdFrame, OT_DIRECT_ORIGIN_GATEWAY)) {
      otCmdPending = false;
    }
    return;
  }

  // Round-robin through the schedule table
  uint8_t startIdx = otScheduleIdx;
  do {
    OTScheduleEntry &entry = otSchedule[otScheduleIdx];
    otScheduleIdx = (otScheduleIdx + 1) % OT_SCHEDULE_SIZE;

    if ((now - entry.lastSentMs) >= entry.intervalMs) {
      unsigned long request;
      if (entry.msgId == 0) {
        request = buildStatusRequest();
      } else {
        request = OpenTherm::buildRequest(
          OpenThermLibMessageType::READ_DATA,
          static_cast<OpenThermLibMessageID>(entry.msgId),
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
  // Keep frame pending until send succeeds (retry on next loop).
  if (!otMasterRequestActive && otSlaveFramePending) {
    bridgeFrameToParser('T', otSlaveFrame);
    if (sendMasterRequestAsync(otSlaveFrame, OT_DIRECT_ORIGIN_THERMOSTAT)) {
      otSlaveFramePending = false;
    }
  }

  // Schedule periodic master requests (non-blocking, skips if bus busy)
  DECLARE_TIMER_MS(timerOTSchedule, 100, SKIP_MISSED_TICKS);
  if (DUE(timerOTSchedule)) {
    scheduleMasterRequest();
  }
}

// ---------------------------------------------------------------------------
// handleOTDirectCommand — translate PIC-style commands to OT frames
// Called from sendOTGW() when HAS_DIRECT_OT is enabled
// ---------------------------------------------------------------------------
void handleOTDirectCommand(const char* buf, int len) {
  if (len < 4) return;  // minimum "XX=Y"

  // Parse the two-letter command prefix
  char cmd0 = buf[0];
  char cmd1 = buf[1];

  // Must have '=' at position 2
  if (buf[2] != '=') return;

  const char* value = buf + 3;

  // TT=xx.x — Room setpoint / thermostat override (MsgID 16 = TrSet)
  if (cmd0 == 'T' && cmd1 == 'T') {
    float setpoint = atof(value);
    uint16_t f88 = (uint16_t)((int16_t)(setpoint * 256.0f));
    otCmdFrame = OpenTherm::buildRequest(
      OpenThermLibMessageType::WRITE_DATA,
      OpenThermLibMessageID::TrSet,
      f88
    );
    otCmdPending = true;
    if (state.debug.bOTmsg) DebugTf(PSTR("OT-direct: TT=%.1f -> frame 0x%08lX\r\n"), setpoint, otCmdFrame);
  }
  // CS=xx.x — Control setpoint (MsgID 1 = TSet)
  else if (cmd0 == 'C' && cmd1 == 'S') {
    float setpoint = atof(value);
    uint16_t f88 = (uint16_t)((int16_t)(setpoint * 256.0f));
    otCmdFrame = OpenTherm::buildRequest(
      OpenThermLibMessageType::WRITE_DATA,
      OpenThermLibMessageID::TSet,
      f88
    );
    otCmdPending = true;
    if (state.debug.bOTmsg) DebugTf(PSTR("OT-direct: CS=%.1f -> frame 0x%08lX\r\n"), setpoint, otCmdFrame);
  }
  // C2=xx.x — Control setpoint CH2 (MsgID 8 = TsetCH2)
  else if (cmd0 == 'C' && cmd1 == '2') {
    float setpoint = atof(value);
    uint16_t f88 = (uint16_t)((int16_t)(setpoint * 256.0f));
    otCmdFrame = OpenTherm::buildRequest(
      OpenThermLibMessageType::WRITE_DATA,
      OpenThermLibMessageID::TsetCH2,
      f88
    );
    otCmdPending = true;
    if (state.debug.bOTmsg) DebugTf(PSTR("OT-direct: C2=%.1f -> frame 0x%08lX\r\n"), setpoint, otCmdFrame);
  }
  // HW=0/1/A — DHW enable (modifies status flags for MsgID 0)
  else if (cmd0 == 'H' && cmd1 == 'W') {
    if (value[0] == '1' || value[0] == 'A') {
      otMasterStatusFlags |= 0x02;   // bit1 = DHW enable
      if (state.debug.bOTmsg) DebugTln(F("OT-direct: DHW enabled"));
    } else {
      otMasterStatusFlags &= ~0x02;
      if (state.debug.bOTmsg) DebugTln(F("OT-direct: DHW disabled"));
    }
  }
  // CH=0/1 — CH enable (modifies status flags for MsgID 0)
  else if (cmd0 == 'C' && cmd1 == 'H') {
    if (value[0] == '1') {
      otMasterStatusFlags |= 0x01;   // bit0 = CH enable
      if (state.debug.bOTmsg) DebugTln(F("OT-direct: CH enabled"));
    } else {
      otMasterStatusFlags &= ~0x01;
      if (state.debug.bOTmsg) DebugTln(F("OT-direct: CH disabled"));
    }
  }
  // H2=0/1 — CH2 enable (modifies status flags for MsgID 0)
  else if (cmd0 == 'H' && cmd1 == '2') {
    if (value[0] == '1') {
      otMasterStatusFlags |= 0x10;   // bit4 = CH2 enable
      if (state.debug.bOTmsg) DebugTln(F("OT-direct: CH2 enabled"));
    } else {
      otMasterStatusFlags &= ~0x10;
      if (state.debug.bOTmsg) DebugTln(F("OT-direct: CH2 disabled"));
    }
  }
  // SW=xx.x — Max DHW setpoint (MsgID 56 = TdhwSet)
  else if (cmd0 == 'S' && cmd1 == 'W') {
    float setpoint = atof(value);
    uint16_t f88 = (uint16_t)((int16_t)(setpoint * 256.0f));
    otCmdFrame = OpenTherm::buildRequest(
      OpenThermLibMessageType::WRITE_DATA,
      OpenThermLibMessageID::TdhwSet,
      f88
    );
    otCmdPending = true;
    if (state.debug.bOTmsg) DebugTf(PSTR("OT-direct: SW=%.1f -> MsgID 56 frame 0x%08lX\r\n"), setpoint, otCmdFrame);
  }
  // SH=xx.x — Max CH setpoint (MsgID 57 = MaxTSet)
  else if (cmd0 == 'S' && cmd1 == 'H') {
    float setpoint = atof(value);
    uint16_t f88 = (uint16_t)((int16_t)(setpoint * 256.0f));
    otCmdFrame = OpenTherm::buildRequest(
      OpenThermLibMessageType::WRITE_DATA,
      OpenThermLibMessageID::MaxTSet,
      f88
    );
    otCmdPending = true;
    if (state.debug.bOTmsg) DebugTf(PSTR("OT-direct: SH=%.1f -> MsgID 57 frame 0x%08lX\r\n"), setpoint, otCmdFrame);
  }
  // MM=xx — Max relative modulation level (MsgID 14)
  else if (cmd0 == 'M' && cmd1 == 'M') {
    float level = atof(value);
    uint16_t f88 = (uint16_t)((int16_t)(level * 256.0f));
    otCmdFrame = OpenTherm::buildRequest(
      OpenThermLibMessageType::WRITE_DATA,
      OpenThermLibMessageID::MaxRelModLevelSetting,
      f88
    );
    otCmdPending = true;
    if (state.debug.bOTmsg) DebugTf(PSTR("OT-direct: MM=%.0f -> MsgID 14 frame 0x%08lX\r\n"), level, otCmdFrame);
  }
  // OT=xx.x — Outside temperature (MsgID 27 = Toutside)
  else if (cmd0 == 'O' && cmd1 == 'T') {
    float temp = atof(value);
    uint16_t f88 = (uint16_t)((int16_t)(temp * 256.0f));
    otCmdFrame = OpenTherm::buildRequest(
      OpenThermLibMessageType::WRITE_DATA,
      OpenThermLibMessageID::Toutside,
      f88
    );
    otCmdPending = true;
    if (state.debug.bOTmsg) DebugTf(PSTR("OT-direct: OT=%.1f -> MsgID 27 frame 0x%08lX\r\n"), temp, otCmdFrame);
  }
  // TC=xx.x — Constant temperature override (same as CS= for OT-direct)
  else if (cmd0 == 'T' && cmd1 == 'C') {
    float setpoint = atof(value);
    uint16_t f88 = (uint16_t)((int16_t)(setpoint * 256.0f));
    otCmdFrame = OpenTherm::buildRequest(
      OpenThermLibMessageType::WRITE_DATA,
      OpenThermLibMessageID::TSet,
      f88
    );
    otCmdPending = true;
    if (state.debug.bOTmsg) DebugTf(PSTR("OT-direct: TC=%.1f -> MsgID 1 frame 0x%08lX\r\n"), setpoint, otCmdFrame);
  }
  // VS=xx — Ventilation setpoint (MsgID 71)
  else if (cmd0 == 'V' && cmd1 == 'S') {
    int level = atoi(value);
    uint16_t data = ((uint16_t)(level & 0xFF)) << 8;  // value in HB
    otCmdFrame = OpenTherm::buildRequest(
      OpenThermLibMessageType::WRITE_DATA,
      static_cast<OpenThermLibMessageID>(71),
      data
    );
    otCmdPending = true;
    if (state.debug.bOTmsg) DebugTf(PSTR("OT-direct: VS=%d -> MsgID 71 frame 0x%08lX\r\n"), level, otCmdFrame);
  }
  // PS=1/PS=0 — Print Summary mode toggle (PIC-only feature)
  else if (cmd0 == 'P' && cmd1 == 'S') {
    if (state.debug.bOTmsg) DebugTf(PSTR("OT-direct: PS command ignored (PIC-only feature)\r\n"));
  }
  // GW=R — Reset (restart OT interfaces)
  else if (cmd0 == 'G' && cmd1 == 'W' && value[0] == 'R') {
    if (state.debug.bOTmsg) DebugTln(F("OT-direct: Resetting OT interfaces"));
    otMaster.end();
    otSlave.end();
    delay(100);
    otMaster.begin(masterISR);
    otSlave.begin(slaveISR, handleSlaveRequest);
    if (state.debug.bOTmsg) DebugTln(F("OT-direct: OT interfaces restarted"));
  }
  // PIC-only commands — explicitly reject with message
  else if ((cmd0 == 'S' && cmd1 == 'B') || (cmd0 == 'A' && cmd1 == 'A') ||
           (cmd0 == 'D' && cmd1 == 'A') || (cmd0 == 'U' && cmd1 == 'I') ||
           (cmd0 == 'K' && cmd1 == 'I') || (cmd0 == 'P' && cmd1 == 'M') ||
           (cmd0 == 'S' && cmd1 == 'R') || (cmd0 == 'C' && cmd1 == 'R') ||
           (cmd0 == 'R' && cmd1 == 'S') || (cmd0 == 'I' && cmd1 == 'T') ||
           (cmd0 == 'O' && cmd1 == 'H') || (cmd0 == 'F' && cmd1 == 'T') ||
           (cmd0 == 'V' && cmd1 == 'R') || (cmd0 == 'D' && cmd1 == 'P')) {
    if (state.debug.bOTmsg) DebugTf(PSTR("OT-direct: %c%c command not supported (PIC-only)\r\n"), cmd0, cmd1);
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
