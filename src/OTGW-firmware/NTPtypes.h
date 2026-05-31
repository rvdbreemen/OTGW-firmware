/*
***************************************************************************
**  Program  : NTPtypes.h
**  Version  : v2.0.0-alpha.114
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for NTP time-sync configuration (ADR-079).
**  Contents:
**    - NTPSection (settings.ntp — NTP host, timezone, send-time toggle)
**
**  Requires NTP_DEFAULT_TIMEZONE and NTP_HOST_DEFAULT from OTGW-firmware.h;
**  must therefore be included AFTER the #define block in that header.
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

struct NTPSection {
  bool bEnable        = true;
  char sTimezone[65]  = NTP_DEFAULT_TIMEZONE;
  char sHostname[65]  = NTP_HOST_DEFAULT;
  bool bSendtime      = false;
};
