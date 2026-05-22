# C4 Code Level: OTDirect Module (ESP32 Native OpenTherm Master/Slave)

## Overview

- **Name**: OTDirect Module
- **Description**: Direct GPIO-based OpenTherm protocol implementation for OTGW32 (ESP32-S3), providing master/slave mode capability without the PIC gateway. Handles boiler communication (master), thermostat communication (slave), frame bridging to the existing OT parser, and comprehensive command emulation matching the original PIC firmware interface.
- **Location**: `/src/OTGW-firmware/OTDirect.ino`
- **Language**: Arduino C++ (ESP32)
- **Purpose**: Replace PIC gateway for native OpenTherm protocol handling on ESP32, supporting bidirectional communication (boiler master, thermostat slave), operating modes (gateway/monitor/bypass/master/loopback), and full PIC command compatibility via text-based serial API.

## Architecture

The OTDirect module operates as a cooperative OpenTherm stack layered on the project's existing asynchronous framework:

1. **Hardware Interface**: Two independent OpenTherm instances (master and slave) using GPIO pins and ISR-based Manchester encoding
2. **Asynchronous Scheduling**: Non-blocking frame scheduler with configurable polling intervals (status every 800ms, temps every 10s, slow queries every 60s)
3. **Frame Bridging**: 32-bit OT frames formatted as 9-char hex strings ("B%08lX") and fed to `processOT()`, reusing the entire existing parser/MQTT/REST/WebSocket stack
4. **Command Emulation**: Text-based command interface (XX=value) matching PIC firmware, translating to OpenTherm frames or local state changes
5. **Operating Modes**: Five distinct modes (bypass, gateway, monitor, master, loopback) with different forwarding/override behaviors

### Key Design Patterns

- **Non-Blocking Async**: Uses `OpenTherm::sendRequestAsync()` with completion checks in `loopOTDirect()` — never blocks the scheduler
- **Ring Buffers**: Schedule table (66 entries), command queue (12 frames, **coalesce-by-MsgID** — TASK-494, repeated set commands for the same MsgID overwrite the pending entry instead of stacking), override tables (8 repeater including MsgID 100 for TT/TC flag synthesis, 16 response, 8 response-modify), unknown-ID blacklist (16 entries)
- **3-Strike Auto-Blacklist**: MsgIDs that return UNKNOWN_DATA_ID three times are auto-disabled from polling
- **Periodic Write Cache**: Boiler setpoints are written every 15s to keep values alive if thermostat disconnects
- **Repeater Overrides**: Thermostat frames can be intercepted and their data values replaced before forwarding to boiler (gateway mode)
- **Response Modifiers**: Boiler responses can be intercepted and their data bytes modified before forwarding to thermostat
- **Thermostat Timeout & Setback**: Fail-safe mechanism applies room setpoint override when thermostat offline for >N seconds
- **PI Room Compensation (TASK-183)**: Weather-compensated heating curve with PI controller feedback loop from room temperature
- **Flame Ratio Tracking (TASK-184)**: 3-hour rolling window of flame on/off duty cycle and transition frequency

## Code Elements

### Initialization & Setup

#### `void initOTDirect()`
- **Location**: OTDirect.ino:484
- **Purpose**: Initialize OT-direct hardware, master/slave interfaces, load persisted settings, probe OT bus, send protocol handshake to boiler, auto-detect thermostat
- **Behavior**:
  - Restores operating mode from `settings.otd.iMode` (or forces gateway if bypass disabled)
  - Initializes bypass relay (if present) and step-up 24V converter
  - Probes master input for idle bus (≥70% HIGH samples)
  - Starts master interface with `otMaster.begin(masterISR)`
  - Starts slave interface with `otSlave.begin(slaveISR, handleSlaveRequest)` (conditional on mode)
  - Sends protocol handshake: MsgID 2 (master config), 3 (slave config query), 124 (OT version 2.2), 126 (product info)
  - Auto-detects thermostat if enabled: waits 5s for MsgID 0 frame on slave bus; switches to master mode if none received

#### ISR Handlers (static)
- **Location**: OTDirect.ino:31-32
- **`static void IRAM_ATTR masterISR()`** — Calls `otMaster.handleInterrupt()` (delegates to OpenTherm library)
- **`static void IRAM_ATTR slaveISR()`** — Calls `otSlave.handleInterrupt()` (delegates to OpenTherm library)

### Master (Boiler) Communication

#### `void loopOTDirect()`
- **Location**: OTDirect.ino:1456
- **Purpose**: Main cooperative loop called from `doBackgroundTasks()` (every iteration, non-blocking)
- **Behavior**:
  - Calls `otMaster.process()` and `otSlave.process()` to drive ISR-based TX/RX state machines
  - Checks if async master request completed (`otMasterRequestActive && otMaster.isReady()`), calls `handleMasterResponse()`
  - Detects thermostat connectivity from pending slave frames
  - Handles pending thermostat frames based on mode:
    - **Master mode**: `handleMasterModeSlaveFrame()` — respond from cache instead of forwarding
    - **Monitor mode**: Forward unmodified without overrides
    - **Gateway mode**: Check UI/SR tables, apply value overrides, forward to boiler
  - Schedules next periodic master request (every 100ms, skips if in monitor mode)
  - Emits PS=1 summary line if pending
  - Updates state.otd stats once per second
  - Runs PI room compensation every 60s
  - Publishes flame ratio metrics every 60s

