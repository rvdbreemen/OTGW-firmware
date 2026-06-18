/*
***************************************************************************
**  Program  : PICtypes.h
**  Version  : v2.0.0-alpha.213
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for the PIC co-processor subsystem (ADR-079).
**  Bundles three related structs:
**    - PICSection        (state.pic         — identity and availability)
**    - PicSettingsSection (state.picSettings — values polled via PR= commands)
**    - PICBootSection    (settings.picBoot  — commands to inject at boot)
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

// TASK-865.14: state of the deferred (loop-context) PIC firmware update-check.
// The outbound HTTP HEAD to otgw.tclcode.com is no longer run on the AsyncTCP
// task; the async handler queues a check and the loop() worker fills these in.
// Values are small ints so the struct stays POD and ADR-004 char[]-only.
enum PicUpdateCheck : uint8_t {
  PIC_UPDATE_IDLE     = 0,  // no check requested / result consumed
  PIC_UPDATE_CHECKING = 1,  // queued or in flight on the loop worker
  PIC_UPDATE_READY    = 2,  // sLatestFw holds a fresh result
  PIC_UPDATE_ERROR    = 3,  // last check failed (host unreachable / non-200)
};

struct PICSection {            // state.pic — PIC microcontroller identity/status
  bool bAvailable     = false;           // was bPICavailable
  char sFwversion[32] = "no pic found";  // was sPICfwversion
  char sDeviceid[32]  = "no pic found";  // was sPICdeviceid
  char sType[32]      = "no pic found";  // was sPICtype

  // TASK-865.14: deferred update-check result cache. Written by the loop()-context
  // worker (handlePendingPicHttp) and read by the AsyncTCP REST handler
  // (sendPICUpdateCheck) with no barrier. This is intentionally eventual-consistent:
  // a torn read just yields one extra "checking" poll and self-heals on the next
  // one — same pattern as the accepted pendingUpgradePath single-slot bridge.
  char    sLatestFw[32]    = "";                 // latest version from the server ("" until first OK check)
  uint8_t iUpdateCheck     = PIC_UPDATE_IDLE;    // PicUpdateCheck state
};

struct PicSettingsSection {    // state.picSettings — settings polled from PIC via PR= commands
  // Source: Schelte Bron's OTGW firmware documentation (https://otgw.tclcode.com/firmware.html)
  // PR=A (About/version) is handled by getpicfwversion(); PR=M (mode) by queryOTGWgatewaymode().
  // All other PR= reports are polled on-demand by queryNextPICsetting(), one per 3s tick.

  // --- Active settings (most useful for HA integration) ---
  char sSetpointOverride[16]  = "";  // PR=O: setpoint override ("T20.5" TT active, "C20.5" TC active, "N" none)
  char sSetback[16]           = "";  // PR=S: setback temperature (SB command value, e.g. "15.0")
  char sDhwOverride[8]        = "";  // PR=W: DHW/hot-water override ("0"=off, "1"=on, "A"=auto)

  // --- Hardware configuration ---
  char sGpio[8]               = "";  // PR=G: GPIO A+B function codes (two digits, e.g. "05")
  char sGpioStates[8]         = "";  // PR=I: GPIO A+B current input states (two digits, e.g. "00")
  char sLed[8]                = "";  // PR=L: LED A–F function chars (six chars, e.g. "RFFTTT")
  char sTweaks[8]             = "";  // PR=T: tweaks (two chars: ignore_transitions + ovrd_high_byte)
  char sTempSensor[4]         = "";  // PR=D: external temp sensor function ("O"=outside, "R"=return; v5+ only)
  char sSmartPower[16]        = "";  // PR=P: smart power mode ("L"/"Low power", "M"/"Medium power", "H"/"High power", "N"/"Normal power")
  char sThermostatDetect[8]   = "";  // PR=R: thermostat detection setting

  // --- Diagnostics ---
  char sBuilddate[24]         = "";  // PR=B: firmware build date/time (e.g. "17:52 12-03-2023")
  char sClockMHz[8]           = "";  // PR=C: PIC clock speed in MHz (e.g. "4", "4 MHz")
  char sResetCause[4]         = "";  // PR=Q: last reset cause ("W"=watchdog, "B"=brownout, "P"=power-on)
  char sStandaloneInterval[8] = "";  // PR=N: message interval in standalone mode (seconds)
  char sVoltageRef[4]         = "";  // PR=V: voltage reference setting (numeric)
};

struct PICBootSection {            // PIC boot-time command injection
  bool bEnable        = false;
  char sCommands[129] = "";
};
