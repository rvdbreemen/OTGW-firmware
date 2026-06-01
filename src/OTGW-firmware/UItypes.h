/*
***************************************************************************
**  Program  : UItypes.h
**  Version  : v2.0.0-alpha.128
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for web-UI preferences (ADR-079).
**  Contents:
**    - UISection (settings.ui — toggle switches + graph window)
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

struct UISection {
  bool bAutoScroll      = true;
  bool bShowTimestamp   = true;
  bool bCaptureMode     = false;
  bool bAutoScreenshot  = false;
  bool bAutoDownloadLog = false;
  bool bAutoExport      = false;
  int  iGraphTimeWindow = 60;      // Default to 1 Hour (60 minutes)
};