#### `static bool sendMasterRequestAsync(unsigned long request, OTDirectRequestOrigin origin)`
- **Location**: OTDirect.ino:891
- **Purpose**: Initiate non-blocking OT master request (poll boiler or forward thermostat frame)
- **Parameters**:
  - `request`: 32-bit OpenTherm frame
  - `origin`: `OT_DIRECT_ORIGIN_GATEWAY` (scheduled poll) or `OT_DIRECT_ORIGIN_THERMOSTAT` (forwarded frame)
- **Behavior**:
  - Returns `false` if master busy (collision avoidance) or loopback mode handles it instantly
  - Calls `otMaster.sendRequestAsync(request)` to initiate non-blocking send
  - Bridges request frame to parser (prefix 'T' for thermostat, 'R' for gateway)
  - Updates `otLastAnySendMs` for MI= (message interval) gap tracking
  - Returns `true` if async initiated successfully

#### `static void handleMasterResponse()`
- **Location**: OTDirect.ino:925
- **Purpose**: Process completed async master request (called from `loopOTDirect()` when ready)
- **Behavior**:
  - Gets response via `otMaster.getLastResponse()` and status via `otMaster.getLastResponseStatus()`
  - On SUCCESS:
    - Bridges response frame to parser (prefix 'B')
    - Sets `state.otBus.bOnline = true`
    - Caches boiler response in `otBoilerCache[cacheId]` for master-mode slave responses
    - Updates flame ratio state from MsgID 0 slave status (bit 3 = flame active)
    - Implements 3-strike auto-blacklist: increments 2-bit counter for UNKNOWN_DATA_ID; disables schedule entry on third strike
    - Monitors DHW push state machine (MsgID 0): transitions PENDING→STARTED→IDLE, detects timeout
    - Triggers PS=1 summary emission if in summary mode
    - Forwards response to thermostat slave if request was from thermostat (applies response modifiers in gateway mode)
  - On failure: Sets `state.otBus.bOnline = false` only for status request (MsgID 0)
  - Clears `otMasterRequestActive` flag

#### `static void scheduleMasterRequest()`
- **Location**: OTDirect.ino:1026
- **Purpose**: Pick next message to send (priority: commands, then schedule round-robin)
- **Behavior**:
  - Returns early if master busy (`otMasterRequestActive`)
  - Returns early if MI= minimum gap not satisfied
  - Dequeues and sends commands from ring buffer (command queue takes priority)
  - Round-robins through schedule table (`otScheduleIdx`), skipping disabled entries:
    - Status (MsgID 0): always ready, uses `buildStatusRequest()` with current flags
    - Read entries: sent on interval with READ_DATA
    - Write entries: sent only if `valueSet=true` (cache populated), uses cached value with WRITE_DATA
  - Updates `entry.lastSentMs` only on successful async send (timer advances only on send, not on collision)

#### `unsigned long buildStatusRequest()`
- **Location**: OTDirect.ino:447
- **Purpose**: Construct MsgID 0 status request with current master flags
- **Returns**: READ_DATA frame with master status flags in HB
- **Details**: Uses `otMasterStatusFlags` (bits: 0=CH, 1=DHW, 2=cooling, 4=CH2, 5=summer, 6=DHW blocking)

### Slave (Thermostat) Communication

#### `static void handleSlaveRequest(unsigned long request, OpenThermResponseStatus status)`
- **Location**: OTDirect.ino:427
- **Purpose**: Callback when thermostat sends frame on slave bus
- **Behavior**:
  - If status is SUCCESS: stores frame in `otSlaveFrame` and sets `otSlaveFramePending = true`
  - Called from OpenTherm library ISR context

#### `static void handleMasterModeSlaveFrame(unsigned long frame)`
- **Location**: OTDirect.ino:1824
- **Purpose**: In master mode (standalone), respond to thermostat as if we ARE the boiler
- **Behavior**:
  - Extracts msgId and msgType (bits 28-30)
  - If READ_DATA (type 0):
    - Returns cached boiler value via `buildOTResponse(4, msgId, cache)` (READ_ACK)
    - Returns UNKNOWN_DATA_ID (type 7) if no cached value yet
  - If WRITE_DATA (type 1):
    - Echoes back as WRITE_ACK (type 5)
    - Updates write cache for MsgID 1 (TSet), 16 (room setpoint), 24 (room temp) so scheduler keeps sending to boiler
  - Otherwise: Returns UNKNOWN_DATA_ID
  - Bridges frame and response to parser ('T' + 'A' prefixes)
  - Sends response via `otSlave.sendResponse()`

### Frame Bridging & Parsing

#### `static void bridgeFrameToParser(char prefix, unsigned long frame)`
- **Location**: OTDirect.ino:437
- **Purpose**: Format 32-bit OT frame and feed to `processOT()` for parsing/logging
- **Behavior**:
  - Returns early if `otHideReports` is true (PS=1 mode suppresses individual frames)
  - Formats as 9-char string: "P%08lX" (prefix + 8-digit hex)
  - Calls `processOT(buf, 9)` which reuses entire existing stack (OT parser, MQTT, REST, WebSocket, HA discovery)
- **Prefixes**:
  - 'T' = thermostat frame
  - 'R' = gateway request
  - 'B' = boiler response
  - 'A' = gateway answer (direct response without boiler)

