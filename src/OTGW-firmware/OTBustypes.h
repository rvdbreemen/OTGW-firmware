/*
***************************************************************************
**  Program  : OTBustypes.h
**  Version  : v2.0.0-alpha.317
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
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
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
};
