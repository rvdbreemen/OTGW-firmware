/*
***************************************************************************
**  Program  : S0types.h
**  Version  : v2.0.0-alpha.316
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for the S0 pulse-counting input (ADR-079).
**  Contents:
**    - S0Section (settings.s0 — GPIO pin, debounce, pulses/kW, interval)
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

struct S0Section {
  bool     bEnabled      = false;
  uint8_t  iPin          = 12;     // GPIO 12 = D6, preferred
  uint16_t iDebounceTime = 80;     // Depending on S0 switch
  uint16_t iPulsekw      = 1000;   // Most S0 counters have 1000 pulses per kW
  uint16_t iInterval     = 60;     // Suggested measurement reporting interval
};
