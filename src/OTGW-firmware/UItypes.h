/*
***************************************************************************
**  Program  : UItypes.h
**  Version  : v2.0.0-alpha.343
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for web-UI preferences (ADR-079).
**  Contents:
**    - UISection (settings.ui — toggle switches + graph window)
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
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
  bool bUseV2           = false;   // TASK-908: device-wide default UI (false=classic, true=v2 redesign)
  bool bOnboarded       = false;   // TASK-997: first-time-setup wizard shown once; set true on finish/skip. Re-runnable from Settings.
  bool bSatOnboarded    = false;   // TASK-1012: SAT onboarding wizard shown once; set true on finish (enable path) or dismiss (migrate path). Re-runnable. Defaults false on every install so existing SAT users get it once.
  int  iGraphTimeWindow = 60;      // Default to 1 Hour (60 minutes)
};
