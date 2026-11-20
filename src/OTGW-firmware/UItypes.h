/*
***************************************************************************
**  Program  : UItypes.h
**  Version  : v2.0.0-beta.403
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for web-UI preferences (ADR-079).
**  Contents:
**    - UISection (settings.ui — first-time-setup wizard flags)
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

// TASK-2023 (ADR-262): the web UI checks the GitHub releases API from the BROWSER for a newer firmware (the gateway itself makes no
// outbound request, ADR-003). The default is one constant: 0 = off and silent, 1 = off and the UI asks once,
// 2 = on without asking. Robert chose 1 (option B, DM 1572847348177047675, 2026-11-19).
#define UI_UPDATE_CHECK_DEFAULT_MODE 1

struct UISection {
  bool bOnboarded       = false;   // TASK-997: first-time-setup wizard shown once; set true on finish/skip. Re-runnable from Settings.
  bool bSatOnboarded    = false;   // TASK-1012: SAT onboarding wizard shown once; set true on finish (enable path) or dismiss (migrate path). Re-runnable. Defaults false on every install so existing SAT users get it once.
  bool bUpdateCheck      = (UI_UPDATE_CHECK_DEFAULT_MODE == 2);   // TASK-2023: the browser checks GitHub for a newer release
  bool bUpdateCheckAsked = (UI_UPDATE_CHECK_DEFAULT_MODE != 1);   // TASK-2023: the one-time question was answered (or is not asked)
  bool bUpdateCheckBeta  = false;   // TASK-2023: a stable build is also told about newer betas (a beta build always is)
};
