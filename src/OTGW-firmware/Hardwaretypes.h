/*
***************************************************************************
**  Program  : Hardwaretypes.h
**  Version  : v2.0.0-alpha.281
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for detected hardware capabilities (ADR-079).
**  Contents:
**    - OTGWHardwareMode enum (HW_MODE_PIC / OT_DIRECT / DEGRADED / UNKNOWN)
**    - HardwareSection (state.hw — runtime-detected capabilities)
**
**  Requires HAS_ETH_CAPABLE from boards.h; must therefore be included
**  AFTER boards.h in OTGW-firmware.h. (bOLEDPresent is unconditional: OLED
**  presence is detected at runtime, not gated at compile time — ADR-114.)
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

//===================[ Hardware Mode — detected at boot ]===================
enum OTGWHardwareMode : uint8_t {
  HW_MODE_UNKNOWN   = 0,   // Not yet detected
  HW_MODE_PIC       = 1,   // PIC16F co-processor on UART (traditional OTGW)
  HW_MODE_OT_DIRECT = 2,   // Direct GPIO OpenTherm via opentherm_library (OTGW32)
  HW_MODE_DEGRADED  = 3,   // Hardware detected but non-functional (OT bus dead, PIC missing)
};

struct HardwareSection {       // state.hw — detected hardware capabilities
  OTGWHardwareMode eMode       = HW_MODE_UNKNOWN;
  bool bOLEDPresent            = false;   // runtime I2C probe at 0x3C (ADR-114) — any board
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  bool bEthernetPresent        = false;
#endif
#if HAS_RUNTIME_HW_DETECT
  bool bClassicPro             = false;   // ADR-157: LOLIN S3 Mini Pro in the Classic socket (IMU-detected; different Classic pin map)
#endif
};
