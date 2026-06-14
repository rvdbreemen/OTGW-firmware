/*
***************************************************************************
**  Program  : OTDirecttypes.h
**  Version  : v2.0.0-alpha.184
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for the OT-direct (OTGW32) subsystem.
**  Extracted from OTGW-firmware.h per ADR-079. Bundles every OTDirect
**  type declaration in one place:
**
**    - OTDirectRequestOrigin enum (used by OTDirect.ino's async
**      master request helpers)
**    - OTDirectMode enum (gateway/monitor/bypass/master/loopback)
**    - OTDirectSection    (state.otd — transient runtime status)
**    - OTDirectSettingsSection (settings.otd — persisted configuration)
**
**  The whole file is a no-op on builds where HAS_DIRECT_OT is 0 (ESP8266
**  without OT-direct hardware), matching the original inline guards.
**
**  Naming and Hungarian-prefix conventions come from ADR-051 (unchanged).
**  Accessors still use state.otd.<field> / settings.otd.<field>; this
**  file only controls where the types are declared.
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT

//====================================================================
//=== OT-direct runtime enums ===
//====================================================================

// Origin of a master-side OT request dispatched by OTDirect.ino.
enum OTDirectRequestOrigin : uint8_t {
  OT_DIRECT_ORIGIN_GATEWAY = 0,
  OT_DIRECT_ORIGIN_THERMOSTAT
};

// OT-direct operating modes (gateway perspective)
enum OTDirectMode : uint8_t {
  OTD_MODE_GATEWAY  = 1,   // Full gateway: scheduler + thermostat forwarding + overrides (default)
  OTD_MODE_MONITOR  = 2,   // Transparent: forward all frames unmodified, log everything
  OTD_MODE_BYPASS   = 0,   // Thermostat direct to boiler via relay, OT-direct inactive
  OTD_MODE_MASTER   = 3,   // Sole OT master: scheduler only, no thermostat expected
  OTD_MODE_LOOPBACK = 4,   // Internal test: simulated boiler responses, no hardware needed
};

// TASK-466: PIC-style remote-override mode for TT=/TC= room-setpoint commands.
// MsgID 16 (TrSet) carries the actual setpoint value; MsgID 100
// (RemoteOverrideFunction) low byte carries flags telling the thermostat how
// to treat it. PIC distinguishes:
//   TT (temporary) -> Program override priority bit set (bit1 = 0x02)
//   TC (constant)  -> Manual  override priority bit set (bit0 = 0x01)
enum OTRemoteOverrideMode : uint8_t {
  OT_OVERRIDE_NONE      = 0,
  OT_OVERRIDE_TEMPORARY = 1,   // TT=
  OT_OVERRIDE_CONSTANT  = 2,   // TC=
};

// TASK-466: runtime state for the TT=/TC= remote-override state machine.
// Not persisted: matches PIC behaviour where heartbeat resets clear overrides.
struct OTRemoteOverrideState {
  OTRemoteOverrideMode mode;
  uint16_t f88Value;            // current override value (f8.8 of MsgID 16)
  uint32_t setAtMs;             // millis() when override was set
  uint32_t lastThermostatMs;    // millis() of last MsgID 16 frame from thermostat
  uint16_t lastThermostatVal;   // last thermostat-reported MsgID 16 value (f8.8); valid only when bHaveLastThermostat=true
  bool     bHaveLastThermostat; // TASK-498 (1A-M2): true after first observed thermostat MsgID 16; replaces the 0xFFFF magic-value sentinel that collided with a valid -0.0039 °C
  uint8_t  honoredCount;        // number of cycles thermostat echoed our override
};

//====================================================================
//=== state.otd — transient OT-direct runtime status ===
//====================================================================

struct OTDirectSection {       // state.otd — OT-direct (OTGW32) runtime status
  uint8_t  iScheduleTotal    = 0;   // total schedule entries
  uint8_t  iScheduleActive   = 0;   // entries not disabled by boiler
  uint8_t  iScheduleDisabled = 0;   // entries disabled (UNKNOWN_DATA_ID)
  uint8_t  iOverrideCount    = 0;   // number of active write overrides
  bool     bBypassActive     = false; // true = thermostat direct to boiler (relay)
  bool     bStepUpEnabled    = false; // 24V step-up converter on
  bool     bMonitorMode      = false; // true = transparent pass-through, no overrides applied
  OTDirectMode eMode         = OTD_MODE_GATEWAY; // current operating mode
  bool     bMasterMode       = false;    // true = standalone master, no thermostat
  bool     bThermostatConnected = false; // thermostat recently seen (within timeout)
  bool     bSetbackActive    = false;    // thermostat disconnected → setback override engaged
  uint32_t iLastThermostatMs = 0;        // millis() of last thermostat frame received
  // TASK-466: PIC-style TT=/TC= remote-override mode (none/temporary/constant)
  OTRemoteOverrideMode eOverrideMode = OT_OVERRIDE_NONE;
  uint16_t iOverrideF88      = 0;        // f8.8 value of the active TT/TC override (0 when none)
  // TASK-582: CH hysteresis suspension state
  bool     bCHSuspended      = false;    // true = CH suspended by hysteresis deadband logic
};

//====================================================================
//=== settings.otd — persisted OT-direct configuration ===
//====================================================================

struct OTDirectSettingsSection {
  uint8_t  iMode              = 1;     // OTD_MODE_GATEWAY default, persisted across reboot
  bool     bAutoDetect        = true;  // Auto-detect thermostat presence at boot
  float    fSetbackTemp       = 16.0f; // Setback temp on thermostat disconnect (°C)
  uint8_t  iSetbackTimeout    = 30;    // Seconds before thermostat considered disconnected
  bool     bEnableSlave       = true;  // Enable slave interface in master mode
  bool     bSummerMode        = false; // SM= summer mode (bit5 of master status)
  bool     bFailSafe          = true;  // FS= fail-safe setback on thermostat disconnect
  uint16_t iMsgInterval       = 100;   // MI= minimum inter-message gap (ms, 100-1275)
  bool     bHasBypassRelay    = false; // Runtime: bypass relay present on this board
  // --- TASK-183: PI room compensation + weather-compensated heating curve ---
  uint8_t  iCHMode            = 1;     // 0=off, 1=fixed flow, 2=heating curve (AUTO)
  float    fFlowTemp          = 45.0f; // Fixed flow temp for CH mode=fixed (°C)
  float    fFlowMax           = 75.0f; // Maximum flow temperature (°C)
  float    fRoomSetpoint      = 20.0f; // Default room setpoint for heating curve (°C)
  float    fGradient          = 1.5f;  // Heating curve gradient
  float    fExponent          = 1.0f;  // Heating curve exponent (1.0 = linear)
  float    fOffset            = 0.0f;  // Heating curve offset (°C)
  bool     bRoomCompEnabled   = false; // PI room compensation enabled
  float    fKp                = 5.0f;  // Proportional gain (K/K)
  float    fKi                = 0.5f;  // Integral gain (1/h)
  float    fKboost            = 2.0f;  // Boost gain (K/K), applied when error > 1°C
  // --- TASK-582: CH hysteresis deadband ---
  bool     bHysteresisEnable  = false; // Enable CH suspension hysteresis (false = backward compat)
  float    fHysteresis        = 0.5f;  // Total deadband width in °C (range 0.0-2.0); each side = fHysteresis/2
  // --- TASK-584: ventilation override persistence ---
  bool     bVentEnable        = false; // Ventilation control enabled (OT MsgID 70 HB bit0)
  bool     bOpenBypass        = false; // Open bypass valve (OT MsgID 70 HB bit2)
  bool     bAutoBypass        = false; // Automatic bypass valve mode (OT MsgID 70 HB bit3)
  bool     bFreeVentEnable    = false; // Free ventilation mode (OT MsgID 70 HB bit4)
  uint8_t  iVentSetpoint      = 0;     // Ventilation setpoint 0-100% (OT MsgID 71 HB)
};

#endif // HAS_DIRECT_OT