#### `static void emitSummaryLine()`
- **Location**: OTDirect.ino:1087
- **Purpose**: PS=1 mode: emit comma-separated summary matching PIC format (34 fields)
- **Behavior**:
  - Appends MsgID values in order: 0,1,6,7,8,14,15,16,17,18,19,23,24,25,26,27,28,31,33,48,49,56,57,70,71,77,116-123
  - Formats binary status as "BBBBBBBB/BBBBBBBB" (HB/LB)
  - Formats floats with 2 decimal places, signed floats with +/- prefix
  - Reads from `OTcurrentSystemState` (populated by parser)
  - Calls `processOT(line, pos)` to emit single-line output

### Command Handler

#### `void handleOTDirectCommand(const char* buf, int len)`
- **Location**: OTDirect.ino:1923
- **Purpose**: Process PIC-style text commands (XX=value) and translate to OT frames/state changes
- **Behavior**:
  - Parses buf[0-1] as command code, buf[2] must be '=', buf[3+] is value
  - Implements full PIC command set across 9 categories:

**Category 1: Setpoint Write Commands** (enqueue + cache + override)
- `CS=xx.x` — Control setpoint (MsgID 1, TSet). Value 0 clears.
- `TT=xx.x` — Temporary thermostat-side room override (MsgID 16, TrSet + MsgID 100 RemoteOverrideFunction priority bit 0x02). Auto-clears on next thermostat broadcast unless honoured (TASK-466 PIC-parity semantics). Value 0 clears.
- `TC=xx.x` — Constant thermostat-side room override (MsgID 16, TrSet + MsgID 100 RemoteOverrideFunction priority bit 0x01). Persists until cleared (TASK-466 PIC-parity semantics). Value 0 clears.
- `BS=xx.x` — Fake boiler room setpoint (intercepts thermostat MsgID 16 frame, replaces data bytes before forwarding). Value 0 clears.
- `C2=xx.x` — Control setpoint CH2 (MsgID 8, TsetCH2)
- `CC=xx.x` — Cooling control (MsgID 7, 0-100%)
- `SW=xx.x` — DHW setpoint (MsgID 56, TdhwSet)
- `SH=xx.x` — Max CH water setpoint (MsgID 57, MaxTSet)
- `MM=xx` — Max relative modulation (MsgID 14)
- `OT=xx.x` — Outside temperature (MsgID 27, Toutside)
- `VS=xx` — Ventilation setpoint (MsgID 71)
- `SC=HH:MM/DOW` — Time sync (MsgID 20)

**Category 2: Status Flag Commands** (immediate effect on MsgID 0 master status byte)
- `HW=0/1/A/P` — DHW enable + DHW push state machine
- `CH=0/1` — CH enable (bit 0)
- `H2=0/1` — CH2 enable (bit 4)
- `SM=0/1` — Summer mode (bit 5, persisted)
- `BW=0/1` — DHW blocking (bit 6)
- `CE=0/1` — Cooling enable (bit 2)
- `CL=xx.x` — Cooling level (alias for CC=)

**Category 3: Operating Mode Commands** (MsgID 99)
- `MH=x` — Operating mode CH1 (0-15, lower nibble of byte4)
- `MW=x` — Operating mode DHW (0-15, lower nibble of byte3)
- `M2=x` — Operating mode CH2 (0-15, upper nibble of byte4)
- `RR=x` — Remote request (MsgID 4, one-shot WRITE_DATA)

**Category 4: Gateway Control**
- `GW=0` — Bypass mode (relay direct thermostat to boiler)
- `GW=1` — Gateway mode (full override processing, slave interface enabled)
- `GW=2` / `GW=S` — Master mode (sole OT master, thermostat optional)
- `GW=M` — Monitor mode (transparent pass-through, no overrides)
- `GW=L` — Loopback test mode (simulated boiler responses)
- `GW=R` — Full reset (clears transient state, restarts OT interfaces)

**Category 5: PR= Register Queries** (read-only responses)
- `PR=A` → "OpenTherm Gateway OTGW32"
- `PR=M` → Mode char (G/P/M/S/L)
- `PR=O` → Override status (N/C value/T value)
- `PR=S` → Setback temp
- `PR=W` → DHW override (A/0/1)
- `PR=G` / `PR=I` / `PR=L` / `PR=T` / `PR=D` / `PR=P` / `PR=R` / `PR=B` / `PR=C` / `PR=Q` / `PR=N` / `PR=V` → Various status

**Category 6: PS= Summary Mode**
- `PS=1` — Enable (suppress individual frames, emit summary per MsgID 0)
- `PS=0` — Disable (resume normal frame output)

**Category 7: Schedule Management**
- `AA=MsgID` — Re-enable disabled schedule entry
- `DA=MsgID` — Disable schedule entry
- `PM=MsgID` — Priority: send immediately on next cycle (or one-shot READ if not in schedule)

**Category 8: Override Tables**
- `SR=MsgID:HHHH` — Set response override (gateway answers thermostat with stored value)
- `CR=MsgID` — Clear response override
- `UI=MsgID` — Mark as unknown (respond UNKNOWN_DATA_ID to thermostat)
- `KI=MsgID` — Mark as known again (restore normal forwarding)
- `RM=MsgID:HHHH` — Set response modifier (modify boiler→thermostat response data bytes)
- `CM=MsgID` — Clear response modifier

