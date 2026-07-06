/*
***************************************************************************
**  Program  : Outputstypes.h
**  Version  : v2.0.0-alpha.330
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for GPIO relay outputs (ADR-079).
**  Contents:
**    - OutputsSection (settings.outputs — relay GPIO + trigger bit)
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

struct OutputsSection {             // GPIO relay outputs
  bool   bEnabled    = false;
  int8_t iPin        = 16;
  int8_t iTriggerBit = 0;
};
