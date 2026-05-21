/*
***************************************************************************
**  Program  : Devicetypes.h
**  Version  : v2.0.0-alpha.50
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for Home Assistant device-registry identity
**  (ADR-079). Contents:
**    - DeviceSection (settings.device — manufacturer + model strings)
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

// Hardware identity for HA device registry discovery.
// Defaults set per platform; user can override via settings.ini or web UI.
struct DeviceSection {
  char sManufacturer[32] = "NodoShop";
  char sModel[32]        = "OTGW";
};
