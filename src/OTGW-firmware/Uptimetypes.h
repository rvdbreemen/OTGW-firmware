/*
***************************************************************************
**  Program  : Uptimetypes.h
**  Version  : v2.0.0-alpha.10
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for system longevity counters (ADR-079).
**  Contents:
**    - UptimeSection (state.uptime — uptime seconds + reboot count)
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

struct UptimeSection {         // state.uptime — System longevity counters
  uint32_t iSeconds      = 0;  // was upTimeSeconds
  uint32_t iRebootCount  = 0;  // was rebootCount
};
