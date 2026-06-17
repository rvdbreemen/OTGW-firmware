/*
***************************************************************************
**  Program  : OTBustypes.h
**  Version  : v2.0.0-alpha.203
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

struct OTBusState {          // state.otBus — OpenTherm protocol & bus state (semantic name: OT bus traffic, not the gateway as a whole)
  bool bOnline           = false;  // was bOTGWonline — serial link alive
  bool bPSmode           = false;  // was bPSmode — Print Summary mode (PS=1)
  bool bGatewayMode      = false;  // was bOTGWgatewaystate — true=gateway, false=monitor
  bool bGatewayModeKnown = false;  // was bOTGWgatewaystateKnown
  bool bBoilerState      = false;  // was bOTGWboilerstate — CH/boiler active
  bool bThermostatState  = false;  // was bOTGWthermostatstate
};
