/*
***************************************************************************
**  Program  : OTBustypes.h
**  Version  : v2.0.0-alpha.544
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for OpenTherm bus state (ADR-079).
**  Contents:
**    - OTBusState (state.otBus — OpenTherm protocol & bus traffic state)
**
**  Semantic name "otBus" refers to OT-bus traffic, not the gateway as a
**  whole. See ADR-051 + the OTBusState docstring for the rationale.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>
#include <time.h>

struct OTBusState {          // state.otBus — OpenTherm protocol & bus state (semantic name: OT bus traffic, not the gateway as a whole)
  bool bOnline           = false;  // was bOTGWonline — serial link alive
  bool bPSmode           = false;  // was bPSmode — Print Summary mode (PS=1)
  bool bGatewayMode      = false;  // was bOTGWgatewaystate — true=gateway, false=monitor
  bool bGatewayModeKnown = false;  // was bOTGWgatewaystateKnown
  bool bBoilerState      = false;  // was bOTGWboilerstate — CH/boiler active
  bool bThermostatState  = false;  // was bOTGWthermostatstate
  // Epoch (s) of the last frame seen from each side. Stamped by the PIC/OT-frame
  // parser (OTGW-Core.ino); 0 = never seen since boot. Drives the 30 s connected
  // window AND the v2 connectivity per-link recency / "degraded/stale" state (ADR-155).
  time_t tBoilerLastSeen     = 0;
  time_t tThermostatLastSeen = 0;
  // Epoch (s) of the last B frame that is boiler evidence: not an OT-Direct
  // loopback answer and not a replayed /otgw_simulation.log line. Only SAT's
  // availability gate reads it (otRealBoilerSeenRecently(), ADR-117 section 2),
  // so a simulated boiler never switches SAT simulation off (TASK-1185).
  time_t tRealBoilerLastSeen = 0;
  // millis() of the last B frame, any kind (Data-Invalid, loopback and replay
  // included, like tBoilerLastSeen); 0 = never. SAT's OpenTherm-loss check reads
  // it (TASK-1365): millis() does not jump at NTP sync, time() does.
  uint32_t iBoilerLastSeenMs = 0;
  // millis() of the last room temperature (MsgID 24) the THERMOSTAT wrote, from a
  // live T frame or the PS=1 summary; 0 = never. Gateway-originated MsgID 24 frames
  // (the master scheduler re-sending its write cache, TR= overrides) do not stamp
  // it, so a vanished thermostat's value ages out for SAT (TASK-1157).
  uint32_t iTrThermostatMs   = 0;
  // millis() of the last live boiler Read-Ack for MsgID 27 (outside temperature); 0 = never. The PS=1
  // summary does not stamp it: it repeats stored values with no age (ADR-181). SAT counts MsgID 27 as a
  // valid outside temperature only while this is younger than 1 hour (TASK-1390, entries 57, 126).
  uint32_t iToutsideBoilerMs = 0;
};
