/*
***************************************************************************
**  Program  : Flashtypes.h
**  Version  : v2.0.0-beta.241
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for firmware-upgrade operations (ADR-079).
**  Contents:
**    - FlashSection (state.flash — ESP/PIC upgrade progress + errors)
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

struct FlashSection {          // state.flash — Firmware upgrade operations
  bool bESPactive        = false;  // was isESPFlashing
  bool bPICactive        = false;  // was isPICFlashing
  bool bFsImageWriting   = false;  // TASK-1909: a filesystem image is being written; LittleFS is unmounted on purpose
  char sError[129]       = "";     // was errorupgrade
  char sPICfile[65]      = "";     // was currentPICFlashFile
  int  iPICprogress      = 0;      // was currentPICFlashProgress — percent 0-100
};