**Category 9: Configuration & State**
- `SB=xx.x` — Setback temperature (engaged when thermostat times out)
- `IT=0/1` — Ignore transitions
- `OH=0/1` — Override high byte
- `RS=HBS/...` — Reset boiler counter (WRITE_DATA with value 0)
- `MI=nnn` — Message interval (100-1275ms, 10ms units)
- `FS=0/1` — Fail safety (controls setback on disconnect)
- `TP=MsgID:Index[=Value]` — Thermostat slave parameters (TSP/FHB)
- `GA=x` / `GB=x` — GPIO function codes (no-op, store locally)
- `LA=-LF=` — LED function config (no-op, store locally)
- `VR=x` — Voltage reference (no-op, store locally)
- `TS=O/R` — Temperature sensor function (no-op, store locally)
- `FT=x` — Force thermostat detection (no-op, store locally)
- `DP=x` — Debug pointer (no-op, acknowledge)

#### `static void enqueueWriteCommand(uint8_t msgId, uint16_t dataValue, const char* label)`
- **Location**: OTDirect.ino:1579
- **Purpose**: Helper to build frame, enqueue, update cache, and activate override
- **Behavior**:
  - Builds WRITE_DATA frame via `OpenTherm::buildRequest()`
  - Enqueues in ring buffer (returns false if queue full, logs error)
  - Updates periodic write cache via `updateWriteCache()` so scheduler keeps sending value to boiler
  - Activates repeater override via `setOverride()` so thermostat frames for this MsgID get modified
  - Logs frame if debug mode enabled

#### `static void synthesizeResponse(char c0, char c1, const char* value)`
- **Location**: OTDirect.ino:1602
- **Purpose**: Format PIC-style response and feed to `processOT()` for command confirmation
- **Behavior**:
  - Formats as "XX: value" (e.g., "CS: 50.25")
  - Calls `processOT()` to emit response, triggering command queue clearing, MQTT confirmations, WebSocket events, settings updates

### Response Path Control

#### `static unsigned long applyResponseModifiers(unsigned long response)`
- **Location**: OTDirect.ino:1903
- **Purpose**: Modify boiler response data bytes before forwarding to thermostat (gateway mode only)
- **Behavior**:
  - Searches `otResponseModifiers[]` table for active entry matching response msgId
  - Replaces lower 16 bits (data value) while keeping message type and msgId
  - Recalculates parity via `setOTParityBit()`
  - Returns modified response

#### `static void setResponseModifier(uint8_t msgId, uint16_t value)`
- **Location**: OTDirect.ino:1870
- **Purpose**: Register or update response modifier for a MsgID

#### `static void clearResponseModifier(uint8_t msgId)`
- **Location**: OTDirect.ino:1889
- **Purpose**: Remove response modifier for a MsgID

#### `static void clearAllResponseModifiers()`
- **Location**: OTDirect.ino:1897
- **Purpose**: Clear all response modifiers (called on reset)

### Override Processing (Repeater Mode)

#### `static unsigned long applyOverrides(unsigned long frame, bool &modified)`
- **Location**: OTDirect.ino:390
- **Purpose**: Intercept thermostat frame and replace data value if override active for that MsgID
- **Behavior**:
  - Searches `otOverrides[]` table for active entry matching msgId
  - Replaces lower 16 bits (data value) while keeping message type and msgId
  - Recalculates parity
  - Sets `modified = true` if changed (triggers bridge output of original 'T' frame and modified 'R' frame)
  - Returns modified frame

#### `static void setOverride(uint8_t msgId, uint16_t value)`
- **Location**: OTDirect.ino:295
- **Purpose**: Activate repeater override for a MsgID

#### `static void clearOverride(uint8_t msgId)`
- **Location**: OTDirect.ino:306
- **Purpose**: Deactivate repeater override for a MsgID

### Operating Mode Management

#### `static void setOTDirectMode(OTDirectMode newMode)`
- **Location**: OTDirect.ino:1711
- **Purpose**: Switch operating mode with hardware reconfiguration
- **Parameters**: `OTD_MODE_BYPASS`, `OTD_MODE_GATEWAY`, `OTD_MODE_MONITOR`, `OTD_MODE_MASTER`, `OTD_MODE_LOOPBACK`
- **Behavior**:
  - Calls `resetTransientState()` to clear non-persisted state
  - Manages bypass relay: HIGH for bypass, LOW for other modes
  - Conditionally starts/stops slave interface based on mode:
    - Bypass: skips slave
    - Gateway/Monitor: ensures slave running
    - Master: starts if `bEnableSlave=true`, stops otherwise
    - Loopback: ensures slave running
  - Persists mode to settings via `updateSetting("OTDmode", ...)`

#### `static void resetTransientState()`
- **Location**: OTDirect.ino:1644
- **Purpose**: Clear all non-persisted transient state (called on mode change or GW=R reset)
- **Behavior**:
  - Resets operating mode overrides (MH, MW, M2)
  - Clears DHW push state machine
  - Disables summary mode
  - Resets thermostat connectivity tracking
  - Clears schedule disable flags (re-enables all entries)
  - Clears unknown-ID list and counters
  - Clears response overrides and write caches
  - Clears repeater overrides
  - Clears response modifiers
  - Resets setback engagement
  - Resets PI controller state (TASK-183)
  - Clears fake room setpoint (BS=)

#### `static void checkThermostatTimeout()`
- **Location**: OTDirect.ino:1775
- **Purpose**: Detect thermostat disconnect and apply fail-safe setback (called once per second)
- **Behavior**:
  - Returns if no thermostat frame ever seen
  - Compares elapsed time since last frame against `settings.otd.iSetbackTimeout`
  - On timeout:
    - Sets `state.otd.bThermostatConnected = false`
    - If fail-safe enabled and in gateway mode: engages setback by overriding MsgID 1 (TSet) to `settings.otd.fSetbackTemp`
  - On reconnect: disables setback, releases TSet override

