/*
***************************************************************************
**  Program  : OTDirect.ino
**  Version  : v1.4.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Direct GPIO OpenTherm driver for OTGW32 (ESP32-S3, no PIC).
**  Uses the opentherm_library (Phunkafizer fork) to drive the OT bus
**  via GPIO interrupts + hardware timer for Manchester encoding.
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
  {  0, OT_STATUS_INTERVAL_MS,  0 },  // Status (mandatory, ≤1s)
  { 25, OT_TEMP_INTERVAL_MS,    0 },  // Boiler flow temp (Tboiler)
  { 28, OT_TEMP_INTERVAL_MS,    0 },  // Return water temp (Tret)
  { 26, OT_TEMP_INTERVAL_MS,    0 },  // DHW temp (Tdhw)
  { 27, OT_TEMP_INTERVAL_MS,    0 },  // Outside temp (Toutside)
  { 17, OT_TEMP_INTERVAL_MS,    0 },  // Relative modulation level
  { 18, OT_TEMP_INTERVAL_MS,    0 },  // CH water pressure
  {  1, OT_TEMP_INTERVAL_MS,    0 },  // Control setpoint (TSet)
  {  3, OT_SLOW_INTERVAL_MS,    0 },  // Slave configuration
  {115, OT_SLOW_INTERVAL_MS,    0 },  // OEM fault code
};
static constexpr uint8_t OT_SCHEDULE_SIZE = sizeof(otSchedule) / sizeof(otSchedule[0]);
static uint8_t otScheduleIdx = 0;

// Pending command from addOTWGcmdtoqueue() routed here via sendOTGW()
static bool     otCmdPending = false;
static uint32_t otCmdFrame   = 0;

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
  // MsgID 0: Master status (HB) = flags, slave status (LB) = 0 (will be filled by boiler)
  return ((unsigned long)0 << 28)    // msg-type: READ_DATA = 0
       | ((unsigned long)0 << 16)    // reserved
       | ((unsigned long)0 << 8)     // data-id = 0
       | ((unsigned long)otMasterStatusFlags);  // master status flags in LB of request
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
  otSlave.begin(slaveISR);
  DebugTln(F("OT-direct: Slave interface started"));

  // 5. Set hardware mode
  state.hw.eMode = HW_MODE_OT_DIRECT;
  DebugTln(F("OT-direct: Hardware mode set to OT_DIRECT"));

  // 6. Send initial status request to boiler to check connectivity
  unsigned long request = buildStatusRequest();
  unsigned long response = otMaster.sendRequest(request);
  if (otMaster.isValidResponse(response)) {
    state.otgw.bOnline = true;
    DebugTln(F("OT-direct: Boiler responded — OT bus online"));
    // Bridge both request and response
    bridgeFrameToParser('T', request);
    bridgeFrameToParser('B', response);
  } else {
    state.otgw.bOnline = false;
    DebugTln(F("OT-direct: No valid boiler response — OT bus offline"));
  }
}

// ---------------------------------------------------------------------------
// scheduleMasterRequest — pick next scheduled message and send to boiler
// ---------------------------------------------------------------------------
static void scheduleMasterRequest() {
  uint32_t now = millis();

  // If there's a pending command from the queue, send it first
  if (otCmdPending) {
    otCmdPending = false;
    unsigned long response = otMaster.sendRequest(otCmdFrame);
    bridgeFrameToParser('T', otCmdFrame);
    if (otMaster.isValidResponse(response)) {
      bridgeFrameToParser('B', response);
      state.otgw.bOnline = true;
    } else {
      state.otgw.bOnline = false;
    }
    return;
  }

  // Round-robin through the schedule table
  uint8_t startIdx = otScheduleIdx;
  do {
    OTScheduleEntry &entry = otSchedule[otScheduleIdx];
    otScheduleIdx = (otScheduleIdx + 1) % OT_SCHEDULE_SIZE;

    if ((now - entry.lastSentMs) >= entry.intervalMs) {
      entry.lastSentMs = now;

      unsigned long request;
      if (entry.msgId == 0) {
        request = buildStatusRequest();
      } else {
        // READ_DATA request for the message ID
        request = ((unsigned long)0 << 28)    // READ_DATA
                | ((unsigned long)entry.msgId << 8);
      }

      unsigned long response = otMaster.sendRequest(request);
      bridgeFrameToParser('T', request);
      if (otMaster.isValidResponse(response)) {
        bridgeFrameToParser('B', response);
        state.otgw.bOnline = true;
      } else {
        // Don't immediately mark offline for a single failed request
        // Only MsgID 0 failures indicate true offline state
        if (entry.msgId == 0) {
          state.otgw.bOnline = false;
        }
      }
      return;  // One request per call
    }
  } while (otScheduleIdx != startIdx);
}

// ---------------------------------------------------------------------------
// loopOTDirect — called from doBackgroundTasks() on OTGW32 builds
// ---------------------------------------------------------------------------
void loopOTDirect() {
  // Process OpenTherm library state machines
  otMaster.process();
  otSlave.process();

  // Handle incoming thermostat frames (slave mode)
  if (otSlave.hasMessage()) {
    unsigned long thermostatFrame = otSlave.getMessage();
    bridgeFrameToParser('T', thermostatFrame);

    // In repeater mode: forward thermostat request to boiler
    unsigned long boilerResponse = otMaster.sendRequest(thermostatFrame);
    if (otMaster.isValidResponse(boilerResponse)) {
      bridgeFrameToParser('B', boilerResponse);
      otSlave.sendResponse(boilerResponse);
      state.otgw.bOnline = true;
    }
  }

  // Schedule periodic master requests (status, temps, etc.)
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

  // TT=xx.x — Control setpoint (MsgID 1)
  if (cmd0 == 'T' && cmd1 == 'T') {
    float setpoint = atof(value);
    int16_t fixed = (int16_t)(setpoint * 256.0f);
    unsigned long frame = ((unsigned long)1 << 28)   // WRITE_DATA
                        | ((unsigned long)1 << 8)    // MsgID 1 = TSet
                        | ((unsigned long)((fixed >> 8) & 0xFF) << 8)
                        | ((unsigned long)(fixed & 0xFF));
    // Correct: pack data value in low 16 bits
    frame = ((unsigned long)1 << 28)  // WRITE_DATA
          | ((unsigned long)1 << 8)   // MsgID 1
          | (((uint16_t)fixed) & 0xFFFF);
    otCmdFrame = frame;
    otCmdPending = true;
    OTGWDebugTf(PSTR("OT-direct: TT=%.1f -> frame 0x%08lX\r\n"), setpoint, frame);
  }
  // CS=xx.x — Control setpoint CH2 (MsgID 8)
  else if (cmd0 == 'C' && cmd1 == 'S') {
    float setpoint = atof(value);
    int16_t fixed = (int16_t)(setpoint * 256.0f);
    unsigned long frame = ((unsigned long)1 << 28)  // WRITE_DATA
                        | ((unsigned long)8 << 8)   // MsgID 8
                        | (((uint16_t)fixed) & 0xFFFF);
    otCmdFrame = frame;
    otCmdPending = true;
    OTGWDebugTf(PSTR("OT-direct: CS=%.1f -> frame 0x%08lX\r\n"), setpoint, frame);
  }
  // HW=0/1/A — DHW enable (modifies status flags for MsgID 0)
  else if (cmd0 == 'H' && cmd1 == 'W') {
    if (value[0] == '1' || value[0] == 'A') {
      otMasterStatusFlags |= 0x02;   // bit1 = DHW enable
      OTGWDebugTln(F("OT-direct: DHW enabled"));
    } else {
      otMasterStatusFlags &= ~0x02;
      OTGWDebugTln(F("OT-direct: DHW disabled"));
    }
  }
  // CH=0/1 — CH enable (modifies status flags for MsgID 0)
  else if (cmd0 == 'C' && cmd1 == 'H') {
    if (value[0] == '1') {
      otMasterStatusFlags |= 0x01;   // bit0 = CH enable
      OTGWDebugTln(F("OT-direct: CH enabled"));
    } else {
      otMasterStatusFlags &= ~0x01;
      OTGWDebugTln(F("OT-direct: CH disabled"));
    }
  }
  // PS=1/PS=0 — Print Summary mode toggle (no OT frame, handled by processOT)
  else if (cmd0 == 'P' && cmd1 == 'S') {
    // No-op for OT-direct; PS mode is a PIC firmware feature
    OTGWDebugTf(PSTR("OT-direct: PS command ignored (PIC-only feature)\r\n"));
  }
  // GW=R — Reset (restart OT interfaces)
  else if (cmd0 == 'G' && cmd1 == 'W' && value[0] == 'R') {
    OTGWDebugTln(F("OT-direct: Resetting OT interfaces"));
    otMaster.end();
    otSlave.end();
    delay(100);
    otMaster.begin(masterISR);
    otSlave.begin(slaveISR);
    OTGWDebugTln(F("OT-direct: OT interfaces restarted"));
  }
  else {
    OTGWDebugTf(PSTR("OT-direct: Unknown command [%c%c=%s] ignored\r\n"), cmd0, cmd1, value);
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
