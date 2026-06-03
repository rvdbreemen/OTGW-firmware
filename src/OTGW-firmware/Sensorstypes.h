/*
***************************************************************************
**  Program  : Sensorstypes.h
**  Version  : v2.0.0-alpha.154
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for Dallas DS18B20 sensors (ADR-079).
**  Contents:
**    - SensorsSection (settings.sensors — Dallas 1-Wire configuration)
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

struct SensorsSection {             // Dallas DS18B20 external sensors
  bool    bEnabled       = false;
  bool    bLegacyFormat  = false;   // Default to false (new standard format)
  int8_t  iPin           = 10;     // GPIO 13 = D7, GPIO 10 = SDIO 3
  int16_t iInterval      = 20;     // Interval time to read out temp and send to MQ
};