### Loopback Test Mode

#### `static unsigned long simulateLoopbackResponse(unsigned long request)`
- **Location**: OTDirect.ino:869
- **Purpose**: Generate fake boiler response for loopback testing (no hardware needed)
- **Behavior**:
  - Reads simulated value from `otLoopbackData[]` (128 entries indexed by msgId)
  - WRITE_DATA (type 1): accepts write, echoes as WRITE_ACK with same data
  - READ_DATA (type 0): returns READ_ACK with simulated value or UNKNOWN_DATA_ID if 0xFFFF
  - Returns response with correct type and parity

#### `static const uint16_t PROGMEM otLoopbackData[128]`
- **Location**: OTDirect.ino:747
- **Purpose**: Simulated boiler data table for loopback mode
- **Values**: Realistic f8.8 temperatures, modulation levels, counters, etc.; 0xFFFF for unsupported MsgIDs

### Room Temperature Compensation (TASK-183)

#### `float getFlowTemp()`
- **Location**: OTDirect.ino:1201
- **Purpose**: Compute target flow temperature from weather-compensated heating curve + PI correction
- **Behavior**:
  - Returns 0.0 if CH is off
  - Fixed-flow mode: returns `settings.otd.fFlowTemp`
  - Auto mode (heating curve):
    - Reads outside temp from `otBoilerCache[27]` (MsgID 27, Toutside, f8.8)
    - Applies OT-Thing curve formula: `flow = rsp + c1 * pow(rsp - outTmp, 1/exponent) + offset`
    - Parameters from settings: `fFlowMax`, `fGradient`, `fExponent`, `fOffset`, `fRoomSetpoint`
  - Adds PI correction term (`otPiDeltaT`) if room compensation enabled
  - Clips result to [0, fFlowMax]

#### `static void loopPiCtrl()`
- **Location**: OTDirect.ino:1254
- **Purpose**: Run PI room temperature compensation loop (called every 60s)
- **Behavior**:
  - Reads room temp (MsgID 24) and room setpoint (MsgID 16) from boiler cache
  - Applies low-pass filter (0.1x new + 0.9x previous) to room temperature
  - Computes proportional term: `p = Kp * (rsp - rt)`
  - Computes integral term: accumulates error weighted by heating active flag; clipped to [-5, +5] K
  - Computes boost term: extra push when significantly cold (error > 1K)
  - Clips total correction `otPiDeltaT` to [-5, +12] K
  - Logs debug info if `state.debug.bOTmsg` enabled
  - Forces immediate write of MsgID 1 (TSet) on next scheduler cycle

### Flame Ratio Tracking (TASK-184)

#### `static void initFlameRatio()`
- **Location**: OTDirect.ino:1358
- **Purpose**: Initialize flame tracking ring buffers (called from `initOTDirect()`)

#### `static void flameRatioSet(bool flame)`
- **Location**: OTDirect.ino:1382
- **Purpose**: Called when flame state changes (from MsgID 0 response, bit 3)
- **Behavior**:
  - On first transition: pre-fills on-time buffer with 60 (sensible default for first incomplete minute)
  - Counts rising edges as one cycle
  - Accumulates elapsed on-time in current accumulator

#### `static void loopFlameRatio()`
- **Location**: OTDirect.ino:1411
- **Purpose**: Commit ring buffer every 60s and publish to MQTT
- **Behavior**:
  - Every 60 seconds: finalizes on-time for current minute, commits buffers, advances ring index
  - Publishes `otgw32/flame_duty_pct` (0-100) and `otgw32/flame_cycles_per_hour` (decimal) to MQTT

#### `uint8_t getFlameRatioDuty()`
- **Location**: OTDirect.ino:1398
- **Returns**: Flame on-time percentage over 3-hour window (0-100%)

#### `float getFlameRatioFreq()`
- **Location**: OTDirect.ino:1404
- **Returns**: Flame cycle frequency (cycles/hour, one decimal place)

### MQTT-Sourced Input Routing (TASK-185)

#### `void otdMqttSetRoomTemp(float tempC)`
- **Location**: OTDirect.ino:1437
- **Purpose**: Route room temperature from MQTT into MsgID 24 write cache
- **Behavior**:
  - Validates range [-40, 127]°C
  - Converts to f8.8 format
  - Enqueues WRITE_DATA frame via `enqueueWriteCommand()`

#### `void otdMqttSetRoomSetpoint(float tempC)`
- **Location**: OTDirect.ino:1445
- **Purpose**: Route room setpoint from MQTT into MsgID 16 write cache
- **Behavior**:
  - Validates range [-40, 127]°C
  - Converts to f8.8 format
  - Enqueues WRITE_DATA frame

### Status & JSON Serialization

#### `void updateOTDirectStatus()`
- **Location**: OTDirect.ino:672
- **Purpose**: Refresh `state.otd` with current stats (called once per second)
- **Updates**:
  - `iScheduleTotal`, `iScheduleActive`, `iScheduleDisabled` (counts from schedule table)
  - `iOverrideCount` (active repeater overrides)
  - `bBypassActive`, `bStepUpEnabled`, `bMonitorMode`, `bMasterMode`
  - `eMode`, `iLastThermostatMs`

