/*
***************************************************************************
**  Program  : Debugtypes.h
**  Version  : v2.0.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for runtime diagnostic flags (ADR-079).
**  Contents:
**    - DebugSection (state.debug — per-subsystem trace toggles)
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

struct DebugSection {          // state.debug — Runtime diagnostic output flags
  bool     bOTmsg                 = true;   // was bDebugOTmsg — OpenTherm message trace
  bool     bRestAPI               = false;  // was bDebugRestAPI — REST API request trace
  bool     bMQTT                  = false;  // was bDebugMQTT — MQTT publish/receive trace
  bool     bMQTTGate              = false;  // MQTT gate decisions: interval/change-based publish logic
  bool     bSensors               = false;  // was bDebugSensors — Dallas sensor scan trace
  bool     bNTP                   = true;   // NTP time sync telemetry (on by default for diagnostics)
  bool     bSensorSim             = false;  // was bDebugSensorSimulation
  bool     bOTGWSimulation        = false;  // was bDebugOTGWSimulation
  bool     bSAT                   = true;   // SAT control loop + cycle + HCR trace (default on)
  bool     bOTDirect              = true;   // OTDirect frame handling + PI loop trace (default on)
  uint32_t iOTGWSimulationIntervalMs = 750;
  uint32_t iOTGWSimulationNextDueMs  = 0;
};