#### `int getOTDirectOverridesJSON(char* buf, size_t bufSize)`
- **Location**: OTDirect.ino:699
- **Purpose**: Serialize all active overrides into JSON for REST API
- **Format**: `{"overrides":{"write":[...], "response":[...], "modify":[...], "unknown":[...]}}`
- **Returns**: Bytes written

### Data Structures & State Variables

#### OpenTherm Interface Instances
- **Location**: OTDirect.ino:27-28
- `static OpenTherm otMaster(PIN_OT_MASTER_IN, PIN_OT_MASTER_OUT)` — Master (boiler)
- `static OpenTherm otSlave(PIN_OT_SLAVE_IN, PIN_OT_SLAVE_OUT, true)` — Slave (thermostat), isSlave=true

#### Schedule Entry
```cpp
struct OTScheduleEntry {
  uint8_t  msgId;              // Message ID 0-127
  uint32_t intervalMs;         // Poll interval
  uint32_t lastSentMs;         // Timestamp of last send
  bool     disabled;           // Auto-set on 3× UNKNOWN_DATA_ID
  bool     isWrite;            // true=periodic WRITE, false=READ
  bool     valueSet;           // true=cache populated (write entries only)
  uint16_t cachedValue;        // f8.8 or raw
};
static OTScheduleEntry otSchedule[OT_SCHEDULE_SIZE] // 66 entries
```

#### Frame Override (Repeater Mode)
```cpp
struct OTFrameOverride {
  uint8_t  msgId;
  bool     active;
  uint16_t overrideValue;      // Data to inject into thermostat frame
};
static OTFrameOverride otOverrides[] // 8 entries: MsgID 1,7,8,14,16,56,57,100 (100=RemoteOverrideFunction flag bits, TASK-466)
```

**TT=/TC= remote-override state machine (TASK-466, PIC-parity)**: MsgID 100 (`RemoteOverrideFunction`) is a UX hint published to the thermostat alongside the MsgID 16 setpoint inject:
- TT (temporary): MsgID 100 low byte = `0x02` (priority bit, thermostat may auto-clear on next broadcast)
- TC (constant): MsgID 100 low byte = `0x01` (priority bit, thermostat must honor until cleared)
- `clearRemoteOverride()` (CS=0, TT=0, TC=0) drops both the MsgID 16 inject and the MsgID 100 flag.

The boiler ignores MsgID 100 and only sees MsgID 16 — MsgID 100 is purely a hint to the thermostat. If the MsgID 16 enqueue succeeds but MsgID 100 hits a full queue, the partial state is accepted: boiler-side override stays in effect, thermostat-side hint catches up on the next attempt. The state machine variable `otRemoteOverride` tracks the active mode (`OT_OVERRIDE_NONE`/`TEMPORARY`/`CONSTANT`), the f8.8 value, the timestamp it was set, and whether the thermostat has been observed to honor it (`honoredCount`, `bHaveLastThermostat`).

#### Response Override
```cpp
struct OTResponseOverride {
  uint8_t  msgId;
  bool     active;
  uint16_t value;              // Data to return directly to thermostat
};
static OTResponseOverride otResponseOverrides[OT_RESPONSE_OVERRIDE_MAX] // 16 entries
```

#### Response Modifier
```cpp
struct OTResponseModify {
  uint8_t  msgId;
  bool     active;
  uint16_t value;              // Data to substitute in boiler response
};
static OTResponseModify otResponseModifiers[OT_RESPONSE_MODIFY_MAX] // 8 entries
```

#### Flame Ratio Buffer
```cpp
struct FlameRatioBuf {
  uint8_t buf[FLAME_BUF_SIZE]; // 180 slots (3 hours)
  uint32_t sum;                // Running sum for current ratio
  uint8_t  current;            // Accumulator for current minute
};
static struct {
  FlameRatioBuf on;            // Seconds of flame-on per minute
  FlameRatioBuf cycles;        // Flame-on transitions per minute
  uint8_t  idx;                // Current ring buffer index
  bool     currentFlame;       // Last observed flame state
  bool     init;               // true once first minute completes
  uint32_t lastEdge;           // millis() of last transition
  uint32_t lastInc;            // millis() of last 1-minute increment
} otFlame
```

#### Command Queue (Ring Buffer)
```cpp
static constexpr uint8_t OT_CMD_QUEUE_SIZE = 12;
static uint32_t otCmdQueue[OT_CMD_QUEUE_SIZE];
static uint8_t  otCmdHead = 0;   // Write position
static uint8_t  otCmdTail = 0;   // Read position
static uint8_t  otCmdQueueHighWater = 0;  // TASK-494: peak depth observed
```

**Coalesce-by-MsgID (TASK-494)**: `enqueueCommand()` walks the pending entries for any frame with the same MsgID and overwrites it in place rather than appending. This is the only correct behavior for the rapid `set MsgID 100=X` / `set MsgID 100=Y` sequences MQTT can produce — the older value is stale by definition; appending stacks would saturate the queue and surface as a queue-full error even when the actual demand is one slot. High-water (`otCmdQueueHighWater`) is published as a diagnostic so a genuine saturation versus a stale-value churn can be told apart at runtime.

#### Boiler Response Cache
```cpp
static uint16_t otBoilerCache[128];       // Indexed by MsgID
static bool     otBoilerCacheValid[128];  // Validity flags
```

#### Unknown-ID 3-Strike Counter
```cpp
static uint8_t otUnknownCounters[32];     // 128 MsgIDs × 2 bits = 32 bytes
```

### Utility Functions

#### `static bool probeOTBus()`
- **Location**: OTDirect.ino:470
- **Purpose**: Check if OT master input shows idle (HIGH) state
- **Returns**: true if ≥70% of 10 samples are HIGH

#### `static void enableStepUp()`
- **Location**: OTDirect.ino:460
- **Purpose**: Enable 24V step-up converter for OT slave bus

#### `static unsigned long buildOTResponse(uint8_t type, uint8_t msgId, uint16_t data)`
- **Location**: OTDirect.ino:1818
- **Purpose**: Build OT response frame with parity
- **Parameters**: type (4=READ_ACK, 5=WRITE_ACK, 7=UNKNOWN_DATAID)

#### `static inline void setOTParityBit(unsigned long &frame)`
- **Location**: OTDirect.ino:63
- **Purpose**: Set bit 31 to make total number of 1-bits even

#### `static void updateWriteCache(uint8_t msgId, uint16_t value)`
- **Location**: OTDirect.ino:406
- **Purpose**: Update periodic write cache for a MsgID in schedule table

#### `static void clearWriteOverride(uint8_t msgId)`
- **Location**: OTDirect.ino:1630
- **Purpose**: Clear both repeater override and write cache for a MsgID

#### `static bool checkBoolean(const char* value)`
- **Location**: OTDirect.ino:1614
- **Purpose**: Validate strict 0/1 input

#### 3-Strike Auto-Blacklist Helpers
- **Location**: OTDirect.ino:367-385
- `getUnknownCount(uint8_t msgId)` — Get 2-bit counter for MsgID
- `incUnknownCount(uint8_t msgId)` — Increment (saturates at 3)
- `clearUnknownCount(uint8_t msgId)` — Reset counter

#### Unknown-ID List Helpers
- **Location**: OTDirect.ino:348-353
- `isUnknownId(uint8_t msgId)` — Check if MsgID in UI table

## Dependencies

### Internal Dependencies
- `processOT(buf, len)` — OT parser callback; feeds formatted frame strings for parsing, MQTT publication, WebSocket broadcast, HA discovery
- `updateSetting(key, value)` — Persist settings changes to LittleFS
- `sendMQTTData(topic, value)` — Publish to MQTT
- `state.otd.*`, `state.otBus.*`, `state.debug.*` — Runtime state structure
- `settings.otd.*` — Persistent configuration structure
- `OTcurrentSystemState` — Parsed boiler data cache (populated by parser from OT frames)
- `DebugTln()`, `DebugTf()` — Telnet debug output

### External Dependencies
- **OpenTherm.h** (project-pinned library, `/src/libraries/OpenTherm/src/OpenTherm.h`)
  - Classes: `OpenTherm` (master/slave, async/sync modes)
  - Enums: `OpenThermMessageType`, `OpenThermMessageID`, `OpenThermResponseStatus`
  - Methods: `begin()`, `sendRequestAsync()`, `sendResponse()`, `process()`, `getLastResponse()`, `getLastResponseStatus()`, `isReady()`, `handleInterrupt()`, `buildRequest()`, `buildResponse()`, `getMessageType()`, `getDataID()`, `isValidResponse()`, `parity()`
- **Arduino Core (ESP32)**
  - `attachInterrupt()` — Register ISRs
  - `digitalWrite()`, `pinMode()`, `digitalRead()`
  - `millis()` — Timer
  - `delay()`, `delayMicroseconds()`
  - `snprintf_P()`, `strlcpy()`, `dtostrf()` — String utilities
  - `constrain()`, `abs()`, `pow()`, `powf()`
- **Hardware**
  - GPIO pins: `PIN_OT_MASTER_IN`, `PIN_OT_MASTER_OUT`, `PIN_OT_SLAVE_IN`, `PIN_OT_SLAVE_OUT`, `PIN_STEPUP_ENABLE`, `PIN_BYPASS_RELAY` (if enabled)
  - UART: Serial (for debug output)

## Operating Modes & Behavior

### Mode: Bypass (GW=0)
- **Thermostat**: Direct to boiler via relay; OT-direct master/slave not involved
- **Gateway**: Relay closed (thermostat bypasses ESP32)
- **Use case**: Emergency failover when OT-direct disabled
- **Precondition**: `HAS_BYPASS_RELAY && settings.otd.bHasBypassRelay`

### Mode: Gateway (GW=1, default)
- **Thermostat**: Sends to OT-direct slave
- **Master**: Forwards to boiler with optional overrides
- **Overrides**: UI (unknown-ID), SR (response), repeater (value injection)
- **Scheduler**: Runs periodic reads/writes to boiler
- **Use case**: Normal operation with thermostat + boiler

### Mode: Monitor (GW=M)
- **Thermostat**: Transparent pass-through (all frames unmodified)
- **Master**: Never injects own frames (scheduler skipped)
- **Overrides**: Disabled
- **Use case**: Logging/monitoring without interference

### Mode: Master (GW=2/S)
- **Thermostat**: Optional; if present, we respond from cache (we ARE the boiler)
- **Master**: Sole OT master, runs full scheduler
- **Slave**: Conditional (depends on `settings.otd.bEnableSlave`)
- **Use case**: Standalone control without thermostat

### Mode: Loopback (GW=L)
- **Thermostat**: Simulated; responses come from `otLoopbackData[]`
- **Master**: Simulated; requests handled instantly
- **Use case**: Testing full stack (parser, MQTT, WebSocket, HA) without hardware

## Key Intervals & Timing

| Interval Name           | Duration  | Purpose |
|-------------------------|-----------|---------|
| OT_STATUS_INTERVAL_MS   | 800 ms    | MsgID 0 status polling (mandatory, ≤1s per OT spec) |
| OT_TEMP_INTERVAL_MS     | 10 s      | Temperature, modulation, pressure reads |
| OT_SLOW_INTERVAL_MS     | 60 s      | Config, diagnostics, boundaries, counters |
| OT_WRITE_INTERVAL_MS    | 15 s      | Periodic writes (keep boiler values alive) |
| OT_DHW_PUSH_TIMEOUT_MS  | 30 s      | DHW push state machine timeout |
| OT_PI_INTERVAL_MS       | 60 s      | PI room compensation loop |
| FLAME_BUF_SIZE          | 180 min   | 3-hour rolling window (flame ratio) |
| Loop schedule check     | 100 ms    | Scheduler polling interval |
| Status update           | 1 s       | `updateOTDirectStatus()` cadence |

## Known Constraints & Edge Cases

1. **Manchester Encoding**: OT bus operates at ~1kHz; frame transmission takes ~150ms
2. **Thermostat Timeout**: Configurable via `settings.otd.iSetbackTimeout`; fail-safe applies setback (MsgID 1 override) if enabled
3. **3-Strike Auto-Blacklist**: MsgIDs returning UNKNOWN_DATA_ID ≥3 times are auto-disabled; manual `AA=MsgID` re-enables
4. **Response vs Request Paths**: Response modifiers (RM=) applied only in gateway mode; repeater overrides (value injection) applied in both gateway and master modes
5. **Loopback Mode**: Simulation is instant (no timing); useful for testing parser/MQTT without OT hardware
6. **PI Integrator**: Clipped to [-5, +5] K; decays at 0.95× when CH inactive; includes boost term for rapid warm-up
7. **Flame Ratio**: Pre-filled to 60s on first transition; 0 if not yet initialized
8. **Command Queue**: 12-frame limit; drops commands if full (logs error)
9. **Write Cache**: Periodic writes inactive until `valueSet=true` (first command sets value)
10. **Fake Room Setpoint (BS=)**: Intercepts thermostat's own MsgID 16 frame, replaces data bytes before forwarding (same mechanism as PIC gateway.asm:3019-3024)

## Testing & Verification

- **Loopback Mode**: `GW=L` enables full-stack testing with simulated boiler (parser, MQTT, WebSocket, HA discovery all functional)
- **Summary Mode**: `PS=1` emits comma-separated values matching PIC format (useful for scripting/monitoring)
- **Debug Output**: `state.debug.bOTmsg` enables telnet logging of OT frames and PI controller state
- **REST API**: `/api/v2/otdirect/status` returns JSON with schedule/override counts, mode, connectivity
- **MQTT Topics**:
  - `OTGW/otgw32/flame_duty_pct` — Flame on-time %
  - `OTGW/otgw32/flame_cycles_per_hour` — Flame cycle frequency
  - Standard OT topics populated by parser (temperature, modulation, etc.)

---

## File Locations & References

- **Main Implementation**: `/src/OTGW-firmware/OTDirect.ino`
- **OpenTherm Library**: `/src/libraries/OpenTherm/src/OpenTherm.h` (frame encoding, Manchester encoding, ISR-based state machine)
- **OTGWSerial Library**: `/src/libraries/OTGWSerial/OTGWSerial.h` (PIC communication — not used in OT-direct, included for reference)
- **Settings Structure**: `OTGW-firmware.h` (`settings.otd.*`)
- **State Structure**: `OTGW-firmware.h` (`state.otd.*`, `state.otBus.*`)
- **Parser Integration**: `OTGW-Core.ino` (`processOT()` callback)
- **MQTT Integration**: `MQTTstuff.ino` (`sendMQTTData()`)

---

## Implementation Notes

### Async Non-Blocking Design
The OTDirect module never blocks the main scheduler. Boiler communication uses OpenTherm's `sendRequestAsync()` method with completion checks via `isReady()` in the main loop. Thermostat frames are processed as they arrive on the slave ISR; responses are sent asynchronously from `handleMasterResponse()` or `handleMasterModeSlaveFrame()`.

### Frame Bridging Pattern
All 32-bit OT frames (request, response, gateway-generated) are formatted as 9-character hex strings and fed through `processOT()`, which reuses the entire existing stack (parser, MQTT, REST, WebSocket, HA discovery). This design eliminates code duplication and ensures consistent handling across all communication paths.

### Command Emulation Strategy
The `handleOTDirectCommand()` function implements full PIC command compatibility by translating text commands to OpenTherm frames, state changes, or local overrides. Commands are processed immediately or queued (for boiler transmission) and synthesized responses are generated to maintain the illusion of a real PIC gateway.

### PI Controller for Room Compensation
The room temperature feedback loop (TASK-183) implements proportional-integral-boost control with low-pass filtering, setpoint change detection, and anti-windup clipping. The heating curve is weather-compensated (outside temperature dependent) and runs independently of the main OT schedule.

### Flame Ratio Tracking
The flame on-time and cycle frequency (TASK-184) are tracked in a 180-minute rolling window (3 hours), matching OT-Thing behavior. A ring buffer structure with running sums provides O(1) updates and efficient duty-cycle/frequency calculations.
